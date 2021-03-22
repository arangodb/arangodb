////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>
#include <boost/algorithm/clamp.hpp>

#include <chrono>
#include <thread>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/NumberOfCores.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/application-exit.h"
#include "Basics/files.h"
#include "Basics/system-functions.h"
#include "FeaturePhases/BasicFeaturePhaseClient.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
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

/// @brief return the target replication factor for the specified collection
uint64_t getReplicationFactor(arangodb::RestoreFeature::Options const& options,
                              arangodb::velocypack::Slice const& slice, bool& isSatellite) {
  uint64_t result = options.defaultReplicationFactor;
  isSatellite = false;

  arangodb::velocypack::Slice s = slice.get(arangodb::StaticStrings::ReplicationFactor);
  if (s.isInteger()) {
    result = s.getNumericValue<uint64_t>();
  } else if (s.isString()) {
    if (s.copyString() == arangodb::StaticStrings::Satellite) {
      isSatellite = true;
    }
  }

  s = slice.get("name");
  if (!s.isString()) {
    // should not happen, but anyway, let's be safe here
    return result;
  }

  if (!options.replicationFactor.empty()) {
    std::string const name = s.copyString();

    for (auto const& it : options.replicationFactor) {
      auto parts = arangodb::basics::StringUtils::split(it, '=');
      if (parts.size() == 1) {
        // this is the default value, e.g. `--replicationFactor 2`
        if (parts[0] == arangodb::StaticStrings::Satellite) {
          isSatellite = true;
        } else {
          result = arangodb::basics::StringUtils::uint64(parts[0]);
        }
      }

      // look if we have a more specific value, e.g. `--replicationFactor myCollection=3`
      if (parts.size() != 2 || parts[0] != name) {
        // somehow invalid or different collection
        continue;
      }
      if (parts[1] == arangodb::StaticStrings::Satellite) {
        isSatellite = true;
      } else {
        result = arangodb::basics::StringUtils::uint64(parts[1]);
      }
      break;
    }
  }

  return result;
}

/// @brief return the target number of shards for the specified collection
uint64_t getNumberOfShards(arangodb::RestoreFeature::Options const& options,
                           arangodb::velocypack::Slice const& slice) {
  uint64_t result = options.defaultNumberOfShards;

  arangodb::velocypack::Slice s = slice.get("numberOfShards");
  if (s.isInteger()) {
    result = s.getNumericValue<uint64_t>();
  }

  s = slice.get("name");
  if (!s.isString()) {
    // should not happen, but anyway, let's be safe here
    return result;
  }

  if (!options.numberOfShards.empty()) {
    std::string const name = s.copyString();

    for (auto const& it : options.numberOfShards) {
      auto parts = arangodb::basics::StringUtils::split(it, '=');
      if (parts.size() == 1) {
        // this is the default value, e.g. `--numberOfShards 2`
        result = arangodb::basics::StringUtils::uint64(parts[0]);
      }

      // look if we have a more specific value, e.g. `--numberOfShards myCollection=3`
      if (parts.size() != 2 || parts[0] != name) {
        // somehow invalid or different collection
        continue;
      }
      result = arangodb::basics::StringUtils::uint64(parts[1]);
      break;
    }
  }

  return result;
}

/// @brief check whether HTTP response is valid, complete, and not an error
arangodb::Result checkHttpResponse(arangodb::httpclient::SimpleHttpClient& client,
                                   std::unique_ptr<arangodb::httpclient::SimpleHttpResult>& response,
                                   char const* requestAction,
                                   std::string const& originalRequest) {
  using arangodb::basics::StringUtils::itoa;
  if (response == nullptr || !response->isComplete()) {
    return {TRI_ERROR_INTERNAL,
            "got invalid response from server: '" + client.getErrorMessage() +
                "' while executing " + requestAction + (originalRequest.empty() ? "" : " with this payload: '" +
                originalRequest + "'")};
  }
  if (response->wasHttpError()) {
    auto errorNum = static_cast<int>(TRI_ERROR_INTERNAL);
    std::string errorMsg = response->getHttpReturnMessage();
    std::shared_ptr<arangodb::velocypack::Builder> bodyBuilder(response->getBodyVelocyPack());
    arangodb::velocypack::Slice error = bodyBuilder->slice();
    if (!error.isNone() && error.hasKey(arangodb::StaticStrings::ErrorMessage)) {
      errorNum = error.get(arangodb::StaticStrings::ErrorNum).getNumericValue<int>();
      errorMsg = error.get(arangodb::StaticStrings::ErrorMessage).copyString();
    }
    auto err = ErrorCode{errorNum};
    return {err, "got invalid response from server: HTTP " +
                     itoa(response->getHttpReturnCode()) + ": '" + errorMsg +
                     "' while executing " + requestAction +
                     (originalRequest.empty() ? "" : "' with this payload: '" + originalRequest + "'")};
  }
  return {TRI_ERROR_NO_ERROR};
}

/// @brief Sort collections for proper recreation order
bool sortCollectionsForCreation(VPackBuilder const& l, VPackBuilder const& r) {
  VPackSlice const left = l.slice().get("parameters");
  VPackSlice const right = r.slice().get("parameters");

  std::string leftName =
      arangodb::basics::VelocyPackHelper::getStringValue(left, "name", "");
  std::string rightName =
      arangodb::basics::VelocyPackHelper::getStringValue(right, "name", "");

  // First we sort by shard distribution.
  // We first have to create the collections which have no dependencies.
  // NB: Dependency graph has depth at most 1, no need to manage complex DAG
  VPackSlice leftDist = left.get(arangodb::StaticStrings::DistributeShardsLike);
  VPackSlice rightDist = right.get(arangodb::StaticStrings::DistributeShardsLike);
  if (leftDist.isNone() && rightDist.isString() &&
      rightDist.copyString() == leftName) {
    return true;
  }
  if (rightDist.isNone() && leftDist.isString() &&
      leftDist.copyString() == rightName) {
    return false;
  }

  // Next we sort by collection type so that vertex collections are recreated
  // before edge, etc.
  int leftType =
      arangodb::basics::VelocyPackHelper::getNumericValue<int>(left, "type", 0);
  int rightType =
      arangodb::basics::VelocyPackHelper::getNumericValue<int>(right, "type", 0);
  if (leftType != rightType) {
    return leftType < rightType;
  }

  // Finally, sort by name so we have stable, reproducible results
  // Sort system collections first
  if (!leftName.empty() && leftName[0] == '_' &&
      !rightName.empty() && rightName[0] != '_') {
    return true;
  }
  if (!leftName.empty() && leftName[0] != '_' &&
      !rightName.empty() && rightName[0] == '_') {
    return false;
  }
  return strcasecmp(leftName.c_str(), rightName.c_str()) < 0;
}

void makeAttributesUnique(arangodb::velocypack::Builder& builder,
                          arangodb::velocypack::Slice slice) {
  if (slice.isObject()) {
    std::unordered_set<arangodb::velocypack::StringRef> keys;

    builder.openObject();

    auto it = arangodb::velocypack::ObjectIterator(slice, true);

    while (it.valid()) {
      if (!keys.emplace(it.key().stringRef()).second) {
        // duplicate key
        it.next();
        continue;
      }

      // process attributes recursively
      builder.add(it.key());
      makeAttributesUnique(builder, it.value());
      it.next();
    }

    builder.close();
  } else if (slice.isArray()) {
    builder.openArray();

    auto it = arangodb::velocypack::ArrayIterator(slice);

    while (it.valid()) {
      // recurse into array
      makeAttributesUnique(builder, it.value());
      it.next();
    }

    builder.close();
  } else {
    // non-compound value!
    builder.add(slice);
  }
}

/// @brief Create the database to restore to, connecting manually
arangodb::Result tryCreateDatabase(arangodb::application_features::ApplicationServer& server,
                                   std::string const& name,
                                   VPackSlice properties,
                                   arangodb::RestoreFeature::Options const& options) {
  using arangodb::httpclient::SimpleHttpClient;
  using arangodb::httpclient::SimpleHttpResult;
  using arangodb::rest::RequestType;
  using arangodb::rest::ResponseCode;
  using arangodb::velocypack::ArrayBuilder;
  using arangodb::velocypack::ObjectBuilder;

  // get client feature for configuration info
  arangodb::ClientFeature& client =
      server.getFeature<arangodb::HttpEndpointProvider, arangodb::ClientFeature>();

  client.setDatabaseName(arangodb::StaticStrings::SystemDatabase);

  // get httpclient by hand rather than using manager, to bypass any built-in
  // checks which will fail if the database doesn't exist
  std::unique_ptr<SimpleHttpClient> httpClient;
  try {
    httpClient = client.createHttpClient();
    httpClient->params().setLocationRewriter(static_cast<void*>(&client),
                                             arangodb::ClientManager::rewriteLocation);
    httpClient->params().setUserNamePassword("/", client.username(), client.password());

  } catch (...) {
    LOG_TOPIC("832ef", FATAL, arangodb::Logger::RESTORE)
        << "cannot create server connection, giving up!";
    return {TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT};
  }

  VPackBuilder builder;
  {
    ObjectBuilder object(&builder);
    object->add(arangodb::StaticStrings::DatabaseName, VPackValue(name));

    // add replication factor write concern etc
    if (properties.isObject()) {
      ObjectBuilder guard(&builder, "options");
      for (auto const& key : std::vector<std::string>{
        arangodb::StaticStrings::ReplicationFactor,
        arangodb::StaticStrings::Sharding,
        arangodb::StaticStrings::WriteConcern
      }) {
        VPackSlice slice = properties.get(key);
        if (key == arangodb::StaticStrings::ReplicationFactor) {
          // overwrite replicationFactor if set
          bool isSatellite = false;
          uint64_t replicationFactor = getReplicationFactor(options, properties, isSatellite);
          if (!isSatellite) {
            object->add(key, VPackValue(replicationFactor));
            continue;
          }
        }
        if (!slice.isNone()) {
          object->add(key, slice);
        }
      }
    }

    {
      ArrayBuilder users(&builder, "users");
      {
        ObjectBuilder user(&builder);
        user->add("username", VPackValue(client.username()));
        user->add("passwd", VPackValue(client.password()));
      }
    }
  }

  std::string const body = builder.slice().toJson();

  std::unique_ptr<SimpleHttpResult> response(
      httpClient->request(RequestType::POST, "/_api/database", body.c_str(), body.size()));
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
      LOG_TOPIC("cc58e", WARN, Logger::RESTORE)
          << "the dump data seems to be encrypted with " << directory.encryptionType()
          << ", but no key information was specified to decrypt the dump";
      LOG_TOPIC("1a5a4", WARN, Logger::RESTORE)
          << "it is recommended to specify either "
             "`--encryption.keyfile` or `--encryption.key-generator` "
             "when invoking arangorestore with an encrypted dump";
    } else {
      LOG_TOPIC("4f9cf", INFO, Logger::RESTORE)
          << "# using encryption type " << directory.encryptionType()
          << " for reading dump";
    }
#endif
  }
}

void getDBProperties(arangodb::ManagedDirectory& directory, VPackBuilder& builder) {
  VPackBuilder fileContentBuilder;

  VPackSlice slice = VPackSlice::emptyObjectSlice();
  try {
    fileContentBuilder = directory.vpackFromJsonFile("dump.json");
  } catch (...) {
    LOG_TOPIC("3a5a4", WARN, arangodb::Logger::RESTORE)
        << "could not read dump.json file: "
        << directory.status().errorMessage();
    builder.add(slice);
    return;
  }

  try {
    auto props = fileContentBuilder.slice().get(arangodb::StaticStrings::Properties);
    if (props.isObject()) {
      slice = props;
    }
  } catch (...) {
    LOG_TOPIC("3b6a4", INFO, arangodb::Logger::RESTORE)
        << "no properties object found in dump.json file";
  }
  builder.add(slice);
}

/// @brief Check the database name specified by the dump file
arangodb::Result checkDumpDatabase(arangodb::application_features::ApplicationServer& server,
                                   arangodb::ManagedDirectory& directory,
                                   bool forceSameDatabase,
                                   bool& useEnvelope) {
  using arangodb::ClientFeature;
  using arangodb::HttpEndpointProvider;
  using arangodb::Logger;
  using arangodb::application_features::ApplicationServer;

  std::string databaseName;
  try {
    VPackBuilder fileContentBuilder = directory.vpackFromJsonFile("dump.json");
    VPackSlice const fileContent = fileContentBuilder.slice();
    databaseName = fileContent.get("database").copyString();
    if (VPackSlice s = fileContent.get("useEnvelope"); s.isBoolean()) {
      useEnvelope = s.getBoolean();
    }
  } catch (...) {
    // the above may go wrong for several reasons
  }

  if (!databaseName.empty()) {
    LOG_TOPIC("abeb4", INFO, Logger::RESTORE)
        << "Database name in source dump is '" << databaseName << "'";
  }

  ClientFeature& client = server.getFeature<HttpEndpointProvider, ClientFeature>();
  if (forceSameDatabase && databaseName != client.databaseName()) {
    return {TRI_ERROR_BAD_PARAMETER,
            std::string("database name in dump.json ('") + databaseName +
                "') does not match specified database name ('" +
                client.databaseName() + "')"};
  }

  return {};
}

/// @brief Send the command to recreate a collection
arangodb::Result sendRestoreCollection(arangodb::httpclient::SimpleHttpClient& httpClient,
                                       arangodb::RestoreFeature::Options const& options,
                                       VPackSlice const& slice, std::string const& name) {
  using arangodb::Logger;
  using arangodb::httpclient::SimpleHttpResult;

  const std::string url =
      "/_api/replication/restore-collection"
      "?overwrite=" +
      std::string(options.overwrite ? "true" : "false") +
      "&force=" + std::string(options.force ? "true" : "false") +
      "&ignoreDistributeShardsLikeErrors=" +
      std::string(options.ignoreDistributeShardsLikeErrors ? "true" : "false");

  VPackSlice const parameters = slice.get("parameters");

  // build cluster options using command-line parameter values
  VPackBuilder newOptions;
  newOptions.openObject();
  bool isSatellite = false;
  uint64_t replicationFactor = getReplicationFactor(options, parameters, isSatellite);
  if (isSatellite) {
    newOptions.add(arangodb::StaticStrings::ReplicationFactor, VPackValue(arangodb::StaticStrings::Satellite));
  } else {
    newOptions.add(arangodb::StaticStrings::ReplicationFactor, VPackValue(replicationFactor));
  }
  newOptions.add(arangodb::StaticStrings::NumberOfShards, VPackValue(getNumberOfShards(options, parameters)));
  newOptions.close();

  VPackBuilder b;
  b.openObject();
  b.add("indexes", slice.get("indexes"));
  b.add(VPackValue("parameters"));
  VPackCollection::merge(b, parameters, newOptions.slice(), true, false);
  b.close();

  std::string const body = b.slice().toJson();

  std::unique_ptr<SimpleHttpResult> response(
      httpClient.request(arangodb::rest::RequestType::PUT, url, body.c_str(), body.size()));
  return ::checkHttpResponse(httpClient, response, "restoring collection", body);
}

/// @brief Send command to restore a collection's indexes
arangodb::Result sendRestoreIndexes(arangodb::httpclient::SimpleHttpClient& httpClient,
                                    arangodb::RestoreFeature::Options const& options,
                                    VPackSlice const& slice) {
  using arangodb::httpclient::SimpleHttpResult;

  std::string const url = "/_api/replication/restore-indexes?force=" +
                          std::string(options.force ? "true" : "false");
  std::string const body = slice.toJson();

  std::unique_ptr<SimpleHttpResult> response(
      httpClient.request(arangodb::rest::RequestType::PUT, url, body.c_str(), body.size()));
  return ::checkHttpResponse(httpClient, response, "restoring indexes", body);
}

/// @brief Send a command to restore actual data
arangodb::Result sendRestoreData(arangodb::httpclient::SimpleHttpClient& httpClient,
                                 arangodb::RestoreFeature::Options const& options,
                                 std::string const& cname, char const* buffer,
                                 size_t bufferSize, bool useEnvelope) {
  using arangodb::basics::StringUtils::urlEncode;
  using arangodb::httpclient::SimpleHttpResult;

  // the following two structs are needed for cleaning up duplicate attributes
  arangodb::velocypack::Builder result;
  arangodb::basics::StringBuffer cleaned;

  if (options.cleanupDuplicateAttributes) {
    auto res = cleaned.reserve(bufferSize);

    if (res != TRI_ERROR_NO_ERROR) {
      // out of memory
      THROW_ARANGO_EXCEPTION(res);
    }

    arangodb::velocypack::Options opts = arangodb::velocypack::Options::Defaults;
    // do *not* check duplicate attributes here (because that would throw)
    opts.checkAttributeUniqueness = false;
    arangodb::velocypack::Builder builder(&opts);

    // instead, we need to manually check for duplicate attributes...
    char const* p = buffer;
    char const* e = p + bufferSize;

    while (p < e) {
      while (p < e && (*p == ' ' || *p == '\r' || *p == '\n' || *p == '\t')) {
        ++p;
      }

      // detect line ending
      size_t length;
      char const* nl = static_cast<char const*>(memchr(p, '\n', e - p));
      if (nl == nullptr) {
        length = e - p;
      } else {
        length = nl - p;
      }

      builder.clear();
      try {
        VPackParser parser(builder, builder.options);
        parser.parse(p, length);
      } catch (arangodb::velocypack::Exception const& ex) {
        return {TRI_ERROR_HTTP_CORRUPTED_JSON, ex.what()};
      } catch (std::bad_alloc const&) {
        return {TRI_ERROR_OUT_OF_MEMORY};
      } catch (std::exception const& ex) {
        return {TRI_ERROR_INTERNAL, ex.what()};
      }

      // recursively clean up duplicate attributes in the document
      result.clear();
      makeAttributesUnique(result, builder.slice());

      std::string const json = result.toJson();
      cleaned.appendText(json.data(), json.size());

      if (nl == nullptr) {
        // done
        break;
      }

      cleaned.appendChar('\n');
      // advance behind newline
      p = nl + 1;
    }

    // now point to the cleaned up data
    buffer = cleaned.c_str();
    bufferSize = cleaned.length();
  }

  std::string const url = "/_api/replication/restore-data?collection=" + urlEncode(cname) +
                          "&force=" + (options.force ? "true" : "false") +
                          "&useEnvelope=" + (useEnvelope ? "true" : "false");

  std::unordered_map<std::string, std::string> headers;
  headers.emplace(arangodb::StaticStrings::ContentTypeHeader,
                  arangodb::StaticStrings::MimeTypeDump);

  std::unique_ptr<SimpleHttpResult> response(
      httpClient.request(arangodb::rest::RequestType::PUT, url, buffer, bufferSize, headers));
  return ::checkHttpResponse(httpClient, response, "restoring data", "");
}

/// @brief Recreate a collection given its description
arangodb::Result recreateCollection(arangodb::httpclient::SimpleHttpClient& httpClient,
                                    arangodb::RestoreFeature::JobData& jobData) {
  using arangodb::Logger;

  arangodb::Result result;
  VPackSlice const parameters = jobData.collection.get("parameters");
  std::string const cname =
      arangodb::basics::VelocyPackHelper::getStringValue(parameters, "name", "");
  int type = arangodb::basics::VelocyPackHelper::getNumericValue<int>(parameters,
                                                                      "type", 2);
  std::string const collectionType(type == 2 ? "document" : "edge");

  // re-create collection
  if (jobData.options.progress) {
    if (jobData.options.overwrite) {
      LOG_TOPIC("9b414", INFO, Logger::RESTORE) << "# Re-creating " << collectionType
                                       << " collection '" << cname << "'...";
    } else {
      LOG_TOPIC("a9123", INFO, Logger::RESTORE) << "# Creating " << collectionType
                                       << " collection '" << cname << "'...";
    }
  }

  result = ::sendRestoreCollection(httpClient, jobData.options, jobData.collection, cname);

  if (result.fail()) {
    if (jobData.options.force) {
      LOG_TOPIC("c6658", WARN, Logger::RESTORE)
          << "Error while creating " << collectionType << " collection '"
          << cname << "': " << result.errorMessage();
      result.reset();
    } else {
      LOG_TOPIC("e8e7a", ERR, Logger::RESTORE)
          << "Error while creating " << collectionType << " collection '"
          << cname << "': " << result.errorMessage();
    }
  }
  return result;
}

/// @brief Restore a collection's indexes given its description
arangodb::Result restoreIndexes(arangodb::httpclient::SimpleHttpClient& httpClient,
                                arangodb::RestoreFeature::JobData& jobData) {
  using arangodb::Logger;

  arangodb::Result result{};
  VPackSlice const indexes = jobData.collection.get("indexes");
  // re-create indexes
  if (indexes.length() > 0) {
    // we actually have indexes

    VPackSlice const parameters = jobData.collection.get("parameters");

    std::string const cname =
        arangodb::basics::VelocyPackHelper::getStringValue(parameters, "name",
                                                             "");
    if (jobData.options.progress) {
      LOG_TOPIC("d88c6", INFO, Logger::RESTORE)
          << "# Creating indexes for collection '" << cname << "'...";
    }

    result = ::sendRestoreIndexes(httpClient, jobData.options, jobData.collection);

    if (result.fail()) {
      if (jobData.options.force) {
        LOG_TOPIC("db937", WARN, Logger::RESTORE)
            << "Error while creating indexes for collection '" << cname
            << "': " << result.errorMessage();
        result.reset();
      } else {
        LOG_TOPIC("d5d06", ERR, Logger::RESTORE)
            << "Error while creating indexes for collection '" << cname
            << "': " << result.errorMessage();
      }
    }
  }

  // cppcheck-suppress uninitvar ; false positive
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
  std::string const cname =
      arangodb::basics::VelocyPackHelper::getStringValue(parameters, "name", "");
  int type = arangodb::basics::VelocyPackHelper::getNumericValue<int>(parameters,
                                                                      "type", 2);
  std::string const collectionType(type == 2 ? "document" : "edge");

  auto&& currentStatus = jobData.progressTracker.getStatus(cname);

  if (currentStatus.state >= arangodb::RestoreFeature::RESTORED) {
    LOG_TOPIC("9a814", INFO, Logger::RESTORE)
        << "# skipping restoring " << collectionType << " collection '" << cname
        << "', as it was restored previously";
    return result;
  }

  TRI_ASSERT(currentStatus.state == arangodb::RestoreFeature::CREATED ||
             currentStatus.state == arangodb::RestoreFeature::RESTORING);

  // import data. check if we have a datafile
  //  ... there are 4 possible names
  auto datafile = jobData.directory.readableFile(
      cname + "_" + arangodb::rest::SslInterface::sslMD5(cname) + ".data.json");
  if (!datafile || datafile->status().fail()) {
    datafile = jobData.directory.readableFile(
      cname + "_" + arangodb::rest::SslInterface::sslMD5(cname) + ".data.json.gz");
  }
  if (!datafile || datafile->status().fail()) {
    datafile = jobData.directory.readableFile(cname + ".data.json.gz");
  }
  if (!datafile || datafile->status().fail()) {
    datafile = jobData.directory.readableFile(cname + ".data.json");
  }
  if (!datafile || datafile->status().fail()) {
    result = {TRI_ERROR_CANNOT_READ_FILE, "could not open data file for collection '" + cname + "'"};
    return result;
  }

  int64_t const fileSize = TRI_SizeFile(datafile->path().c_str());

  if (jobData.options.progress) {
    LOG_TOPIC("95913", INFO, Logger::RESTORE)
        << "# Loading data into " << collectionType << " collection '" << cname
        << "', data size: " << fileSize << " byte(s)";
  }

  int64_t numReadForThisCollection = 0;
  int64_t numReadSinceLastReport = 0;

  bool const isGzip = datafile->path().size() > 3 &&
                      (0 == datafile->path().substr(datafile->path().size() - 3).compare(".gz"));

  size_t datafileReadOffset = 0;
  if (currentStatus.state == arangodb::RestoreFeature::RESTORING) {
    LOG_TOPIC("94913", INFO, Logger::RESTORE)
      << "# continuing restoring " << collectionType << " collection '" << cname
      << "' from offset " << currentStatus.bytes_acked;
    datafileReadOffset = currentStatus.bytes_acked;
    datafile->skip(datafileReadOffset);
    if (datafile->status().fail()) {
      return datafile->status();
    }
  }

  buffer.clear();
  while (true) {
    constexpr size_t bufferSize = 32768;
    if (buffer.reserve(bufferSize) != TRI_ERROR_NO_ERROR) {
      result = {TRI_ERROR_OUT_OF_MEMORY, "out of memory"};
      return result;
    }

    ssize_t numRead = datafile->read(buffer.end(), bufferSize);
    if (datafile->status().fail()) {  // error while reading
      result = datafile->status();
      return result;
    }
    
    if (numRead > 0) {
      // we read something
      buffer.increaseLength(numRead);
      jobData.stats.totalRead += static_cast<uint64_t>(numRead);
      numReadForThisCollection += numRead;
      numReadSinceLastReport += numRead;

      if (buffer.length() < jobData.options.chunkSize) {
        continue;  // still continue reading
      }
    }
    
    // do we have a buffer?
    if (buffer.length() > 0) {
      // look for the last \n in the buffer
      char* found = (char*)memrchr((const void*)buffer.begin(), '\n', buffer.length());
      size_t length;
    
      if (found == nullptr) {  // no \n in buffer...
        if (numRead == 0) {
          // we're at the end of the file, so send the complete buffer anyway
          length = buffer.length();
        } else {
          continue;  // don't have a complete line yet, read more
        }
      } else {
        if (numRead == 0) {
          // we're at the end of the file, so send the complete buffer anyway
          length = buffer.length();
        } else {
          length = found - buffer.begin();  // found a \n somewhere; break at line
        }
      }
      
      jobData.stats.totalBatches++;
      result = ::sendRestoreData(httpClient, jobData.options, cname, buffer.begin(), length, jobData.useEnvelope);
      jobData.stats.totalSent += length;

      if (result.fail()) {
        if (jobData.options.force) {
          LOG_TOPIC("a595a", WARN, Logger::RESTORE)
              << "Error while restoring data into collection '" << cname
              << "': " << result.errorMessage();
          result.reset();
          continue;
        } else {
          LOG_TOPIC("a89bf", ERR, Logger::RESTORE)
              << "Error while restoring data into collection '" << cname
              << "': " << result.errorMessage();
        }
        return result;
      }

      // bytes successfully sent
      // note that we have to store the uncompressed offset here, because we
      // potentially have consumed more data than we have sent.
      datafileReadOffset += length;
      [[maybe_unused]] bool wasSynced = jobData.progressTracker.updateStatus(
          cname, arangodb::RestoreFeature::CollectionStatus{arangodb::RestoreFeature::RESTORING,
                                                            datafileReadOffset});
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
      if (wasSynced && jobData.options.failOnUpdateContinueFile && length != 0) {
        LOG_TOPIC("a87bf", WARN, Logger::RESTORE) << "triggered failure point at offset " << datafileReadOffset << "!";
        FATAL_ERROR_EXIT_CODE(38); // exit with exit code 38 to report to the test frame work that this was an intentional crash
      }
#endif
      buffer.erase_front(length);

      if (jobData.options.progress && fileSize > 0 &&
          numReadSinceLastReport > 1024 * 1024 * 8) {
        // report every 8MB of transferred data
        //   currently do not have unzipped size for .gz files
        std::stringstream percentage, ofFilesize;
        if (isGzip) {
          ofFilesize << "";
          percentage << "";
        } else {
          ofFilesize << " of " << fileSize;
          percentage << " ("
            << int(100. * double(numReadForThisCollection) / double(fileSize)) << " %)";
        } // else

        LOG_TOPIC("69a73", INFO, Logger::RESTORE)
            << "# Still loading data into " << collectionType << " collection '"
            << cname << "', " << numReadForThisCollection << ofFilesize.str()
            << " byte(s) restored" << percentage.str();
        numReadSinceLastReport = 0;
      }
    }

    if (numRead == 0) {  // EOF
      break;
    }
  }

  jobData.progressTracker.updateStatus(cname, arangodb::RestoreFeature::CollectionStatus{
                                                  arangodb::RestoreFeature::RESTORED, 0});

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
  std::unique_ptr<SimpleHttpResult> response(
      httpClient.request(arangodb::rest::RequestType::PUT, url, body.c_str(), body.size()));
  return ::checkHttpResponse(httpClient, response, "restoring view", body);
}

arangodb::Result triggerFoxxHeal(arangodb::httpclient::SimpleHttpClient& httpClient) {
  using arangodb::Logger;
  using arangodb::httpclient::SimpleHttpResult;
  std::string body = "";

  // check if the foxx api is available.
  const std::string statusUrl = "/_admin/status";
  std::unique_ptr<SimpleHttpResult> response(
      httpClient.request(arangodb::rest::RequestType::POST, statusUrl,
                         body.c_str(), body.length()));

  auto res =  ::checkHttpResponse(httpClient, response, "check status", body);
  if (res.ok() && response) {
    try {
      if (!response->getBodyVelocyPack()->slice().get("foxxApi").getBool()) {
        LOG_TOPIC("9e9b9", INFO, Logger::RESTORE)
          << "skipping foxx self-healing because Foxx API is disabled";
        return { };
      }
    } catch (...) {
      //API Not available because of older version or whatever
    }
  }

  const std::string FoxxHealUrl = "/_api/foxx/_local/heal";
  response.reset(
    httpClient.request(arangodb::rest::RequestType::POST, FoxxHealUrl,
                       body.c_str(), body.length())
  );
  return ::checkHttpResponse(httpClient, response, "trigger self heal", body);
}

arangodb::Result processInputDirectory(
    arangodb::httpclient::SimpleHttpClient& httpClient,
    arangodb::ClientTaskQueue<arangodb::RestoreFeature::JobData>& jobQueue,
    arangodb::RestoreFeature& feature, arangodb::RestoreFeature::Options const& options,
    arangodb::ManagedDirectory& directory,
    arangodb::RestoreFeature::RestoreProgressTracker& progressTracker,
    arangodb::RestoreFeature::Stats& stats,
    bool useEnvelope) {
  using arangodb::Logger;
  using arangodb::Result;
  using arangodb::StaticStrings;
  using arangodb::basics::VelocyPackHelper;
  using arangodb::basics::FileUtils::listFiles;
  using arangodb::basics::StringUtils::concatT;

  auto fill = [](std::unordered_map<std::string, bool>& map, std::vector<std::string> const& requested) {
    for (auto const& name : requested) {
      map.emplace(name, false);
    }
  };

  auto checkRequested = [](std::unordered_map<std::string, bool>& map, std::string const& name) {
    if (map.empty()) {
      // no restrictions, so restore everything
      return true;
    }
    auto it = map.find(name);
    if (it == map.end()) {
      return false;
    }
    // mark collection as seen
    (*it).second = true;
    return true;
  };

  // create a lookup table for collections and views
  std::unordered_map<std::string, bool> restrictColls, restrictViews;
  fill(restrictColls, options.collections);
  fill(restrictViews, options.views);

  try {
    std::vector<std::string> const files = listFiles(directory.path());
    std::string const collectionSuffix = std::string(".structure.json");
    std::string const viewsSuffix = std::string(".view.json");
    std::vector<VPackBuilder> collections;
    std::vector<VPackBuilder> views;

    // Step 1 determine all collections to process
    {
      // loop over all files in InputDirectory, and look for all structure.json
      // files
      for (std::string const& file : files) {
        size_t const nameLength = file.size();

        if (nameLength > viewsSuffix.size() &&
            file.substr(file.size() - viewsSuffix.size()) == viewsSuffix) {
          if (!restrictColls.empty() && restrictViews.empty()) {
            continue;  // skip view if not specifically included
          }

          VPackBuilder contentBuilder = directory.vpackFromJsonFile(file);
          VPackSlice const fileContent = contentBuilder.slice();
          if (!fileContent.isObject()) {
            return {TRI_ERROR_INTERNAL,
                    concatT("could not read view file '", directory.pathToFile(file),
                            "': ", directory.status().errorMessage())};
          }

          std::string const name =
              VelocyPackHelper::getStringValue(fileContent, StaticStrings::DataSourceName,
                                               "");
          if (!checkRequested(restrictViews, name)) {
            // view name not in list
            continue;
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
                  concatT("could not read collection structure file '",
                          directory.pathToFile(file),
                          "': ", directory.status().errorMessage())};
        }

        VPackSlice const parameters = fileContent.get("parameters");
        VPackSlice const indexes = fileContent.get("indexes");
        if (!parameters.isObject() || !indexes.isArray()) {
          return {TRI_ERROR_BAD_PARAMETER,
                  "could not read collection structure file '" +
                      directory.pathToFile(file) + "': file has wrong internal format"};
        }
        std::string const cname =
            VelocyPackHelper::getStringValue(parameters,
                                             StaticStrings::DataSourceName, "");
        bool overwriteName = false;
        if (cname != name &&
            name != (cname + "_" + arangodb::rest::SslInterface::sslMD5(cname))) {
          // file has a different name than found in structure file
          if (options.importStructure) {
            // we cannot go on if there is a mismatch
            return {TRI_ERROR_INTERNAL,
                    "collection name mismatch in collection structure file '" +
                        directory.pathToFile(file) + "' (offending value: '" +
                        cname + "')"};
          } else {
            // we can patch the name in our array and go on
            LOG_TOPIC("8e7b7", INFO, Logger::RESTORE)
                << "ignoring collection name mismatch in collection "
                   "structure file '" +
                       directory.pathToFile(file) + "' (offending value: '" +
                       cname + "')";

            overwriteName = true;
          }
        }

        if (!checkRequested(restrictColls, cname)) {
          // collection name not in list
          continue;
        }

        if (overwriteName) {
          // TODO: we have a JSON object with sub-object "parameters" with
          // attribute "name". we only want to replace this. how?
        } else {
          VPackSlice s = fileContentBuilder.slice();
          VPackSlice indexes = s.get("indexes");
          VPackSlice parameters = s.get("parameters");
          if ((indexes.isNone() || indexes.isEmptyArray()) &&
              parameters.get("indexes").isArray()) {
            // old format
            VPackBuilder const parametersWithoutIndexes =
              VPackCollection::remove(parameters, std::vector<std::string>{"indexes"});

            VPackBuilder rewritten;
            rewritten.openObject();
            rewritten.add("indexes", parameters.get("indexes"));
            rewritten.add("parameters", parametersWithoutIndexes.slice());
            rewritten.close();

            collections.emplace_back(std::move(rewritten));
          } else {
            // new format
            collections.emplace_back(std::move(fileContentBuilder));
          }
        }
      }
    }

    if (!options.collections.empty()) {
      bool found = false;
      for (auto const& it : restrictColls) {
        if (!it.second) {
          LOG_TOPIC("5163e", WARN, Logger::RESTORE)
            << "Requested collection '" << it.first << "' not found in dump";
        } else {
          found = true;
        }
      }
      if (!found) {
        LOG_TOPIC("3ef18", FATAL, arangodb::Logger::RESTORE)
            << "None of the requested collections were found in the dump";
        FATAL_ERROR_EXIT();
      }
    }

    if (!options.views.empty()) {
      bool found = false;
      for (auto const& it : restrictViews) {
        if (!it.second) {
          LOG_TOPIC("810df", WARN, Logger::RESTORE)
            << "Requested view '" << it.first << "' not found in dump";
        } else {
          found = true;
        }
      }
      if (!found) {
        LOG_TOPIC("14051", FATAL, arangodb::Logger::RESTORE)
            << "None of the requested Views were found in the dump";
        FATAL_ERROR_EXIT();
      }
    }

    // order collections so that prototypes for distributeShardsLike come first
    std::sort(collections.begin(), collections.end(), ::sortCollectionsForCreation);

    std::unique_ptr<arangodb::RestoreFeature::JobData> usersData;
    std::unique_ptr<arangodb::RestoreFeature::JobData> analyzersData;
    std::vector<std::unique_ptr<arangodb::RestoreFeature::JobData>> jobs;
    jobs.reserve(collections.size());

    bool didModifyFoxxCollection = false;
    // Step 3: create collections
    for (VPackBuilder const& b : collections) {
      VPackSlice const collection = b.slice();

      LOG_TOPIC("c601a", DEBUG, Logger::RESTORE)
        << "# Processing collection: " << collection.toJson();

      VPackSlice params = collection.get("parameters");
      VPackSlice name = VPackSlice::emptyStringSlice();
      if (params.isObject()) {
        name = params.get("name");
        // Only these two are relevant for FOXX.
        if (name.isString() && (name.isEqualString(arangodb::StaticStrings::AppsCollection) ||
                                name.isEqualString(arangodb::StaticStrings::AppBundlesCollection))) {
          didModifyFoxxCollection = true;
        }
      }

      auto jobData =
          std::make_unique<arangodb::RestoreFeature::JobData>(directory, feature,
                                                              progressTracker, options,
                                                              stats, collection, useEnvelope);

      // take care of collection creation now, serially
      if (options.importStructure &&
          progressTracker.getStatus(name.copyString()).state <
              arangodb::RestoreFeature::CREATED) {
        Result result = ::recreateCollection(httpClient, *jobData);
        if (result.fail()) {
          return result;
        }
      }

      if (progressTracker.getStatus(name.copyString()).state < arangodb::RestoreFeature::CREATED) {
        progressTracker.updateStatus(name.copyString(),
                                     arangodb::RestoreFeature::CollectionStatus{
                                         arangodb::RestoreFeature::CREATED});
      }

      if (name.isString() && name.stringRef() == arangodb::StaticStrings::UsersCollection) {
        // special treatment for _users collection - this must be the very last,
        // and run isolated from all previous data loading operations - the
        // reason is that loading into the users collection may change the
        // credentials for the current arangorestore connection!
        usersData = std::move(jobData);
      } else if (name.isString() && name.stringRef() == StaticStrings::AnalyzersCollection) {
        // special treatment for _analyzers collection - this must be the very first
        stats.totalCollections++;
        analyzersData = std::move(jobData);
      } else {
        stats.totalCollections++;
        jobs.push_back(std::move(jobData));
      }
    }

    // Step 4: restore data from _analyzers collection
    if (analyzersData) {
      // restore analyzers
      if (!jobQueue.queueJob(std::move(analyzersData))) {
        return Result(TRI_ERROR_OUT_OF_MEMORY, "unable to queue restore job");
      }

      jobQueue.waitForIdle();
    }

    // Step 5: create arangosearch views
    if (options.importStructure && !views.empty()) {
      LOG_TOPIC("f723c", INFO, Logger::RESTORE) << "# Creating views...";

      for (auto const& viewDefinition : views) {
        LOG_TOPIC("c608d", DEBUG, Logger::RESTORE)
          << "# Creating view: " << viewDefinition.toJson();

        auto res = ::restoreView(httpClient, options, viewDefinition.slice());

        if (!res.ok()) {
          return res;
        }
      }
    }

    // Step 6: fire up data transfer
    for (auto& job : jobs) {
      if (!jobQueue.queueJob(std::move(job))) {
        return Result(TRI_ERROR_OUT_OF_MEMORY, "unable to queue restore job");
      }
    }

    // wait for all jobs to finish, then check for errors
    if (options.progress) {
      LOG_TOPIC("6d69f", INFO, Logger::RESTORE)
          << "# Dispatched " << stats.totalCollections << " job(s), using "
          << options.threadCount << " worker(s)";

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
          LOG_TOPIC("75e65", INFO, Logger::RESTORE)
              << "# Current restore progress: restored " << stats.restoredCollections
              << " of " << stats.totalCollections << " collection(s), read "
              << stats.totalRead << " byte(s) from datafiles, "
              << "sent " << stats.totalBatches << " data batch(es) of "
              << stats.totalSent << " byte(s) total size"
              << ", queued jobs: " << std::get<0>(queueStats)
              << ", workers: " << std::get<1>(queueStats);
          start = now;
        }

        // don't sleep for too long, as we want to quickly terminate
        // when the gets empty
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
      }
    }

    jobQueue.waitForIdle();
    jobs.clear();

    Result firstError = feature.getFirstError();
    if (firstError.fail()) {
      return firstError;
    }

    if (didModifyFoxxCollection) {
      // if we get here we need to trigger foxx heal
      Result res = ::triggerFoxxHeal(httpClient);
      if (res.fail()) {
        LOG_TOPIC("47cd7", WARN, Logger::RESTORE)
            << "Reloading of Foxx services failed: " << res.errorMessage()
            << "- in the cluster Foxx services will be available eventually, On single servers send "
            << "a POST to '/_api/foxx/_local/heal' on the current database, "
            << "with an empty body. Please note that any of this is not necessary if the Foxx APIs "
            << "have been turned off on the server using the option `--foxx.api false`.";
      }
    }

    // Last step: reload data into _users. Note: this can change the credentials
    // of the arangorestore user itself
    if (usersData) {
      TRI_ASSERT(jobs.empty());
      if (!jobQueue.queueJob(std::move(usersData))) {
        return Result(TRI_ERROR_OUT_OF_MEMORY, "unable to queue restore job");
      }
      jobQueue.waitForIdle();
      jobs.clear();

      Result firstError = feature.getFirstError();
      if (firstError.fail()) {
        return firstError;
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
  return {};
}

/// @brief process a single job from the queue
void processJob(arangodb::httpclient::SimpleHttpClient& httpClient,
                arangodb::RestoreFeature::JobData& jobData) {
  VPackSlice const parameters = jobData.collection.get("parameters");
  std::string const cname =
      arangodb::basics::VelocyPackHelper::getStringValue(parameters, "name", "");

  arangodb::Result res;
  if (cname == arangodb::StaticStrings::UsersCollection) {
    // special case: never restore data in the _users collection first as it could
    // potentially change user permissions. In that case index creation will fail.
    res = ::restoreIndexes(httpClient, jobData);
    if (res.ok()) {
      res = ::restoreData(httpClient, jobData);
    }
  } else {
    // restore indexes first
    res = ::restoreIndexes(httpClient, jobData);
    if (res.ok() && jobData.options.importData) {
      res = ::restoreData(httpClient, jobData);
    }
  }

  if (res.ok()) {
    ++jobData.stats.restoredCollections;

    if (jobData.options.progress) {
      VPackSlice const parameters = jobData.collection.get("parameters");
      std::string const cname =
          arangodb::basics::VelocyPackHelper::getStringValue(parameters, "name",
                                                             "");
      int type = arangodb::basics::VelocyPackHelper::getNumericValue<int>(parameters,
                                                                          "type", 2);
      std::string const collectionType(type == 2 ? "document" : "edge");
      LOG_TOPIC("6ae09", INFO, arangodb::Logger::RESTORE) << "# Successfully restored " << collectionType
                                                 << " collection '" << cname << "'";
    }
  }

  if (res.fail()) {
    jobData.feature.reportError(res);
  }
}

}  // namespace

namespace arangodb {

RestoreFeature::JobData::JobData(ManagedDirectory& directory, RestoreFeature& feature,
                                 RestoreProgressTracker& progressTracker,
                                 RestoreFeature::Options const& options,
                                 RestoreFeature::Stats& stats, VPackSlice collection,
                                 bool useEnvelope)
    : directory{directory}, 
      feature{feature}, 
      progressTracker{progressTracker}, 
      options{options}, 
      stats{stats}, 
      collection{collection},
      useEnvelope{useEnvelope} {}

RestoreFeature::RestoreFeature(application_features::ApplicationServer& server, int& exitCode)
    : ApplicationFeature(server, RestoreFeature::featureName()),
      _clientManager{server, Logger::RESTORE},
      _clientTaskQueue{server, ::processJob},
      _exitCode{exitCode} {
  requiresElevatedPrivileges(false);
  setOptional(false);
  startsAfter<application_features::BasicFeaturePhaseClient>();

  using arangodb::basics::FileUtils::buildFilename;
  using arangodb::basics::FileUtils::currentDirectory;
  _options.inputPath = buildFilename(currentDirectory().result(), "dump");
}

void RestoreFeature::collectOptions(std::shared_ptr<options::ProgramOptions> options) {
  using arangodb::options::BooleanParameter;
  using arangodb::options::StringParameter;
  using arangodb::options::UInt32Parameter;
  using arangodb::options::UInt64Parameter;
  using arangodb::options::VectorParameter;

  options->addOption(
      "--collection",
      "restrict to collection name (can be specified multiple times)",
      new VectorParameter<StringParameter>(&_options.collections));

  options->addOption("--view",
                     "restrict to view name (can be specified multiple times)",
                     new VectorParameter<StringParameter>(&_options.views));

  options->addObsoleteOption("--recycle-ids",
                             "collection ids are now handled automatically", false);

  options->addOption("--batch-size",
                     "maximum size for individual data batches (in bytes)",
                     new UInt64Parameter(&_options.chunkSize));

  options
      ->addOption("--threads",
                  "maximum number of collections to process in parallel",
                  new UInt32Parameter(&_options.threadCount))
      .setIntroducedIn(30400);

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

  options->addOption(
      "--all-databases", "restore data to all databases",
      new BooleanParameter(&_options.allDatabases))
      .setIntroducedIn(30500);

  options->addOption("--input-directory", "input directory",
                     new StringParameter(&_options.inputPath));

  options
      ->addOption(
          "--cleanup-duplicate-attributes",
          "clean up duplicate attributes (use first specified value) in input "
          "documents instead of making the restore operation fail",
          new BooleanParameter(&_options.cleanupDuplicateAttributes),
          arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden))
      .setIntroducedIn(30322)
      .setIntroducedIn(30402);

  options->addOption("--import-data", "import data into collection",
                     new BooleanParameter(&_options.importData));

  options->addOption("--create-collection", "create collection structure",
                     new BooleanParameter(&_options.importStructure));

  options->addOption("--progress", "show progress",
                     new BooleanParameter(&_options.progress));

  options->addOption("--overwrite", "overwrite collections if they exist",
                     new BooleanParameter(&_options.overwrite));

  options->addOption("--continue", "continue restore operation",
                     new BooleanParameter(&_options.continueRestore));
  
  options->addOption("--envelope", "wrap each document into a {type, data} envelope "
                     "(this is required from compatibility with v3.7 and before)",
                     new BooleanParameter(&_options.useEnvelope))
                     .setIntroducedIn(30800);

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  options->addOption("--fail-after-update-continue-file", "",
                     new BooleanParameter(&_options.failOnUpdateContinueFile),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));
#endif

  options
      ->addOption(
          "--number-of-shards",
          "override value for numberOfShards (can be specified multiple times, "
          "e.g. --number-of-shards 2 --number-of-shards myCollection=3)",
          new VectorParameter<StringParameter>(&_options.numberOfShards))
      .setIntroducedIn(30322)
      .setIntroducedIn(30402);

  options
      ->addOption("--replication-factor",
                  "override value for replicationFactor (can be specified "
                  "multiple times, e.g. --replication-factor 2 "
                  "--replication-factor myCollection=3)",
                  new VectorParameter<StringParameter>(&_options.replicationFactor))
      .setIntroducedIn(30322)
      .setIntroducedIn(30402);

  options->addOption(
      "--ignore-distribute-shards-like-errors",
      "continue restore even if sharding prototype collection is missing",
      new BooleanParameter(&_options.ignoreDistributeShardsLikeErrors));

  options->addOption(
      "--force", "continue restore even in the face of some server-side errors",
      new BooleanParameter(&_options.force));

  // deprecated options
  options
      ->addOption("--default-number-of-shards",
                  "default value for numberOfShards if not specified in dump",
                  new UInt64Parameter(&_options.defaultNumberOfShards),
                  arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden))
      .setDeprecatedIn(30322)
      .setDeprecatedIn(30402);

  options
      ->addOption(
          "--default-replication-factor",
          "default value for replicationFactor if not specified in dump",
          new UInt64Parameter(&_options.defaultReplicationFactor),
          arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden))
      .setDeprecatedIn(30322)
      .setDeprecatedIn(30402);
}

void RestoreFeature::validateOptions(std::shared_ptr<options::ProgramOptions> options) {
  using arangodb::basics::StringUtils::join;

  auto const& positionals = options->processingResult()._positionals;
  size_t n = positionals.size();

  if (1 == n) {
    _options.inputPath = positionals[0];
  } else if (1 < n) {
    LOG_TOPIC("d249a", FATAL, arangodb::Logger::RESTORE)
        << "expecting at most one directory, got " + join(positionals, ", ");
    FATAL_ERROR_EXIT();
  }

  if (_options.allDatabases) {
    if (options->processingResult().touched("server.database")) {
      LOG_TOPIC("94d22", FATAL, arangodb::Logger::RESTORE)
          << "cannot use --server.database and --all-databases at the same time";
      FATAL_ERROR_EXIT();
    }

    if (_options.forceSameDatabase) {
      LOG_TOPIC("fd66a", FATAL, arangodb::Logger::RESTORE)
          << "cannot use --force-same-database and --all-databases at the same time";
      FATAL_ERROR_EXIT();
    }
  }

  // use a minimum value for batches
  if (_options.chunkSize < 1024 * 128) {
    _options.chunkSize = 1024 * 128;
  }

  auto clamped = boost::algorithm::clamp(_options.threadCount, uint32_t(1),
                                         uint32_t(4 * NumberOfCores::getValue()));
  if (_options.threadCount != clamped) {
    LOG_TOPIC("53570", WARN, Logger::RESTORE) << "capping --threads value to " << clamped;
    _options.threadCount = clamped;
  }

  // validate shards and replication factor
  if (_options.defaultNumberOfShards == 0) {
    LOG_TOPIC("248ee", FATAL, arangodb::Logger::RESTORE)
        << "invalid value for `--default-number-of-shards`, expecting at least "
           "1";
    FATAL_ERROR_EXIT();
  }

  if (_options.defaultReplicationFactor == 0) {
    LOG_TOPIC("daf22", FATAL, arangodb::Logger::RESTORE)
        << "invalid value for `--default-replication-factor, expecting at "
           "least 1";
    FATAL_ERROR_EXIT();
  }

  for (auto& it : _options.numberOfShards) {
    auto parts = basics::StringUtils::split(it, '=');
    if (parts.size() == 1 && basics::StringUtils::int64(parts[0]) > 0) {
      // valid
      continue;
    } else if (parts.size() == 2 && basics::StringUtils::int64(parts[1]) > 0) {
      // valid
      continue;
    }
    // invalid!
    LOG_TOPIC("1951e", FATAL, arangodb::Logger::RESTORE)
        << "got invalid value '" << it << "' for `--number-of-shards";
    FATAL_ERROR_EXIT();
  }

  for (auto& it : _options.replicationFactor) {
    auto parts = basics::StringUtils::split(it, '=');
    if (parts.size() == 1) {
      if (parts[0] == "satellite" || basics::StringUtils::int64(parts[0]) > 0) {
        // valid
        continue;
      }
    } else if (parts.size() == 2) {
      if (parts[1] == "satellite" || basics::StringUtils::int64(parts[1]) > 0) {
        // valid
        continue;
      }
    }
    // invalid!
    LOG_TOPIC("d038e", FATAL, arangodb::Logger::RESTORE)
        << "got invalid value '" << it << "' for `--replication-factor";
    FATAL_ERROR_EXIT();
  }
}

void RestoreFeature::prepare() {
  if (!_options.inputPath.empty() && _options.inputPath.back() == TRI_DIR_SEPARATOR_CHAR) {
    // trim trailing slash from path because it may cause problems on ...
    // Windows
    TRI_ASSERT(_options.inputPath.size() > 0);
    _options.inputPath.pop_back();
  }
  TRI_NormalizePath(_options.inputPath);

  if (!_options.importStructure && !_options.importData) {
    LOG_TOPIC("1281f", FATAL, arangodb::Logger::RESTORE)
        << "Error: must specify either --create-collection or --import-data";
    FATAL_ERROR_EXIT();
  }
}

void RestoreFeature::start() {
  using arangodb::basics::StringUtils::concatT;
  using arangodb::httpclient::SimpleHttpClient;

  double const start = TRI_microtime();

  // set up the output directory, not much else
  _directory = std::make_unique<ManagedDirectory>(server(), _options.inputPath,
                                                  false, false, true);
  if (_directory->status().fail()) {
    switch (static_cast<int>(_directory->status().errorNumber())) {
      case static_cast<int>(TRI_ERROR_FILE_NOT_FOUND):
        LOG_TOPIC("3246c", FATAL, arangodb::Logger::RESTORE)
            << "input directory '" << _options.inputPath << "' does not exist";
        break;
      default:
        LOG_TOPIC("535b3", FATAL, arangodb::Logger::RESTORE)
            << _directory->status().errorMessage();
        break;
    }
    FATAL_ERROR_EXIT();
  }

  ClientFeature& client = server().getFeature<HttpEndpointProvider, ClientFeature>();

  _exitCode = EXIT_SUCCESS;

  // enumerate all databases present in the dump directory (in case of
  // --all-databases=true, or use just the flat files in case of --all-databases=false)
  std::vector<std::pair<std::string, VPackBuilder>> databases;
  if (_options.allDatabases) {
    for (auto const& it : basics::FileUtils::listFiles(_options.inputPath)) {
      std::string path = basics::FileUtils::buildFilename(_options.inputPath, it);
      if (basics::FileUtils::isDirectory(path)) {
        databases.push_back(std::pair(it, VPackBuilder{}));
      }
    }

    // sort by name, with _system last
    // this is necessary because in the system database there is the _users collection,
    // and we have to process users last of all. otherwise we risk updating the
    // credentials for the user which users the current arangorestore connection, and
    // this will make subsequent arangorestore calls to the server fail with "unauthorized"
    std::sort(databases.begin(), databases.end(), [](auto const& lhs, auto const& rhs) {
      if (lhs.first == StaticStrings::SystemDatabase && rhs.first != StaticStrings::SystemDatabase) {
        return false;
      } else if (rhs.first == StaticStrings::SystemDatabase && lhs.first != StaticStrings::SystemDatabase) {
        return true;
      }
      return lhs.first < rhs.first;
    });
    if (databases.empty()) {
      LOG_TOPIC("b41d9", FATAL, Logger::RESTORE) << "Unable to find per-database subdirectories in input directory '" << _options.inputPath << "'. No data will be restored!";
      FATAL_ERROR_EXIT();
    }
  } else {
    databases.push_back(std::pair(client.databaseName(), VPackBuilder{}));
  }

  std::unique_ptr<SimpleHttpClient> httpClient;

  // final result
  Result result = _clientManager.getConnectedClient(httpClient, _options.force,
                                             true, !_options.createDatabase, false);
  if (result.is(TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT)) {
    LOG_TOPIC("c23bf", FATAL, Logger::RESTORE)
        << "cannot create server connection, giving up!";
    FATAL_ERROR_EXIT();
  }
  if (result.is(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND)) {
    std::string dbName = client.databaseName();
    if (_options.createDatabase) {
      // database not found, but database creation requested
      LOG_TOPIC("9b5a6", INFO, Logger::RESTORE) << "Creating database '" << dbName << "'";

      VPackBuilder properties;
      getDBProperties(*_directory, properties);
      Result res = ::tryCreateDatabase(server(), dbName, properties.slice(), _options);
      if (res.fail()) {
        LOG_TOPIC("b19db", FATAL, Logger::RESTORE) << "Could not create database '" << dbName << "': " << res.errorMessage();
        FATAL_ERROR_EXIT();
      }

      // restore old database name
      client.setDatabaseName(dbName);

      // re-check connection and version
      result = _clientManager.getConnectedClient(httpClient, _options.force, true, true, false);
    } else {
      LOG_TOPIC("ad95b", WARN, Logger::RESTORE) << "Database '" << dbName << "' does not exist on target endpoint. In order to create this database along with the restore, please use the --create-database option";
    }
  }

  if (result.fail() && !_options.force) {
    LOG_TOPIC("62a31", FATAL, Logger::RESTORE)
        << "cannot create server connection: " << result.errorMessage();
    FATAL_ERROR_EXIT();
  }

  // check if we are in cluster or single-server mode
  std::string role;
  std::tie(result, role) = _clientManager.getArangoIsCluster(*httpClient);
  _options.clusterMode = (role == "COORDINATOR");
  if (result.fail()) {
    LOG_TOPIC("b18ac", FATAL, arangodb::Logger::RESTORE)
        << "Error: could not detect ArangoDB instance type: " << result.errorMessage();
    _exitCode = EXIT_FAILURE;
    return;
  }

  if (role == "DBSERVER" || role == "PRIMARY") {
    LOG_TOPIC("1fc99", WARN, arangodb::Logger::RESTORE) << "You connected to a DBServer node, but operations in a cluster should be carried out via a Coordinator. This is an unsupported operation!";
  }

  bool isRocksDB;
  std::tie(result, isRocksDB) =
      _clientManager.getArangoIsUsingEngine(*httpClient, "rocksdb");
  if (result.fail()) {
    LOG_TOPIC("b90ec", FATAL, arangodb::Logger::RESTORE)
        << "Error while trying to determine server storage engine: "
        << result.errorMessage();
    _exitCode = EXIT_FAILURE;
    return;
  }

  if (_options.progress) {
    LOG_TOPIC("05c30", INFO, Logger::RESTORE)
        << "Connected to ArangoDB '" << httpClient->getEndpointSpecification() << "'";
  }

  if (!isRocksDB) {
    LOG_TOPIC("ae10c", WARN, arangodb::Logger::RESTORE) << "You connected to a server with a potentially incompatible storage engine.";
  }

  // set up threads and workers
  _clientTaskQueue.spawnWorkers(_clientManager, _options.threadCount);

  LOG_TOPIC("6bb3c", DEBUG, Logger::RESTORE) << "Using " << _options.threadCount << " worker thread(s)";

  if (_options.allDatabases) {
    std::vector<std::string> dbs;
    std::transform(databases.begin(), databases.end(), std::back_inserter(dbs), [](auto const& pair) { return pair.first; } );
    LOG_TOPIC("7c10a", INFO, Logger::RESTORE)
      << "About to restore databases '"
      << basics::StringUtils::join(dbs, "', '") << "' from dump directory '" << _options.inputPath << "'...";
  }

  std::vector<std::string> filesToClean;
    
  for (auto& db : databases) {
    result.reset();

    if (_options.allDatabases) {
      // inject current database
      client.setDatabaseName(db.first);
      LOG_TOPIC("36075", INFO, Logger::RESTORE) << "Restoring database '" << db.first << "'";
      _directory = std::make_unique<ManagedDirectory>(
          server(), basics::FileUtils::buildFilename(_options.inputPath, db.first),
          false, false, true);

      getDBProperties(*_directory, db.second);
      result = _clientManager.getConnectedClient(httpClient, _options.force,
                                                 false, !_options.createDatabase, false);

      if (result.is(TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT)) {
        LOG_TOPIC("3e715", FATAL, Logger::RESTORE)
            << "cannot create server connection, giving up!";
        FATAL_ERROR_EXIT();
      }

      if (result.is(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND)) {
        if (_options.createDatabase) {
          // database not found, but database creation requested
          LOG_TOPIC("080f3", INFO, Logger::RESTORE) << "Creating database '" << db.first << "'";

          result = ::tryCreateDatabase(server(), db.first, db.second.slice(), _options);
          if (result.fail()) {
            LOG_TOPIC("7a35f", ERR, Logger::RESTORE) << "Could not create database '" << db.first << "': " << result.errorMessage();
            break;
          }

          // restore old database name
          client.setDatabaseName(db.first);

          // re-check connection and version
          result = _clientManager.getConnectedClient(httpClient, _options.force, false, true, false);
        } else {
          LOG_TOPIC("be594", WARN, Logger::RESTORE) << "Database '" << db.first << "' does not exist on target endpoint. In order to create this database along with the restore, please use the --create-database option";
        }
      }

      if (result.fail()) {
        result.reset(result.errorNumber(),
                     concatT("cannot create server connection: ", result.errorMessage()));

        if (!_options.force) {
          break;
        }

        LOG_TOPIC("be86d", ERR, arangodb::Logger::RESTORE) << result.errorMessage();
        // continue with next db
        continue;
      }
    } else {
      getDBProperties(*_directory, db.second);
    }

    // read encryption info
    ::checkEncryption(*_directory);

    // read dump info
    bool useEnvelope = _options.useEnvelope;
    result = ::checkDumpDatabase(server(), *_directory, _options.forceSameDatabase, useEnvelope);
    if (result.fail()) {
      LOG_TOPIC("0cbdf", FATAL, arangodb::Logger::RESTORE) << result.errorMessage();
      FATAL_ERROR_EXIT();
    }

    LOG_TOPIC_IF("52b23", INFO, arangodb::Logger::RESTORE, _options.continueRestore) << "try to continue previous restore";
    _progressTracker = std::make_unique<RestoreProgressTracker>(*_directory, !_options.continueRestore);

    filesToClean.push_back(_progressTracker->filename());

    // run the actual restore
    try {
      result = ::processInputDirectory(*httpClient, _clientTaskQueue, *this,
                                      _options, *_directory, *_progressTracker, _stats, useEnvelope);
    } catch (basics::Exception const& ex) {
      LOG_TOPIC("52b22", ERR, arangodb::Logger::RESTORE) << "caught exception: " << ex.what();
      result = {ex.code(), ex.what()};
    } catch (std::exception const& ex) {
      LOG_TOPIC("8f13f", ERR, arangodb::Logger::RESTORE) << "caught exception: " << ex.what();
      result = {TRI_ERROR_INTERNAL, ex.what()};
    } catch (...) {
      LOG_TOPIC("a74e8", ERR, arangodb::Logger::RESTORE) << "caught unknown exception";
      result = {TRI_ERROR_INTERNAL};
    }

    _clientTaskQueue.waitForIdle();

    if (result.fail()) {
      break;
    }
  }

  if (result.fail()) {
    LOG_TOPIC("cb69f", ERR, arangodb::Logger::RESTORE) << result.errorMessage();
    _exitCode = EXIT_FAILURE;
  } else {
    for (auto const& fn : filesToClean) {
      [[maybe_unused]] auto result = basics::FileUtils::remove(fn);
    }
  }

  if (_options.progress) {
    double totalTime = TRI_microtime() - start;

    if (_options.importData) {
      LOG_TOPIC("a66e1", INFO, Logger::RESTORE)
          << "Processed " << _stats.restoredCollections << " collection(s) in "
          << Logger::FIXED(totalTime, 6) << " s, "
          << "read " << _stats.totalRead << " byte(s) from datafiles, "
          << "sent " << _stats.totalBatches << " data batch(es) of "
          << _stats.totalSent << " byte(s) total size";
    } else if (_options.importStructure) {
      LOG_TOPIC("147ca", INFO, Logger::RESTORE)
          << "Processed " << _stats.restoredCollections << " collection(s) in "
          << Logger::FIXED(totalTime, 6) << " s";
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

RestoreFeature::CollectionStatus::CollectionStatus(VPackSlice slice) {
  using arangodb::basics::VelocyPackHelper;
  state = VelocyPackHelper::getNumericValue<CollectionState>(slice, "state", CollectionState::UNKNOWN);
  bytes_acked = VelocyPackHelper::getNumericValue<size_t>(slice, "bytes-acked", 0);
}

void RestoreFeature::CollectionStatus::toVelocyPack(VPackBuilder& builder) const {
  {
    VPackObjectBuilder object(&builder);
    builder.add("state", VPackValue(state));
    if (bytes_acked != 0) {
      builder.add("bytes-acked", VPackValue(bytes_acked));
    }
  }
}

RestoreFeature::CollectionStatus::CollectionStatus(RestoreFeature::CollectionState state,
                                                   size_t bytes_acked)
    : state(state), bytes_acked(bytes_acked) {}

RestoreFeature::CollectionStatus::CollectionStatus() = default;

}  // namespace arangodb
