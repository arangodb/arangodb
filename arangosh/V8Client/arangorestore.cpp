////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include <iostream>

#include <velocypack/Options.h>
#include <velocypack/velocypack-aliases.h>

#include "ArangoShell/ArangoClient.h"
#include "Basics/FileUtils.h"
#include "Basics/ProgramOptions.h"
#include "Basics/ProgramOptionsDescription.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/files.h"
#include "Basics/init.h"
#include "Basics/terminal-utils.h"
#include "Basics/tri-strings.h"
#include "Rest/Endpoint.h"
#include "Rest/HttpResponse.h"
#include "Rest/InitializeRest.h"
#include "Rest/SslInterface.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief base class for clients
////////////////////////////////////////////////////////////////////////////////

ArangoClient BaseClient("arangorestore");

////////////////////////////////////////////////////////////////////////////////
/// @brief the initial default connection
////////////////////////////////////////////////////////////////////////////////

arangodb::httpclient::GeneralClientConnection* Connection = nullptr;

////////////////////////////////////////////////////////////////////////////////
/// @brief HTTP client
////////////////////////////////////////////////////////////////////////////////

arangodb::httpclient::SimpleHttpClient* Client = nullptr;

////////////////////////////////////////////////////////////////////////////////
/// @brief chunk size
////////////////////////////////////////////////////////////////////////////////

static uint64_t ChunkSize = 1024 * 1024 * 8;

////////////////////////////////////////////////////////////////////////////////
/// @brief collections
////////////////////////////////////////////////////////////////////////////////

static std::vector<std::string> Collections;

////////////////////////////////////////////////////////////////////////////////
/// @brief include system collections
////////////////////////////////////////////////////////////////////////////////

static bool IncludeSystemCollections;

////////////////////////////////////////////////////////////////////////////////
/// @brief create target database
////////////////////////////////////////////////////////////////////////////////

static bool CreateDatabase = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief input directory
////////////////////////////////////////////////////////////////////////////////

static std::string InputDirectory;

////////////////////////////////////////////////////////////////////////////////
/// @brief import data
////////////////////////////////////////////////////////////////////////////////

static bool ImportData = true;

////////////////////////////////////////////////////////////////////////////////
/// @brief import structure
////////////////////////////////////////////////////////////////////////////////

static bool ImportStructure = true;

////////////////////////////////////////////////////////////////////////////////
/// @brief progress
////////////////////////////////////////////////////////////////////////////////

static bool Progress = true;

////////////////////////////////////////////////////////////////////////////////
/// @brief overwrite collections if they exist
////////////////////////////////////////////////////////////////////////////////

static bool Overwrite = true;

////////////////////////////////////////////////////////////////////////////////
/// @brief re-use collection ids and revision ids on import
////////////////////////////////////////////////////////////////////////////////

static bool RecycleIds = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief continue restore even in the face of errors
////////////////////////////////////////////////////////////////////////////////

static bool Force = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief cluster mode flag
////////////////////////////////////////////////////////////////////////////////

static bool ClusterMode = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief last error code received
////////////////////////////////////////////////////////////////////////////////

static int LastErrorCode = TRI_ERROR_NO_ERROR;

////////////////////////////////////////////////////////////////////////////////
/// @brief default number of shards
////////////////////////////////////////////////////////////////////////////////

static int DefaultNumberOfShards = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief statistics
////////////////////////////////////////////////////////////////////////////////

static struct {
  uint64_t _totalBatches;
  uint64_t _totalCollections;
  uint64_t _totalRead;
} Stats;

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the program options
////////////////////////////////////////////////////////////////////////////////

static void ParseProgramOptions(int argc, char* argv[]) {
  ProgramOptionsDescription description("STANDARD options");

  description("collection", &Collections,
              "restrict to collection name (can be specified multiple times)")(
      "create-database", &CreateDatabase,
      "create the target database if it does not exist")(
      "batch-size", &ChunkSize,
      "maximum size for individual data batches (in bytes)")(
      "import-data", &ImportData, "import data into collection")(
      "recycle-ids", &RecycleIds,
      "recycle collection and revision ids from dump")(
      "default-number-of-shards", &DefaultNumberOfShards,
      "default value for numberOfShards if not specified")(
      "force", &Force,
      "continue restore even in the face of some server-side errors")(
      "create-collection", &ImportStructure, "create collection structure")(
      "include-system-collections", &IncludeSystemCollections,
      "include system collections")("input-directory", &InputDirectory,
                                    "input directory")(
      "overwrite", &Overwrite, "overwrite collections if they exist")(
      "progress", &Progress, "show progress");

  BaseClient.setupGeneral(description);
  BaseClient.setupServer(description);

  std::vector<std::string> arguments;
  description.arguments(&arguments);

  ProgramOptions options;
  BaseClient.parse(options, description, "", argc, argv, "arangorestore.conf");

  if (1 == arguments.size()) {
    InputDirectory = arguments[0];
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief startup and exit functions
////////////////////////////////////////////////////////////////////////////////

static void LocalEntryFunction();
static void LocalExitFunction(int, void*);

#ifdef _WIN32

// .............................................................................
// Call this function to do various initializations for windows only
// .............................................................................

static void LocalEntryFunction() {
  int maxOpenFiles = 1024;
  int res = 0;

  // ...........................................................................
  // Uncomment this to call this for extended debug information.
  // If you familiar with valgrind ... then this is not like that, however
  // you do get some similar functionality.
  // ...........................................................................
  // res = initializeWindows(TRI_WIN_INITIAL_SET_DEBUG_FLAG, 0);

  res = initializeWindows(TRI_WIN_INITIAL_SET_INVALID_HANLE_HANDLER, 0);
  if (res != 0) {
    _exit(1);
  }

  res = initializeWindows(TRI_WIN_INITIAL_SET_MAX_STD_IO,
                          (char const*)(&maxOpenFiles));
  if (res != 0) {
    _exit(1);
  }

  res = initializeWindows(TRI_WIN_INITIAL_WSASTARTUP_FUNCTION_CALL, 0);
  if (res != 0) {
    _exit(1);
  }

  TRI_Application_Exit_SetExit(LocalExitFunction);
}

static void LocalExitFunction(int exitCode, void* data) {
  int res = finalizeWindows(TRI_WIN_FINAL_WSASTARTUP_FUNCTION_CALL, 0);

  if (res != 0) {
    exit(1);
  }

  exit(exitCode);
}
#else

static void LocalEntryFunction() {}

static void LocalExitFunction(int exitCode, void* data) {}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief extract an error message from a response
////////////////////////////////////////////////////////////////////////////////

static std::string GetHttpErrorMessage(SimpleHttpResult* result) {
  LastErrorCode = TRI_ERROR_NO_ERROR;
  std::string details;
  try {
    std::shared_ptr<VPackBuilder> parsedBody = result->getBodyVelocyPack();
    VPackSlice const body = parsedBody->slice();

    std::string const& errorMessage =
        arangodb::basics::VelocyPackHelper::getStringValue(body, "errorMessage",
                                                           "");
    int const errorNum =
        arangodb::basics::VelocyPackHelper::getNumericValue<int>(body,
                                                                 "errorNum", 0);
    if (errorMessage != "" && errorNum > 0) {
      details =
          ": ArangoError " + StringUtils::itoa(errorNum) + ": " + errorMessage;
      LastErrorCode = errorNum;
    }
  } catch (...) {
    // No action
  }
  return "got error from server: HTTP " +
         StringUtils::itoa(result->getHttpReturnCode()) + " (" +
         result->getHttpReturnMessage() + ")" + details;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief try to create a database on the server
////////////////////////////////////////////////////////////////////////////////

static int TryCreateDatabase(std::string const& name) {
  arangodb::basics::Json json(arangodb::basics::Json::Object);
  json("name", arangodb::basics::Json(name));

  arangodb::basics::Json user(arangodb::basics::Json::Object);
  user("username", arangodb::basics::Json(BaseClient.username()));
  user("passwd", arangodb::basics::Json(BaseClient.password()));

  arangodb::basics::Json users(arangodb::basics::Json::Array);
  users.add(user);
  json("users", users);

  std::string const body(arangodb::basics::JsonHelper::toString(json.json()));

  std::unique_ptr<SimpleHttpResult> response(
      Client->request(HttpRequest::HTTP_REQUEST_POST, "/_api/database",
                      body.c_str(), body.size()));

  if (response == nullptr || !response->isComplete()) {
    return TRI_ERROR_INTERNAL;
  }

  auto returnCode = response->getHttpReturnCode();

  if (returnCode == HttpResponse::OK || returnCode == HttpResponse::CREATED) {
    // all ok
    return TRI_ERROR_NO_ERROR;
  } else if (returnCode == HttpResponse::UNAUTHORIZED ||
             returnCode == HttpResponse::FORBIDDEN) {
    // invalid authorization
    Client->setErrorMessage(GetHttpErrorMessage(response.get()), false);
    return TRI_ERROR_FORBIDDEN;
  }

  // any other error
  Client->setErrorMessage(GetHttpErrorMessage(response.get()), false);
  return TRI_ERROR_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetch the version from the server
////////////////////////////////////////////////////////////////////////////////

static std::string GetArangoVersion() {
  std::unique_ptr<SimpleHttpResult> response(Client->request(
      HttpRequest::HTTP_REQUEST_GET, "/_api/version", nullptr, 0));

  if (response == nullptr || !response->isComplete()) {
    return "";
  }

  std::string version;

  if (response->getHttpReturnCode() == HttpResponse::OK) {
    // default value
    version = "arango";

    try {
      // convert response body to VPack
      std::shared_ptr<VPackBuilder> parsedBody = response->getBodyVelocyPack();
      VPackSlice const body = parsedBody->slice();

      // look up "server" value
      std::string const server =
          arangodb::basics::VelocyPackHelper::getStringValue(body, "server",
                                                             "");

      // "server" value is a string and content is "arango"
      if (server == "arango") {
        // look up "version" value
        version = arangodb::basics::VelocyPackHelper::getStringValue(
            body, "version", "");
      }
    } catch (...) {
      // No action
    }
  } else {
    if (response->wasHttpError()) {
      Client->setErrorMessage(GetHttpErrorMessage(response.get()), false);
    }

    Connection->disconnect();
  }

  return version;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if server is a coordinator of a cluster
////////////////////////////////////////////////////////////////////////////////

static bool GetArangoIsCluster() {
  std::unique_ptr<SimpleHttpResult> response(Client->request(
      HttpRequest::HTTP_REQUEST_GET, "/_admin/server/role", "", 0));

  if (response == nullptr || !response->isComplete()) {
    return false;
  }

  std::string role = "UNDEFINED";

  if (response->getHttpReturnCode() == HttpResponse::OK) {
    // convert response body to json
    try {
      std::shared_ptr<VPackBuilder> parsedBody = response->getBodyVelocyPack();
      VPackSlice const body = parsedBody->slice();
      role = arangodb::basics::VelocyPackHelper::getStringValue(body, "role",
                                                                "UNDEFINED");
    } catch (...) {
      // No action
    }
  } else {
    if (response->wasHttpError()) {
      Client->setErrorMessage(GetHttpErrorMessage(response.get()), false);
    }

    Connection->disconnect();
  }

  return role == "COORDINATOR";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send the request to re-create a collection
////////////////////////////////////////////////////////////////////////////////

static int SendRestoreCollection(VPackSlice const& slice,
                                 std::string const& name,
                                 std::string& errorMsg) {
  std::string url =
      "/_api/replication/restore-collection"
      "?overwrite=" +
      std::string(Overwrite ? "true" : "false") + "&recycleIds=" +
      std::string(RecycleIds ? "true" : "false") + "&force=" +
      std::string(Force ? "true" : "false");

  if (ClusterMode &&
      !slice.hasKey(std::vector<std::string>({"parameters", "shards"})) &&
      !slice.hasKey(
          std::vector<std::string>({"parameters", "numberOfShards"}))) {
    // no "shards" and no "numberOfShards" attribute present. now assume
    // default value from --default-number-of-shards
    std::cerr << "# no sharding information specified for collection '" << name
              << "', using default number of shards " << DefaultNumberOfShards
              << std::endl;
    url += "&numberOfShards=" + std::to_string(DefaultNumberOfShards);
  }

  std::string const body = slice.toJson();

  std::unique_ptr<SimpleHttpResult> response(Client->request(
      HttpRequest::HTTP_REQUEST_PUT, url, body.c_str(), body.size()));

  if (response == nullptr || !response->isComplete()) {
    errorMsg = "got invalid response from server: " + Client->getErrorMessage();

    return TRI_ERROR_INTERNAL;
  }

  if (response->wasHttpError()) {
    errorMsg = GetHttpErrorMessage(response.get());
    if (LastErrorCode != TRI_ERROR_NO_ERROR) {
      return LastErrorCode;
    }

    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send the request to re-create indexes for a collection
////////////////////////////////////////////////////////////////////////////////

static int SendRestoreIndexes(VPackSlice const& slice, std::string& errorMsg) {
  std::string const url = "/_api/replication/restore-indexes?force=" +
                          std::string(Force ? "true" : "false");
  std::string const body = slice.toJson();

  std::unique_ptr<SimpleHttpResult> response(Client->request(
      HttpRequest::HTTP_REQUEST_PUT, url, body.c_str(), body.size()));

  if (response == nullptr || !response->isComplete()) {
    errorMsg = "got invalid response from server: " + Client->getErrorMessage();

    return TRI_ERROR_INTERNAL;
  }

  if (response->wasHttpError()) {
    errorMsg = GetHttpErrorMessage(response.get());
    if (LastErrorCode != TRI_ERROR_NO_ERROR) {
      return LastErrorCode;
    }

    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send the request to load data into a collection
////////////////////////////////////////////////////////////////////////////////

static int SendRestoreData(std::string const& cname, char const* buffer,
                           size_t bufferSize, std::string& errorMsg) {
  std::string const url = "/_api/replication/restore-data?collection=" +
                          StringUtils::urlEncode(cname) + "&recycleIds=" +
                          (RecycleIds ? "true" : "false") + "&force=" +
                          (Force ? "true" : "false");

  std::unique_ptr<SimpleHttpResult> response(
      Client->request(HttpRequest::HTTP_REQUEST_PUT, url, buffer, bufferSize));

  if (response == nullptr || !response->isComplete()) {
    errorMsg = "got invalid response from server: " + Client->getErrorMessage();

    return TRI_ERROR_INTERNAL;
  }

  if (response->wasHttpError()) {
    errorMsg = GetHttpErrorMessage(response.get());
    if (LastErrorCode != TRI_ERROR_NO_ERROR) {
      return LastErrorCode;
    }

    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief comparator to sort collections
/// sort order is by collection type first (vertices before edges, this is
/// because edges depend on vertices being there), then name
////////////////////////////////////////////////////////////////////////////////

static bool SortCollections(VPackSlice const& l, VPackSlice const& r) {
  VPackSlice const& left = l.get("parameters");
  VPackSlice const& right = r.get("parameters");

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

////////////////////////////////////////////////////////////////////////////////
/// @brief process all files from the input directory
////////////////////////////////////////////////////////////////////////////////

static int ProcessInputDirectory(std::string& errorMsg) {
  // create a lookup table for collections
  std::map<std::string, bool> restrictList;
  for (size_t i = 0; i < Collections.size(); ++i) {
    restrictList.insert(std::pair<std::string, bool>(Collections[i], true));
  }
  try {
    std::vector<std::string> const files = FileUtils::listFiles(InputDirectory);
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

        if (!IncludeSystemCollections && name[0] == '_') {
          continue;
        }

        std::string const fqn = InputDirectory + TRI_DIR_SEPARATOR_STR + file;
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
          if (ImportStructure) {
            // we cannot go on if there is a mismatch
            errorMsg =
                "collection name mismatch in collection structure file '" +
                fqn + "' (offending value: '" + cname + "')";
            return TRI_ERROR_INTERNAL;
          } else {
            // we can patch the name in our array and go on
            std::cout << "ignoring collection name mismatch in collection "
                         "structure file '" +
                             fqn + "' (offending value: '" + cname +
                             "')" << std::endl;

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

      if (ImportStructure) {
        // re-create collection
        if (Progress) {
          if (Overwrite) {
            std::cout << "# Re-creating " << collectionType << " collection '"
                      << cname << "'..." << std::endl;
          } else {
            std::cout << "# Creating " << collectionType << " collection '"
                      << cname << "'..." << std::endl;
          }
        }

        int res = SendRestoreCollection(collection, cname, errorMsg);

        if (res != TRI_ERROR_NO_ERROR) {
          if (Force) {
            std::cerr << errorMsg << std::endl;
            continue;
          }
          return TRI_ERROR_INTERNAL;
        }
      }
      Stats._totalCollections++;

      if (ImportData) {
        // import data. check if we have a datafile
        std::string datafile =
            InputDirectory + TRI_DIR_SEPARATOR_STR + cname + "_" +
            arangodb::rest::SslInterface::sslMD5(cname) + ".data.json";
        if (!TRI_ExistsFile(datafile.c_str())) {
          datafile =
              InputDirectory + TRI_DIR_SEPARATOR_STR + cname + ".data.json";
        }

        if (TRI_ExistsFile(datafile.c_str())) {
          // found a datafile

          if (Progress) {
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

            Stats._totalRead += (uint64_t)numRead;

            if (buffer.length() < ChunkSize && numRead > 0) {
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

              Stats._totalBatches++;

              int res =
                  SendRestoreData(cname, buffer.begin(), length, errorMsg);

              if (res != TRI_ERROR_NO_ERROR) {
                TRI_CLOSE(fd);
                if (errorMsg.empty()) {
                  errorMsg = std::string(TRI_errno_string(res));
                } else {
                  errorMsg =
                      std::string(TRI_errno_string(res)) + ": " + errorMsg;
                }

                if (Force) {
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

      if (ImportStructure) {
        // re-create indexes
        if (indexes.length() > 0) {
          // we actually have indexes
          if (Progress) {
            std::cout << "# Creating indexes for collection '" << cname
                      << "'..." << std::endl;
          }

          int res = SendRestoreIndexes(collection, errorMsg);

          if (res != TRI_ERROR_NO_ERROR) {
            if (Force) {
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

////////////////////////////////////////////////////////////////////////////////
/// @brief request location rewriter (injects database name)
////////////////////////////////////////////////////////////////////////////////

static std::string rewriteLocation(void* data, std::string const& location) {
  if (location.substr(0, 5) == "/_db/") {
    // location already contains /_db/
    return location;
  }

  if (location[0] == '/') {
    return "/_db/" + BaseClient.databaseName() + location;
  } else {
    return "/_db/" + BaseClient.databaseName() + "/" + location;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief main
////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]) {
  int ret = EXIT_SUCCESS;

  LocalEntryFunction();

  TRIAGENS_C_INITIALIZE(argc, argv);
  TRIAGENS_REST_INITIALIZE(argc, argv);

  Logger::initialize(false);

  // .............................................................................
  // set defaults
  // .............................................................................

  BaseClient.setEndpointString(Endpoint::getDefaultEndpoint());

  // .............................................................................
  // parse the program options
  // .............................................................................

  ParseProgramOptions(argc, argv);

  // use a minimum value for batches
  if (ChunkSize < 1024 * 128) {
    ChunkSize = 1024 * 128;
  }

  if (!InputDirectory.empty() &&
      InputDirectory.back() == TRI_DIR_SEPARATOR_CHAR) {
    // trim trailing slash from path because it may cause problems on ...
    // Windows
    TRI_ASSERT(InputDirectory.size() > 0);
    InputDirectory.pop_back();
  }

  // .............................................................................
  // check input directory
  // .............................................................................

  if (InputDirectory == "" || !TRI_IsDirectory(InputDirectory.c_str())) {
    std::cerr << "Error: input directory '" << InputDirectory
              << "' does not exist" << std::endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  if (!ImportStructure && !ImportData) {
    std::cerr
        << "Error: must specify either --create-collection or --import-data"
        << std::endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  // .............................................................................
  // set-up client connection
  // .............................................................................

  BaseClient.createEndpoint();

  if (BaseClient.endpointServer() == nullptr) {
    std::cerr << "Error: invalid value for --server.endpoint ('"
              << BaseClient.endpointString() << "')" << std::endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  Connection = GeneralClientConnection::factory(
      BaseClient.endpointServer(), BaseClient.requestTimeout(),
      BaseClient.connectTimeout(), ArangoClient::DEFAULT_RETRIES,
      BaseClient.sslProtocol());

  Client = new SimpleHttpClient(Connection, BaseClient.requestTimeout(), false);

  Client->setLocationRewriter(nullptr, &rewriteLocation);
  Client->setUserNamePassword("/", BaseClient.username(),
                              BaseClient.password());

  std::string versionString = GetArangoVersion();

  if (CreateDatabase && LastErrorCode == TRI_ERROR_ARANGO_DATABASE_NOT_FOUND) {
    // database not found, but database creation requested

    std::string old = BaseClient.databaseName();
    std::cout << "Creating database '" << old << "'" << std::endl;

    BaseClient.setDatabaseName("_system");

    int res = TryCreateDatabase(old);

    if (res != TRI_ERROR_NO_ERROR) {
      std::cerr << "Could not create database '" << old << "'" << std::endl;
      std::cerr << "Error message: '" << Client->getErrorMessage() << "'"
                << std::endl;
      TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
    }

    // restore old database name
    BaseClient.setDatabaseName(old);

    // re-fetch version
    versionString = GetArangoVersion();
  }

  if (!Connection->isConnected()) {
    std::cerr << "Could not connect to endpoint "
              << BaseClient.endpointServer()->getSpecification() << std::endl;
    std::cerr << "Error message: '" << Client->getErrorMessage() << "'"
              << std::endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  // successfully connected
  std::cout << "Server version: " << versionString << std::endl;

  // validate server version
  int major = 0;
  int minor = 0;

  if (sscanf(versionString.c_str(), "%d.%d", &major, &minor) != 2) {
    std::cerr << "Error: invalid server version '" << versionString << "'"
              << std::endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  if (major < 1 || major > 2 || (major == 1 && minor < 4)) {
    // we can connect to 1.4, 2.0 and higher only
    std::cerr << "Error: got incompatible server version '" << versionString
              << "'" << std::endl;
    if (!Force) {
      TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
    }
  }

  if (major >= 2) {
    // Version 1.4 did not yet have a cluster mode
    ClusterMode = GetArangoIsCluster();
  }

  if (Progress) {
    std::cout << "# Connected to ArangoDB '"
              << BaseClient.endpointServer()->getSpecification() << "'"
              << std::endl;
  }

  memset(&Stats, 0, sizeof(Stats));

  std::string errorMsg = "";

  int res;
  try {
    res = ProcessInputDirectory(errorMsg);
  } catch (std::exception const& ex) {
    std::cerr << "Error: caught exception " << ex.what() << std::endl;
    res = TRI_ERROR_INTERNAL;
  } catch (...) {
    std::cerr << "Error: caught unknown exception" << std::endl;
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    if (!errorMsg.empty()) {
      std::cerr << "Error: " << errorMsg << std::endl;
    } else {
      std::cerr << "An error occurred" << std::endl;
    }
    ret = EXIT_FAILURE;
  }

  if (Progress) {
    if (ImportData) {
      std::cout << "Processed " << Stats._totalCollections << " collection(s), "
                << "read " << Stats._totalRead << " byte(s) from datafiles, "
                << "sent " << Stats._totalBatches << " batch(es)" << std::endl;
    } else if (ImportStructure) {
      std::cout << "Processed " << Stats._totalCollections << " collection(s)"
                << std::endl;
    }
  }

  if (Client != nullptr) {
    delete Client;
  }

  TRIAGENS_REST_SHUTDOWN;

  LocalExitFunction(ret, nullptr);

  return ret;
}
