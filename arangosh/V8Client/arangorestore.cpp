////////////////////////////////////////////////////////////////////////////////
/// @brief arango restore tool
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "BasicsC/common.h"

#include <stdio.h>
#include <fstream>

#include "ArangoShell/ArangoClient.h"
#include "Basics/FileUtils.h"
#include "Basics/JsonHelper.h"
#include "Basics/ProgramOptions.h"
#include "Basics/ProgramOptionsDescription.h"
#include "Basics/StringUtils.h"
#include "BasicsC/files.h"
#include "BasicsC/init.h"
#include "BasicsC/logging.h"
#include "BasicsC/tri-strings.h"
#include "BasicsC/terminal-utils.h"
#include "Logger/Logger.h"
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
/// @addtogroup Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief base class for clients
////////////////////////////////////////////////////////////////////////////////

ArangoClient BaseClient;

////////////////////////////////////////////////////////////////////////////////
/// @brief the initial default connection
////////////////////////////////////////////////////////////////////////////////

triagens::httpclient::GeneralClientConnection* Connection = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief HTTP client
////////////////////////////////////////////////////////////////////////////////
      
triagens::httpclient::SimpleHttpClient* Client = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief chunk size
////////////////////////////////////////////////////////////////////////////////

static uint64_t ChunkSize = 1024 * 1024 * 4;

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

static bool Progress = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief overwrite collections if they exist
////////////////////////////////////////////////////////////////////////////////

static bool Overwrite = false;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the program options
////////////////////////////////////////////////////////////////////////////////

static void ParseProgramOptions (int argc, char* argv[]) {
  ProgramOptionsDescription description("STANDARD options");

  description
    ("collection", &Collections, "restrict to collection name (can be specified multiple times)")
    ("batch-size", &ChunkSize, "size for individual data batches (in bytes)")
    ("import-data", &ImportData, "import data into collection")
    ("create-collection", &ImportStructure, "create collection structure")
    ("include-system-collections", &IncludeSystemCollections, "include system collections")
    ("input-directory", &InputDirectory, "output directory")
    ("overwrite", &Overwrite, "overwrite collections if they exist")
    ("progress", &Progress, "show progress")
  ;

  BaseClient.setupGeneral(description);
  BaseClient.setupServer(description);

  vector<string> arguments;
  description.arguments(&arguments);

  ProgramOptions options;
  BaseClient.parse(options, description, argc, argv, "arangorestore.conf");
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief startup and exit functions
////////////////////////////////////////////////////////////////////////////////

static void arangorestoreEntryFunction ();
static void arangorestoreExitFunction (int, void*);

#ifdef _WIN32

// .............................................................................
// Call this function to do various initialistions for windows only
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

  TRI_Application_Exit_SetExit(arangoimpExitFunction);

}

static void arangorestoreExitFunction (int exitCode, void* data) {
  int res = 0;
  // ...........................................................................
  // TODO: need a terminate function for windows to be called and cleanup
  // any windows specific stuff.
  // ...........................................................................

  res = finaliseWindows(TRI_WIN_FINAL_WSASTARTUP_FUNCTION_CALL, 0);

  if (res != 0) {
    _exit(1);
  }

  _exit(exitCode);
}
#else

static void arangorestoreEntryFunction () {
}

static void arangorestoreExitFunction (int exitCode, void* data) {
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief fetch the version from the server
////////////////////////////////////////////////////////////////////////////////

static string GetVersion () {
  map<string, string> headers;

  SimpleHttpResult* response = Client->request(HttpRequest::HTTP_REQUEST_GET, 
                                               "/_api/version",
                                               0, 
                                               0,  
                                               headers); 

  if (response == 0 || ! response->isComplete()) {
    if (response != 0) {
      delete response;
    }

    return "";
  }

  string version;
    
  if (response->getHttpReturnCode() == HttpResponse::OK) {
    // default value
    version = "arango";
  
    // convert response body to json
    TRI_json_t* json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, response->getBody().str().c_str());

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

  delete response;

  return version;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send the request to re-create a single collection
////////////////////////////////////////////////////////////////////////////////

static int SendRestoreCollection (TRI_json_t* const json, 
                                  string& errorMsg) {
  map<string, string> headers;

  const string url = "/_api/replication/restore-collection?overwrite=" + 
                     string(Overwrite ? "true" : "false");

  const string body = JsonHelper::toString(json);

  SimpleHttpResult* response = Client->request(HttpRequest::HTTP_REQUEST_PUT, 
                                               url,
                                               body.c_str(), 
                                               body.size(),  
                                               headers); 

  if (response == 0 || ! response->isComplete()) {
    errorMsg = "got invalid response from server: " + Client->getErrorMessage();

    if (response != 0) {
      delete response;
    }

    return TRI_ERROR_INTERNAL;
  }

  if (response->wasHttpError()) {
    // TODO: include "real" error message
    errorMsg = "got invalid response from server: HTTP " + 
               StringUtils::itoa(response->getHttpReturnCode()) + 
               ": " + response->getHttpReturnMessage();
      
    delete response;

    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
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

    const string fqn  = InputDirectory + TRI_DIR_SEPARATOR_STR + files[i];

    TRI_json_t* json = TRI_JsonFile(TRI_UNKNOWN_MEM_ZONE, fqn.c_str(), 0);

    if (! JsonHelper::isArray(json)) {
      errorMsg = "could not read collection structure file '" + name + "'";

      if (json != 0) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      }

      return TRI_ERROR_INTERNAL;
    }

    TRI_json_t const* parameters = JsonHelper::getArrayElement(json, "parameters");

    if (! JsonHelper::isArray(parameters)) {
      errorMsg = "invalid collection structure file '" + name + "'";
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

      return TRI_ERROR_INTERNAL;
    }

    const string cid   = JsonHelper::getStringValue(parameters, "cid", "");
    const string cname = JsonHelper::getStringValue(parameters, "name", "");

    if (cname != name) {
      errorMsg = "collection name mismatch in collection structure file '" + name + "'";
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

      return TRI_ERROR_INTERNAL;
    }

    // TODO: remove this line
    cout << "FOUND FILE " << files[i] << "\n";

    if (ImportStructure) {
      // re-create collection
      int res = SendRestoreCollection(json, errorMsg);

      if (res != TRI_ERROR_NO_ERROR) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

        return TRI_ERROR_INTERNAL;
      }
    }

    if (ImportData) {
      // import data. check if we have a datafile
      // TODO: externalise file extension
      const string datafile = InputDirectory + TRI_DIR_SEPARATOR_STR + name + ".data.json";

      if (TRI_ExistsFile(datafile.c_str())) {
        // found a datafile

        ifstream ifs;
  
        ifs.open(datafile.c_str(), ifstream::in | ifstream::binary);

        if (! ifs.is_open()) {
          errorMsg = "cannot open collection data file '" + datafile + "'";
          TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

          return TRI_ERROR_INTERNAL;
        }

        while (ifs.good()) {
          string line;

          std::getline(ifs, line, '\n');

          // std::cout << "FOUND LINE: " << line << "\n";
        }

        ifs.close();
      }
    }
        
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  }

  return TRI_ERROR_NO_ERROR;
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

  int err = 0;
  InputDirectory = FileUtils::currentDirectory(&err).append(TRI_DIR_SEPARATOR_STR).append("dump");
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
    TRI_EXIT_FUNCTION(EXIT_FAILURE, NULL);
  }

  // .............................................................................
  // set-up client connection
  // .............................................................................

  BaseClient.createEndpoint();

  if (BaseClient.endpointServer() == 0) {
    cerr << "invalid value for --server.endpoint ('" << BaseClient.endpointString() << "')" << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, NULL);
  }

  Connection = GeneralClientConnection::factory(BaseClient.endpointServer(),
                                                BaseClient.requestTimeout(),
                                                BaseClient.connectTimeout(),
                                                ArangoClient::DEFAULT_RETRIES);
  
  if (Connection == 0) {
    cerr << "out of memory" << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, NULL);
  }
  
  Client = new SimpleHttpClient(Connection, BaseClient.requestTimeout(), false);

  if (Client == 0) {
    cerr << "out of memory" << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, NULL);
  }

  Client->setUserNamePassword("/", BaseClient.username(), BaseClient.password());

  const string version = GetVersion();

  if (! Connection->isConnected()) {
    cerr << "Could not connect to endpoint " << BaseClient.endpointServer()->getSpecification() << endl;
    cerr << "Error message: '" << Client->getErrorMessage() << "'" << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, NULL);
  }
    
  // successfully connected

  if (Progress) {
    cout << "Connected to ArangoDB '" << BaseClient.endpointServer()->getSpecification() << endl;
  }

#if 0
  string errorMsg = "";
  int res = ProcessInputDirectory(errorMsg);

  if (res != TRI_ERROR_NO_ERROR) {
    cerr << errorMsg << endl;
    ret = EXIT_FAILURE;
  }
#endif

  TRIAGENS_REST_SHUTDOWN;

  arangorestoreExitFunction(ret, NULL);

  return ret;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
