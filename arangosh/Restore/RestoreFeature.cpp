////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "RestoreFeature.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>
#include <boost/algorithm/clamp.hpp>

#include <chrono>
#include <thread>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Shell/ClientFeature.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Ssl/SslInterface.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Encryption/EncryptionFeature.h"
#endif

namespace {

/// @brief name of the feature to report to application server
constexpr auto FeatureName = "Restore";

/// @brief check whether HTTP response is valid, complete, and not an error
arangodb::Result checkHttpResponse(
    arangodb::httpclient::SimpleHttpClient& client,
    std::unique_ptr<arangodb::httpclient::SimpleHttpResult>& response,
    char const* requestAction,
    std::string const& originalRequest) {
  using arangodb::basics::StringUtils::itoa;
  if (response == nullptr || !response->isComplete()) {
    return {TRI_ERROR_INTERNAL,
        "got invalid response from server: '" +
        client.getErrorMessage() +
        "' while executing '" +
        requestAction +
        "' with this payload: '" +
        originalRequest +
        "'"
        };
  }
  if (response->wasHttpError()) {
    int errorNum = TRI_ERROR_INTERNAL;
    std::string errorMsg = response->getHttpReturnMessage();
    std::shared_ptr<arangodb::velocypack::Builder> bodyBuilder(response->getBodyVelocyPack());
    arangodb::velocypack::Slice error = bodyBuilder->slice();
    if (!error.isNone() && error.hasKey(arangodb::StaticStrings::ErrorMessage)) {
      errorNum = error.get(arangodb::StaticStrings::ErrorNum).getNumericValue<int>();
      errorMsg = error.get(arangodb::StaticStrings::ErrorMessage).copyString();
    }
    return {errorNum,
        "got invalid response from server: HTTP " +
        itoa(response->getHttpReturnCode()) + ": '" +
        errorMsg +
        "' while executing '" +
        requestAction +
        "' with this payload: '" +
        originalRequest +
        "'"};
  }
  return {TRI_ERROR_NO_ERROR};
}

/// @brief Sort collections for proper recreation order
bool sortCollections(VPackBuilder const& l, VPackBuilder const& r) {
  VPackSlice const left = l.slice().get("parameters");
  VPackSlice const right = r.slice().get("parameters");

  // First we sort by shard distribution.
  // We first have to create the collections which have no dependencies.
  // NB: Dependency graph has depth at most 1, no need to manage complex DAG
  VPackSlice leftDist = left.get("distributeShardsLike");
  VPackSlice rightDist = right.get("distributeShardsLike");
  if (leftDist.isNone() && !rightDist.isNone()) {
    return true;
  }
  if (rightDist.isNone() && !leftDist.isNone()) {
    return false;
  }

  // Next we sort by collection type so that vertex collections are recreated
  // before edge, etc.
  int leftType =
      arangodb::basics::VelocyPackHelper::getNumericValue<int>(left, "type", 0);
  int rightType = arangodb::basics::VelocyPackHelper::getNumericValue<int>(
      right, "type", 0);
  if (leftType != rightType) {
    return leftType < rightType;
  }

  // Finally, sort by name so we have stable, reproducible results
  std::string leftName =
      arangodb::basics::VelocyPackHelper::getStringValue(left, "name", "");
  std::string rightName =
      arangodb::basics::VelocyPackHelper::getStringValue(right, "name", "");

  return strcasecmp(leftName.c_str(), rightName.c_str()) < 0;
}

/// @brief Create the database to restore to, connecting manually
arangodb::Result tryCreateDatabase(std::string const& name) {
  using arangodb::httpclient::SimpleHttpClient;
  using arangodb::httpclient::SimpleHttpResult;
  using arangodb::rest::RequestType;
  using arangodb::rest::ResponseCode;
  using arangodb::velocypack::ArrayBuilder;
  using arangodb::velocypack::ObjectBuilder;

  // get client feature for configuration info
  auto client = arangodb::application_features::ApplicationServer::getFeature<
      arangodb::ClientFeature>("Client");
  TRI_ASSERT(nullptr != client);

  // get httpclient by hand rather than using manager, to bypass any built-in
  // checks which will fail if the database doesn't exist
  std::unique_ptr<SimpleHttpClient> httpClient;
  try {
    httpClient = client->createHttpClient();
    httpClient->params().setLocationRewriter(
        static_cast<void*>(client), arangodb::ClientManager::rewriteLocation);
    httpClient->params().setUserNamePassword("/", client->username(),
                                             client->password());
  } catch (...) {
    LOG_TOPIC(FATAL, arangodb::Logger::RESTORE)
        << "cannot create server connection, giving up!";
    return {TRI_SIMPLE_CLIENT_COULD_NOT_CONNECT};
  }

  VPackBuilder builder;
  {
    ObjectBuilder object(&builder);
    object->add("name", VPackValue(name));
    {
      ArrayBuilder users(&builder, "users");
      {
        ObjectBuilder user(&builder);
        user->add("username", VPackValue(client->username()));
        user->add("passwd", VPackValue(client->password()));
      }
    }
  }
  std::string const body = builder.slice().toJson();

  std::unique_ptr<SimpleHttpResult> response(httpClient->request(
      RequestType::POST, "/_api/database", body.c_str(), body.size()));
  if (response == nullptr || !response->isComplete()) {
    return {TRI_ERROR_INTERNAL};
  }

  auto returnCode = response->getHttpReturnCode();
  if (returnCode == static_cast<int>(ResponseCode::OK) ||
      returnCode == static_cast<int>(ResponseCode::CREATED)) {
    // all ok
    return {TRI_ERROR_NO_ERROR};
  }
  if (returnCode == static_cast<int>(ResponseCode::UNAUTHORIZED) ||
      returnCode == static_cast<int>(ResponseCode::FORBIDDEN)) {
    // invalid authorization
    auto res = ::checkHttpResponse(*httpClient, response, "creating database", body);
    return {TRI_ERROR_FORBIDDEN, res.errorMessage()};
  }

  // any other error
  auto res = ::checkHttpResponse(*httpClient, response, "creating database", body);
  return {TRI_ERROR_INTERNAL, res.errorMessage()};
}

/// @check If directory is encrypted, check that key option is specified
void checkEncryption(arangodb::ManagedDirectory& directory) {
  using arangodb::Logger;
  if (directory.isEncrypted()) {
#ifdef USE_ENTERPRISE
    if (!directory.encryptionFeature()->keyOptionSpecified()) {
      LOG_TOPIC(WARN, Logger::RESTORE)
          << "the dump data seems to be encrypted with "
          << directory.encryptionType()
          << ", but no key information was specified to decrypt the dump";
      LOG_TOPIC(WARN, Logger::RESTORE)
          << "it is recommended to specify either "
             "`--encryption.keyfile` or `--encryption.key-generator` "
             "when invoking arangorestore with an encrypted dump";
    } else {
      LOG_TOPIC(INFO, Logger::RESTORE)
          << "# using encryption type " << directory.encryptionType()
          << " for reading dump";
    }
#endif
  }
}

/// @brief Check the database name specified by the dump file
arangodb::Result checkDumpDatabase(arangodb::ManagedDirectory& directory,
                                   bool forceSameDatabase) {
  using arangodb::ClientFeature;
  using arangodb::Logger;
  using arangodb::application_features::ApplicationServer;

  std::string databaseName;
  try {
    VPackBuilder fileContentBuilder = directory.vpackFromJsonFile("dump.json");
    VPackSlice const fileContent = fileContentBuilder.slice();
    databaseName = fileContent.get("database").copyString();
  } catch (...) {
    // the above may go wrong for several reasons
  }

  if (!databaseName.empty()) {
    LOG_TOPIC(INFO, Logger::RESTORE)
        << "Database name in source dump is '" << databaseName << "'";
  }

  ClientFeature* client =
      ApplicationServer::getFeature<ClientFeature>("Client");
  if (forceSameDatabase && databaseName != client->databaseName()) {
    return {TRI_ERROR_BAD_PARAMETER,
            std::string("database name in dump.json ('") + databaseName +
                "') does not match specified database name ('" +
                client->databaseName() + "')"};
  }

  return {};
}

/// @brief Send the command to recreate a collection
arangodb::Result sendRestoreCollection(
    arangodb::httpclient::SimpleHttpClient& httpClient,
    arangodb::RestoreFeature::Options const& options, VPackSlice const& slice,
    std::string const& name) {
  using arangodb::Logger;
  using arangodb::httpclient::SimpleHttpResult;

  std::string url =
      "/_api/replication/restore-collection"
      "?overwrite=" +
      std::string(options.overwrite ? "true" : "false") +
      "&force=" + std::string(options.force ? "true" : "false") +
      "&ignoreDistributeShardsLikeErrors=" +
      std::string(options.ignoreDistributeShardsLikeErrors ? "true" : "false");

  if (options.clusterMode) {
    // check for cluster-specific parameters
    if (!slice.hasKey(std::vector<std::string>({"parameters", "shards"})) &&
        !slice.hasKey(
            std::vector<std::string>({"parameters", "numberOfShards"}))) {
      // no "shards" and no "numberOfShards" attribute present. now assume
      // default value from --default-number-of-shards
      LOG_TOPIC(WARN, Logger::RESTORE)
          << "# no sharding information specified for collection '" << name
          << "', using default number of shards "
          << options.defaultNumberOfShards;
      url += "&numberOfShards=" + std::to_string(options.defaultNumberOfShards);
    }
    if (!slice.hasKey(
            std::vector<std::string>({"parameters", "replicationFactor"}))) {
      // No replication factor given, so take the default:
      LOG_TOPIC(INFO, Logger::RESTORE)
          << "# no replication information specified for collection '" << name
          << "', using default replication factor "
          << options.defaultReplicationFactor;
      url += "&replicationFactor=" +
             std::to_string(options.defaultReplicationFactor);
    }
  }

  std::string const body = slice.toJson();

  std::unique_ptr<SimpleHttpResult> response(httpClient.request(
      arangodb::rest::RequestType::PUT, url, body.c_str(), body.size()));
  return ::checkHttpResponse(httpClient, response, "restoring collection", body);
}

/// @brief Send command to restore a collection's indexes
arangodb::Result sendRestoreIndexes(
    arangodb::httpclient::SimpleHttpClient& httpClient,
    arangodb::RestoreFeature::Options const& options, VPackSlice const& slice) {
  using arangodb::httpclient::SimpleHttpResult;

  std::string const url = "/_api/replication/restore-indexes?force=" +
                          std::string(options.force ? "true" : "false");
  std::string const body = slice.toJson();

  std::unique_ptr<SimpleHttpResult> response(httpClient.request(
      arangodb::rest::RequestType::PUT, url, body.c_str(), body.size()));
  return ::checkHttpResponse(httpClient, response, "restoring indexes", body);
}

/// @brief Send a command to restore actual data
arangodb::Result sendRestoreData(
    arangodb::httpclient::SimpleHttpClient& httpClient,
    arangodb::RestoreFeature::Options const& options, std::string const& cname,
    char const* buffer, size_t bufferSize) {
  using arangodb::basics::StringUtils::urlEncode;
  using arangodb::httpclient::SimpleHttpResult;

  std::string const url =
      "/_api/replication/restore-data?collection=" + urlEncode(cname) +
      "&force=" + (options.force ? "true" : "false");

  std::unique_ptr<SimpleHttpResult> response(httpClient.request(
      arangodb::rest::RequestType::PUT, url, buffer, bufferSize));
  return ::checkHttpResponse(httpClient, response, "restoring data", "");
}

/// @brief Recreate a collection given its description
arangodb::Result recreateCollection(
    arangodb::httpclient::SimpleHttpClient& httpClient,
    arangodb::RestoreFeature::JobData& jobData) {
  using arangodb::Logger;

  arangodb::Result result;
  VPackSlice const parameters = jobData.collection.get("parameters");
  std::string const cname = arangodb::basics::VelocyPackHelper::getStringValue(
      parameters, "name", "");
  int type = arangodb::basics::VelocyPackHelper::getNumericValue<int>(
      parameters, "type", 2);
  std::string const collectionType(type == 2 ? "document" : "edge");

  // re-create collection
  if (jobData.options.progress) {
    if (jobData.options.overwrite) {
      LOG_TOPIC(INFO, Logger::RESTORE) << "# Re-creating " << collectionType
                                       << " collection '" << cname << "'...";
    } else {
      LOG_TOPIC(INFO, Logger::RESTORE) << "# Creating " << collectionType
                                       << " collection '" << cname << "'...";
    }
  }

  result = ::sendRestoreCollection(httpClient, jobData.options,
                                   jobData.collection, cname);

  if (result.fail()) {
    if (jobData.options.force) {
      LOG_TOPIC(WARN, Logger::RESTORE) << "Error while creating " << collectionType << " collection '" << cname << "': " << result.errorMessage();
      result.reset();
    } else {
      LOG_TOPIC(ERR, Logger::RESTORE) << "Error while creating " << collectionType << " collection '" << cname << "': " << result.errorMessage();
    }
  }
  return result;
}

/// @brief Restore a collection's indexes given its description
arangodb::Result restoreIndexes(
    arangodb::httpclient::SimpleHttpClient& httpClient,
    arangodb::RestoreFeature::JobData& jobData) {
  using arangodb::Logger;

  arangodb::Result result;
  VPackSlice const parameters = jobData.collection.get("parameters");
  VPackSlice const indexes = jobData.collection.get("indexes");
  // re-create indexes
  if (indexes.length() > 0) {
    // we actually have indexes
    if (jobData.options.progress) {
      std::string const cname =
          arangodb::basics::VelocyPackHelper::getStringValue(parameters, "name", "");
      LOG_TOPIC(INFO, Logger::RESTORE)
          << "# Creating indexes for collection '" << cname << "'...";
    }

    result =
        ::sendRestoreIndexes(httpClient, jobData.options, jobData.collection);
  
    if (result.fail()) {
      std::string const cname = arangodb::basics::VelocyPackHelper::getStringValue(parameters, "name", "");
      if (jobData.options.force) {
        LOG_TOPIC(WARN, Logger::RESTORE) << "Error while creating indexes for collection '" << cname << "': " << result.errorMessage();
        result.reset();
      } else {
        LOG_TOPIC(ERR, Logger::RESTORE) << "Error while creating indexes for collection '" << cname << "': " << result.errorMessage();
      }
    }
  }

  return result;
}

/// @brief Restore the data for a given collection
arangodb::Result restoreData(arangodb::httpclient::SimpleHttpClient& httpClient,
                             arangodb::RestoreFeature::JobData& jobData) {
  using arangodb::Logger;
  using arangodb::basics::StringBuffer;

  arangodb::Result result;
  StringBuffer buffer(true);

  VPackSlice const parameters = jobData.collection.get("parameters");
  std::string const cname = arangodb::basics::VelocyPackHelper::getStringValue(
      parameters, "name", "");
  int type = arangodb::basics::VelocyPackHelper::getNumericValue<int>(
      parameters, "type", 2);
  std::string const collectionType(type == 2 ? "document" : "edge");

  // import data. check if we have a datafile
  auto datafile = jobData.directory.readableFile(
      cname + "_" + arangodb::rest::SslInterface::sslMD5(cname) + ".data.json");
  if (!datafile || datafile->status().fail()) {
    datafile = jobData.directory.readableFile(cname + ".data.json");
    if (!datafile || datafile->status().fail()) {
      result = {TRI_ERROR_CANNOT_READ_FILE, "could not open file"};
      return result;
    }
  }

  int64_t const fileSize =  TRI_SizeFile(datafile->path().c_str());

  if (jobData.options.progress) {
    LOG_TOPIC(INFO, Logger::RESTORE) << "# Loading data into " << collectionType
                                     << " collection '" << cname << "', data size: " << fileSize << " byte(s)";
  }

  int64_t numReadForThisCollection = 0;
  int64_t numReadSinceLastReport = 0;

  buffer.clear();
  while (true) {
    if (buffer.reserve(16384) != TRI_ERROR_NO_ERROR) {
      result = {TRI_ERROR_OUT_OF_MEMORY, "out of memory"};
      return result;
    }

    ssize_t numRead = datafile->read(buffer.end(), 16384);
    if (datafile->status().fail()) {  // error while reading
      result = datafile->status();
      return result;
    }
    // we read something
    buffer.increaseLength(numRead);
    jobData.stats.totalRead += (uint64_t)numRead;
    numReadForThisCollection += numRead;
    numReadSinceLastReport += numRead;

    if (buffer.length() < jobData.options.chunkSize && numRead > 0) {
      continue;  // still continue reading
    }

    // do we have a buffer?
    if (buffer.length() > 0) {
      // look for the last \n in the buffer
      char* found =
          (char*)memrchr((const void*)buffer.begin(), '\n', buffer.length());
      size_t length;

      if (found == nullptr) {  // no \n in buffer...
        if (numRead == 0) {
          // we're at the end of the file, so send the complete buffer anyway
          length = buffer.length();
        } else {
          continue;  // don't have a complete line yet, read more
        }
      } else {
        length = found - buffer.begin();  // found a \n somewhere; break at line
      }


      jobData.stats.totalBatches++;
      result = ::sendRestoreData(httpClient, jobData.options, cname,
                                 buffer.begin(), length);
      jobData.stats.totalSent += length;

      if (result.fail()) {
        if (jobData.options.force) {
          LOG_TOPIC(WARN, Logger::RESTORE) << "Error while restoring data into collection '" << cname << "': " << result.errorMessage();
          result.reset();
          continue;
        } else {
          LOG_TOPIC(ERR, Logger::RESTORE) << "Error while restoring data into collection '" << cname << "': " << result.errorMessage();
        }
        return result;
      }

      buffer.erase_front(length);

      if (jobData.options.progress &&
          fileSize > 0 &&
          numReadSinceLastReport > 1024 * 1024 * 8) {
        // report every 8MB of transferred data
        LOG_TOPIC(INFO, Logger::RESTORE) << "# Still loading data into " << collectionType
                                         << " collection '" << cname << "', "
                                         << numReadForThisCollection << " of " << fileSize
                                         << " byte(s) restored (" << int(100. * double(numReadForThisCollection) / double(fileSize)) << " %)";
        numReadSinceLastReport = 0;
      }
    }

    if (numRead == 0) {  // EOF
      break;
    }
  }

  return result;
}

/// @brief Restore the data for a given view
arangodb::Result restoreView(arangodb::httpclient::SimpleHttpClient& httpClient,
                             arangodb::RestoreFeature::Options const& options,
                             VPackSlice const& viewDefinition) {
  using arangodb::httpclient::SimpleHttpResult;

  std::string url = "/_api/replication/restore-view?overwrite=" +
    std::string(options.overwrite ? "true" : "false") +
    "&force=" + std::string(options.force ? "true" : "false");

  std::string const body = viewDefinition.toJson();
  std::unique_ptr<SimpleHttpResult> response(httpClient.request(arangodb::rest::RequestType::PUT,
                                                                url, body.c_str(), body.size()));
  return ::checkHttpResponse(httpClient, response, "restoring view", body);
}

arangodb::Result triggerFoxxHeal(
    arangodb::httpclient::SimpleHttpClient& httpClient) {
  using arangodb::Logger;
  using arangodb::httpclient::SimpleHttpResult;
  const std::string FoxxHealUrl = "/_api/foxx/_local/heal";

  std::string body = "";

  std::unique_ptr<SimpleHttpResult> response(httpClient.request(
      arangodb::rest::RequestType::POST, FoxxHealUrl, body.c_str(), body.length()));
  return ::checkHttpResponse(httpClient, response, "trigger self heal", body);
}

arangodb::Result processInputDirectory(
    arangodb::httpclient::SimpleHttpClient& httpClient,
    arangodb::ClientTaskQueue<arangodb::RestoreFeature::JobData>& jobQueue,
    arangodb::RestoreFeature& feature,
    arangodb::RestoreFeature::Options const& options,
    arangodb::ManagedDirectory& directory,
    arangodb::RestoreFeature::Stats& stats) {
  using arangodb::Logger;
  using arangodb::Result;
  using arangodb::StaticStrings;
  using arangodb::basics::FileUtils::listFiles;
  using arangodb::basics::VelocyPackHelper;

  // create a lookup table for collections
  std::set<std::string> restrictColls, restrictViews;
  restrictColls.insert(options.collections.begin(), options.collections.end());
  restrictViews.insert(options.views.begin(), options.views.end());

  try {
    std::vector<std::string> const files = listFiles(options.inputPath);
    std::string const collectionSuffix = std::string(".structure.json");
    std::string const viewsSuffix = std::string(".view.json");
    std::vector<VPackBuilder> collections, views;

    // Step 1 determine all collections to process
    {
      // loop over all files in InputDirectory, and look for all structure.json
      // files
      for (std::string const& file : files) {
        size_t const nameLength = file.size();

        if (nameLength > viewsSuffix.size() &&
            file.substr(file.size() - viewsSuffix.size()) == viewsSuffix) {
          
          if (!restrictColls.empty() && restrictViews.empty()) {
            continue; // skip view if not specifically included
          }
          
          VPackBuilder contentBuilder = directory.vpackFromJsonFile(file);
          VPackSlice const fileContent = contentBuilder.slice();
          if (!fileContent.isObject()) {
            return {TRI_ERROR_INTERNAL,
              "could not read view file '" + directory.pathToFile(file) + "'"};
          }
          
          if (!restrictViews.empty()) {
            std::string const name = VelocyPackHelper::getStringValue(fileContent,
                                                                      StaticStrings::DataSourceName, "");
            if (restrictViews.find(name) == restrictViews.end()) {
              continue;
            }
          }
          
          views.emplace_back(std::move(contentBuilder));
          continue;
        }

        if (nameLength <= collectionSuffix.size() ||
            file.substr(file.size() - collectionSuffix.size()) != collectionSuffix) {
          // some other file
          continue;
        }

        // found a structure.json file
        std::string name = file.substr(0, file.size() - collectionSuffix.size());
        if (!options.includeSystemCollections && name[0] == '_') {
          continue;
        }

        VPackBuilder fileContentBuilder = directory.vpackFromJsonFile(file);
        VPackSlice const fileContent = fileContentBuilder.slice();
        if (!fileContent.isObject()) {
          return {TRI_ERROR_INTERNAL,
                  "could not read collection structure file '" +
                      directory.pathToFile(file) + "'"};
        }

        VPackSlice const parameters = fileContent.get("parameters");
        VPackSlice const indexes = fileContent.get("indexes");
        if (!parameters.isObject() || !indexes.isArray()) {
          return {TRI_ERROR_INTERNAL,
                  "could not read collection structure file '" +
                      directory.pathToFile(file) + "'"};
        }
        std::string const cname = VelocyPackHelper::getStringValue(parameters,
                                                                   StaticStrings::DataSourceName, "");
        bool overwriteName = false;
        if (cname != name &&
            name !=
                (cname + "_" + arangodb::rest::SslInterface::sslMD5(cname))) {
          // file has a different name than found in structure file
          if (options.importStructure) {
            // we cannot go on if there is a mismatch
            return {TRI_ERROR_INTERNAL,
                    "collection name mismatch in collection structure file '" +
                        directory.pathToFile(file) + "' (offending value: '" +
                        cname + "')"};
          } else {
            // we can patch the name in our array and go on
            LOG_TOPIC(INFO, Logger::RESTORE)
                << "ignoring collection name mismatch in collection "
                   "structure file '" +
                       directory.pathToFile(file) + "' (offending value: '" +
                       cname + "')";

            overwriteName = true;
          }
        }

        if (!restrictColls.empty() &&
            restrictColls.find(cname) == restrictColls.end()) {
          continue; // collection name not in list
        }

        if (overwriteName) {
          // TODO: we have a JSON object with sub-object "parameters" with
          // attribute "name". we only want to replace this. how?
        } else {
          collections.emplace_back(std::move(fileContentBuilder));
        }
      }
    }
    std::sort(collections.begin(), collections.end(), ::sortCollections);

    std::vector<std::unique_ptr<arangodb::RestoreFeature::JobData>> jobs(collections.size());

    bool didModifyFoxxCollection = false;
    // Step 2: create collections
    for (VPackBuilder const& b : collections) {
      VPackSlice const collection = b.slice();
      VPackSlice params = collection.get("parameters");
      if (params.isObject()) {
        params = params.get("name");
        // Only these two are relevant for FOXX.
        if (params.isString() && (params.isEqualString("_apps") || params.isEqualString("_appbundles"))) {
          didModifyFoxxCollection = true;
        }
      };


      auto jobData = std::make_unique<arangodb::RestoreFeature::JobData>(
          directory, feature, options, stats, collection);

      // take care of collection creation now, serially
      if (options.importStructure) {
        Result result = ::recreateCollection(httpClient, *jobData);
        if (result.fail()) {
          return result;
        }
      }
      stats.totalCollections++;

      jobs.push_back(std::move(jobData));
    }

    // Step 3: create views
    if (options.importStructure && !views.empty()) {
      LOG_TOPIC(INFO, Logger::RESTORE) << "# Creating views...";
      // Step 3: recreate all views
      for (VPackBuilder const& viewDefinition : views) {
        LOG_TOPIC(DEBUG, Logger::RESTORE) << "# Creating view: " << viewDefinition.toJson();
        Result res = ::restoreView(httpClient, options, viewDefinition.slice());
        if (res.fail()) {
          return res;
        }
      }
    }

    // Step 4: fire up data transfer
    for (auto &job : jobs) {
      if (!jobQueue.queueJob(std::move(job))) {
         return Result(TRI_ERROR_OUT_OF_MEMORY, "unable to queue restore job");
      }
    }

    // wait for all jobs to finish, then check for errors
    if (options.progress) {
      LOG_TOPIC(INFO, Logger::RESTORE) << "# Dispatched " << stats.totalCollections << " job(s) to " << options.threadCount << " worker(s)";

      double start = TRI_microtime();

      while (true) {
        if (jobQueue.isQueueEmpty() && jobQueue.allWorkersIdle()) {
          // done
          break;
        }

        double now = TRI_microtime();
        if (now - start >= 5.0) {
          // returns #queued jobs, #workers total, #workers busy
          auto queueStats = jobQueue.statistics();
          // periodically report current status, but do not spam user
          LOG_TOPIC(INFO, Logger::RESTORE)
              << "# Current restore progress: restored " << stats.restoredCollections
              << " of " << stats.totalCollections << " collection(s), read " << stats.totalRead << " byte(s) from datafiles, "
              << "sent " << stats.totalBatches << " data batch(es) of " << stats.totalSent << " byte(s) total size"
              << ", queued jobs: " << std::get<0>(queueStats) << ", workers: " << std::get<1>(queueStats);
          start = now;
        }

        // don't sleep for too long, as we want to quickly terminate
        // when the gets empty
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
      }
    }

    // should instantly return
    jobQueue.waitForIdle();

    Result firstError = feature.getFirstError();
    if (firstError.fail()) {
      return firstError;
    }
    
    if (didModifyFoxxCollection) {
      // if we get here we need to trigger foxx heal
      Result res = ::triggerFoxxHeal(httpClient);
      if (res.fail()) {
        LOG_TOPIC(WARN, Logger::RESTORE) << "Reloading of Foxx services failed. In the cluster Foxx services will be available eventually, On single servers send a POST to '/_api/foxx/_local/heal' on the current database, with an empty body.";
      }
    }

  } catch (std::exception const& ex) {
    return {TRI_ERROR_INTERNAL,
            std::string(
                "arangorestore terminated because of an unhandled exception: ")
                .append(ex.what())};
  } catch (...) {
    return {TRI_ERROR_OUT_OF_MEMORY, "arangorestore out of memory"};
  }
  return {TRI_ERROR_NO_ERROR};
}

/// @brief process a single job from the queue
arangodb::Result processJob(arangodb::httpclient::SimpleHttpClient& httpClient,
                            arangodb::RestoreFeature::JobData& jobData) {
  arangodb::Result result;
  if (jobData.options.indexesFirst && jobData.options.importStructure) {
    // restore indexes first if we are using rocksdb
    result = ::restoreIndexes(httpClient, jobData);
    if (result.fail()) {
      return result;
    }
  }
  if (jobData.options.importData) {
    result = ::restoreData(httpClient, jobData);
    if (result.fail()) {
      return result;
    }
  }
  if (!jobData.options.indexesFirst && jobData.options.importStructure) {
    // restore indexes second if we are using mmfiles
    result = ::restoreIndexes(httpClient, jobData);
    if (result.fail()) {
      return result;
    }
  }

  ++jobData.stats.restoredCollections;

  if (jobData.options.progress) {
    VPackSlice const parameters = jobData.collection.get("parameters");
    std::string const cname = arangodb::basics::VelocyPackHelper::getStringValue(
        parameters, "name", "");
    int type = arangodb::basics::VelocyPackHelper::getNumericValue<int>(
        parameters, "type", 2);
    std::string const collectionType(type == 2 ? "document" : "edge");
    LOG_TOPIC(INFO, arangodb::Logger::RESTORE)
                << "# Successfully restored " << collectionType << " collection '" << cname << "'";
  }

  return result;
}

/// @brief handle the result of a single job
void handleJobResult(
    std::unique_ptr<arangodb::RestoreFeature::JobData>&& jobData,
    arangodb::Result const& result) {
  if (result.fail()) {
    jobData->feature.reportError(result);
  }
}

}  // namespace

namespace arangodb {

RestoreFeature::JobData::JobData(ManagedDirectory& d, RestoreFeature& f,
                                 RestoreFeature::Options const& o,
                                 RestoreFeature::Stats& s, VPackSlice const& c)
    : directory{d}, feature{f}, options{o}, stats{s}, collection{c} {}

RestoreFeature::RestoreFeature(
    application_features::ApplicationServer& server,
    int& exitCode
)
    : ApplicationFeature(server, RestoreFeature::featureName()),
      _clientManager{Logger::RESTORE},
      _clientTaskQueue{::processJob, ::handleJobResult},
      _exitCode{exitCode} {
  requiresElevatedPrivileges(false);
  setOptional(false);
  startsAfter("BasicsPhase");

  using arangodb::basics::FileUtils::buildFilename;
  using arangodb::basics::FileUtils::currentDirectory;
  _options.inputPath = buildFilename(currentDirectory().result(), "dump");
}

void RestoreFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  using arangodb::options::BooleanParameter;
  using arangodb::options::StringParameter;
  using arangodb::options::UInt32Parameter;
  using arangodb::options::UInt64Parameter;
  using arangodb::options::VectorParameter;

  options->addOption(
      "--collection",
      "restrict to collection name (can be specified multiple times)",
      new VectorParameter<StringParameter>(&_options.collections));
  
  options->addOption(
      "--view",
      "restrict to view name (can be specified multiple times)",
     new VectorParameter<StringParameter>(&_options.views));

  options->addObsoleteOption(
      "--recycle-ids", "collection ids are now handled automatically", false);

  options->addOption("--batch-size",
                     "maximum size for individual data batches (in bytes)",
                     new UInt64Parameter(&_options.chunkSize));

  options->addOption("--threads",
                     "maximum number of collections to process in parallel",
                     new UInt32Parameter(&_options.threadCount));

  options->addOption("--include-system-collections",
                     "include system collections",
                     new BooleanParameter(&_options.includeSystemCollections));

  options->addOption("--create-database",
                     "create the target database if it does not exist",
                     new BooleanParameter(&_options.createDatabase));

  options->addOption(
      "--force-same-database",
      "force usage of the same database name as in the source dump.json file",
      new BooleanParameter(&_options.forceSameDatabase));

  options->addOption("--input-directory", "input directory",
                     new StringParameter(&_options.inputPath));

  options->addOption("--import-data", "import data into collection",
                     new BooleanParameter(&_options.importData));

  options->addOption("--create-collection", "create collection structure",
                     new BooleanParameter(&_options.importStructure));

  options->addOption("--progress", "show progress",
                     new BooleanParameter(&_options.progress));

  options->addOption("--overwrite", "overwrite collections if they exist",
                     new BooleanParameter(&_options.overwrite));

  options->addOption("--default-number-of-shards",
                     "default value for numberOfShards if not specified",
                     new UInt64Parameter(&_options.defaultNumberOfShards));

  options->addOption("--default-replication-factor",
                     "default value for replicationFactor if not specified",
                     new UInt64Parameter(&_options.defaultReplicationFactor));

  options->addOption(
      "--ignore-distribute-shards-like-errors",
      "continue restore even if sharding prototype collection is missing",
      new BooleanParameter(&_options.ignoreDistributeShardsLikeErrors));

  options->addOption(
      "--force", "continue restore even in the face of some server-side errors",
      new BooleanParameter(&_options.force));
}

void RestoreFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  using arangodb::basics::StringUtils::join;

  auto const& positionals = options->processingResult()._positionals;
  size_t n = positionals.size();

  if (1 == n) {
    _options.inputPath = positionals[0];
  } else if (1 < n) {
    LOG_TOPIC(FATAL, arangodb::Logger::RESTORE)
        << "expecting at most one directory, got " + join(positionals, ", ");
    FATAL_ERROR_EXIT();
  }

  // use a minimum value for batches
  if (_options.chunkSize < 1024 * 128) {
    _options.chunkSize = 1024 * 128;
  }

  auto clamped = boost::algorithm::clamp(_options.threadCount, uint32_t(1),
                                         uint32_t(4 * TRI_numberProcessors()));
  if (_options.threadCount != clamped) {
    LOG_TOPIC(WARN, Logger::RESTORE) << "capping --threads value to " << clamped;
    _options.threadCount = clamped;
  }
}

void RestoreFeature::prepare() {
  if (!_options.inputPath.empty() &&
      _options.inputPath.back() == TRI_DIR_SEPARATOR_CHAR) {
    // trim trailing slash from path because it may cause problems on ...
    // Windows
    TRI_ASSERT(_options.inputPath.size() > 0);
    _options.inputPath.pop_back();
  }

  if (!_options.importStructure && !_options.importData) {
    LOG_TOPIC(FATAL, arangodb::Logger::RESTORE)
        << "Error: must specify either --create-collection or --import-data";
    FATAL_ERROR_EXIT();
  }
}

void RestoreFeature::start() {
  using arangodb::httpclient::SimpleHttpClient;

  double const start = TRI_microtime();

  // set up the output directory, not much else
  _directory =
      std::make_unique<ManagedDirectory>(_options.inputPath, false, false);
  if (_directory->status().fail()) {
    switch (_directory->status().errorNumber()) {
      case TRI_ERROR_FILE_NOT_FOUND:
        LOG_TOPIC(FATAL, arangodb::Logger::RESTORE)
            << "input directory '" << _options.inputPath << "' does not exist";
        break;
      default:
        LOG_TOPIC(FATAL, arangodb::Logger::RESTORE)
            << _directory->status().errorMessage();
        break;
    }
    FATAL_ERROR_EXIT();
  }

  ClientFeature* client =
      application_features::ApplicationServer::getFeature<ClientFeature>(
          "Client");

  _exitCode = EXIT_SUCCESS;

  std::unique_ptr<SimpleHttpClient> httpClient;
  Result result =
      _clientManager.getConnectedClient(httpClient, _options.force, true, !_options.createDatabase);
  if (result.is(TRI_SIMPLE_CLIENT_COULD_NOT_CONNECT)) {
    LOG_TOPIC(FATAL, Logger::RESTORE)
        << "cannot create server connection, giving up!";
    FATAL_ERROR_EXIT();
  } else if (result.is(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND) &&
             _options.createDatabase) {
    // database not found, but database creation requested
    std::string dbName = client->databaseName();
    LOG_TOPIC(INFO, Logger::RESTORE) << "Creating database '" << dbName << "'";

    client->setDatabaseName("_system");

    Result res = ::tryCreateDatabase(dbName);
    if (res.fail()) {
      LOG_TOPIC(ERR, Logger::RESTORE)
          << "Could not create database '" << dbName << "'";
      LOG_TOPIC(FATAL, Logger::RESTORE) << httpClient->getErrorMessage();
      FATAL_ERROR_EXIT();
    }

    // restore old database name
    client->setDatabaseName(dbName);

    // re-check connection and version
    result =
        _clientManager.getConnectedClient(httpClient, _options.force, true, true);
  }

  if (result.fail() && !_options.force) {
    LOG_TOPIC(FATAL, Logger::RESTORE) << "cannot create server connection: " << result.errorMessage();
    FATAL_ERROR_EXIT();
  }

  // read encryption info
  ::checkEncryption(*_directory);

  // read dump info
  result = ::checkDumpDatabase(*_directory, _options.forceSameDatabase);
  if (result.fail()) {
    LOG_TOPIC(FATAL, arangodb::Logger::RESTORE) << result.errorMessage();
    FATAL_ERROR_EXIT();
  }

  // Version 1.4 did not yet have a cluster mode
  std::tie(result, _options.clusterMode) =
      _clientManager.getArangoIsCluster(*httpClient);
  if (result.fail()) {
    LOG_TOPIC(FATAL, arangodb::Logger::RESTORE) << "Error: could not detect ArangoDB instance type: " << result.errorMessage();
    _exitCode = EXIT_FAILURE;
    return;
  }

  std::tie(result, _options.indexesFirst) =
      _clientManager.getArangoIsUsingEngine(*httpClient, "rocksdb");
  if (result.fail()) {
    LOG_TOPIC(FATAL, arangodb::Logger::RESTORE) << "Error while trying to determine server storage engine: " << result.errorMessage();
    _exitCode = EXIT_FAILURE;
    return;
  }

  if (_options.progress) {
    LOG_TOPIC(INFO, Logger::RESTORE)
        << "Connected to ArangoDB '" << httpClient->getEndpointSpecification()
        << "'";
  }

  // set up threads and workers
  _clientTaskQueue.spawnWorkers(_clientManager, _options.threadCount);

  LOG_TOPIC(DEBUG, Logger::RESTORE) << "Using " << _options.threadCount << " worker thread(s)";

  // run the actual restore
  try {
    result = ::processInputDirectory(*httpClient, _clientTaskQueue, *this,
                                     _options, *_directory, _stats);
  } catch (basics::Exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::RESTORE) << "caught exception: " << ex.what();
    result = {ex.code(), ex.what()};
  } catch (std::exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::RESTORE) << "caught exception: " << ex.what();
    result = {TRI_ERROR_INTERNAL, ex.what()};
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::RESTORE)
        << "caught unknown exception";
    result = {TRI_ERROR_INTERNAL};
  }

  if (result.fail()) {
    LOG_TOPIC(ERR, arangodb::Logger::RESTORE) << result.errorMessage();
    _exitCode = EXIT_FAILURE;
  }

  if (_options.progress) {
    double totalTime = TRI_microtime() - start;

    if (_options.importData) {
      LOG_TOPIC(INFO, Logger::RESTORE)
          << "Processed " << _stats.restoredCollections
          << " collection(s) in " << Logger::FIXED(totalTime, 6) << " s, "
          << "read " << _stats.totalRead << " byte(s) from datafiles, "
          << "sent " << _stats.totalBatches << " data batch(es) of " << _stats.totalSent << " byte(s) total size";
    } else if (_options.importStructure) {
      LOG_TOPIC(INFO, Logger::RESTORE)
          << "Processed " << _stats.restoredCollections
          << " collection(s) in " << Logger::FIXED(totalTime, 6) << " s";
    }
  }
}

std::string RestoreFeature::featureName() { return ::FeatureName; }

void RestoreFeature::reportError(Result const& error) {
  try {
    MUTEX_LOCKER(lock, _workerErrorLock);
    _workerErrors.emplace(error);
    _clientTaskQueue.clearQueue();
  } catch (...) {
  }
}

Result RestoreFeature::getFirstError() const {
  {
    MUTEX_LOCKER(lock, _workerErrorLock);
    if (!_workerErrors.empty()) {
      return _workerErrors.front();
    }
  }
  return {TRI_ERROR_NO_ERROR};
}

}  // namespace arangodb
