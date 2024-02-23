////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/BumpFileDescriptorsFeature.h"
#include "ApplicationFeatures/GreetingsFeature.h"
#include "Basics/FileUtils.h"
#include "Basics/NumberOfCores.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/Utf8Helper.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Basics/files.h"
#include "Basics/system-functions.h"
#include "FeaturePhases/BasicFeaturePhaseClient.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Random/RandomGenerator.h"
#include "Shell/ClientFeature.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/HttpResponseChecker.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Ssl/SslInterface.h"
#include "Utilities/NameValidator.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Encryption/EncryptionFeature.h"
#endif

#include <absl/strings/str_cat.h>

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/Validator.h>

#include <algorithm>
#include <chrono>
#include <regex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>

namespace {
constexpr std::string_view getSuffix(bool useVPack) noexcept {
  std::string_view suffix("json");
  if (useVPack) {
    suffix = "vpack";
  }
  return suffix;
}

std::regex const splitFilesRegex(".*\\.[0-9]+\\.data\\.(json|vpack)(\\.gz)?$",
                                 std::regex::ECMAScript);

std::string escapedCollectionName(std::string const& name,
                                  VPackSlice parameters) {
  std::string escapedName = name;
  if (arangodb::CollectionNameValidator::validateName(/*isSystem*/ true, false,
                                                      name)
          .fail()) {
    // we have a collection name with special characters.
    // we should not try to save the collection under its name in the
    // filesystem. instead, we will use the collection id as part of the
    // filename. try looking up collection id in "cid"
    VPackSlice idSlice = parameters.get(arangodb::StaticStrings::DataSourceCid);
    if (idSlice.isNone() &&
        parameters.hasKey(arangodb::StaticStrings::DataSourceId)) {
      // "cid" not present, try "id" (there seems to be difference between
      // cluster and single server about which attribute is present)
      idSlice = parameters.get(arangodb::StaticStrings::DataSourceId);
    }
    if (idSlice.isString()) {
      escapedName = idSlice.copyString();
    } else if (idSlice.isNumber<uint64_t>()) {
      escapedName = std::to_string(idSlice.getNumber<uint64_t>());
    } else {
      escapedName =
          std::to_string(arangodb::RandomGenerator::interval(UINT64_MAX));
    }
  }
  return escapedName;
}

/// @brief return the target replication factor for the specified collection
uint64_t getReplicationFactor(arangodb::RestoreFeature::Options const& options,
                              arangodb::velocypack::Slice slice,
                              bool& isSatellite) {
  uint64_t result = options.defaultReplicationFactor;
  isSatellite = false;

  arangodb::velocypack::Slice s =
      slice.get(arangodb::StaticStrings::ReplicationFactor);
  if (s.isInteger()) {
    result = s.getNumericValue<uint64_t>();
  } else if (s.isString() &&
             s.stringView() == arangodb::StaticStrings::Satellite) {
    isSatellite = true;
  }

  s = slice.get("name");
  if (!s.isString()) {
    // should not happen, but anyway, let's be safe here
    return result;
  }

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

    // look if we have a more specific value, e.g. `--replicationFactor
    // myCollection=3`
    if (parts.size() != 2 || parts[0] != s.stringView()) {
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

  return result;
}

/// @brief return the target replication factor for the specified collection
uint64_t getWriteConcern(arangodb::RestoreFeature::Options const& options,
                         arangodb::velocypack::Slice const& slice) {
  uint64_t result = 1;

  arangodb::velocypack::Slice s =
      slice.get(arangodb::StaticStrings::WriteConcern);
  if (s.isInteger()) {
    result = s.getNumericValue<uint64_t>();
  }

  s = slice.get("name");
  if (!s.isString()) {
    // should not happen, but anyway, let's be safe here
    return result;
  }

  if (!options.writeConcern.empty()) {
    std::string const name = s.copyString();

    for (auto const& it : options.writeConcern) {
      auto parts = arangodb::basics::StringUtils::split(it, '=');
      if (parts.size() == 1) {
        result = arangodb::basics::StringUtils::uint64(parts[0]);
      }
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

/// @brief return the target number of shards for the specified collection
uint64_t getNumberOfShards(arangodb::RestoreFeature::Options const& options,
                           arangodb::velocypack::Slice slice) {
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

  for (auto const& it : options.numberOfShards) {
    auto parts = arangodb::basics::StringUtils::split(it, '=');
    if (parts.size() == 1) {
      // this is the default value, e.g. `--numberOfShards 2`
      result = arangodb::basics::StringUtils::uint64(parts[0]);
    }

    // look if we have a more specific value, e.g. `--numberOfShards
    // myCollection=3`
    if (parts.size() != 2 || parts[0] != s.stringView()) {
      // somehow invalid or different collection
      continue;
    }
    result = arangodb::basics::StringUtils::uint64(parts[1]);
    break;
  }

  return result;
}

void makeAttributesUnique(arangodb::velocypack::Builder& builder,
                          arangodb::velocypack::Slice slice) {
  if (slice.isObject()) {
    std::unordered_set<std::string_view> keys;

    builder.openObject();

    auto it = arangodb::velocypack::ObjectIterator(slice, true);

    while (it.valid()) {
      if (!keys.emplace(it.key().stringView()).second) {
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
arangodb::Result tryCreateDatabase(
    arangodb::ArangoRestoreServer& server, std::string const& name,
    VPackSlice properties, arangodb::RestoreFeature::Options const& options) {
  using arangodb::httpclient::SimpleHttpClient;
  using arangodb::httpclient::SimpleHttpResult;
  using arangodb::rest::RequestType;
  using arangodb::rest::ResponseCode;
  using arangodb::velocypack::ArrayBuilder;
  using arangodb::velocypack::ObjectBuilder;

  // get client feature for configuration info
  arangodb::ClientFeature& client =
      server.getFeature<arangodb::HttpEndpointProvider,
                        arangodb::ClientFeature>();

  client.setDatabaseName(arangodb::StaticStrings::SystemDatabase);

  // get httpclient by hand rather than using manager, to bypass any built-in
  // checks which will fail if the database doesn't exist
  std::unique_ptr<SimpleHttpClient> httpClient;
  try {
    httpClient = client.createHttpClient(0);  // thread number zero
  } catch (...) {
    LOG_TOPIC("832ef", FATAL, arangodb::Logger::RESTORE)
        << "cannot create server connection, giving up!";
    return {TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT};
  }

  VPackBuilder builder;
  {
    ObjectBuilder object(&builder);
    object->add(arangodb::StaticStrings::DatabaseName, VPackValue(name));

    // add replication factor, write concern, sharding, if set
    if (properties.isObject()) {
      ObjectBuilder guard(&builder, "options");
      for (auto const& key : std::array<std::string_view, 3>{
               arangodb::StaticStrings::ReplicationFactor,
               arangodb::StaticStrings::Sharding,
               arangodb::StaticStrings::WriteConcern}) {
        VPackSlice slice = properties.get(key);
        if (key == arangodb::StaticStrings::ReplicationFactor) {
          // overwrite replicationFactor if set
          bool isSatellite = false;
          uint64_t replicationFactor =
              getReplicationFactor(options, properties, isSatellite);
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
  std::unique_ptr<SimpleHttpResult> response(httpClient->request(
      RequestType::POST, "/_api/database", body.c_str(), body.size()));
  return arangodb::HttpResponseChecker::check(
      httpClient->getErrorMessage(), response.get(), "creating database", body,
      arangodb::HttpResponseChecker::PayloadType::JSON);
}

/// @check If directory is encrypted, check that key option is specified
void checkEncryption(arangodb::ManagedDirectory& directory) {
#ifdef USE_ENTERPRISE
  using arangodb::Logger;
  if (directory.isEncrypted()) {
    if (!directory.encryptionFeature()->keyOptionSpecified()) {
      LOG_TOPIC("cc58e", WARN, Logger::RESTORE)
          << "the dump data seems to be encrypted with "
          << directory.encryptionType()
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
  }
#endif
}

void getDBProperties(arangodb::ManagedDirectory& directory,
                     VPackBuilder& builder) {
  VPackBuilder fileContentBuilder;

  VPackSlice slice = VPackSlice::emptyObjectSlice();
  try {
    fileContentBuilder = directory.vpackFromJsonFile("dump.json");
  } catch (std::exception const& ex) {
    LOG_TOPIC("5ad64", WARN, arangodb::Logger::RESTORE)
        << "could not read dump.json file: " << ex.what();
    builder.add(slice);
  } catch (...) {
    LOG_TOPIC("3a5a4", WARN, arangodb::Logger::RESTORE)
        << "could not read dump.json file: "
        << directory.status().errorMessage();
    builder.add(slice);
    return;
  }

  try {
    auto props =
        fileContentBuilder.slice().get(arangodb::StaticStrings::Properties);
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
arangodb::Result checkDumpDatabase(arangodb::ArangoRestoreServer& server,
                                   arangodb::ManagedDirectory& directory,
                                   bool forceSameDatabase, bool& useEnvelope,
                                   bool& useVPack) {
  using arangodb::ClientFeature;
  using arangodb::HttpEndpointProvider;
  using arangodb::Logger;
  using arangodb::application_features::ApplicationServer;

  std::string databaseName;
  try {
    VPackBuilder fileContentBuilder = directory.vpackFromJsonFile("dump.json");
    VPackSlice fileContent = fileContentBuilder.slice();
    databaseName = fileContent.get("database").copyString();
    if (VPackSlice s = fileContent.get("useEnvelope"); s.isBoolean()) {
      useEnvelope = s.getBoolean();
    }
    if (VPackSlice s = fileContent.get("useVPack"); s.isBoolean()) {
      useVPack = s.getBoolean();
    }
  } catch (...) {
    // the above may go wrong for several reasons
  }

  if (!databaseName.empty()) {
    LOG_TOPIC("abeb4", INFO, Logger::RESTORE)
        << "Database name in source dump is '" << databaseName << "'";
  }

  ClientFeature& client =
      server.getFeature<HttpEndpointProvider, ClientFeature>();
  if (forceSameDatabase && databaseName != client.databaseName()) {
    return {TRI_ERROR_BAD_PARAMETER,
            absl::StrCat("database name in dump.json ('", databaseName,
                         "') does not match specified database name ('",
                         client.databaseName(), "')")};
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

  std::string url = absl::StrCat(
      "/_api/replication/restore-collection?overwrite=",
      (options.overwrite ? "true" : "false"),
      "&force=", (options.force ? "true" : "false"),
      "&ignoreDistributeShardsLikeErrors=",
      (options.ignoreDistributeShardsLikeErrors ? "true" : "false"));

  VPackSlice parameters = slice.get("parameters");

  // build cluster options using command-line parameter values
  VPackBuilder newOptions;
  newOptions.openObject();
  bool isSatellite = false;
  uint64_t replicationFactor =
      getReplicationFactor(options, parameters, isSatellite);
  if (isSatellite) {
    newOptions.add(arangodb::StaticStrings::ReplicationFactor,
                   VPackValue(arangodb::StaticStrings::Satellite));
  } else {
    newOptions.add(arangodb::StaticStrings::ReplicationFactor,
                   VPackValue(replicationFactor));
  }
  newOptions.add(arangodb::StaticStrings::NumberOfShards,
                 VPackValue(getNumberOfShards(options, parameters)));
  newOptions.add(arangodb::StaticStrings::WriteConcern,
                 VPackValue(getWriteConcern(options, parameters)));

  // enable revision trees for the collection if the parameters are not set
  // (likely a collection from the pre-3.8 era)
  if (options.enableRevisionTrees) {
    if ((parameters.get(arangodb::StaticStrings::SyncByRevision).isNone() ||
         parameters.get(arangodb::StaticStrings::SyncByRevision).isTrue()) &&
        (parameters.get(arangodb::StaticStrings::UsesRevisionsAsDocumentIds)
             .isNone() ||
         parameters.get(arangodb::StaticStrings::UsesRevisionsAsDocumentIds)
             .isTrue())) {
      newOptions.add(arangodb::StaticStrings::SyncByRevision, VPackValue(true));
      newOptions.add(arangodb::StaticStrings::UsesRevisionsAsDocumentIds,
                     VPackValue(true));
    }
  }

  newOptions.close();

  VPackBuilder b;
  b.openObject();
  b.add("indexes", slice.get("indexes"));
  b.add(VPackValue("parameters"));
  VPackCollection::merge(b, parameters, newOptions.slice(), true, false);
  b.close();

  std::string const body = b.slice().toJson();

  std::unique_ptr<SimpleHttpResult> response(httpClient.request(
      arangodb::rest::RequestType::PUT, url, body.c_str(), body.size()));
  return arangodb::HttpResponseChecker::check(
      httpClient.getErrorMessage(), response.get(), "restoring collection",
      body, arangodb::HttpResponseChecker::PayloadType::JSON);
}

/// @brief Recreate a collection given its description
arangodb::Result recreateCollection(
    arangodb::httpclient::SimpleHttpClient& httpClient,
    arangodb::RestoreFeature::RestoreMainJob& job) {
  using arangodb::Logger;

  VPackSlice parameters = job.parameters;
  int type = arangodb::basics::VelocyPackHelper::getNumericValue<int>(
      parameters.get(std::vector<std::string_view>({"parameters", "type"})), 2);
  std::string const collectionType(type == 2 ? "document" : "edge");

  // re-create collection
  if (job.options.progress) {
    if (job.options.overwrite) {
      LOG_TOPIC("9b414", INFO, Logger::RESTORE)
          << "# Re-creating " << collectionType << " collection '"
          << job.collectionName << "'...";
    } else {
      LOG_TOPIC("a9123", INFO, Logger::RESTORE)
          << "# Creating " << collectionType << " collection '"
          << job.collectionName << "'...";
    }
  }

  arangodb::Result result = ::sendRestoreCollection(
      httpClient, job.options, job.parameters, job.collectionName);

  if (result.fail()) {
    LOG_TOPIC("c6658", WARN, Logger::RESTORE)
        << "Error while creating " << collectionType << " collection '"
        << job.collectionName << "': " << result.errorMessage();

    if (job.options.force) {
      result.reset();
    }
  }
  return result;
}

/// @brief Restore the data for a given view
arangodb::Result restoreView(arangodb::httpclient::SimpleHttpClient& httpClient,
                             arangodb::RestoreFeature::Options const& options,
                             VPackSlice const& viewDefinition) {
  using arangodb::httpclient::SimpleHttpResult;

  std::string url = absl::StrCat("/_api/replication/restore-view?overwrite=",
                                 (options.overwrite ? "true" : "false"),
                                 "&force=", (options.force ? "true" : "false"));

  std::string const body = viewDefinition.toJson();
  std::unique_ptr<SimpleHttpResult> response(httpClient.request(
      arangodb::rest::RequestType::PUT, url, body.c_str(), body.size()));
  return arangodb::HttpResponseChecker::check(
      httpClient.getErrorMessage(), response.get(), "restoring view", body,
      arangodb::HttpResponseChecker::PayloadType::JSON);
}

arangodb::Result triggerFoxxHeal(
    arangodb::httpclient::SimpleHttpClient& httpClient) {
  using arangodb::Logger;
  using arangodb::httpclient::SimpleHttpResult;
  std::string body = "";

  // check if the foxx api is available.
  std::string statusUrl = "/_admin/status";
  std::unique_ptr<SimpleHttpResult> response(
      httpClient.request(arangodb::rest::RequestType::POST, statusUrl,
                         body.c_str(), body.length()));

  auto res = arangodb::HttpResponseChecker::check(
      httpClient.getErrorMessage(), response.get(), "check status", body,
      arangodb::HttpResponseChecker::PayloadType::JSON);
  if (res.ok() && response) {
    try {
      if (!response->getBodyVelocyPack()->slice().get("foxxApi").getBool()) {
        LOG_TOPIC("9e9b9", INFO, Logger::RESTORE)
            << "skipping foxx self-healing because Foxx API is disabled";
        return {};
      }
    } catch (...) {
      // API Not available because of older version or whatever
    }
  }

  std::string foxxHealUrl = "/_api/foxx/_local/heal";
  response.reset(httpClient.request(arangodb::rest::RequestType::POST,
                                    foxxHealUrl, body.c_str(), body.length()));
  return arangodb::HttpResponseChecker::check(
      httpClient.getErrorMessage(), response.get(), "trigger self heal", body,
      arangodb::HttpResponseChecker::PayloadType::JSON);
}

arangodb::Result processInputDirectory(
    arangodb::httpclient::SimpleHttpClient& httpClient,
    arangodb::ClientTaskQueue<arangodb::RestoreFeature::RestoreJob>& jobQueue,
    arangodb::RestoreFeature& feature,
    arangodb::RestoreFeature::Options const& options,
    arangodb::ManagedDirectory& directory,
    arangodb::RestoreFeature::RestoreProgressTracker& progressTracker,
    arangodb::RestoreFeature::Stats& stats, bool useEnvelope, bool useVPack) {
  using arangodb::Logger;
  using arangodb::Result;
  using arangodb::StaticStrings;
  using arangodb::basics::VelocyPackHelper;
  using arangodb::basics::FileUtils::listFiles;
  using arangodb::basics::StringUtils::formatSize;

  auto fill = [](std::unordered_map<std::string, bool>& map,
                 std::vector<std::string> const& requested) {
    for (auto const& name : requested) {
      map.emplace(name, false);
    }
  };

  auto checkRequested = [](std::unordered_map<std::string, bool>& map,
                           std::string const& name) {
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
        if (file.ends_with(viewsSuffix)) {
          if (!restrictColls.empty() && restrictViews.empty()) {
            continue;  // skip view if not specifically included
          }

          VPackBuilder contentBuilder = directory.vpackFromJsonFile(file);
          VPackSlice const fileContent = contentBuilder.slice();
          if (!fileContent.isObject()) {
            return {TRI_ERROR_INTERNAL,
                    absl::StrCat("could not read view file '",
                                 directory.pathToFile(file),
                                 "': ", directory.status().errorMessage())};
          }

          std::string name = VelocyPackHelper::getStringValue(
              fileContent, StaticStrings::DataSourceName, "");
          if (!checkRequested(restrictViews, name)) {
            // view name not in list
            continue;
          }

          views.emplace_back(std::move(contentBuilder));
          continue;
        }

        if (!file.ends_with(collectionSuffix)) {
          // some other file
          continue;
        }

        // found a structure.json file
        std::string name =
            file.substr(0, file.size() - collectionSuffix.size());
        if (!options.includeSystemCollections && name.starts_with('_')) {
          continue;
        }

        VPackBuilder fileContentBuilder = directory.vpackFromJsonFile(file);
        VPackSlice fileContent = fileContentBuilder.slice();
        if (!fileContent.isObject()) {
          return {TRI_ERROR_INTERNAL,
                  absl::StrCat("could not read collection structure file '",
                               directory.pathToFile(file),
                               "': ", directory.status().errorMessage())};
        }

        VPackSlice const parameters = fileContent.get("parameters");
        VPackSlice const indexes = fileContent.get("indexes");
        if (!parameters.isObject() || !indexes.isArray()) {
          return {TRI_ERROR_BAD_PARAMETER,
                  absl::StrCat("could not read collection structure file '",
                               directory.pathToFile(file),
                               "': file has wrong internal format")};
        }
        std::string cname = VelocyPackHelper::getStringValue(
            parameters, StaticStrings::DataSourceName, "");

        // problem: collection name may contain arbitrary characters
        std::string escapedName = escapedCollectionName(cname, parameters);
        bool overwriteName = false;
        if (cname != name && name != escapedName &&
            name != absl::StrCat(cname, "_",
                                 arangodb::rest::SslInterface::sslMD5(cname)) &&
            name != absl::StrCat(escapedName, "_",
                                 arangodb::rest::SslInterface::sslMD5(cname))) {
          // file has a different name than found in structure file
          if (options.importStructure) {
            // we cannot go on if there is a mismatch
            return {
                TRI_ERROR_INTERNAL,
                absl::StrCat(
                    "collection name mismatch in collection structure file '",
                    directory.pathToFile(file), "' (offending value: '", cname,
                    "')")};
          } else {
            // we can patch the name in our array and go on
            LOG_TOPIC("8e7b7", INFO, Logger::RESTORE)
                << "ignoring collection name mismatch in collection "
                   "structure file '"
                << directory.pathToFile(file) << "' (offending value: '"
                << cname << "')";

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
            VPackBuilder parametersWithoutIndexes = VPackCollection::remove(
                parameters, std::vector<std::string>{"indexes"});

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
    arangodb::RestoreFeature::sortCollectionsForCreation(collections);

    std::unique_ptr<arangodb::RestoreFeature::RestoreMainJob> usersData;
    std::unique_ptr<arangodb::RestoreFeature::RestoreMainJob> analyzersData;
    std::vector<std::unique_ptr<arangodb::RestoreFeature::RestoreMainJob>> jobs;
    jobs.reserve(collections.size());

    bool didModifyFoxxCollection = false;
    // Step 3: create collections
    for (VPackBuilder const& b : collections) {
      VPackSlice collection = b.slice();

      LOG_TOPIC("c601a", DEBUG, Logger::RESTORE)
          << "# Processing collection: " << collection.toJson();

      VPackSlice params = collection.get("parameters");
      VPackSlice name = VPackSlice::emptyStringSlice();
      if (params.isObject()) {
        name = params.get("name");
        // Only these two are relevant for FOXX.
        if (name.isString() &&
            (name.isEqualString(arangodb::StaticStrings::AppsCollection) ||
             name.isEqualString(
                 arangodb::StaticStrings::AppBundlesCollection))) {
          didModifyFoxxCollection = true;
        }
      }

      auto job = std::make_unique<arangodb::RestoreFeature::RestoreMainJob>(
          directory, feature, progressTracker, options, stats, collection,
          useEnvelope, useVPack);

      // take care of collection creation now, serially
      if (options.importStructure &&
          progressTracker.getStatus(name.copyString()).state <
              arangodb::RestoreFeature::CREATED) {
        Result result = ::recreateCollection(httpClient, *job);
        if (result.fail()) {
          return result;
        }
      }

      if (progressTracker.getStatus(name.copyString()).state <
          arangodb::RestoreFeature::CREATED) {
        progressTracker.updateStatus(
            name.copyString(), arangodb::RestoreFeature::CollectionStatus{
                                   arangodb::RestoreFeature::CREATED, {}});
      }

      if (name.isString() &&
          name.stringView() == arangodb::StaticStrings::UsersCollection) {
        // special treatment for _users collection - this must be the very last,
        // and run isolated from all previous data loading operations - the
        // reason is that loading into the users collection may change the
        // credentials for the current arangorestore connection!
        usersData = std::move(job);
      } else if (name.isString() &&
                 name.stringView() == StaticStrings::AnalyzersCollection) {
        // special treatment for _analyzers collection - this must be the very
        // first
        stats.totalCollections++;
        analyzersData = std::move(job);
      } else {
        stats.totalCollections++;
        jobs.push_back(std::move(job));
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

    auto createViews = [&](std::string_view type) {
      auto const specialName = absl::StrCat("_VIEW_MARKER_", type);
      auto status = progressTracker.getStatus(specialName);

      if (status.state == arangodb::RestoreFeature::RESTORED) {
        LOG_TOPIC("79e1b", INFO, Logger::RESTORE)
            << "# " << type << " views already created...";
        return Result{};
      }

      if (options.importStructure && !views.empty()) {
        LOG_TOPIC("f723c", INFO, Logger::RESTORE)
            << "# Creating " << type << " views...";

        for (auto const& viewDefinition : views) {
          auto slice = viewDefinition.slice();
          LOG_TOPIC("c608d", DEBUG, Logger::RESTORE)
              << "# Creating view: " << slice.toJson();
          if (auto viewType = slice.get("type");
              !viewType.isString() || viewType.stringView() != type) {
            continue;
          }
          if (auto r = ::restoreView(httpClient, options, slice); !r.ok()) {
            return r;
          }
        }
      }
      status.state = arangodb::RestoreFeature::RESTORED;
      progressTracker.updateStatus(specialName, status);

      return Result{};
    };

    // Step 5: create arangosearch views
    auto r = createViews("arangosearch");
    if (!r.ok()) {
      return r;
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
              << "# Current restore progress: restored "
              << stats.restoredCollections << " of " << stats.totalCollections
              << " collection(s), read " << formatSize(stats.totalRead)
              << " from datafiles (after decompression), "
              << "sent " << stats.totalBatches << " data batch(es) of "
              << formatSize(stats.totalSent) << " total size"
              << ", queued jobs: " << std::get<0>(queueStats)
              << ", total workers: " << std::get<1>(queueStats)
              << ", idle workers: " << std::get<2>(queueStats);
          start = now;
        }

        // don't sleep for too long, as we want to quickly terminate
        // when the gets empty
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
      }
    }

    jobQueue.waitForIdle();
    jobs.clear();

    // Step 7: create search-alias views
    r = createViews("search-alias");
    if (!r.ok()) {
      return r;
    }

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
            << "- in the cluster Foxx services will be available eventually, "
               "On single servers send "
            << "a POST to '/_api/foxx/_local/heal' on the current database, "
            << "with an empty body. Please note that any of this is not "
               "necessary if the Foxx APIs "
            << "have been turned off on the server using the option "
               "`--foxx.api false`.";
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
void processJob(arangodb::httpclient::SimpleHttpClient& client,
                arangodb::RestoreFeature::RestoreJob& job) {
  arangodb::Result res;
  try {
    res = job.run(client);
  } catch (arangodb::basics::Exception const& ex) {
    res.reset(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    res.reset(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    res.reset(TRI_ERROR_INTERNAL, "unknown exception");
  }

  if (res.fail()) {
    job.feature.reportError(res);
  }
}

}  // namespace

namespace arangodb {

/// @brief Sort collections for proper recreation order
void RestoreFeature::sortCollectionsForCreation(
    std::vector<VPackBuilder>& collections) {
  enum class Rel {
    Less,
    Equal,
    Greater,
  };

  // use an existing operator<() to get a Rel
  auto constexpr cmp = [](auto const& left, auto const& right) -> Rel {
    if (left < right) {
      return Rel::Less;
    } else if (right < left) {
      return Rel::Greater;
    } else {
      return Rel::Equal;
    }
  };

  // project both values via supplied function before comparison
  auto const cmpBy = [&cmp](auto const& projection, auto const& left,
                            auto const& right) {
    return cmp(projection(left), projection(right));
  };

  // Orders distributeShardsLike-followers after (all) other collections,
  // apart from that imposes no additional ordering.
  auto constexpr dslFollowerToOrderedValue = [](auto const& slice) {
    return !slice.get(StaticStrings::DistributeShardsLike).isNone();
  };

  // Orders document collections before edge collections,
  // apart from that imposes no additional ordering.
  auto constexpr typeToOrderedValue = [](auto const& slice) {
    // If type is not set, default to document collection (2). Edge collections
    // are 3.
    return basics::VelocyPackHelper::getNumericValue<int>(
        slice, StaticStrings::DataSourceType, 2);
  };

  // Orders system collections before other collections,
  // apart from that imposes no additional ordering.
  auto constexpr systemToOrderedValue = [](auto const& name) {
    auto isSystem = name.at(0) == '_';
    return isSystem ? 0 : 1;
  };

  auto const chainedComparison = [&](auto const& left,
                                     auto const& right) -> Rel {
    auto const leftName = left.get(StaticStrings::DataSourceName).copyString();
    auto const rightName =
        right.get(StaticStrings::DataSourceName).copyString();
    if (auto cmpRes = cmpBy(dslFollowerToOrderedValue, left, right);
        cmpRes != Rel::Equal) {
      return cmpRes;
    }
    if (auto cmpRes = cmpBy(typeToOrderedValue, left, right);
        cmpRes != Rel::Equal) {
      return cmpRes;
    }
    if (auto cmpRes = cmpBy(systemToOrderedValue, leftName, rightName);
        cmpRes != Rel::Equal) {
      return cmpRes;
    }

    auto res = strcasecmp(leftName.c_str(), rightName.c_str());
    auto cmpRes = Rel{};
    if (res < 0) {
      cmpRes = Rel::Less;
    } else if (res > 0) {
      cmpRes = Rel::Greater;
    } else {
      cmpRes = Rel::Equal;
    }
    return cmpRes;
  };

  auto const colLesserThan = [&](auto const& l, auto const& r) {
    auto const left = l.slice().get(StaticStrings::DataSourceParameters);
    auto const right = r.slice().get(StaticStrings::DataSourceParameters);
    return chainedComparison(left, right) == Rel::Less;
  };

  std::sort(collections.begin(), collections.end(), colLesserThan);
}

std::vector<RestoreFeature::DatabaseInfo> RestoreFeature::determineDatabaseList(
    std::string const& databaseName) {
  std::vector<RestoreFeature::DatabaseInfo> databases;
  if (_options.allDatabases) {
    for (auto const& it : basics::FileUtils::listFiles(_options.inputPath)) {
      std::string path =
          basics::FileUtils::buildFilename(_options.inputPath, it);
      if (basics::FileUtils::isDirectory(path)) {
        EncryptionFeature* encryption{};
        if constexpr (Server::contains<EncryptionFeature>()) {
          if (server().hasFeature<EncryptionFeature>()) {
            encryption = &server().getFeature<EncryptionFeature>();
          }
        }

        ManagedDirectory dbDirectory(encryption, path, false, false, false);
        databases.push_back({it, VPackBuilder{}, ""});
        getDBProperties(dbDirectory, databases.back().properties);
        try {
          databases.back().name =
              databases.back().properties.slice().get("name").copyString();
        } catch (...) {
          databases.back().name = it;
        }
      }
    }

    // sort by name, with _system last
    // this is necessary because in the system database there is the _users
    // collection, and we have to process users last of all. otherwise we risk
    // updating the credentials for the user which uses the current
    // arangorestore connection, and this will make subsequent arangorestore
    // calls to the server fail with "unauthorized"
    std::sort(databases.begin(), databases.end(),
              [](auto const& lhs, auto const& rhs) {
                if (lhs.name == StaticStrings::SystemDatabase &&
                    rhs.name != StaticStrings::SystemDatabase) {
                  return false;
                } else if (rhs.name == StaticStrings::SystemDatabase &&
                           lhs.name != StaticStrings::SystemDatabase) {
                  return true;
                }
                return lhs.name < rhs.name;
              });
    if (databases.empty()) {
      LOG_TOPIC("b41d9", FATAL, Logger::RESTORE)
          << "Unable to find per-database subdirectories in input directory '"
          << _options.inputPath << "'. No data will be restored!";
      FATAL_ERROR_EXIT();
    }
  } else {
    databases.push_back({databaseName, VPackBuilder{}, databaseName});
  }
  return databases;
}

RestoreFeature::RestoreJob::RestoreJob(RestoreFeature& feature,
                                       RestoreProgressTracker& progressTracker,
                                       RestoreFeature::Options const& options,
                                       RestoreFeature::Stats& stats,
                                       std::string const& collectionName,
                                       std::shared_ptr<SharedState> sharedState)
    : feature{feature},
      progressTracker{progressTracker},
      options{options},
      stats{stats},
      collectionName(collectionName),
      sharedState(std::move(sharedState)) {}

RestoreFeature::RestoreJob::~RestoreJob() = default;

Result RestoreFeature::RestoreJob::sendRestoreData(
    arangodb::httpclient::SimpleHttpClient& client,
    MultiFileReadOffset readOffset, char const* buffer, size_t bufferSize,
    bool useVPack) {
  using arangodb::basics::StringUtils::urlEncode;
  using arangodb::httpclient::SimpleHttpResult;

  std::string url = absl::StrCat(
      "/_api/replication/restore-data?collection=", urlEncode(collectionName),
      "&force=", (options.force ? "true" : "false"));

  std::unordered_map<std::string, std::string> headers;
  if (useVPack) {
    // tell server we are sending VPack
    headers.emplace(arangodb::StaticStrings::ContentTypeHeader,
                    arangodb::StaticStrings::MimeTypeVPack);
  } else {
    // tell server we are sending JSONL
    headers.emplace(arangodb::StaticStrings::ContentTypeHeader,
                    arangodb::StaticStrings::MimeTypeDump);
  }

  std::unique_ptr<SimpleHttpResult> response(client.request(
      arangodb::rest::RequestType::PUT, url, buffer, bufferSize, headers));
  arangodb::Result res = arangodb::HttpResponseChecker::check(
      client.getErrorMessage(), response.get(), "restoring data",
      std::string_view(buffer, bufferSize),
      (useVPack ? arangodb::HttpResponseChecker::PayloadType::VPACK
                : arangodb::HttpResponseChecker::PayloadType::JSONL));

  if (res.fail()) {
    // error
    LOG_TOPIC("a595a", WARN, Logger::RESTORE)
        << "Error while restoring data into collection '" << collectionName
        << "': " << res.errorMessage();
    // leave readOffset in place in readOffsets, because the job did not succeed
    {
      // store error
      std::lock_guard locker{sharedState->mutex};
      sharedState->result = res;
    }
  } else {
    // no error

    {
      std::lock_guard locker{sharedState->mutex};
      TRI_ASSERT(!sharedState->readOffsets.empty());

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
      if (options.failOnUpdateContinueFile) {
        auto it = sharedState->readOffsets.find(readOffset);
        TRI_ASSERT(it != sharedState->readOffsets.end());
        [[maybe_unused]] bool wasSynced = progressTracker.updateStatus(
            collectionName, arangodb::RestoreFeature::CollectionStatus{
                                arangodb::RestoreFeature::RESTORING,
                                readOffset + (*it).second});
        if (wasSynced) {
          LOG_TOPIC("a87bf", WARN, Logger::RESTORE)
              << "triggered failure point at offset " << readOffset << "!";
          FATAL_ERROR_EXIT_CODE(
              38);  // exit with exit code 38 to report to the test frame work
                    // that this was an intentional crash
        }
      }
#endif
      sharedState->readOffsets.erase(readOffset);
    }

    updateProgress();
  }

  stats.totalBatches++;
  stats.totalSent += bufferSize;
  return res;
}

void RestoreFeature::RestoreJob::updateProgress() {
  std::unique_lock locker{sharedState->mutex};

  if (!sharedState->readOffsets.empty()) {
    auto it = sharedState->readOffsets.begin();
    auto readOffset = (*it).first;

    // progressTracker has its own lock
    locker.unlock();

    progressTracker.updateStatus(
        collectionName, arangodb::RestoreFeature::CollectionStatus{
                            arangodb::RestoreFeature::RESTORING, readOffset});
  } else if (sharedState->readCompleteInputfile) {
    // we are done with restoring the entire collection

    // progressTracker has its own lock
    locker.unlock();

    progressTracker.updateStatus(collectionName,
                                 arangodb::RestoreFeature::CollectionStatus{
                                     arangodb::RestoreFeature::RESTORED, {}});
  }
}

RestoreFeature::RestoreMainJob::RestoreMainJob(
    ManagedDirectory& directory, RestoreFeature& feature,
    RestoreProgressTracker& progressTracker,
    RestoreFeature::Options const& options, RestoreFeature::Stats& stats,
    VPackSlice parameters, bool useEnvelope, bool useVPack)
    : RestoreJob(feature, progressTracker, options, stats,
                 parameters.get({"parameters", "name"}).copyString(),
                 std::make_shared<SharedState>()),
      directory{directory},
      parameters{parameters},
      useEnvelope{useEnvelope},
      useVPack{useVPack} {}

Result RestoreFeature::RestoreMainJob::run(
    arangodb::httpclient::SimpleHttpClient& client) {
  // restore indexes first
  arangodb::Result res = restoreIndexes(client);
  if (res.ok() && options.importData) {
    res = restoreData(client, useVPack);

    if (res.ok()) {
      ++stats.restoredCollections;

      if (options.progress) {
        int type = arangodb::basics::VelocyPackHelper::getNumericValue<int>(
            parameters.get(
                std::vector<std::string_view>({"parameters", "type"})),
            2);
        std::string const collectionType(type == 2 ? "document" : "edge");
        LOG_TOPIC("6ae09", INFO, arangodb::Logger::RESTORE)
            << "# Successfully restored " << collectionType << " collection '"
            << collectionName << "'";
      }
    }
  }

  return res;
}

/// @brief dispatch restore data
Result RestoreFeature::RestoreMainJob::dispatchRestoreData(
    arangodb::httpclient::SimpleHttpClient& client,
    MultiFileReadOffset readOffset, char const* data, size_t length,
    bool forceDirect) {
  size_t readLength = length;

  // the following object is needed for cleaning up duplicate attributes.
  // this does not perform any allocations in case we don't store any
  // data into it (which is the normal case)
  arangodb::basics::StringBuffer cleaned;

  if (options.cleanupDuplicateAttributes) {
    auto res = cleaned.reserve(length);

    if (res != TRI_ERROR_NO_ERROR) {
      // out of memory
      THROW_ARANGO_EXCEPTION(res);
    }

    arangodb::velocypack::Builder result;
    arangodb::velocypack::Options opts =
        arangodb::velocypack::Options::Defaults;
    // do *not* check duplicate attributes here (because that would throw)
    opts.checkAttributeUniqueness = false;
    arangodb::velocypack::Builder builder(&opts);

    // instead, we need to manually check for duplicate attributes...
    char const* p = data;
    char const* e = p + length;

    while (p < e) {
      while (p < e && (*p == ' ' || *p == '\r' || *p == '\n' || *p == '\t')) {
        ++p;
      }

      // detect line ending
      size_t len;
      char const* nl = static_cast<char const*>(memchr(p, '\n', e - p));
      if (nl == nullptr) {
        len = e - p;
      } else {
        len = nl - p;
      }

      builder.clear();
      try {
        VPackParser parser(builder, builder.options);
        parser.parse(p, len);
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
    data = cleaned.c_str();
    length = cleaned.length();
  }

  {
    // insert the current readoffset
    std::lock_guard locker{sharedState->mutex};
    sharedState->readOffsets.emplace(readOffset, readLength);
  }

  if (length == 0) {
    return {};
  }

  // check if we have an idle worker to which we can dispatch the sending of the
  // data. there is a small possibility for a race here, if we find that not all
  // workers are busy and decide to dispatch a background send job, and another
  // thread that shortly after occupies the last available idle worker. this is
  // however not a problem, because workers don't depend on each other and thus
  // this will only lead to negligible overhead if it happens.
  if (forceDirect || feature.taskQueue().allWorkersBusy()) {
    // all other workers are busy. so we cannot dispatch sending to another
    // worker, but rather do it ourselves
    return sendRestoreData(client, readOffset, data, length, useVPack);
  }

  // not all workers busy.
  // now dispatch a background send job, which can be executed by any of the
  // idle threads
  auto buffer = feature.leaseBuffer();
  buffer->appendText(data, length);

  feature.taskQueue().queueJob(
      std::make_unique<arangodb::RestoreFeature::RestoreSendJob>(
          feature, progressTracker, options, stats, collectionName, sharedState,
          readOffset, std::move(buffer), useVPack));

  // we just scheduled an async job, and no result will be returned from here
  return {};
}

/// @brief Restore the data for a given collection
Result RestoreFeature::RestoreMainJob::restoreData(
    arangodb::httpclient::SimpleHttpClient& client, bool useVPack) {
  using arangodb::Logger;
  using arangodb::basics::StringBuffer;
  using arangodb::basics::StringUtils::formatSize;

  int type = arangodb::basics::VelocyPackHelper::getNumericValue<int>(
      parameters.get(std::vector<std::string_view>({"parameters", "type"})), 2);
  std::string const collectionType(type == 2 ? "document" : "edge");

  auto&& currentStatus = progressTracker.getStatus(collectionName);

  if (currentStatus.state >= arangodb::RestoreFeature::RESTORED) {
    LOG_TOPIC("9a814", INFO, Logger::RESTORE)
        << "# skipping restoring " << collectionType << " collection '"
        << collectionName << "', as it was restored previously";
    return {};
  }

  TRI_ASSERT(currentStatus.state == arangodb::RestoreFeature::CREATED ||
             currentStatus.state == arangodb::RestoreFeature::RESTORING);

  // import data. check if we have a datafile
  //  ... there are 6 possible names
  std::string escapedName =
      escapedCollectionName(collectionName, parameters.get("parameters"));
  std::string const nameHash =
      arangodb::rest::SslInterface::sslMD5(collectionName);

  auto datafile = directory.readableFile(absl::StrCat(
      escapedName, "_", nameHash, ".data.", ::getSuffix(useVPack)));
  if (!datafile || datafile->status().fail()) {
    datafile = directory.readableFile(absl::StrCat(
        escapedName, "_", nameHash, ".data.", ::getSuffix(useVPack), ".gz"));
  }
  if (!datafile || datafile->status().fail()) {
    datafile = directory.readableFile(
        absl::StrCat(escapedName, ".data.", ::getSuffix(useVPack), ".gz"));
  }
  if (!datafile || datafile->status().fail()) {
    datafile = directory.readableFile(absl::StrCat(
        escapedName, "_", nameHash, ".0.data.", ::getSuffix(useVPack), ".gz"));
  }
  if (!datafile || datafile->status().fail()) {
    datafile = directory.readableFile(absl::StrCat(
        escapedName, "_", nameHash, ".0.data.", ::getSuffix(useVPack)));
  }
  if (!datafile || datafile->status().fail()) {
    datafile = directory.readableFile(
        absl::StrCat(escapedName, ".data.", ::getSuffix(useVPack)));
  }
  if (!datafile || datafile->status().fail()) {
    {
      std::lock_guard locker{sharedState->mutex};
      sharedState->readCompleteInputfile = true;
    }

    updateProgress();
    return {};
  }

  TRI_ASSERT(datafile);

  // check if we are dealing with compressed file(s)
  bool const isCompressed = datafile->path().ends_with(".gz");
  // check if we are dealing with multiple files (created via `--split-file
  // true`)
  bool const isMultiFile =
      std::regex_match(datafile->path(), ::splitFilesRegex);

  int64_t fileSize = TRI_SizeFile(datafile->path().c_str());
  if (isMultiFile) {
    fileSize = 0;
    auto allFiles = arangodb::basics::FileUtils::listFiles(directory.path());
    std::string prefix = absl::StrCat(escapedName, "_", nameHash, ".");
    for (auto const& it : allFiles) {
      if (!it.starts_with(prefix) || !std::regex_match(it, ::splitFilesRegex)) {
        continue;
      }
      int64_t s = TRI_SizeFile(
          arangodb::basics::FileUtils::buildFilename(directory.path(), it)
              .c_str());
      if (s >= 0) {
        fileSize += s;
      }
    }
  }

  if (options.progress) {
    LOG_TOPIC("95913", INFO, Logger::RESTORE)
        << "# Loading data into " << collectionType << " collection '"
        << collectionName << "', data size: " << formatSize(fileSize)
        << (isCompressed ? " (compressed)" : "");
  }

  int64_t numReadForThisCollection = 0;
  int64_t numReadSinceLastReport = 0;

  bool const isGzip = isCompressed;

  std::string ofFilesize;
  if (!isGzip) {
    ofFilesize = " of " + formatSize(fileSize);
  }

  MultiFileReadOffset datafileReadOffset;
  if (currentStatus.state == arangodb::RestoreFeature::RESTORING) {
    LOG_TOPIC("94913", INFO, Logger::RESTORE)
        << "# continuing restoring " << collectionType << " collection '"
        << collectionName << "' from offset " << currentStatus.bytesAcked;
    datafileReadOffset = currentStatus.bytesAcked;

    // open the nth file
    if (datafileReadOffset.fileNo != 0) {
      datafile = directory.readableFile(absl::StrCat(
          escapedName, "_", nameHash, ".", datafileReadOffset.fileNo, ".data.",
          ::getSuffix(useVPack), (isCompressed ? ".gz" : "")));
      if (datafile->status().fail()) {
        return datafile->status();
      }
    }

    datafile->skip(datafileReadOffset.readOffset);
    if (datafile->status().fail()) {
      return datafile->status();
    }
  }

  // 1MB buffer by default
  size_t bufferSize = 1048576;
  if (bufferSize > options.chunkSize) {
    bufferSize = options.chunkSize;
  }
  // minimum buffer size is 64KB
  bufferSize = std::max<size_t>(bufferSize, 65536);

  arangodb::Result result;

  auto buffer = feature.leaseBuffer();

  while (true) {
    TRI_ASSERT(buffer != nullptr);

    if (buffer->reserve(bufferSize) != TRI_ERROR_NO_ERROR) {
      return {TRI_ERROR_OUT_OF_MEMORY, "out of memory"};
    }

    auto numRead = datafile->read(buffer->end(), bufferSize);
    if (datafile->status().fail()) {  // error while reading
      return datafile->status();
    }

    if (numRead > 0) {
      // we read something
      buffer->increaseLength(numRead);
      stats.totalRead += static_cast<uint64_t>(numRead);
      numReadForThisCollection += numRead;
      numReadSinceLastReport += numRead;

      if (buffer->length() < options.chunkSize) {
        continue;  // still continue reading
      }
    }

    // do we have a buffer?
    if (buffer->length() > 0) {
      size_t length;

      if (useVPack) {
        // look for valid velocypack
        velocypack::Validator validator;
        try {
          validator.validate(buffer->begin(), buffer->length(), true);
        } catch (std::exception const& ex) {
          // we do not have a complete velocypack array yet.
          if (numRead > 0) {
            continue;
          }
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_FAILED,
              absl::StrCat("error processing velocypack data from input file '",
                           datafile->path(), "': ", ex.what()));
        }
        VPackSlice data(reinterpret_cast<uint8_t const*>(buffer->begin()));
        TRI_ASSERT(data.isArray());
        length = data.byteSize();

        if (data.isEmptyArray()) {
          buffer->erase_front(length);
          continue;
        }
      } else {
        // JSONL case
        if (numRead == 0) {
          // we're at the end of the file, so send the complete buffer anyway
          length = buffer->length();
        } else {
          // look for the last \n in the buffer
          char const* found = static_cast<char const*>(
              memrchr(static_cast<void const*>(buffer->begin()), '\n',
                      buffer->length()));

          if (found == nullptr) {
            // no \n in buffer...
            // don't have a complete line yet, read more
            continue;
          }
          // found a \n somewhere; break at line
          length = found - buffer->begin();
        }
      }

      // if we are already at the end of the input file, there is no need
      // to start a background task for the restore. we can simply do it
      // in this same thread.
      // in addition, if we are using the enveloped format, we must restore
      // data in order. this is because the enveloped data format may contain
      // documents to insert *AND* documents to remove (this is an MMFiles
      // legacy).
      bool const forceDirect = (numRead == 0) || useEnvelope;

      result = dispatchRestoreData(client, datafileReadOffset, buffer->begin(),
                                   length, forceDirect);

      // check if our status was changed by background jobs
      if (result.ok()) {
        std::lock_guard locker{sharedState->mutex};

        if (sharedState->result.fail()) {
          // yes, something failed
          result = sharedState->result;
        }
      }

      if (result.fail()) {
        if (options.force) {
          // pretend nothing happened
          result.reset();
        } else {
          // some error occurred. now exit main loop
          break;
        }
      }

      datafileReadOffset.readOffset += length;

      // bytes successfully sent
      buffer->erase_front(length);

      if (options.progress && fileSize > 0 &&
          numReadSinceLastReport > 1024 * 1024 * 8) {
        // report every 8MB of transferred data
        //   currently do not have unzipped size for .gz files
        std::string percentage;
        if (!isGzip) {
          percentage = absl::StrCat(
              " (",
              int(100. * double(numReadForThisCollection) / double(fileSize)),
              " %)");
        }

        LOG_TOPIC("69a73", INFO, Logger::RESTORE)
            << "# Loading data into " << collectionType << " collection '"
            << collectionName << "', " << formatSize(numReadForThisCollection)
            << ofFilesize << " read" << percentage;
        numReadSinceLastReport = 0;
      }
    }

    if (numRead == 0 && buffer->length() == 0) {  // EOF
      if (!isMultiFile) {
        break;
      }
      datafileReadOffset.fileNo += 1;
      datafileReadOffset.readOffset = 0;
      datafile = directory.readableFile(absl::StrCat(
          escapedName, "_", nameHash, ".", datafileReadOffset.fileNo, ".data.",
          ::getSuffix(useVPack), (isCompressed ? ".gz" : "")));
      if (!datafile || datafile->status().fail()) {
        break;
      }
    }
  }

  feature.returnBuffer(std::move(buffer));

  // end of main job
  if (result.ok()) {
    while (true) {
      {
        std::lock_guard locker{sharedState->mutex};
        if (sharedState->pendingJobs == 0) {
          // no more pending jobs. we are done!
          sharedState->readCompleteInputfile = true;
          break;
        }
      }
      // we still have pending jobs, which are dispatched to other
      // threads but not yet finished. wait for all of them to be
      // fully processed
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    updateProgress();
  }

  return result;
}

/// @brief Restore a collection's indexes given its description
arangodb::Result RestoreFeature::RestoreMainJob::restoreIndexes(
    arangodb::httpclient::SimpleHttpClient& client) {
  using arangodb::Logger;

  arangodb::Result result;
  VPackSlice const indexes = parameters.get("indexes");
  // re-create indexes
  if (indexes.length() > 0) {
    // we actually have indexes

    if (options.progress) {
      LOG_TOPIC("d88c6", INFO, Logger::RESTORE)
          << "# Creating indexes for collection '" << collectionName << "'...";
    }

    result = sendRestoreIndexes(client, parameters);

    if (result.fail()) {
      LOG_TOPIC("db937", WARN, Logger::RESTORE)
          << "Error while creating indexes for collection '" << collectionName
          << "': " << result.errorMessage();

      if (options.force) {
        result.reset();
      }
    }
  }

  // cppcheck-suppress uninitvar ; false positive
  return result;
}

/// @brief Send command to restore a collection's indexes
Result RestoreFeature::RestoreMainJob::sendRestoreIndexes(
    arangodb::httpclient::SimpleHttpClient& client,
    arangodb::velocypack::Slice slice) {
  std::string url = absl::StrCat("/_api/replication/restore-indexes?force=",
                                 (options.force ? "true" : "false"));
  std::string body = slice.toJson();

  std::unique_ptr<arangodb::httpclient::SimpleHttpResult> response(
      client.request(arangodb::rest::RequestType::PUT, url, body.c_str(),
                     body.size()));
  return arangodb::HttpResponseChecker::check(
      client.getErrorMessage(), response.get(), "restoring indexes", body,
      arangodb::HttpResponseChecker::PayloadType::JSON);
}

RestoreFeature::RestoreSendJob::RestoreSendJob(
    RestoreFeature& feature, RestoreProgressTracker& progressTracker,
    RestoreFeature::Options const& options, RestoreFeature::Stats& stats,
    std::string const& collectionName, std::shared_ptr<SharedState> sharedState,
    MultiFileReadOffset readOffset,
    std::unique_ptr<basics::StringBuffer> buffer, bool useVPack)
    : RestoreJob(feature, progressTracker, options, stats, collectionName,
                 sharedState),
      readOffset(readOffset),
      buffer(std::move(buffer)),
      useVPack(useVPack) {
  // count number of pending jobs up
  std::lock_guard locker{sharedState->mutex};
  ++sharedState->pendingJobs;
}

RestoreFeature::RestoreSendJob::~RestoreSendJob() {
  // count number of pending jobs back at the end
  std::lock_guard locker{sharedState->mutex};
  TRI_ASSERT(sharedState->pendingJobs > 0);
  --sharedState->pendingJobs;
}

Result RestoreFeature::RestoreSendJob::run(
    arangodb::httpclient::SimpleHttpClient& client) {
  TRI_ASSERT(buffer != nullptr);
  Result res = sendRestoreData(client, readOffset, buffer->data(),
                               buffer->size(), useVPack);

  feature.returnBuffer(std::move(buffer));
  return res;
}

RestoreFeature::RestoreFeature(Server& server, int& exitCode)
    : ArangoRestoreFeature{server, *this},
      _clientManager{server.getFeature<HttpEndpointProvider, ClientFeature>(),
                     Logger::RESTORE},
      _clientTaskQueue{server, ::processJob},
      _exitCode{exitCode} {
  static_assert(Server::isCreatedAfter<RestoreFeature, HttpEndpointProvider>());

  setOptional(false);
  startsAfter<application_features::BasicFeaturePhaseClient>();
  if constexpr (Server::contains<BumpFileDescriptorsFeature>()) {
    startsAfter<BumpFileDescriptorsFeature>();
  }

  using arangodb::basics::FileUtils::buildFilename;
  using arangodb::basics::FileUtils::currentDirectory;
  _options.inputPath = buildFilename(currentDirectory().result(), "dump");
  _options.threadCount =
      std::max(uint32_t(_options.threadCount),
               static_cast<uint32_t>(NumberOfCores::getValue()));
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
      "Restrict the restore to this collection name (can be specified multiple "
      "times).",
      new VectorParameter<StringParameter>(&_options.collections));

  options->addOption("--view",
                     "Restrict the restore to this view name (can be specified "
                     "multiple times).",
                     new VectorParameter<StringParameter>(&_options.views));

  options->addObsoleteOption(
      "--recycle-ids", "collection ids are now handled automatically", false);

  options->addOption("--batch-size",
                     "The maximum size for individual data batches (in bytes).",
                     new UInt64Parameter(&_options.chunkSize));

  options->addOption(
      "--threads", "The maximum number of collections to process in parallel.",
      new UInt32Parameter(&_options.threadCount),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Dynamic));

  options
      ->addOption("--initial-connect-retries",
                  "The number of connect retries for the initial connection.",
                  new UInt32Parameter(&_options.initialConnectRetries))
      .setIntroducedIn(30713)
      .setIntroducedIn(30801);

  options->addOption("--include-system-collections",
                     "Include system collections.",
                     new BooleanParameter(&_options.includeSystemCollections));

  options->addOption("--create-database",
                     "Create the target database if it does not exist.",
                     new BooleanParameter(&_options.createDatabase));

  options
      ->addOption("--max-unused-buffers-capacity",
                  "Maximum cumulated size of spare in-memory buffers to keep.",
                  new UInt64Parameter(&_options.maxUnusedBufferSize),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(31200)
      .setLongDescription(
          R"(Maximum cumulated size of in-memory buffers to keep around for 
sending batches.
A value > 0 will increase the memory usage of arangorestore, but can help in 
avoiding repeated memory allocations for building new in-memory buffers.)");

  options->addOption(
      "--force-same-database",
      "Force the same database name as in the source `dump.json` file.",
      new BooleanParameter(&_options.forceSameDatabase));

  options->addOption("--all-databases", "Restore the data of all databases.",
                     new BooleanParameter(&_options.allDatabases));

  options->addOption("--input-directory", "The input directory.",
                     new StringParameter(&_options.inputPath));

  options->addOption(
      "--cleanup-duplicate-attributes",
      "Clean up duplicate attributes (use first specified value) in input "
      "documents instead of making the restore operation fail.",
      new BooleanParameter(&_options.cleanupDuplicateAttributes),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options->addOption("--import-data", "Import data into collection.",
                     new BooleanParameter(&_options.importData));

  options->addOption("--create-collection", "Create collection structure.",
                     new BooleanParameter(&_options.importStructure));

  options->addOption("--progress", "Show the progress.",
                     new BooleanParameter(&_options.progress));

  options->addOption("--overwrite", "Overwrite collections if they exist.",
                     new BooleanParameter(&_options.overwrite));

  options->addOption("--continue", "Continue the restore operation.",
                     new BooleanParameter(&_options.continueRestore));

  options->addObsoleteOption(
      "--envelope",
      "wrap each document into a {type, data} envelope "
      "(this is required for compatibility with v3.7 and before).",
      false);

  options
      ->addOption("--enable-revision-trees",
                  "Enable revision trees for new collections if the collection "
                  "attributes `syncByRevision` and "
                  "`usesRevisionsAsDocumentIds` are missing.",
                  new BooleanParameter(&_options.enableRevisionTrees))
      .setIntroducedIn(30807);

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  options->addOption(
      "--fail-after-update-continue-file", "",
      new BooleanParameter(&_options.failOnUpdateContinueFile),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));
#endif

  options->addOption(
      "--number-of-shards",
      "Override the `numberOfShards` value (can be specified multiple "
      "times, e.g. --number-of-shards 2 --number-of-shards "
      "myCollection=3).",
      new VectorParameter<StringParameter>(&_options.numberOfShards));

  options->addOption(
      "--replication-factor",
      "Override the `replicationFactor` value (can be specified "
      "multiple times, e.g. --replication-factor 2 "
      "--replication-factor myCollection=3).",
      new VectorParameter<StringParameter>(&_options.replicationFactor));

  options
      ->addOption("--write-concern",
                  "Override the `writeConcern` value (can be specified "
                  "multiple times, e.g. --write-concern 2 "
                  "--write-concern myCollection=3).",
                  new VectorParameter<StringParameter>(&_options.writeConcern))
      .setIntroducedIn(31200);

  options->addOption(
      "--ignore-distribute-shards-like-errors",
      "Continue the restore even if the sharding prototype collection is "
      "missing.",
      new BooleanParameter(&_options.ignoreDistributeShardsLikeErrors));

  options->addOption(
      "--force",
      "Continue the restore even in the face of some server-side errors.",
      new BooleanParameter(&_options.force));

  // deprecated options
  options
      ->addOption(
          "--default-number-of-shards",
          "The default `numberOfShards` value if not specified in the dump.",
          new UInt64Parameter(&_options.defaultNumberOfShards),
          arangodb::options::makeDefaultFlags(
              arangodb::options::Flags::Uncommon))
      .setDeprecatedIn(30322)
      .setDeprecatedIn(30402);

  options
      ->addOption(
          "--default-replication-factor",
          "The default `replicationFactor` value if not specified in the dump.",
          new UInt64Parameter(&_options.defaultReplicationFactor),
          arangodb::options::makeDefaultFlags(
              arangodb::options::Flags::Uncommon))
      .setDeprecatedIn(30322)
      .setDeprecatedIn(30402);
}

void RestoreFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> options) {
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
          << "cannot use --server.database and --all-databases at the same "
             "time";
      FATAL_ERROR_EXIT();
    }

    if (_options.forceSameDatabase) {
      LOG_TOPIC("fd66a", FATAL, arangodb::Logger::RESTORE)
          << "cannot use --force-same-database and --all-databases at the same "
             "time";
      FATAL_ERROR_EXIT();
    }
  }

  // use a minimum value for batches
  if (_options.chunkSize < 1024 * 128) {
    _options.chunkSize = 1024 * 128;
  }

  auto clamped = std::clamp(_options.threadCount, uint32_t(1),
                            uint32_t(4 * NumberOfCores::getValue()));
  if (_options.threadCount != clamped) {
    LOG_TOPIC("53570", WARN, Logger::RESTORE)
        << "capping --threads value to " << clamped;
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
  logLGPLNotice();
  if (!_options.inputPath.empty() &&
      _options.inputPath.back() == TRI_DIR_SEPARATOR_CHAR) {
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
  using arangodb::basics::StringUtils::formatSize;
  using arangodb::httpclient::SimpleHttpClient;

  double const start = TRI_microtime();

  EncryptionFeature* encryption{};
  if constexpr (Server::contains<EncryptionFeature>()) {
    if (server().hasFeature<EncryptionFeature>()) {
      encryption = &server().getFeature<EncryptionFeature>();
    }
  }

  // set up the output directory, not much else
  _directory = std::make_unique<ManagedDirectory>(
      encryption, _options.inputPath, false, false, true);

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

  ClientFeature& client =
      server().getFeature<HttpEndpointProvider, ClientFeature>();

  _exitCode = EXIT_SUCCESS;

  // enumerate all databases present in the dump directory (in case of
  // --all-databases=true, or use just the flat files in case of
  // --all-databases=false)
  std::vector<DatabaseInfo> databases =
      determineDatabaseList(client.databaseName());

  std::unique_ptr<SimpleHttpClient> httpClient;

  auto const connectRetry = [&](size_t numRetries) -> Result {
    for (size_t i = 0; i < numRetries; i++) {
      if (i > 0) {
        LOG_TOPIC("5855a", WARN, Logger::RESTORE)
            << "Failed to connect to server, retrying...";
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(i * 1s);
      }
      Result result = _clientManager.getConnectedClient(
          httpClient, _options.force, true, !_options.createDatabase, false, 0);
      if (!result.is(TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT) &&
          !result.is(TRI_ERROR_INTERNAL)) {
        return result;
      }
    }
    return {TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT};
  };

  // final result
  Result result =
      connectRetry(std::max<uint32_t>(1, _options.initialConnectRetries));
  if (result.is(TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT)) {
    LOG_TOPIC("c23bf", FATAL, Logger::RESTORE)
        << "cannot create server connection, giving up!";
    FATAL_ERROR_EXIT();
  }
  if (result.is(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND)) {
    std::string dbName = client.databaseName();
    if (_options.createDatabase) {
      // database not found, but database creation requested
      LOG_TOPIC("9b5a6", INFO, Logger::RESTORE)
          << "Creating database '" << dbName << "'";

      VPackBuilder properties;
      getDBProperties(*_directory, properties);
      Result res =
          ::tryCreateDatabase(server(), dbName, properties.slice(), _options);
      if (res.fail()) {
        LOG_TOPIC("b19db", FATAL, Logger::RESTORE)
            << "Could not create database '" << dbName
            << "': " << res.errorMessage();
        FATAL_ERROR_EXIT();
      }

      // restore old database name
      client.setDatabaseName(dbName);

      // re-check connection and version
      result = _clientManager.getConnectedClient(httpClient, _options.force,
                                                 true, true, false, 0);
    } else {
      LOG_TOPIC("ad95b", WARN, Logger::RESTORE)
          << "Database '" << dbName
          << "' does not exist on target endpoint. In order to create this "
             "database along with the restore, please use the "
             "--create-database option";
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
        << "Error: could not detect ArangoDB instance type: "
        << result.errorMessage();
    _exitCode = EXIT_FAILURE;
    return;
  }

  if (role == "DBSERVER" || role == "PRIMARY") {
    LOG_TOPIC("1fc99", WARN, arangodb::Logger::RESTORE)
        << "You connected to a DBServer node, but operations in a cluster "
           "should be carried out via a Coordinator. This is an unsupported "
           "operation!";
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
        << "Connected to ArangoDB '" << httpClient->getEndpointSpecification()
        << "'";
  }

  if (!isRocksDB) {
    LOG_TOPIC("ae10c", WARN, arangodb::Logger::RESTORE)
        << "You connected to a server with a potentially incompatible storage "
           "engine.";
  }

  // set up threads and workers
  _clientTaskQueue.spawnWorkers(_clientManager, _options.threadCount);

  LOG_TOPIC("6bb3c", DEBUG, Logger::RESTORE)
      << "Using " << _options.threadCount << " worker thread(s)";

  if (_options.allDatabases) {
    std::vector<std::string> dbs;
    std::transform(databases.begin(), databases.end(), std::back_inserter(dbs),
                   [](auto const& dbInfo) { return dbInfo.name; });
    LOG_TOPIC("7c10a", INFO, Logger::RESTORE)
        << "About to restore databases '"
        << basics::StringUtils::join(dbs, "', '") << "' from dump directory '"
        << _options.inputPath << "'...";
  }

  std::vector<std::string> filesToClean;

  for (auto& db : databases) {
    result.reset();

    if (_options.allDatabases) {
      // inject current database
      client.setDatabaseName(db.name);
      LOG_TOPIC("36075", INFO, Logger::RESTORE)
          << "Restoring database '" << db.name << "'";

      EncryptionFeature* encryption{};
      if constexpr (Server::contains<EncryptionFeature>()) {
        if (server().hasFeature<EncryptionFeature>()) {
          encryption = &server().getFeature<EncryptionFeature>();
        }
      }

      _directory = std::make_unique<ManagedDirectory>(
          encryption,
          basics::FileUtils::buildFilename(_options.inputPath, db.directory),
          false, false, true);

      result =
          _clientManager.getConnectedClient(httpClient, _options.force, false,
                                            !_options.createDatabase, false, 0);

      if (result.is(TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT)) {
        LOG_TOPIC("3e715", FATAL, Logger::RESTORE)
            << "cannot create server connection, giving up!";
        FATAL_ERROR_EXIT();
      }

      if (result.is(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND)) {
        if (_options.createDatabase) {
          // database not found, but database creation requested
          LOG_TOPIC("080f3", INFO, Logger::RESTORE)
              << "Creating database '" << db.name << "'";

          result = ::tryCreateDatabase(server(), db.name, db.properties.slice(),
                                       _options);
          if (result.fail()) {
            LOG_TOPIC("7a35f", ERR, Logger::RESTORE)
                << "Could not create database '" << db.name
                << "': " << result.errorMessage();
            break;
          }

          // restore old database name

          client.setDatabaseName(db.name);

          // re-check connection and version
          result = _clientManager.getConnectedClient(httpClient, _options.force,
                                                     false, true, false, 0);
        } else {
          LOG_TOPIC("be594", WARN, Logger::RESTORE)
              << "Database '" << db.name
              << "' does not exist on target endpoint. In order to create this "
                 "database along with the restore, please use the "
                 "--create-database option";
        }
      }

      if (result.fail()) {
        result.reset(result.errorNumber(),
                     absl::StrCat("cannot create server connection: ",
                                  result.errorMessage()));

        if (!_options.force) {
          break;
        }

        LOG_TOPIC("be86d", ERR, arangodb::Logger::RESTORE)
            << result.errorMessage();
        // continue with next db
        continue;
      }
    }

    // read encryption info
    ::checkEncryption(*_directory);

    // read dump info
    bool useEnvelope = false;
    bool useVPack = false;  // for compatibility
    result =
        ::checkDumpDatabase(server(), *_directory, _options.forceSameDatabase,
                            useEnvelope, useVPack);
    if (result.fail()) {
      LOG_TOPIC("0cbdf", FATAL, arangodb::Logger::RESTORE)
          << result.errorMessage();
      FATAL_ERROR_EXIT();
    }

    LOG_TOPIC_IF("52b23", INFO, arangodb::Logger::RESTORE,
                 _options.continueRestore)
        << "trying to continue previous restore";
    _progressTracker = std::make_unique<RestoreProgressTracker>(
        *_directory, !_options.continueRestore);

    filesToClean.push_back(_progressTracker->filename());

    // run the actual restore
    try {
      result = ::processInputDirectory(*httpClient, _clientTaskQueue, *this,
                                       _options, *_directory, *_progressTracker,
                                       _stats, useEnvelope, useVPack);
    } catch (basics::Exception const& ex) {
      LOG_TOPIC("52b22", ERR, arangodb::Logger::RESTORE)
          << "caught exception: " << ex.what();
      result = {ex.code(), ex.what()};
    } catch (std::exception const& ex) {
      LOG_TOPIC("8f13f", ERR, arangodb::Logger::RESTORE)
          << "caught exception: " << ex.what();
      result = {TRI_ERROR_INTERNAL, ex.what()};
    } catch (...) {
      LOG_TOPIC("a74e8", ERR, arangodb::Logger::RESTORE)
          << "caught unknown exception";
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
          << "Processed " << _stats.restoredCollections
          << " collection(s) from " << databases.size() << " database(s) in "
          << Logger::FIXED(totalTime, 2) << " s total time. Read "
          << formatSize(_stats.totalRead)
          << " from datafiles (after decompression), "
          << "sent " << _stats.totalBatches << " data batch(es) of "
          << formatSize(_stats.totalSent) << " total size.";
    } else if (_options.importStructure) {
      LOG_TOPIC("147ca", INFO, Logger::RESTORE)
          << "Processed " << _stats.restoredCollections
          << " collection(s) from " << databases.size() << " database(s) in "
          << Logger::FIXED(totalTime, 2) << " s total time.";
    }
  }
}

ClientTaskQueue<RestoreFeature::RestoreJob>& RestoreFeature::taskQueue() {
  return _clientTaskQueue;
}

void RestoreFeature::reportError(Result const& error) {
  try {
    {
      std::lock_guard lock{_workerErrorLock};
      _workerErrors.emplace_back(error);
    }
    _clientTaskQueue.clearQueue();
  } catch (...) {
  }
}

Result RestoreFeature::getFirstError() const {
  {
    std::lock_guard lock{_workerErrorLock};
    if (!_workerErrors.empty()) {
      return _workerErrors.front();
    }
  }
  return {};
}

std::unique_ptr<basics::StringBuffer> RestoreFeature::leaseBuffer() {
  std::lock_guard lock{_buffersLock};

  if (_buffers.empty()) {
    // no buffers present. now insert one
    _buffers.emplace_back(std::make_unique<basics::StringBuffer>(false));
    _buffersCapacity += _buffers.back()->capacity();
  }

  // pop last buffer from vector of buffers
  TRI_ASSERT(!_buffers.empty());
  std::unique_ptr<basics::StringBuffer> buffer = std::move(_buffers.back());
  _buffers.pop_back();

  _buffersCapacity -= buffer->capacity();

  TRI_ASSERT(buffer != nullptr);
  TRI_ASSERT(buffer->length() == 0);
  return buffer;
}

void RestoreFeature::returnBuffer(
    std::unique_ptr<basics::StringBuffer> buffer) noexcept {
  TRI_ASSERT(buffer != nullptr);
  buffer->clear();

  std::lock_guard lock{_buffersLock};

  if (_buffersCapacity + buffer->capacity() >= _options.maxUnusedBufferSize) {
    // do not waste a lot of memory by keeping a lot of empty buffers
    // around
    return;
  }
  try {
    _buffers.emplace_back(std::move(buffer));
    _buffersCapacity += _buffers.back()->capacity();
  } catch (...) {
    // if this throws, then the unique_ptr will simply go out of scope here and
    // delete the StringBUffer. no leaks in this case, and no need to rethrow
    // the exception
  }
}

RestoreFeature::CollectionStatus::CollectionStatus(VPackSlice slice) {
  using arangodb::basics::VelocyPackHelper;
  state = VelocyPackHelper::getNumericValue<CollectionState>(
      slice, "state", CollectionState::UNKNOWN);
  bytesAcked.fileNo =
      VelocyPackHelper::getNumericValue<size_t>(slice, "file-no", 0);
  bytesAcked.readOffset =
      VelocyPackHelper::getNumericValue<size_t>(slice, "bytes-acked", 0);
}

void RestoreFeature::CollectionStatus::toVelocyPack(
    VPackBuilder& builder) const {
  {
    VPackObjectBuilder object(&builder);
    builder.add("state", VPackValue(state));
    if (bytesAcked != MultiFileReadOffset{}) {
      builder.add("bytes-acked", VPackValue(bytesAcked.readOffset));
      builder.add("file-no", VPackValue(bytesAcked.fileNo));
    }
  }
}

RestoreFeature::CollectionStatus::CollectionStatus(
    RestoreFeature::CollectionState state, MultiFileReadOffset bytesAcked)
    : state(state), bytesAcked(bytesAcked) {}

RestoreFeature::CollectionStatus::CollectionStatus() = default;

}  // namespace arangodb
