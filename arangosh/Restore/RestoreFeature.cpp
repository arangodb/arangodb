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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
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

#include <velocypack/Options.h>
#include <velocypack/velocypack-aliases.h>

#include <iostream>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::options;
using namespace arangodb::rest;

RestoreFeature::RestoreFeature(application_features::ApplicationServer* server,
                               int* result)
    : ApplicationFeature(server, "Restore"),
      _collections(),
      _chunkSize(1024 * 1024 * 8),
      _includeSystemCollections(false),
      _createDatabase(false),
      _inputDirectory(),
      _importData(true),
      _importStructure(true),
      _progress(true),
      _overwrite(true),
      _recycleIds(false),
      _force(false),
      _clusterMode(false),
      _defaultNumberOfShards(1),
      _defaultReplicationFactor(1),
      _result(result),
      _stats{ 0, 0, 0 } {
  requiresElevatedPrivileges(false);
  setOptional(false);
  startsAfter("Client");
  startsAfter("Logger");

  _inputDirectory =
      FileUtils::buildFilename(FileUtils::currentDirectory(), "dump");
}

void RestoreFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options->addOption(
      "--collection",
      "restrict to collection name (can be specified multiple times)",
      new VectorParameter<StringParameter>(&_collections));

  options->addOption("--batch-size",
                     "maximum size for individual data batches (in bytes)",
                     new UInt64Parameter(&_chunkSize));

  options->addOption("--include-system-collections",
                     "include system collections",
                     new BooleanParameter(&_includeSystemCollections));

  options->addOption("--create-database",
                     "create the target database if it does not exist",
                     new BooleanParameter(&_createDatabase));

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

  options->addOption("--recycle-ids",
                     "recycle collection and revision ids from dump",
                     new BooleanParameter(&_recycleIds));

  options->addOption("--default-number-of-shards",
                     "default value for numberOfShards if not specified",
                     new UInt64Parameter(&_defaultNumberOfShards));

  options->addOption("--default-replication-factor",
                     "default value for replicationFactor if not specified",
                     new UInt64Parameter(&_defaultReplicationFactor));

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
    LOG(FATAL) << "expecting at most one directory, got " +
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
    LOG(FATAL) << "input directory '" << _inputDirectory << "' does not exist";
    FATAL_ERROR_EXIT();
  }

  if (!_importStructure && !_importData) {
    LOG(FATAL)
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

  std::unique_ptr<SimpleHttpResult> response(
      _httpClient->request(rest::RequestType::POST, "/_api/database",
                           body.c_str(), body.size()));

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
      std::string(_overwrite ? "true" : "false") + "&recycleIds=" +
      std::string(_recycleIds ? "true" : "false") + "&force=" +
      std::string(_force ? "true" : "false");

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
    if (!slice.hasKey(std::vector<std::string>({"parameters", "replicationFactor"}))) {
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
                          StringUtils::urlEncode(cname) + "&recycleIds=" +
                          (_recycleIds ? "true" : "false") + "&force=" +
                          (_force ? "true" : "false");

  std::unique_ptr<SimpleHttpResult> response(_httpClient->request(
      rest::RequestType::PUT, url, buffer, bufferSize));

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

static bool SortCollections(VPackSlice const& l, VPackSlice const& r) {
  VPackSlice const left = l.get("parameters");
  VPackSlice const right = r.get("parameters");

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
    std::vector<std::shared_ptr<VPackBuilder>> collectionBuilders;
    std::vector<VPackSlice> collections;

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
        std::shared_ptr<VPackBuilder> fileContentBuilder =
            arangodb::basics::VelocyPackHelper::velocyPackFromFile(fqn);
        VPackSlice const fileContent = fileContentBuilder->slice();

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
          // TODO MAX
          // Situation:
          // Ich habe ein Json-Object von Datei (teile des Inhalts im Zweifel
          // unbekannt)
          // Es gibt ein Sub-Json-Object "parameters" mit einem Attribute "name"
          // der gesetzt ist.
          // Ich muss nur diesen namen überschreiben, der Rest soll identisch
          // bleiben.
        } else {
          collectionBuilders.emplace_back(fileContentBuilder);
          collections.emplace_back(fileContent);
        }
      }
    }

    std::sort(collections.begin(), collections.end(), SortCollections);

    StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE);

    // step2: run the actual import
    for (VPackSlice const& collection : collections) {
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
          // found a datafile

          if (_progress) {
            std::cout << "# Loading data into " << collectionType
                      << " collection '" << cname << "'..." << std::endl;
          }

          int fd = TRI_OPEN(datafile.c_str(), O_RDONLY | TRI_O_CLOEXEC);

          if (fd < 0) {
            errorMsg = "cannot open collection data file '" + datafile + "'";

            return TRI_ERROR_INTERNAL;
          }

          buffer.clear();

          while (true) {
            if (buffer.reserve(16384) != TRI_ERROR_NO_ERROR) {
              TRI_CLOSE(fd);
              errorMsg = "out of memory";

              return TRI_ERROR_OUT_OF_MEMORY;
            }

            ssize_t numRead = TRI_READ(fd, buffer.end(), 16384);

            if (numRead < 0) {
              // error while reading
              int res = TRI_errno();
              TRI_CLOSE(fd);
              errorMsg = std::string(TRI_errno_string(res));

              return res;
            }

            // read something
            buffer.increaseLength(numRead);

            _stats._totalRead += (uint64_t)numRead;

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

              TRI_ASSERT(length > 0);

              _stats._totalBatches++;

              int res =
                  sendRestoreData(cname, buffer.begin(), length, errorMsg);

              if (res != TRI_ERROR_NO_ERROR) {
                TRI_CLOSE(fd);
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

                return res;
              }

              buffer.erase_front(length);
            }

            if (numRead == 0) {
              // EOF
              break;
            }
          }

          TRI_CLOSE(fd);
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
    }
  } catch (...) {
    errorMsg = "out of memory";
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  return TRI_ERROR_NO_ERROR;
}

void RestoreFeature::start() {
  ClientFeature* client = application_features::ApplicationServer::getFeature<ClientFeature>("Client");

  int ret = EXIT_SUCCESS;
  *_result = ret;

  try {
    _httpClient = client->createHttpClient();
  } catch (...) {
    LOG(FATAL) << "cannot create server connection, giving up!";
    FATAL_ERROR_EXIT();
  }

  std::string dbName = client->databaseName();

  _httpClient->setLocationRewriter(static_cast<void*>(client), &rewriteLocation);
  _httpClient->setUserNamePassword("/", client->username(), client->password());

  int err = TRI_ERROR_NO_ERROR;
  std::string versionString = _httpClient->getServerVersion(&err);

  if (_createDatabase && err == TRI_ERROR_ARANGO_DATABASE_NOT_FOUND) {
    // database not found, but database creation requested
    std::cout << "Creating database '" << dbName << "'" << std::endl;

    client->setDatabaseName("_system");

    int res = tryCreateDatabase(client, dbName);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG(ERR) << "Could not create database '" << dbName << "'";
      LOG(FATAL) << _httpClient->getErrorMessage() << "'";
      FATAL_ERROR_EXIT();
    }

    // restore old database name
    client->setDatabaseName(dbName);

    // re-fetch version
    versionString = _httpClient->getServerVersion(nullptr);
  }

  if (!_httpClient->isConnected()) {
    LOG(ERR) << "Could not connect to endpoint "
             << _httpClient->getEndpointSpecification();
    LOG(FATAL) << _httpClient->getErrorMessage() << "'";
    FATAL_ERROR_EXIT();
  }

  // successfully connected
  std::cout << "Server version: " << versionString << std::endl;

  // validate server version
  std::pair<int, int> version = Version::parseVersionString(versionString);

  if (version.first < 3) {
    // we can connect to 3.x
    LOG(ERR) << "got incompatible server version '" << versionString << "'";

    if (!_force) {
      LOG(FATAL) << "giving up!";
      FATAL_ERROR_EXIT();
    }
  }

  // Version 1.4 did not yet have a cluster mode
  _clusterMode = getArangoIsCluster(nullptr);

  if (_progress) {
    std::cout << "# Connected to ArangoDB '"
              << _httpClient->getEndpointSpecification() << "'" << std::endl;
  }

  std::string errorMsg = "";

  int res;
  try {
    res = processInputDirectory(errorMsg);
  } catch (std::exception const& ex) {
    LOG(ERR) << "caught exception " << ex.what();
    res = TRI_ERROR_INTERNAL;
  } catch (...) {
    LOG(ERR) << "Error: caught unknown exception";
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    if (!errorMsg.empty()) {
      LOG(ERR) << errorMsg;
    } else {
      LOG(ERR) << "An error occurred";
    }
    ret = EXIT_FAILURE;
  }

  if (_progress) {
    if (_importData) {
      std::cout << "Processed " << _stats._totalCollections
                << " collection(s), "
                << "read " << _stats._totalRead << " byte(s) from datafiles, "
                << "sent " << _stats._totalBatches << " batch(es)" << std::endl;
    } else if (_importStructure) {
      std::cout << "Processed " << _stats._totalCollections << " collection(s)"
                << std::endl;
    }
  }
  
  *_result = ret;
}
