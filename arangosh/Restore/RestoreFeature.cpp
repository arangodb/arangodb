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
////////////////////////////////////////////////////////////////////////////////

#include "RestoreFeature.h"

#include <velocypack/Options.h>
#include <velocypack/velocypack-aliases.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/OpenFilesTracker.h"
#include "Basics/Result.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/files.h"
#include "Basics/terminal-utils.h"
#include "Basics/tri-strings.h"
#include "Endpoint/Endpoint.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Rest/HttpResponse.h"
#include "Rest/InitializeRest.h"
#include "Rest/Version.h"
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
}  // namespace

namespace {
/// @brief check whether HTTP response is valid, complete, and not an error
arangodb::Result checkHttpResponse(
    arangodb::httpclient::SimpleHttpClient& client,
    std::unique_ptr<arangodb::httpclient::SimpleHttpResult>& response) {
  using arangodb::basics::StringUtils::itoa;
  if (response == nullptr || !response->isComplete()) {
    return {TRI_ERROR_INTERNAL,
            "got invalid response from server: " + client.getErrorMessage()};
  }
  if (response->wasHttpError()) {
    return {TRI_ERROR_INTERNAL, "got invalid response from server: HTTP " +
                                    itoa(response->getHttpReturnCode()) + ": " +
                                    response->getHttpReturnMessage()};
  }
  return {TRI_ERROR_NO_ERROR};
}
}  // namespace

namespace {
/// @brief process a single job from the queue
arangodb::Result processJob(arangodb::httpclient::SimpleHttpClient& client,
                            arangodb::RestoreFeature::JobData& jobData) {
  arangodb::Result result{TRI_ERROR_NO_ERROR};

  return result;
}
}  // namespace

namespace {
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

RestoreFeature::RestoreFeature(application_features::ApplicationServer* server,
                               int& exitCode)
    : ApplicationFeature(server, RestoreFeature::featureName()),
      _exitCode(exitCode),
      _clientManager{Logger::RESTORE},
      _clientTaskQueue{::processJob, ::handleJobResult},
      _collections(),
      _chunkSize(1024 * 1024 * 8),
      _includeSystemCollections(false),
      _createDatabase(false),
      _inputPath(),
      _directory(nullptr),
      _forceSameDatabase(false),
      _importData(true),
      _importStructure(true),
      _progress(true),
      _overwrite(true),
      _force(false),
      _ignoreDistributeShardsLikeErrors(false),
      _clusterMode(false),
      _defaultNumberOfShards(1),
      _defaultReplicationFactor(1),
      _stats{0, 0, 0},
      _workerErrorLock{},
      _workerErrors{} {
  requiresElevatedPrivileges(false);
  setOptional(false);
  startsAfter("Client");
  startsAfter("Logger");

#ifdef USE_ENTERPRISE
  startsAfter("Encryption");
#endif

  using arangodb::basics::FileUtils::buildFilename;
  using arangodb::basics::FileUtils::currentDirectory;
  _inputPath = buildFilename(currentDirectory().result(), "dump");
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
      new VectorParameter<StringParameter>(&_collections));

  options->addObsoleteOption(
      "--recycle-ids", "collection ids are now handled automatically", false);

  options->addOption("--batch-size",
                     "maximum size for individual data batches (in bytes)",
                     new UInt64Parameter(&_chunkSize));

  options->addOption("--include-system-collections",
                     "include system collections",
                     new BooleanParameter(&_includeSystemCollections));

  options->addOption("--create-database",
                     "create the target database if it does not exist",
                     new BooleanParameter(&_createDatabase));

  options->addOption(
      "--force-same-database",
      "force usage of the same database name as in the source dump.json file",
      new BooleanParameter(&_forceSameDatabase));

  options->addOption("--input-directory", "input directory",
                     new StringParameter(&_inputPath));

  options->addOption("--import-data", "import data into collection",
                     new BooleanParameter(&_importData));

  options->addOption("--create-collection", "create collection structure",
                     new BooleanParameter(&_importStructure));

  options->addOption("--progress", "show progress",
                     new BooleanParameter(&_progress));

  options->addOption("--overwrite", "overwrite collections if they exist",
                     new BooleanParameter(&_overwrite));

  options->addOption("--default-number-of-shards",
                     "default value for numberOfShards if not specified",
                     new UInt64Parameter(&_defaultNumberOfShards));

  options->addOption("--default-replication-factor",
                     "default value for replicationFactor if not specified",
                     new UInt64Parameter(&_defaultReplicationFactor));

  options->addOption(
      "--ignore-distribute-shards-like-errors",
      "continue restore even if sharding prototype collection is missing",
      new BooleanParameter(&_ignoreDistributeShardsLikeErrors));

  options->addOption(
      "--force", "continue restore even in the face of some server-side errors",
      new BooleanParameter(&_force));
}

void RestoreFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  using arangodb::basics::StringUtils::join;

  auto const& positionals = options->processingResult()._positionals;
  size_t n = positionals.size();

  if (1 == n) {
    _inputPath = positionals[0];
  } else if (1 < n) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "expecting at most one directory, got " + join(positionals, ", ");
    FATAL_ERROR_EXIT();
  }

  // use a minimum value for batches
  if (_chunkSize < 1024 * 128) {
    _chunkSize = 1024 * 128;
  }
}

void RestoreFeature::prepare() {
  if (!_inputPath.empty() && _inputPath.back() == TRI_DIR_SEPARATOR_CHAR) {
    // trim trailing slash from path because it may cause problems on ...
    // Windows
    TRI_ASSERT(_inputPath.size() > 0);
    _inputPath.pop_back();
  }

  if (!_importStructure && !_importData) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "Error: must specify either --create-collection or --import-data";
    FATAL_ERROR_EXIT();
  }
}

int RestoreFeature::tryCreateDatabase(std::string const& name) {
  using arangodb::httpclient::SimpleHttpResult;

  // get client feature for configuration info
  auto client =
      application_features::ApplicationServer::getFeature<ClientFeature>(
          "Client");
  TRI_ASSERT(nullptr != client);
  // get a client to use in main thread
  auto httpClient = _clientManager.getConnectedClient(_force, true);

  VPackBuilder builder;
  builder.openObject();
  builder.add("name", VPackValue(name));
  builder.add("users", VPackValue(VPackValueType::Array));
  builder.openObject();
  builder.add("username", VPackValue(client->username()));
  builder.add("passwd", VPackValue(client->password()));
  builder.close();
  builder.close();
  builder.close();

  std::string const body = builder.slice().toJson();

  std::unique_ptr<SimpleHttpResult> response(httpClient->request(
      rest::RequestType::POST, "/_api/database", body.c_str(), body.size()));

  if (response == nullptr || !response->isComplete()) {
    return TRI_ERROR_INTERNAL;
  }

  auto returnCode = response->getHttpReturnCode();

  if (returnCode == static_cast<int>(rest::ResponseCode::OK) ||
      returnCode == static_cast<int>(rest::ResponseCode::CREATED)) {
    // all ok
    return TRI_ERROR_NO_ERROR;
  }
  if (returnCode == static_cast<int>(rest::ResponseCode::UNAUTHORIZED) ||
      returnCode == static_cast<int>(rest::ResponseCode::FORBIDDEN)) {
    // invalid authorization
    auto res = ::checkHttpResponse(*httpClient, response);
    httpClient->setErrorMessage(res.errorMessage(), false);
    return TRI_ERROR_FORBIDDEN;
  }

  // any other error
  auto res = ::checkHttpResponse(*httpClient, response);
  httpClient->setErrorMessage(res.errorMessage(), false);
  return TRI_ERROR_INTERNAL;
}

int RestoreFeature::sendRestoreCollection(VPackSlice const& slice,
                                          std::string const& name,
                                          std::string& errorMsg) {
  using arangodb::httpclient::SimpleHttpResult;

  std::string url =
      "/_api/replication/restore-collection"
      "?overwrite=" +
      std::string(_overwrite ? "true" : "false") +
      "&force=" + std::string(_force ? "true" : "false") +
      "&ignoreDistributeShardsLikeErrors=" +
      std::string(_ignoreDistributeShardsLikeErrors ? "true" : "false");

  if (_clusterMode) {
    if (!slice.hasKey(std::vector<std::string>({"parameters", "shards"})) &&
        !slice.hasKey(
            std::vector<std::string>({"parameters", "numberOfShards"}))) {
      // no "shards" and no "numberOfShards" attribute present. now assume
      // default value from --default-number-of-shards
      LOG_TOPIC(WARN, Logger::RESTORE)
          << "# no sharding information specified for collection '" << name
          << "', using default number of shards " << _defaultNumberOfShards;
      url += "&numberOfShards=" + std::to_string(_defaultNumberOfShards);
    }
    if (!slice.hasKey(
            std::vector<std::string>({"parameters", "replicationFactor"}))) {
      // No replication factor given, so take the default:
      LOG_TOPIC(INFO, Logger::RESTORE)
          << "# no replication information specified for collection '" << name
          << "', using default replication factor "
          << _defaultReplicationFactor;
      url += "&replicationFactor=" + std::to_string(_defaultReplicationFactor);
    }
  }

  std::string const body = slice.toJson();

  auto httpClient = _clientManager.getConnectedClient(_force, true);
  std::unique_ptr<SimpleHttpResult> response(httpClient->request(
      rest::RequestType::PUT, url, body.c_str(), body.size()));

  if (response == nullptr || !response->isComplete()) {
    errorMsg =
        "got invalid response from server: " + httpClient->getErrorMessage();

    return TRI_ERROR_INTERNAL;
  }

  if (response->wasHttpError()) {
    auto res = ::checkHttpResponse(*httpClient, response);
    errorMsg = res.errorMessage();
    if (res.fail()) {
      return res.errorNumber();
    }

    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

int RestoreFeature::sendRestoreIndexes(VPackSlice const& slice,
                                       std::string& errorMsg) {
  using arangodb::httpclient::SimpleHttpResult;

  std::string const url = "/_api/replication/restore-indexes?force=" +
                          std::string(_force ? "true" : "false");
  std::string const body = slice.toJson();

  auto httpClient = _clientManager.getConnectedClient(_force, true);
  std::unique_ptr<SimpleHttpResult> response(httpClient->request(
      rest::RequestType::PUT, url, body.c_str(), body.size()));

  if (response == nullptr || !response->isComplete()) {
    errorMsg =
        "got invalid response from server: " + httpClient->getErrorMessage();

    return TRI_ERROR_INTERNAL;
  }

  if (response->wasHttpError()) {
    auto res = ::checkHttpResponse(*httpClient, response);
    errorMsg = res.errorMessage();
    if (res.fail()) {
      return res.errorNumber();
    }

    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

int RestoreFeature::sendRestoreData(std::string const& cname,
                                    char const* buffer, size_t bufferSize,
                                    std::string& errorMsg) {
  using arangodb::basics::StringUtils::urlEncode;
  using arangodb::httpclient::SimpleHttpResult;

  std::string const url =
      "/_api/replication/restore-data?collection=" + urlEncode(cname) +
      "&force=" + (_force ? "true" : "false");

  auto httpClient = _clientManager.getConnectedClient(_force, true);
  std::unique_ptr<SimpleHttpResult> response(
      httpClient->request(rest::RequestType::PUT, url, buffer, bufferSize));

  if (response == nullptr || !response->isComplete()) {
    errorMsg =
        "got invalid response from server: " + httpClient->getErrorMessage();

    return TRI_ERROR_INTERNAL;
  }

  if (response->wasHttpError()) {
    auto res = ::checkHttpResponse(*httpClient, response);
    errorMsg = res.errorMessage();
    if (res.fail()) {
      return res.errorNumber();
    }

    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

static bool SortCollections(VPackBuilder const& l, VPackBuilder const& r) {
  VPackSlice const left = l.slice().get("parameters");
  VPackSlice const right = r.slice().get("parameters");

  // First we sort by distribution.
  // We first have to create collections defining the distribution.
  VPackSlice leftDist = left.get("distributeShardsLike");
  VPackSlice rightDist = right.get("distributeShardsLike");

  if (leftDist.isNone() && !rightDist.isNone()) {
    return true;
  }

  if (rightDist.isNone() && !leftDist.isNone()) {
    return false;
  }

  int leftType =
      arangodb::basics::VelocyPackHelper::getNumericValue<int>(left, "type", 0);
  int rightType = arangodb::basics::VelocyPackHelper::getNumericValue<int>(
      right, "type", 0);

  if (leftType != rightType) {
    return leftType < rightType;
  }

  std::string leftName =
      arangodb::basics::VelocyPackHelper::getStringValue(left, "name", "");
  std::string rightName =
      arangodb::basics::VelocyPackHelper::getStringValue(right, "name", "");

  return strcasecmp(leftName.c_str(), rightName.c_str()) < 0;
}

Result RestoreFeature::checkEncryption() {
  if (_directory->isEncrypted()) {
#ifdef USE_ENTERPRISE
    if (!_directory->encryptionFeature()->keyOptionSpecified()) {
      LOG_TOPIC(WARN, Logger::RESTORE)
          << "the dump data seems to be encrypted with "
          << _directory->encryptionType()
          << ", but no key information was specified to decrypt the dump";
      LOG_TOPIC(WARN, Logger::RESTORE)
          << "it is recommended to specify either "
             "`--encryption.key-file` or `--encryption.key-generator` "
             "when invoking arangorestore with an encrypted dump";
    } else {
      LOG_TOPIC(INFO, Logger::RESTORE)
          << "# using encryption type " << _directory->encryptionType()
          << " for reading dump";
    }
#endif
  }

  return Result();
}

Result RestoreFeature::readDumpInfo() {
  std::string databaseName;

  try {
    VPackBuilder fileContentBuilder =
        _directory->vpackFromJsonFile("dump.json");
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
      application_features::ApplicationServer::getFeature<ClientFeature>(
          "Client");
  if (_forceSameDatabase && databaseName != client->databaseName()) {
    return Result(TRI_ERROR_BAD_PARAMETER,
                  std::string("database name in dump.json ('") + databaseName +
                      "') does not match specified database name ('" +
                      client->databaseName() + "')");
  }

  return Result();
}

int RestoreFeature::processInputDirectory(std::string& errorMsg) {
  using arangodb::basics::FileUtils::listFiles;
  using arangodb::basics::StringBuffer;

  // create a lookup table for collections
  std::map<std::string, bool> restrictList;
  for (size_t i = 0; i < _collections.size(); ++i) {
    restrictList.insert(std::pair<std::string, bool>(_collections[i], true));
  }
  try {
    std::vector<std::string> const files = listFiles(_inputPath);
    std::string const suffix = std::string(".structure.json");
    std::vector<VPackBuilder> collections;

    // Step 1 determine all collections to process
    {
      // loop over all files in InputDirectory, and look for all structure.json
      // files
      for (std::string const& file : files) {
        size_t const nameLength = file.size();

        if (nameLength <= suffix.size() ||
            file.substr(file.size() - suffix.size()) != suffix) {
          // some other file
          continue;
        }

        // found a structure.json file
        std::string name = file.substr(0, file.size() - suffix.size());

        if (!_includeSystemCollections && name[0] == '_') {
          continue;
        }

        VPackBuilder fileContentBuilder = _directory->vpackFromJsonFile(file);
        VPackSlice const fileContent = fileContentBuilder.slice();
        if (!fileContent.isObject()) {
          errorMsg = "could not read collection structure file '" +
                     _directory->pathToFile(file) + "'";
          return TRI_ERROR_INTERNAL;
        }

        VPackSlice const parameters = fileContent.get("parameters");
        VPackSlice const indexes = fileContent.get("indexes");

        if (!parameters.isObject() || !indexes.isArray()) {
          errorMsg = "could not read collection structure file '" +
                     _directory->pathToFile(file) + "'";
          return TRI_ERROR_INTERNAL;
        }

        std::string const cname =
            arangodb::basics::VelocyPackHelper::getStringValue(parameters,
                                                               "name", "");

        bool overwriteName = false;

        if (cname != name &&
            name !=
                (cname + "_" + arangodb::rest::SslInterface::sslMD5(cname))) {
          // file has a different name than found in structure file
          if (_importStructure) {
            // we cannot go on if there is a mismatch
            errorMsg =
                "collection name mismatch in collection structure file '" +
                _directory->pathToFile(file) + "' (offending value: '" + cname +
                "')";
            return TRI_ERROR_INTERNAL;
          } else {
            // we can patch the name in our array and go on
            LOG_TOPIC(INFO, Logger::RESTORE)
                << "ignoring collection name mismatch in collection "
                   "structure file '" +
                       _directory->pathToFile(file) + "' (offending value: '" +
                       cname + "')";

            overwriteName = true;
          }
        }

        if (!restrictList.empty() &&
            restrictList.find(cname) == restrictList.end()) {
          // collection name not in list
          continue;
        }

        if (overwriteName) {
          // TODO: we have a JSON object with sub-object "parameters" with
          // attribute "name". we only want to replace this. how?
        } else {
          collections.emplace_back(std::move(fileContentBuilder));
        }
      }
    }
    std::sort(collections.begin(), collections.end(), SortCollections);

    StringBuffer buffer(true);
    // step2: run the actual import
    for (VPackBuilder const& b : collections) {
      VPackSlice const collection = b.slice();
      VPackSlice const parameters = collection.get("parameters");
      VPackSlice const indexes = collection.get("indexes");
      std::string const cname =
          arangodb::basics::VelocyPackHelper::getStringValue(parameters, "name",
                                                             "");
      int type = arangodb::basics::VelocyPackHelper::getNumericValue<int>(
          parameters, "type", 2);

      std::string const collectionType(type == 2 ? "document" : "edge");

      if (_importStructure) {
        // re-create collection
        if (_progress) {
          if (_overwrite) {
            LOG_TOPIC(INFO, Logger::RESTORE)
                << "# Re-creating " << collectionType << " collection '"
                << cname << "'...";
          } else {
            LOG_TOPIC(INFO, Logger::RESTORE)
                << "# Creating " << collectionType << " collection '" << cname
                << "'...";
          }
        }

        int res = sendRestoreCollection(collection, cname, errorMsg);

        if (res != TRI_ERROR_NO_ERROR) {
          if (_force) {
            LOG_TOPIC(ERR, Logger::RESTORE) << errorMsg;
            continue;
          }
          return TRI_ERROR_INTERNAL;
        }
      }
      _stats.totalCollections++;

      if (_importData) {
        // import data. check if we have a datafile
        auto datafile = _directory->readableFile(
            cname + "_" + arangodb::rest::SslInterface::sslMD5(cname) +
            ".data.json");
        if (!datafile || datafile->status().fail()) {
          datafile = _directory->readableFile(cname + ".data.json");
          if (!datafile || datafile->status().fail()) {
            errorMsg = "could not open file";
            return TRI_ERROR_CANNOT_READ_FILE;
          }
        }

        if (TRI_ExistsFile(datafile->path().c_str())) {
          // found a datafile

          if (_progress) {
            LOG_TOPIC(INFO, Logger::RESTORE)
                << "# Loading data into " << collectionType << " collection '"
                << cname << "'...";
          }

          buffer.clear();

          while (true) {
            if (buffer.reserve(16384) != TRI_ERROR_NO_ERROR) {
              errorMsg = "out of memory";
              return TRI_ERROR_OUT_OF_MEMORY;
            }

            ssize_t numRead = datafile->read(buffer.end(), 16384);
            if (datafile->status().fail()) {
              // error while reading
              errorMsg = datafile->status().errorMessage();
              return datafile->status().errorNumber();
            }

            // read something
            buffer.increaseLength(numRead);

            _stats.totalRead += (uint64_t)numRead;

            if (buffer.length() < _chunkSize && numRead > 0) {
              // still continue reading
              continue;
            }

            // do we have a buffer?
            if (buffer.length() > 0) {
              // look for the last \n in the buffer
              char* found = (char*)memrchr((const void*)buffer.begin(), '\n',
                                           buffer.length());
              size_t length;

              if (found == nullptr) {
                // no \n found...
                if (numRead == 0) {
                  // we're at the end. send the complete buffer anyway
                  length = buffer.length();
                } else {
                  // read more
                  continue;
                }
              } else {
                // found a \n somewhere
                length = found - buffer.begin();
              }

              _stats.totalBatches++;

              int res =
                  sendRestoreData(cname, buffer.begin(), length, errorMsg);

              if (res != TRI_ERROR_NO_ERROR) {
                if (errorMsg.empty()) {
                  errorMsg = std::string(TRI_errno_string(res));
                } else {
                  errorMsg =
                      std::string(TRI_errno_string(res)) + ": " + errorMsg;
                }

                if (_force) {
                  LOG_TOPIC(ERR, Logger::RESTORE) << errorMsg;
                  continue;
                }

                return res;
              }

              buffer.erase_front(length);
            }

            if (numRead == 0) {
              // EOF
              break;
            }
          }
        }
      }

      if (_importStructure) {
        // re-create indexes
        if (indexes.length() > 0) {
          // we actually have indexes
          if (_progress) {
            LOG_TOPIC(INFO, Logger::RESTORE)
                << "# Creating indexes for collection '" << cname << "'...";
          }

          int res = sendRestoreIndexes(collection, errorMsg);

          if (res != TRI_ERROR_NO_ERROR) {
            if (_force) {
              LOG_TOPIC(ERR, Logger::RESTORE) << errorMsg;
              continue;
            }
            return TRI_ERROR_INTERNAL;
          }
        }
      }
    }
  } catch (std::exception const& ex) {
    errorMsg = "arangorestore terminated because of an unhandled exception: ";
    errorMsg.append(ex.what());
    return TRI_ERROR_INTERNAL;
  } catch (...) {
    errorMsg = "arangorestore out of memory";
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  return TRI_ERROR_NO_ERROR;
}

void RestoreFeature::start() {
  using arangodb::httpclient::SimpleHttpClient;

  // set up the output directory, not much else
  _directory = std::make_unique<ManagedDirectory>(_inputPath, false, false);
  if (_directory->status().fail()) {
    switch (_directory->status().errorNumber()) {
      case TRI_ERROR_FILE_NOT_FOUND:
        LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
            << "input directory '" << _inputPath << "' does not exist";
        break;
      default:
        LOG_TOPIC(ERR, arangodb::Logger::FIXME)
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
  try {
    httpClient = client->createHttpClient();
  } catch (...) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "cannot create server connection, giving up!";
    FATAL_ERROR_EXIT();
  }

  std::string dbName = client->databaseName();

  httpClient->params().setLocationRewriter(static_cast<void*>(client),
                                           ClientManager::rewriteLocation);
  httpClient->params().setUserNamePassword("/", client->username(),
                                           client->password());

  // read encryption info
  {
    Result r = checkEncryption();

    if (r.fail()) {
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << r.errorMessage();
      FATAL_ERROR_EXIT();
    }
  }

  // read dump info
  {
    Result r = readDumpInfo();

    if (r.fail()) {
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << r.errorMessage();
      FATAL_ERROR_EXIT();
    }
  }

  int err = TRI_ERROR_NO_ERROR;
  std::string versionString = httpClient->getServerVersion(&err);

  if (_createDatabase && err == TRI_ERROR_ARANGO_DATABASE_NOT_FOUND) {
    // database not found, but database creation requested
    LOG_TOPIC(INFO, Logger::RESTORE) << "Creating database '" << dbName << "'";

    client->setDatabaseName("_system");

    int res = tryCreateDatabase(dbName);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "Could not create database '" << dbName << "'";
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
          << httpClient->getErrorMessage();
      FATAL_ERROR_EXIT();
    }

    // restore old database name
    client->setDatabaseName(dbName);

    // re-fetch version
    versionString = httpClient->getServerVersion(nullptr);
  }

  if (!httpClient->isConnected()) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "Could not connect to endpoint "
        << httpClient->getEndpointSpecification();
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << httpClient->getErrorMessage();
    FATAL_ERROR_EXIT();
  }

  // successfully connected
  LOG_TOPIC(INFO, Logger::RESTORE) << "Server version: " << versionString;

  // validate server version
  std::pair<int, int> version = rest::Version::parseVersionString(versionString);

  if (version.first < 3) {
    // we can connect to 3.x
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "got incompatible server version '" << versionString << "'";

    if (!_force) {
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "giving up!";
      FATAL_ERROR_EXIT();
    }
  }

  // Version 1.4 did not yet have a cluster mode
  Result result;
  std::tie(result, _clusterMode) =
      _clientManager.getArangoIsCluster(*httpClient);
  if (result.fail()) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << result.errorMessage();
    _exitCode = EXIT_FAILURE;
    return;
  }

  if (_progress) {
    LOG_TOPIC(INFO, Logger::RESTORE)
        << "# Connected to ArangoDB '" << httpClient->getEndpointSpecification()
        << "'";
  }

  int res;
  std::string errorMsg;
  try {
    res = processInputDirectory(errorMsg);
  } catch (std::exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "caught exception " << ex.what();
    res = TRI_ERROR_INTERNAL;
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "Error: caught unknown exception";
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    if (!errorMsg.empty()) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << errorMsg;
    } else {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "An error occurred";
    }
    _exitCode = EXIT_FAILURE;
  }

  if (_progress) {
    if (_importData) {
      LOG_TOPIC(INFO, Logger::RESTORE)
          << "Processed " << _stats.totalCollections << " collection(s), "
          << "read " << _stats.totalRead << " byte(s) from datafiles, "
          << "sent " << _stats.totalBatches << " batch(es)";
    } else if (_importStructure) {
      LOG_TOPIC(INFO, Logger::RESTORE)
          << "Processed " << _stats.totalCollections << " collection(s)";
    }
  }
}

void RestoreFeature::reportError(Result const& error) {
  try {
    MUTEX_LOCKER(lock, _workerErrorLock);
    _workerErrors.emplace(error);
    _clientTaskQueue.clearQueue();
  } catch (...) {
  }
}

std::string RestoreFeature::featureName() { return ::FeatureName; }

}  // namespace arangodb
