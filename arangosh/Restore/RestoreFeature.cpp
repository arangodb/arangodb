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

#include <iostream>

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
#include "ProgramOptions/ProgramOptions.h"
#include "Rest/HttpResponse.h"
#include "Rest/InitializeRest.h"
#include "Rest/Version.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Ssl/SslInterface.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Encryption/EncryptionFeature.h"
#endif

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::options;
using namespace arangodb::rest;

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

RestoreFeature::RestoreFeature(application_features::ApplicationServer* server,
                               int* result)
    : ApplicationFeature(server, "Restore"),
      _collections(),
      _chunkSize(1024 * 1024 * 8),
      _includeSystemCollections(false),
      _createDatabase(false),
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
      _result(result),
#ifdef USE_ENTERPRISE
      _encryption(nullptr),
#endif
      _stats{0, 0, 0, 0} {
  requiresElevatedPrivileges(false);
  setOptional(false);
  startsAfter("Client");
  startsAfter("Logger");

#ifdef USE_ENTERPRISE
  startsAfter("Encryption");
#endif

  _inputDirectory =
      FileUtils::buildFilename(FileUtils::currentDirectory().result(), "dump");
}

void RestoreFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
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
  
  options->addOption("--force-same-database",
                     "force usage of the same database name as in the source dump.json file",
                     new BooleanParameter(&_forceSameDatabase));

  options->addOption("--input-directory", "input directory",
                     new StringParameter(&_inputDirectory));

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
  auto const& positionals = options->processingResult()._positionals;
  size_t n = positionals.size();

  if (1 == n) {
    _inputDirectory = positionals[0];
  } else if (1 < n) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "expecting at most one directory, got " +
               StringUtils::join(positionals, ", ");
    FATAL_ERROR_EXIT();
  }

  // use a minimum value for batches
  if (_chunkSize < 1024 * 128) {
    _chunkSize = 1024 * 128;
  }
}

void RestoreFeature::prepare() {
  if (!_inputDirectory.empty() &&
      _inputDirectory.back() == TRI_DIR_SEPARATOR_CHAR) {
    // trim trailing slash from path because it may cause problems on ...
    // Windows
    TRI_ASSERT(_inputDirectory.size() > 0);
    _inputDirectory.pop_back();
  }

  // .............................................................................
  // check input directory
  // .............................................................................

  if (_inputDirectory == "" || !TRI_IsDirectory(_inputDirectory.c_str())) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "input directory '" << _inputDirectory << "' does not exist";
    FATAL_ERROR_EXIT();
  }

  if (!_importStructure && !_importData) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "Error: must specify either --create-collection or --import-data";
    FATAL_ERROR_EXIT();
  }
}

int RestoreFeature::tryCreateDatabase(ClientFeature* client,
                                      std::string const& name) {
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

  std::unique_ptr<SimpleHttpResult> response(_httpClient->request(
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
    _httpClient->setErrorMessage(getHttpErrorMessage(response.get(), nullptr),
                                 false);
    return TRI_ERROR_FORBIDDEN;
  }

  // any other error
  _httpClient->setErrorMessage(getHttpErrorMessage(response.get(), nullptr),
                               false);
  return TRI_ERROR_INTERNAL;
}

int RestoreFeature::sendRestoreCollection(VPackSlice const& slice,
                                          std::string const& name,
                                          std::string& errorMsg) {
  std::string url =
      "/_api/replication/restore-collection"
      "?overwrite=" +
      std::string(_overwrite ? "true" : "false") + "&force=" +
      std::string(_force ? "true" : "false") +
      "&ignoreDistributeShardsLikeErrors=" +
      std::string(_ignoreDistributeShardsLikeErrors ? "true" : "false");

  if (_clusterMode) {
    if (!slice.hasKey(std::vector<std::string>({"parameters", "shards"})) &&
        !slice.hasKey(
            std::vector<std::string>({"parameters", "numberOfShards"}))) {
      // no "shards" and no "numberOfShards" attribute present. now assume
      // default value from --default-number-of-shards
      std::cerr << "# no sharding information specified for collection '"
                << name << "', using default number of shards "
                << _defaultNumberOfShards << std::endl;
      url += "&numberOfShards=" + std::to_string(_defaultNumberOfShards);
    }
    if (!slice.hasKey(
            std::vector<std::string>({"parameters", "replicationFactor"}))) {
      // No replication factor given, so take the default:
      std::cerr << "# no replication information specified for collection '"
                << name << "', using default replication factor "
                << _defaultReplicationFactor << std::endl;
      url += "&replicationFactor=" + std::to_string(_defaultReplicationFactor);
    }
  }

  std::string const body = slice.toJson();

  std::unique_ptr<SimpleHttpResult> response(_httpClient->request(
      rest::RequestType::PUT, url, body.c_str(), body.size()));

  if (response == nullptr || !response->isComplete()) {
    errorMsg =
        "got invalid response from server: " + _httpClient->getErrorMessage();

    return TRI_ERROR_INTERNAL;
  }

  if (response->wasHttpError()) {
    int err;
    errorMsg = getHttpErrorMessage(response.get(), &err);

    if (err != TRI_ERROR_NO_ERROR) {
      return err;
    }

    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

int RestoreFeature::sendRestoreIndexes(VPackSlice const& slice,
                                       std::string& errorMsg) {
  std::string const url = "/_api/replication/restore-indexes?force=" +
                          std::string(_force ? "true" : "false");
  std::string const body = slice.toJson();

  std::unique_ptr<SimpleHttpResult> response(_httpClient->request(
      rest::RequestType::PUT, url, body.c_str(), body.size()));

  if (response == nullptr || !response->isComplete()) {
    errorMsg =
        "got invalid response from server: " + _httpClient->getErrorMessage();

    return TRI_ERROR_INTERNAL;
  }

  if (response->wasHttpError()) {
    int err;
    errorMsg = getHttpErrorMessage(response.get(), &err);

    if (err != TRI_ERROR_NO_ERROR) {
      return err;
    }

    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

int RestoreFeature::sendRestoreData(std::string const& cname,
                                    char const* buffer, size_t bufferSize,
                                    std::string& errorMsg) {
  std::string const url = "/_api/replication/restore-data?collection=" +
                          StringUtils::urlEncode(cname) + "&force=" +
                          (_force ? "true" : "false");

  std::unique_ptr<SimpleHttpResult> response(
      _httpClient->request(rest::RequestType::PUT, url, buffer, bufferSize));

  if (response == nullptr || !response->isComplete()) {
    errorMsg =
        "got invalid response from server: " + _httpClient->getErrorMessage();

    return TRI_ERROR_INTERNAL;
  }

  if (response->wasHttpError()) {
    int err;
    errorMsg = getHttpErrorMessage(response.get(), &err);

    if (err != TRI_ERROR_NO_ERROR) {
      return err;
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

Result RestoreFeature::readEncryptionInfo() {
  std::string encryptionType;
  try {
    std::string const encryptionFilename = FileUtils::buildFilename(_inputDirectory, "ENCRYPTION");
    if (FileUtils::exists(encryptionFilename)) {
      encryptionType = StringUtils::trim(FileUtils::slurp(encryptionFilename));
    } else {
      encryptionType = "none";
    }
  } catch (basics::Exception const& ex) {
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    // file not found etc.
  }

  if (encryptionType != "none") {
#ifdef USE_ENTERPRISE
    if (!_encryption->keyOptionSpecified()) {
      std::cerr << "the dump data seems to be encrypted with " << encryptionType << ", but no key information was specified to decrypt the dump" << std::endl;
      std::cerr << "it is recommended to specify either `--encryption.key-file` or `--encryption.key-generator` when invoking arangorestore with an encrypted dump" << std::endl;
    } else {
      std::cout << "# using encryption type " << encryptionType << " for reading dump" << std::endl;
    }
#endif
  }

  return Result();
}

Result RestoreFeature::readDumpInfo() {
  std::string databaseName;

  try {
    std::string const fqn = _inputDirectory + TRI_DIR_SEPARATOR_STR + "dump.json";
#ifdef USE_ENTERPRISE
    VPackBuilder fileContentBuilder =
        (_encryption != nullptr)
            ? _encryption->velocyPackFromFile(fqn)
            : basics::VelocyPackHelper::velocyPackFromFile(fqn);
#else
    VPackBuilder fileContentBuilder =
        basics::VelocyPackHelper::velocyPackFromFile(fqn);
#endif
    VPackSlice const fileContent = fileContentBuilder.slice();

    databaseName = fileContent.get("database").copyString();
  } catch (...) {
    // the above may go wrong for several reasons
  }
  
  if (!databaseName.empty()) {
    std::cout << "Database name in source dump is '" << databaseName << "'" << std::endl;
  }


  ClientFeature* client =
      application_features::ApplicationServer::getFeature<ClientFeature>(
          "Client");
  if (_forceSameDatabase && databaseName != client->databaseName()) {
    return Result(TRI_ERROR_BAD_PARAMETER, std::string("database name in dump.json ('") + databaseName + "') does not match specified database name ('" + client->databaseName() + "')");
  }

  return Result();
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

int RestoreFeature::processInputDirectory(std::string& errorMsg) {
  // create a lookup table for collections
  std::map<std::string, bool> restrictList;
  for (size_t i = 0; i < _collections.size(); ++i) {
    restrictList.insert(std::pair<std::string, bool>(_collections[i], true));
  }
  try {
    std::vector<std::string> const files =
        FileUtils::listFiles(_inputDirectory);
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

        std::string const fqn = _inputDirectory + TRI_DIR_SEPARATOR_STR + file;
#ifdef USE_ENTERPRISE
        VPackBuilder fileContentBuilder =
            (_encryption != nullptr)
                ? _encryption->velocyPackFromFile(fqn)
                : basics::VelocyPackHelper::velocyPackFromFile(fqn);
#else
        VPackBuilder fileContentBuilder =
            basics::VelocyPackHelper::velocyPackFromFile(fqn);
#endif
        VPackSlice const fileContent = fileContentBuilder.slice();

        if (!fileContent.isObject()) {
          errorMsg = "could not read collection structure file '" + fqn + "'";
          return TRI_ERROR_INTERNAL;
        }

        VPackSlice const parameters = fileContent.get("parameters");
        VPackSlice const indexes = fileContent.get("indexes");

        if (!parameters.isObject() || !indexes.isArray()) {
          errorMsg = "could not read collection structure file '" + fqn + "'";
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
                fqn + "' (offending value: '" + cname + "')";
            return TRI_ERROR_INTERNAL;
          } else {
            // we can patch the name in our array and go on
            std::cout << "ignoring collection name mismatch in collection "
                         "structure file '" +
                             fqn + "' (offending value: '" + cname + "')"
                      << std::endl;

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

    bool didModifyFoxxCollection = false;
    StringBuffer buffer(true);
    // Step 2: run the actual import
    for (VPackBuilder const& b : collections) {
      VPackSlice const collection = b.slice();
      VPackSlice const parameters = collection.get("parameters");
      VPackSlice const indexes = collection.get("indexes");
      VPackSlice params = collection.get("parameters");
      if (params.isObject()) {
        params = params.get("name");
        // Only these two are relevant for FOXX.
        if (params.isString() && (params.isEqualString("_apps") || params.isEqualString("_appbundles"))) {
          didModifyFoxxCollection = true;
        }
      };
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
            std::cout << "# Re-creating " << collectionType << " collection '"
                      << cname << "'..." << std::endl;
          } else {
            std::cout << "# Creating " << collectionType << " collection '"
                      << cname << "'..." << std::endl;
          }
        }

        int res = sendRestoreCollection(collection, cname, errorMsg);

        if (res != TRI_ERROR_NO_ERROR) {
          if (_force) {
            std::cerr << errorMsg << std::endl;
            continue;
          }
          return TRI_ERROR_INTERNAL;
        }
      }
      _stats._totalCollections++;

      if (_importData) {
        // import data. check if we have a datafile
        std::string datafile =
            _inputDirectory + TRI_DIR_SEPARATOR_STR + cname + "_" +
            arangodb::rest::SslInterface::sslMD5(cname) + ".data.json";
        if (!TRI_ExistsFile(datafile.c_str())) {
          datafile =
              _inputDirectory + TRI_DIR_SEPARATOR_STR + cname + ".data.json";
        }

        if (TRI_ExistsFile(datafile.c_str())) {
          int64_t dataSize = TRI_SizeFile(datafile.c_str());
          // found a datafile
          int fd =
              TRI_TRACKED_OPEN_FILE(datafile.c_str(), O_RDONLY | TRI_O_CLOEXEC);

          if (fd < 0) {
            errorMsg = "cannot open collection data file '" + datafile + "'";

            return TRI_ERROR_INTERNAL;
          }
          
          if (_progress) {
            std::cout << "# Loading data into " << collectionType
                      << " collection '" << cname << "', data size: " << dataSize << " byte(s)" << std::endl;
          }

          beginDecryption(fd);

          buffer.clear();
          int64_t numReadForThisCollection = 0;
          int64_t numReadSinceLastReport = 0;

          while (true) {
            if (buffer.reserve(16384) != TRI_ERROR_NO_ERROR) {
              endDecryption(fd);
              TRI_TRACKED_CLOSE_FILE(fd);
              errorMsg = "out of memory";
              return TRI_ERROR_OUT_OF_MEMORY;
            }

            ssize_t numRead = readData(fd, buffer.end(), 16384);

            if (numRead < 0) {
              // error while reading
              int res = TRI_errno();
              endDecryption(fd);
              TRI_TRACKED_CLOSE_FILE(fd);
              errorMsg = std::string(TRI_errno_string(res));

              return res;
            }

            // read something
            buffer.increaseLength(numRead);

            _stats._totalRead += (uint64_t)numRead;
            numReadForThisCollection += numRead;
            numReadSinceLastReport += numRead;

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

              _stats._totalBatches++;

              int res =
                  sendRestoreData(cname, buffer.begin(), length, errorMsg);
              
              _stats._totalSent += length;

              if (_progress && dataSize > 0 && numReadSinceLastReport > 1024 * 1024 * 8) {
                // report every 8MB of transferred data
                std::cout << "# Progress: still processing collection '" << cname << "', " << numReadForThisCollection << " of " << dataSize << " byte(s) restored (" << int(100. * double(numReadForThisCollection) / double(dataSize)) << " %)" << std::endl;
                numReadSinceLastReport = 0;
              }

              if (res != TRI_ERROR_NO_ERROR) {
                if (errorMsg.empty()) {
                  errorMsg = std::string(TRI_errno_string(res));
                } else {
                  errorMsg =
                      std::string(TRI_errno_string(res)) + ": " + errorMsg;
                }

                if (_force) {
                  std::cerr << errorMsg << std::endl;
                  continue;
                }

                endDecryption(fd);
                TRI_TRACKED_CLOSE_FILE(fd);

                return res;
              }

              buffer.erase_front(length);
            }

            if (numRead == 0) {
              // EOF
              break;
            }
          }

          endDecryption(fd);
          TRI_TRACKED_CLOSE_FILE(fd);
        }
      }

      if (_importStructure) {
        // re-create indexes
        if (indexes.length() > 0) {
          // we actually have indexes
          if (_progress) {
            std::cout << "# Creating indexes for collection '" << cname
                      << "'..." << std::endl;
          }

          int res = sendRestoreIndexes(collection, errorMsg);

          if (res != TRI_ERROR_NO_ERROR) {
            if (_force) {
              std::cerr << errorMsg << std::endl;
              continue;
            }
            return TRI_ERROR_INTERNAL;
          }
        }
      }

      if (_progress) {
        std::cout << "# Progress: restored " << _stats._totalCollections << " of " << collections.size() 
                  << " collection(s), read " << _stats._totalRead << " byte(s) from datafiles, "
                  << "sent " << _stats._totalBatches << " data batch(es) of " << _stats._totalSent << " byte(s) total size" << std::endl;
      }

      if (didModifyFoxxCollection) {
        // if we get here we need to trigger foxx heal
        Result res = ::triggerFoxxHeal(*_httpClient);
        if (res.fail()) {
          LOG_TOPIC(WARN, Logger::RESTORE) << "Reloading of Foxx failed. In the cluster Foxx Services will be available eventually, On SingleServers send a PUT to ";
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
#ifdef USE_ENTERPRISE
  _encryption =
      application_features::ApplicationServer::getFeature<EncryptionFeature>(
          "Encryption");
#endif

  ClientFeature* client =
      application_features::ApplicationServer::getFeature<ClientFeature>(
          "Client");

  int ret = EXIT_SUCCESS;
  *_result = ret;

  try {
    _httpClient = client->createHttpClient();
  } catch (...) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "cannot create server connection, giving up!";
    FATAL_ERROR_EXIT();
  }

  std::string dbName = client->databaseName();

  _httpClient->params().setLocationRewriter(static_cast<void*>(client),
                                            &rewriteLocation);
  _httpClient->params().setUserNamePassword("/", client->username(),
                                            client->password());

  // read encryption info
  {
    Result r = readEncryptionInfo();

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
  std::string versionString = _httpClient->getServerVersion(&err);

  if (_createDatabase && err == TRI_ERROR_ARANGO_DATABASE_NOT_FOUND) {
    // database not found, but database creation requested
    std::cout << "Creating database '" << dbName << "'" << std::endl;

    client->setDatabaseName("_system");

    int res = tryCreateDatabase(client, dbName);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "Could not create database '"
                                              << dbName << "'";
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
          << _httpClient->getErrorMessage() << "'";
      FATAL_ERROR_EXIT();
    }

    // restore old database name
    client->setDatabaseName(dbName);

    // re-fetch version
    versionString = _httpClient->getServerVersion(nullptr);
  }

  if (!_httpClient->isConnected()) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "Could not connect to endpoint "
        << _httpClient->getEndpointSpecification();
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << _httpClient->getErrorMessage()
                                              << "'";
    FATAL_ERROR_EXIT();
  }

  // successfully connected
  std::cout << "Server version: " << versionString << std::endl;

  // validate server version
  std::pair<int, int> version = Version::parseVersionString(versionString);

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
  _clusterMode = getArangoIsCluster(nullptr);

  if (_progress) {
    std::cout << "# Connected to ArangoDB '"
              << _httpClient->getEndpointSpecification() << "'" << std::endl;
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
    ret = EXIT_FAILURE;
  }

  if (_progress) {
    if (_importData) {
      std::cout << "Processed " << _stats._totalCollections
                << " collection(s), "
                << "read " << _stats._totalRead << " byte(s) from datafiles, "
                << "sent " << _stats._totalBatches << " data batch(es)" << std::endl;
    } else if (_importStructure) {
      std::cout << "Processed " << _stats._totalCollections << " collection(s)"
                << std::endl;
    }
  }

  *_result = ret;
}

ssize_t RestoreFeature::readData(int fd, char* data, size_t len) {
#ifdef USE_ENTERPRISE
  if (_encryption != nullptr) {
    return _encryption->readData(fd, data, len);
  } else {
    return TRI_READ(fd, data, len);
  }
#else
  return TRI_READ(fd, data, len);
#endif
}

void RestoreFeature::beginDecryption(int fd) {
#ifdef USE_ENTERPRISE
  if (_encryption != nullptr) {
    bool result = _encryption->beginDecryption(fd);

    if (!result) {
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
          << "cannot write prefix, giving up!";
      FATAL_ERROR_EXIT();
    }
  }
#endif
}

void RestoreFeature::endDecryption(int fd) {
#ifdef USE_ENTERPRISE
  if (_encryption != nullptr) {
    _encryption->endDecryption(fd);
  }
#endif
}
