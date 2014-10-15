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

#include "ArangoShell/ArangoClient.h"
#include "Basics/FileUtils.h"
#include "Basics/JsonHelper.h"
#include "Basics/ProgramOptions.h"
#include "Basics/ProgramOptionsDescription.h"
#include "Basics/StringUtils.h"
#include "Basics/files.h"
#include "Basics/init.h"
#include "Basics/logging.h"
#include "Basics/tri-strings.h"
#include "Basics/terminal-utils.h"
#include "Rest/Endpoint.h"
#include "Rest/InitialiseRest.h"
#include "Rest/HttpResponse.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

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

ArangoClient BaseClient;

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
/// @brief re-use revision ids on import
////////////////////////////////////////////////////////////////////////////////

static bool RecycleIds = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief continue restore even in the face of errors
////////////////////////////////////////////////////////////////////////////////

static bool Force = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief cluster mode flag
////////////////////////////////////////////////////////////////////////////////

static bool clusterMode = false;

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

static void arangorestoreEntryFunction ();
static void arangorestoreExitFunction (int, void*);

#ifdef _WIN32

// .............................................................................
// Call this function to do various initialisations for windows only
// .............................................................................
void arangorestoreEntryFunction () {
  int maxOpenFiles = 1024;
  int res = 0;

  // ...........................................................................
  // Uncomment this to call this for extended debug information.
  // If you familiar with valgrind ... then this is not like that, however
  // you do get some similar functionality.
  // ...........................................................................
  //res = initialiseWindows(TRI_WIN_INITIAL_SET_DEBUG_FLAG, 0);

  res = initialiseWindows(TRI_WIN_INITIAL_SET_INVALID_HANLE_HANDLER, 0);
  if (res != 0) {
    _exit(1);
  }

  res = initialiseWindows(TRI_WIN_INITIAL_SET_MAX_STD_IO,(const char*)(&maxOpenFiles));
  if (res != 0) {
    _exit(1);
  }

  res = initialiseWindows(TRI_WIN_INITIAL_WSASTARTUP_FUNCTION_CALL, 0);
  if (res != 0) {
    _exit(1);
  }

  TRI_Application_Exit_SetExit(arangorestoreExitFunction);

}

static void arangorestoreExitFunction (int exitCode, void* data) {
  int res = 0;
  // ...........................................................................
  // TODO: need a terminate function for windows to be called and cleanup
  // any windows specific stuff.
  // ...........................................................................

  res = finaliseWindows(TRI_WIN_FINAL_WSASTARTUP_FUNCTION_CALL, 0);

  if (res != 0) {
    exit(1);
  }

  exit(exitCode);
}
#else

static void arangorestoreEntryFunction () {
}

static void arangorestoreExitFunction (int exitCode, void* data) {
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief extract an error message from a response
////////////////////////////////////////////////////////////////////////////////

static string GetHttpErrorMessage (SimpleHttpResult* result) {
  const StringBuffer& body = result->getBody();
  string details;

  TRI_json_t* json = JsonHelper::fromString(body.c_str(), body.length());

  if (json != nullptr) {
    const string& errorMessage = JsonHelper::getStringValue(json, "errorMessage", "");
    const int errorNum = JsonHelper::getNumericValue<int>(json, "errorNum", 0);

    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

    if (errorMessage != "" && errorNum > 0) {
      details = ": ArangoError " + StringUtils::itoa(errorNum) + ": " + errorMessage;
    }
  }

  return "got error from server: HTTP " +
         StringUtils::itoa(result->getHttpReturnCode()) +
         " (" + result->getHttpReturnMessage() + ")" +
         details;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetch the version from the server
////////////////////////////////////////////////////////////////////////////////

static string GetArangoVersion () {
  map<string, string> headers;

  SimpleHttpResult* response = Client->request(HttpRequest::HTTP_REQUEST_GET,
                                               "/_api/version",
                                               nullptr,
                                               0,
                                               headers);

  if (response == nullptr || ! response->isComplete()) {
    if (response != nullptr) {
      delete response;
    }

    return "";
  }

  string version;

  if (response->getHttpReturnCode() == HttpResponse::OK) {
    // default value
    version = "arango";

    // convert response body to json
    TRI_json_t* json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE,
                                      response->getBody().c_str());

    if (json) {
      // look up "server" value
      const string server = JsonHelper::getStringValue(json, "server", "");

      // "server" value is a string and content is "arango"
      if (server == "arango") {
        // look up "version" value
        version = JsonHelper::getStringValue(json, "version", "");
      }

      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }
  }
  else {
    if (response->wasHttpError()) {
      Client->setErrorMessage(GetHttpErrorMessage(response), false);
    }

    Connection->disconnect();
  }

  delete response;

  return version;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if server is a coordinator of a cluster
////////////////////////////////////////////////////////////////////////////////

static bool GetArangoIsCluster () {
  map<string, string> headers;
  SimpleHttpResult* response = Client->request(HttpRequest::HTTP_REQUEST_GET,
                                        "/_admin/server/role",
                                        "",
                                        0,
                                        headers);

  if (response == nullptr || ! response->isComplete()) {
    if (response != nullptr) {
      delete response;
    }

    return false;
  }

  string role = "UNDEFINED";

  if (response->getHttpReturnCode() == HttpResponse::OK) {
    // convert response body to json
    TRI_json_t* json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE,
                                      response->getBody().c_str());

    if (json != nullptr) {
      // look up "server" value
      role = JsonHelper::getStringValue(json, "role", "UNDEFINED");

      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }
  }
  else {
    if (response->wasHttpError()) {
      Client->setErrorMessage(GetHttpErrorMessage(response), false);
    }

    Connection->disconnect();
  }

  delete response;

  return role == "COORDINATOR";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send the request to re-create a collection
////////////////////////////////////////////////////////////////////////////////

static int SendRestoreCollection (TRI_json_t const* json,
                                  string& errorMsg) {
  map<string, string> headers;

  const string url = "/_api/replication/restore-collection"
                     "?overwrite=" + string(Overwrite ? "true" : "false") +
                     "&recycleIds=" + string(RecycleIds ? "true" : "false") +
                     "&force=" + string(Force ? "true" : "false");

  const string body = JsonHelper::toString(json);

  SimpleHttpResult* response = Client->request(HttpRequest::HTTP_REQUEST_PUT,
                                               url,
                                               body.c_str(),
                                               body.size(),
                                               headers);

  if (response == nullptr || ! response->isComplete()) {
    errorMsg = "got invalid response from server: " + Client->getErrorMessage();

    if (response != nullptr) {
      delete response;
    }

    return TRI_ERROR_INTERNAL;
  }

  if (response->wasHttpError()) {
    errorMsg = GetHttpErrorMessage(response);
    delete response;

    return TRI_ERROR_INTERNAL;
  }

  delete response;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send the request to re-create indexes for a collection
////////////////////////////////////////////////////////////////////////////////

static int SendRestoreIndexes (TRI_json_t const* json,
                               string& errorMsg) {
  map<string, string> headers;

  const string url = "/_api/replication/restore-indexes?force=" + string(Force ? "true" : "false");
  const string body = JsonHelper::toString(json);

  SimpleHttpResult* response = Client->request(HttpRequest::HTTP_REQUEST_PUT,
                                               url,
                                               body.c_str(),
                                               body.size(),
                                               headers);

  if (response == nullptr || ! response->isComplete()) {
    errorMsg = "got invalid response from server: " + Client->getErrorMessage();

    if (response != nullptr) {
      delete response;
    }

    return TRI_ERROR_INTERNAL;
  }

  if (response->wasHttpError()) {
    errorMsg = GetHttpErrorMessage(response);
    delete response;

    return TRI_ERROR_INTERNAL;
  }

  delete response;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send the request to load data into a collection
////////////////////////////////////////////////////////////////////////////////

static int SendRestoreData (string const& cname,
                            char const* buffer,
                            size_t bufferSize,
                            string& errorMsg) {
  map<string, string> headers;

  const string url = "/_api/replication/restore-data?collection=" +
                     StringUtils::urlEncode(cname) +
                     "&recycleIds=" + (RecycleIds ? "true" : "false") +
                     "&force=" + (Force ? "true" : "false");

  SimpleHttpResult* response = Client->request(HttpRequest::HTTP_REQUEST_PUT,
                                               url,
                                               buffer,
                                               bufferSize,
                                               headers);


  if (response == nullptr || ! response->isComplete()) {
    errorMsg = "got invalid response from server: " + Client->getErrorMessage();

    if (response != nullptr) {
      delete response;
    }

    return TRI_ERROR_INTERNAL;
  }

  if (response->wasHttpError()) {
    errorMsg = GetHttpErrorMessage(response);
    delete response;

    return TRI_ERROR_INTERNAL;
  }

  delete response;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief comparator to sort collections
/// sort order is by collection type first (vertices before edges, this is
/// because edges depend on vertices being there), then name
////////////////////////////////////////////////////////////////////////////////

static int SortCollections (const void* l,
                            const void* r) {
  TRI_json_t const* left  = JsonHelper::getArrayElement((TRI_json_t const*) l, "parameters");
  TRI_json_t const* right = JsonHelper::getArrayElement((TRI_json_t const*) r, "parameters");

  int leftType  = JsonHelper::getNumericValue<int>(left,  "type", 0);
  int rightType = JsonHelper::getNumericValue<int>(right, "type", 0);

  if (leftType != rightType) {
    return leftType - rightType;
  }

  string leftName  = JsonHelper::getStringValue(left,  "name", "");
  string rightName = JsonHelper::getStringValue(right, "name", "");

  return strcasecmp(leftName.c_str(), rightName.c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process all files from the input directory
////////////////////////////////////////////////////////////////////////////////

static int ProcessInputDirectory (string& errorMsg) {

  // create a lookup table for collections
  map<string, bool> restrictList;
  for (size_t i = 0; i < Collections.size(); ++i) {
    restrictList.insert(pair<string, bool>(Collections[i], true));
  }

  TRI_json_t* collections = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);

  if (collections == nullptr) {
    errorMsg = "out of memory";
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // step1: determine all collections to process
  {
    const vector<string> files = FileUtils::listFiles(InputDirectory);
    const size_t n = files.size();

    // TODO: externalise file extension
    const string suffix = string(".structure.json");

    // loop over all files in InputDirectory, and look for all structure.json files
    for (size_t i = 0; i < n; ++i) {
      const size_t nameLength = files[i].size();

      if (nameLength <= suffix.size() ||
          files[i].substr(files[i].size() - suffix.size()) != suffix) {
        // some other file
        continue;
      }

      // found a structure.json file

      const string name = files[i].substr(0, files[i].size() - suffix.size());

      if (name[0] == '_' && ! IncludeSystemCollections) {
        continue;
      }

      if (restrictList.size() > 0 &&
          restrictList.find(name) == restrictList.end()) {
        // collection name not in list
        continue;
      }

      const string fqn = InputDirectory + TRI_DIR_SEPARATOR_STR + files[i];

      TRI_json_t* json = TRI_JsonFile(TRI_UNKNOWN_MEM_ZONE, fqn.c_str(), 0);
      TRI_json_t const* parameters = JsonHelper::getArrayElement(json, "parameters");
      TRI_json_t const* indexes = JsonHelper::getArrayElement(json, "indexes");

      if (! JsonHelper::isArray(json) ||
          ! JsonHelper::isArray(parameters) ||
          ! JsonHelper::isList(indexes)) {
        errorMsg = "could not read collection structure file '" + name + "'";

        if (json != nullptr) {
          TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
        }

        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, collections);

        return TRI_ERROR_INTERNAL;
      }

      const string cname = JsonHelper::getStringValue(parameters, "name", "");

      if (cname != name) {
        // file has a different name than found in structure file

        if (ImportStructure) {
          // we cannot go on if there is a mismatch
          errorMsg = "collection name mismatch in collection structure file '" + name + "' (offending value: '" + cname + "')";
          TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
          TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, collections);

          return TRI_ERROR_INTERNAL;
        }
        else {
          // we can patch the name in our array and go on
          cout << "ignoring collection name mismatch in collection structure file '" + name + "' (offending value: '" + cname + "')" << endl;

          TRI_json_t* nameAttribute = TRI_LookupArrayJson(parameters, "name");

          if (TRI_IsStringJson(nameAttribute)) {
            char* old = nameAttribute->_value._string.data;

            // file name wins over "name" attribute value
            nameAttribute->_value._string.data = TRI_DuplicateStringZ(TRI_UNKNOWN_MEM_ZONE, name.c_str());
            nameAttribute->_value._string.length = (uint32_t) name.size() + 1; // + NUL byte

            TRI_Free(TRI_UNKNOWN_MEM_ZONE, old);
          }
        }
      }

      TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, collections, json);
    }
  }

  // sort collections according to type (documents before edges)
  qsort(collections->_value._objects._buffer, collections->_value._objects._length, sizeof(TRI_json_t), &SortCollections);

  StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE);

  // step2: run the actual import
  {
    const size_t n = collections->_value._objects._length;
    for (size_t i = 0; i < n; ++i) {
      TRI_json_t const* json = (TRI_json_t const*) TRI_AtVector(&collections->_value._objects, i);
      TRI_json_t const* parameters = JsonHelper::getArrayElement(json, "parameters");
      TRI_json_t const* indexes = JsonHelper::getArrayElement(json, "indexes");
      const string cname = JsonHelper::getStringValue(parameters, "name", "");
      const string cid   = JsonHelper::getStringValue(parameters, "cid", "");

      if (ImportStructure) {
        // re-create collection
        if (Progress) {
          if (Overwrite) {
            cout << "Re-creating collection '" << cname << "'..." << endl;
          }
          else {
            cout << "Creating collection '" << cname << "'..." << endl;
          }
        }

        int res = SendRestoreCollection(json, errorMsg);

        if (res != TRI_ERROR_NO_ERROR) {
          if (Force) {
            cerr << errorMsg << endl;
            continue;
          }

          TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, collections);

          return TRI_ERROR_INTERNAL;
        }
      }

      Stats._totalCollections++;

      if (ImportData) {
        // import data. check if we have a datafile
        // TODO: externalise file extension
        const string datafile = InputDirectory + TRI_DIR_SEPARATOR_STR + cname + ".data.json";

        if (TRI_ExistsFile(datafile.c_str())) {
          // found a datafile

          if (Progress) {
            cout << "Loading data into collection '" << cname << "'..." << endl;
          }

          int fd = TRI_OPEN(datafile.c_str(), O_RDONLY);

          if (fd < 0) {
            errorMsg = "cannot open collection data file '" + datafile + "'";
            TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, collections);

            return TRI_ERROR_INTERNAL;
          }

          buffer.clear();

          while (true) {
            if (buffer.reserve(16384) != TRI_ERROR_NO_ERROR) {
              TRI_CLOSE(fd);
              errorMsg = "out of memory";
              TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, collections);

              return TRI_ERROR_OUT_OF_MEMORY;
            }

            ssize_t numRead = TRI_READ(fd, buffer.end(), 16384);

            if (numRead < 0) {
              // error while reading
              int res = TRI_errno();
              TRI_CLOSE(fd);
              errorMsg = string(TRI_errno_string(res));
              TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, collections);

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

                TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, collections);
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

        if (TRI_LengthVector(&indexes->_value._objects) > 0) {
          // we actually have indexes
          if (Progress) {
            cout << "Creating indexes for collection '" << cname << "'..." << endl;
          }

          int res = SendRestoreIndexes(json, errorMsg);

          if (res != TRI_ERROR_NO_ERROR) {
            if (Force) {
              cerr << errorMsg << endl;
              continue;
            }

            TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, collections);

            return TRI_ERROR_INTERNAL;
          }
        }
      }
    }
  }

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, collections);

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

  arangorestoreEntryFunction();

  TRIAGENS_C_INITIALISE(argc, argv);
  TRIAGENS_REST_INITIALISE(argc, argv);

  TRI_InitialiseLogging(false);

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

  // .............................................................................
  // check input directory
  // .............................................................................

  if (InputDirectory == "" || ! TRI_IsDirectory(InputDirectory.c_str())) {
    cerr << "input directory '" << InputDirectory << "' does not exist" << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  if (! ImportStructure && ! ImportData) {
    cerr << "must specify either --create-collection or --import-data" << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  // .............................................................................
  // set-up client connection
  // .............................................................................

  BaseClient.createEndpoint();

  if (BaseClient.endpointServer() == nullptr) {
    cerr << "invalid value for --server.endpoint ('" << BaseClient.endpointString() << "')" << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  Connection = GeneralClientConnection::factory(BaseClient.endpointServer(),
                                                BaseClient.requestTimeout(),
                                                BaseClient.connectTimeout(),
                                                ArangoClient::DEFAULT_RETRIES,
                                                BaseClient.sslProtocol());

  if (Connection == nullptr) {
    cerr << "out of memory" << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  Client = new SimpleHttpClient(Connection, BaseClient.requestTimeout(), false);

  if (Client == nullptr) {
    cerr << "out of memory" << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  Client->setLocationRewriter(0, &rewriteLocation);
  Client->setUserNamePassword("/", BaseClient.username(), BaseClient.password());

  const string versionString = GetArangoVersion();

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
    cerr << "invalid server version '" << versionString << "'" << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  if (major < 1 ||
      major > 2 ||
      (major == 1 && minor < 4)) {
    // we can connect to 1.4, 2.0 and higher only
    cerr << "got incompatible server version '" << versionString << "'" << endl;
    if (! Force) {
      TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
    }
  }

  if (major >= 2) {
    // Version 1.4 did not yet have a cluster mode
    clusterMode = GetArangoIsCluster();
  }

  if (Progress) {
    cout << "Connected to ArangoDB '" << BaseClient.endpointServer()->getSpecification() << endl;
  }

  memset(&Stats, 0, sizeof(Stats));

  string errorMsg = "";

  int res;
  try {
    res = ProcessInputDirectory(errorMsg);
  }
  catch (std::exception const& ex) {
    cerr << "caught exception " << ex.what() << endl;
    res = TRI_ERROR_INTERNAL;
  }
  catch (...) {
    cerr << "caught unknown exception" << endl;
    res = TRI_ERROR_INTERNAL;
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


  if (res != TRI_ERROR_NO_ERROR) {
    cerr << errorMsg << endl;
    ret = EXIT_FAILURE;
  }

  if (Client != nullptr) {
    delete Client;
  }

  TRIAGENS_REST_SHUTDOWN;

  arangorestoreExitFunction(ret, nullptr);

  return ret;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
