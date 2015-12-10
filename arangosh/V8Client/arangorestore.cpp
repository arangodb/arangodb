////////////////////////////////////////////////////////////////////////////////
/// @brief arango restore tool
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"
#include <iostream>

#include "ArangoShell/ArangoClient.h"
#include "Basics/files.h"
#include "Basics/FileUtils.h"
#include "Basics/init.h"
#include "Basics/logging.h"
#include "Basics/ProgramOptions.h"
#include "Basics/ProgramOptionsDescription.h"
#include "Basics/StringUtils.h"
#include "Basics/terminal-utils.h"
#include "Basics/tri-strings.h"
#include "Basics/VelocyPackHelper.h"
#include "Rest/Endpoint.h"
#include "Rest/InitializeRest.h"
#include "Rest/HttpResponse.h"
#include "Rest/SslInterface.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

#include <velocypack/Options.h>
#include <velocypack/velocypack-aliases.h>
using namespace std;
using namespace triagens::basics;
using namespace triagens::httpclient;
using namespace triagens::rest;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief base class for clients
////////////////////////////////////////////////////////////////////////////////

ArangoClient BaseClient("arangorestore");

////////////////////////////////////////////////////////////////////////////////
/// @brief the initial default connection
////////////////////////////////////////////////////////////////////////////////

triagens::httpclient::GeneralClientConnection* Connection = nullptr;

////////////////////////////////////////////////////////////////////////////////
/// @brief HTTP client
////////////////////////////////////////////////////////////////////////////////

triagens::httpclient::SimpleHttpClient* Client = nullptr;

////////////////////////////////////////////////////////////////////////////////
/// @brief chunk size
////////////////////////////////////////////////////////////////////////////////

static uint64_t ChunkSize = 1024 * 1024 * 8;

////////////////////////////////////////////////////////////////////////////////
/// @brief collections
////////////////////////////////////////////////////////////////////////////////

static vector<string> Collections;

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

static string InputDirectory;

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
/// @brief statistics
////////////////////////////////////////////////////////////////////////////////

static struct {
  uint64_t _totalBatches;
  uint64_t _totalCollections;
  uint64_t _totalRead;
}
Stats;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the program options
////////////////////////////////////////////////////////////////////////////////

static void ParseProgramOptions (int argc, char* argv[]) {
  ProgramOptionsDescription description("STANDARD options");

  description
    ("collection", &Collections, "restrict to collection name (can be specified multiple times)")
    ("create-database", &CreateDatabase, "create the target database if it does not exist")
    ("batch-size", &ChunkSize, "maximum size for individual data batches (in bytes)")
    ("import-data", &ImportData, "import data into collection")
    ("recycle-ids", &RecycleIds, "recycle collection and revision ids from dump")
    ("force", &Force, "continue restore even in the face of some server-side errors")
    ("create-collection", &ImportStructure, "create collection structure")
    ("include-system-collections", &IncludeSystemCollections, "include system collections")
    ("input-directory", &InputDirectory, "input directory")
    ("overwrite", &Overwrite, "overwrite collections if they exist")
    ("progress", &Progress, "show progress")
  ;

  BaseClient.setupGeneral(description);
  BaseClient.setupServer(description);

  vector<string> arguments;
  description.arguments(&arguments);

  ProgramOptions options;
  BaseClient.parse(options, description, "", argc, argv, "arangorestore.conf");

  if (1 == arguments.size()) {
    InputDirectory = arguments[0];
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief startup and exit functions
////////////////////////////////////////////////////////////////////////////////

static void LocalEntryFunction ();
static void LocalExitFunction (int, void*);

#ifdef _WIN32

// .............................................................................
// Call this function to do various initializations for windows only
// .............................................................................

static void LocalEntryFunction () {
  int maxOpenFiles = 1024;
  int res = 0;

  // ...........................................................................
  // Uncomment this to call this for extended debug information.
  // If you familiar with valgrind ... then this is not like that, however
  // you do get some similar functionality.
  // ...........................................................................
  //res = initializeWindows(TRI_WIN_INITIAL_SET_DEBUG_FLAG, 0);

  res = initializeWindows(TRI_WIN_INITIAL_SET_INVALID_HANLE_HANDLER, 0);
  if (res != 0) {
    _exit(1);
  }

  res = initializeWindows(TRI_WIN_INITIAL_SET_MAX_STD_IO,(const char*)(&maxOpenFiles));
  if (res != 0) {
    _exit(1);
  }

  res = initializeWindows(TRI_WIN_INITIAL_WSASTARTUP_FUNCTION_CALL, 0);
  if (res != 0) {
    _exit(1);
  }

  TRI_Application_Exit_SetExit(LocalExitFunction);

}

static void LocalExitFunction (int exitCode, void* data) {
  int res = 0;
  // ...........................................................................
  // TODO: need a terminate function for windows to be called and cleanup
  // any windows specific stuff.
  // ...........................................................................

  res = finalizeWindows(TRI_WIN_FINAL_WSASTARTUP_FUNCTION_CALL, 0);

  if (res != 0) {
    exit(1);
  }

  exit(exitCode);
}
#else

static void LocalEntryFunction () {
}

static void LocalExitFunction (int exitCode, void* data) {
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief extract an error message from a response
////////////////////////////////////////////////////////////////////////////////

static std::string GetHttpErrorMessage (SimpleHttpResult* result) {
  LastErrorCode = TRI_ERROR_NO_ERROR;
  std::string details;
  try {
    std::shared_ptr<VPackBuilder> parsedBody = result->getBodyVelocyPack();
    VPackSlice const body = parsedBody->slice();

    std::string const& errorMessage = triagens::basics::VelocyPackHelper::getStringValue(body, "errorMessage", "");
    int const errorNum = triagens::basics::VelocyPackHelper::getNumericValue<int>(body, "errorNum", 0);
    if (errorMessage != "" && errorNum > 0) {
      details = ": ArangoError " + StringUtils::itoa(errorNum) + ": " + errorMessage;
      LastErrorCode = errorNum;
    }
  }
  catch (...) {
    // No action
  }
  return "got error from server: HTTP " +
         StringUtils::itoa(result->getHttpReturnCode()) +
         " (" + result->getHttpReturnMessage() + ")" +
         details;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief try to create a database on the server
////////////////////////////////////////////////////////////////////////////////

static int TryCreateDatabase (std::string const& name) {
  triagens::basics::Json json(triagens::basics::Json::Object);
  json("name", triagens::basics::Json(name));

  triagens::basics::Json user(triagens::basics::Json::Object);
  user("username", triagens::basics::Json(BaseClient.username()));
  user("passwd", triagens::basics::Json(BaseClient.password()));

  triagens::basics::Json users(triagens::basics::Json::Array);
  users.add(user);
  json("users", users);
                                               
  std::string const body(triagens::basics::JsonHelper::toString(json.json()));

  std::unique_ptr<SimpleHttpResult> response(Client->request(HttpRequest::HTTP_REQUEST_POST,
                                               "/_api/database",
                                               body.c_str(),
                                               body.size()));

  if (response == nullptr || ! response->isComplete()) {
    return TRI_ERROR_INTERNAL;
  }

  auto returnCode = response->getHttpReturnCode();

  if (returnCode == HttpResponse::OK ||
      returnCode == HttpResponse::CREATED) {
    // all ok
    return TRI_ERROR_NO_ERROR;
  }
  else if (returnCode == HttpResponse::UNAUTHORIZED ||
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

static string GetArangoVersion () {
  std::unique_ptr<SimpleHttpResult> response(Client->request(HttpRequest::HTTP_REQUEST_GET,
                                               "/_api/version",
                                               nullptr,
                                               0));

  if (response == nullptr || ! response->isComplete()) {
    return "";
  }

  string version;

  if (response->getHttpReturnCode() == HttpResponse::OK) {
    // default value
    version = "arango";

    try {
      // convert response body to VPack
      std::shared_ptr<VPackBuilder> parsedBody = response->getBodyVelocyPack();
      VPackSlice const body = parsedBody->slice();

      // look up "server" value
      std::string const server = triagens::basics::VelocyPackHelper::getStringValue(body, "server", "");

      // "server" value is a string and content is "arango"
      if (server == "arango") {
        // look up "version" value
        version = triagens::basics::VelocyPackHelper::getStringValue(body, "version", "");
      }
    }
    catch (...) {
      // No action
    }
  }
  else {
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

static bool GetArangoIsCluster () {
  std::unique_ptr<SimpleHttpResult> response(Client->request(HttpRequest::HTTP_REQUEST_GET,
                                        "/_admin/server/role",
                                        "",
                                        0));

  if (response == nullptr || ! response->isComplete()) {
    return false;
  }

  string role = "UNDEFINED";

  if (response->getHttpReturnCode() == HttpResponse::OK) {
    // convert response body to json
    try {
      std::shared_ptr<VPackBuilder> parsedBody = response->getBodyVelocyPack();
      VPackSlice const body = parsedBody->slice();
      role = triagens::basics::VelocyPackHelper::getStringValue(body, "role", "UNDEFINED");
    }
    catch (...) {
      // No action
    }
  }
  else {
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

static int SendRestoreCollection (VPackSlice const& slice,
                                  string& errorMsg) {
  std::string const url = "/_api/replication/restore-collection"
                          "?overwrite=" + string(Overwrite ? "true" : "false") +
                          "&recycleIds=" + string(RecycleIds ? "true" : "false") +
                          "&force=" + string(Force ? "true" : "false");

  std::string const body = slice.toJson();

  std::unique_ptr<SimpleHttpResult> response(Client->request(HttpRequest::HTTP_REQUEST_PUT,
                                               url,
                                               body.c_str(),
                                               body.size()));

  if (response == nullptr || ! response->isComplete()) {
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

static int SendRestoreIndexes (VPackSlice const& slice,
                               string& errorMsg) {
  std::string const url = "/_api/replication/restore-indexes?force=" + string(Force ? "true" : "false");
  std::string const body = slice.toJson();

  std::unique_ptr<SimpleHttpResult> response(Client->request(HttpRequest::HTTP_REQUEST_PUT,
                                               url,
                                               body.c_str(),
                                               body.size()));

  if (response == nullptr || ! response->isComplete()) {
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

static int SendRestoreData (string const& cname,
                            char const* buffer,
                            size_t bufferSize,
                            string& errorMsg) {
  std::string const url = "/_api/replication/restore-data?collection=" +
                          StringUtils::urlEncode(cname) +
                          "&recycleIds=" + (RecycleIds ? "true" : "false") +
                          "&force=" + (Force ? "true" : "false");

  std::unique_ptr<SimpleHttpResult> response(Client->request(HttpRequest::HTTP_REQUEST_PUT,
                                               url,
                                               buffer,
                                               bufferSize));

  if (response == nullptr || ! response->isComplete()) {
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

static bool SortCollections (VPackSlice const& l,
                             VPackSlice const& r) {
  VPackSlice const& left  = l.get("parameters");
  VPackSlice const& right = r.get("parameters");

  int leftType  = triagens::basics::VelocyPackHelper::getNumericValue<int>(left,  "type", 0);
  int rightType = triagens::basics::VelocyPackHelper::getNumericValue<int>(right, "type", 0);

  if (leftType != rightType) {
    return leftType < rightType;
  }

  string leftName  = triagens::basics::VelocyPackHelper::getStringValue(left,  "name", "");
  string rightName = triagens::basics::VelocyPackHelper::getStringValue(right, "name", "");

  return strcasecmp(leftName.c_str(), rightName.c_str()) < 0;
}

////////////////////////////////////////////////////////////////////////////////
///// @brief parses a json file to VelocyPack
//////////////////////////////////////////////////////////////////////////////////

static std::shared_ptr<VPackBuilder> readVelocyPackFile (std::string path) {
  size_t length;
  char* content = TRI_SlurpFile(TRI_UNKNOWN_MEM_ZONE, path.c_str(), &length);
  // The Parser might THROW
  return VPackParser::fromJson(reinterpret_cast<uint8_t const*>(content), length);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process all files from the input directory
////////////////////////////////////////////////////////////////////////////////

static int ProcessInputDirectory (std::string& errorMsg) {
  // create a lookup table for collections
  map<string, bool> restrictList;
  for (size_t i = 0; i < Collections.size(); ++i) {
    restrictList.insert(pair<string, bool>(Collections[i], true));
  }
  try {

    std::vector<std::string> const files = FileUtils::listFiles(InputDirectory);
    std::string const suffix = std::string(".structure.json");
    std::vector<std::shared_ptr<VPackBuilder>> collectionBuilders;
    std::vector<VPackSlice> collections;

    // Step 1 determine all collections to process
    {
      // loop over all files in InputDirectory, and look for all structure.json files
      for (std::string const& file : files) {
        size_t const nameLength = file.size();

        if (nameLength <= suffix.size() ||
            file.substr(file.size() - suffix.size()) != suffix) {
          // some other file
          continue;
        }

        // found a structure.json file
        std::string name = file.substr(0, file.size() - suffix.size());

        if (! IncludeSystemCollections && name[0] == '_') {
          continue;
        }

        const string fqn = InputDirectory + TRI_DIR_SEPARATOR_STR + file;
        std::shared_ptr<VPackBuilder> fileContentBuilder = readVelocyPackFile(fqn);
        VPackSlice const fileContent = fileContentBuilder->slice();

        if (! fileContent.isObject()) {
          errorMsg = "could not read collection structure file '" + fqn + "'";
          return TRI_ERROR_INTERNAL;
        }

        VPackSlice const parameters = fileContent.get("parameters");
        VPackSlice const indexes = fileContent.get("indexes");

        if (! parameters.isObject() ||
            ! indexes.isArray()) {
          errorMsg = "could not read collection structure file '" + fqn + "'";
          return TRI_ERROR_INTERNAL;
        }

        std::string const cname = triagens::basics::VelocyPackHelper::getStringValue(parameters, "name", "");

        bool overwriteName = false;

        if (cname != name && name != (cname + "_" + triagens::rest::SslInterface::sslMD5(cname))) {
          // file has a different name than found in structure file
          if (ImportStructure) {
            // we cannot go on if there is a mismatch
            errorMsg = "collection name mismatch in collection structure file '" + fqn + "' (offending value: '" + cname + "')";
            return TRI_ERROR_INTERNAL;
          }
          else {
            // we can patch the name in our array and go on
            std::cout << "ignoring collection name mismatch in collection structure file '" + fqn + "' (offending value: '" + cname + "')" << std::endl;

            overwriteName = true;
           }
        }

        if (! restrictList.empty() &&
            restrictList.find(cname) == restrictList.end()) {
          // collection name not in list
          continue;
        }

        if (overwriteName) {
          // TODO
          // Situation:
          // Ich habe ein Json-Object von Datei (teile des Inhalts im Zweifel unbekannt)
          // Es gibt ein Sub-Json-Object "parameters" mit einem Attribute "name" der gesetzt ist.
          // Ich muss nur diesen namen überschreiben, der Rest soll identisch bleiben.
        }
        else {
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
      std::string const cname = triagens::basics::VelocyPackHelper::getStringValue(parameters, "name", "");
      std::string const cid   = triagens::basics::VelocyPackHelper::getStringValue(parameters, "cid", "");
      int type = triagens::basics::VelocyPackHelper::getNumericValue<int>(parameters, "type", 2);
      std::string const collectionType(type == 2 ? "document" : "edge");

      if (ImportStructure) {
        // re-create collection
        if (Progress) {
          if (Overwrite) {
            cout << "# Re-creating " << collectionType << " collection '" << cname << "'..." << endl;
          }
          else {
            cout << "# Creating " << collectionType << " collection '" << cname << "'..." << endl;
          }
        }
        int res = SendRestoreCollection(collection, errorMsg);

        if (res != TRI_ERROR_NO_ERROR) {
          if (Force) {
            cerr << errorMsg << endl;
            continue;
          }
          return TRI_ERROR_INTERNAL;
        }
      }
      Stats._totalCollections++;

      if (ImportData) {
        // import data. check if we have a datafile
        std::string datafile = InputDirectory + TRI_DIR_SEPARATOR_STR + cname + "_" + triagens::rest::SslInterface::sslMD5(cname) + ".data.json";
        if (! TRI_ExistsFile(datafile.c_str())) {
          datafile = InputDirectory + TRI_DIR_SEPARATOR_STR + cname + ".data.json";
        }

        if (TRI_ExistsFile(datafile.c_str())) {
          // found a datafile

          if (Progress) {
            cout << "# Loading data into " << collectionType << " collection '" << cname << "'..." << endl;
          }

          int fd = TRI_OPEN(datafile.c_str(), O_RDONLY);

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
              errorMsg = string(TRI_errno_string(res));

              return res;
            }

            // read something
            buffer.increaseLength(numRead);

            Stats._totalRead += (uint64_t) numRead;

            if (buffer.length() < ChunkSize && numRead > 0) {
              // still continue reading
              continue;
            }

            // do we have a buffer?
            if (buffer.length() > 0) {
              // look for the last \n in the buffer
              char* found = (char*) memrchr((const void*) buffer.begin(), '\n', buffer.length());
              size_t length;

              if (found == nullptr) {
                // no \n found...
                if (numRead == 0) {
                  // we're at the end. send the complete buffer anyway
                  length = buffer.length();
                }
                else {
                  // read more
                  continue;
                }
              }
              else {
                // found a \n somewhere
                length = found - buffer.begin();
              }

              TRI_ASSERT(length > 0);

              Stats._totalBatches++;

              int res = SendRestoreData(cname, buffer.begin(), length, errorMsg);

              if (res != TRI_ERROR_NO_ERROR) {
                TRI_CLOSE(fd);
                if (errorMsg.empty()) {
                  errorMsg = string(TRI_errno_string(res));
                }
                else {
                  errorMsg = string(TRI_errno_string(res)) + ": " + errorMsg;
                }

                if (Force) {
                  cerr << errorMsg << endl;
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
            cout << "# Creating indexes for collection '" << cname << "'..." << endl;
          }

          int res = SendRestoreIndexes(collection, errorMsg);

          if (res != TRI_ERROR_NO_ERROR) {
            if (Force) {
              cerr << errorMsg << endl;
              continue;
            }
            return TRI_ERROR_INTERNAL;
          }
        }
      }
    }
  }
  catch (...) {
    errorMsg = "out of memory";
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief request location rewriter (injects database name)
////////////////////////////////////////////////////////////////////////////////

static string rewriteLocation (void* data, const string& location) {
  if (location.substr(0, 5) == "/_db/") {
    // location already contains /_db/
    return location;
  }

  if (location[0] == '/') {
    return "/_db/" + BaseClient.databaseName() + location;
  }
  else {
    return "/_db/" + BaseClient.databaseName() + "/" + location;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief main
////////////////////////////////////////////////////////////////////////////////

int main (int argc, char* argv[]) {
  int ret = EXIT_SUCCESS;

  LocalEntryFunction();

  TRIAGENS_C_INITIALIZE(argc, argv);
  TRIAGENS_REST_INITIALIZE(argc, argv);

  TRI_InitializeLogging(false);

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

  if (! InputDirectory.empty() &&
      InputDirectory.back() == TRI_DIR_SEPARATOR_CHAR) {
    // trim trailing slash from path because it may cause problems on ... Windows
    TRI_ASSERT(InputDirectory.size() > 0);
    InputDirectory.pop_back();
  }

  // .............................................................................
  // check input directory
  // .............................................................................

  if (InputDirectory == "" || ! TRI_IsDirectory(InputDirectory.c_str())) {
    cerr << "Error: input directory '" << InputDirectory << "' does not exist" << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  if (! ImportStructure && ! ImportData) {
    cerr << "Error: must specify either --create-collection or --import-data" << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  // .............................................................................
  // set-up client connection
  // .............................................................................

  BaseClient.createEndpoint();

  if (BaseClient.endpointServer() == nullptr) {
    cerr << "Error: invalid value for --server.endpoint ('" << BaseClient.endpointString() << "')" << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  Connection = GeneralClientConnection::factory(BaseClient.endpointServer(),
                                                BaseClient.requestTimeout(),
                                                BaseClient.connectTimeout(),
                                                ArangoClient::DEFAULT_RETRIES,
                                                BaseClient.sslProtocol());

  Client = new SimpleHttpClient(Connection, BaseClient.requestTimeout(), false);

  Client->setLocationRewriter(nullptr, &rewriteLocation);
  Client->setUserNamePassword("/", BaseClient.username(), BaseClient.password());

  string versionString = GetArangoVersion();
    
  if (CreateDatabase && LastErrorCode == TRI_ERROR_ARANGO_DATABASE_NOT_FOUND) {
    // database not found, but database creation requested
   
    std::string old = BaseClient.databaseName();
    cout << "Creating database '" << old << "'" << endl;

    BaseClient.setDatabaseName("_system");

    int res = TryCreateDatabase(old);

    if (res != TRI_ERROR_NO_ERROR) {
      cerr << "Could not create database '" << old << "'" << endl;
      cerr << "Error message: '" << Client->getErrorMessage() << "'" << endl;
      TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
    }

    // restore old database name
    BaseClient.setDatabaseName(old);

    // re-fetch version
    versionString = GetArangoVersion();
  }

  if (! Connection->isConnected()) {
    cerr << "Could not connect to endpoint " << BaseClient.endpointServer()->getSpecification() << endl;
    cerr << "Error message: '" << Client->getErrorMessage() << "'" << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  // successfully connected
  cout << "Server version: " << versionString << endl;

  // validate server version
  int major = 0;
  int minor = 0;

  if (sscanf(versionString.c_str(), "%d.%d", &major, &minor) != 2) {
    cerr << "Error: invalid server version '" << versionString << "'" << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  if (major < 1 ||
      major > 2 ||
      (major == 1 && minor < 4)) {
    // we can connect to 1.4, 2.0 and higher only
    cerr << "Error: got incompatible server version '" << versionString << "'" << endl;
    if (! Force) {
      TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
    }
  }

  if (major >= 2) {
    // Version 1.4 did not yet have a cluster mode
    ClusterMode = GetArangoIsCluster();
  }

  if (Progress) {
    cout << "# Connected to ArangoDB '" << BaseClient.endpointServer()->getSpecification() << "'" << endl;
  }

  memset(&Stats, 0, sizeof(Stats));

  string errorMsg = "";

  int res;
  try {
    res = ProcessInputDirectory(errorMsg);
  }
  catch (std::exception const& ex) {
    cerr << "Error: caught exception " << ex.what() << endl;
    res = TRI_ERROR_INTERNAL;
  }
  catch (...) {
    cerr << "Error: caught unknown exception" << endl;
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    if (! errorMsg.empty()) {
      cerr << "Error: " << errorMsg << endl;
    }
    else {
      cerr << "An error occurred" << endl;
    }
    ret = EXIT_FAILURE;
  }


  if (Progress) {
    if (ImportData) {
      cout << "Processed " << Stats._totalCollections << " collection(s), " <<
              "read " << Stats._totalRead << " byte(s) from datafiles, " <<
              "sent " << Stats._totalBatches << " batch(es)" << endl;
    }
    else if (ImportStructure) {
      cout << "Processed " << Stats._totalCollections << " collection(s)" << endl;
    }
  }

  if (Client != nullptr) {
    delete Client;
  }

  TRIAGENS_REST_SHUTDOWN;

  LocalExitFunction(ret, nullptr);

  return ret;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
