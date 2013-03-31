////////////////////////////////////////////////////////////////////////////////
/// @brief simple arango importer
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
/// @author Dr. Frank Celler
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "BasicsC/common.h"

#include <stdio.h>
#include <fstream>

#include "ArangoShell/ArangoClient.h"
#include "Basics/FileUtils.h"
#include "Basics/ProgramOptions.h"
#include "Basics/ProgramOptionsDescription.h"
#include "Basics/StringUtils.h"
#include "BasicsC/files.h"
#include "BasicsC/init.h"
#include "BasicsC/logging.h"
#include "BasicsC/tri-strings.h"
#include "BasicsC/terminal-utils.h"
#include "ImportHelper.h"
#include "Logger/Logger.h"
#include "Rest/Endpoint.h"
#include "Rest/InitialiseRest.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "V8Client/V8ClientConnection.h"

#include "build.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::httpclient;
using namespace triagens::rest;
using namespace triagens::v8client;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief base class for clients
////////////////////////////////////////////////////////////////////////////////

ArangoClient BaseClient;

////////////////////////////////////////////////////////////////////////////////
/// @brief the initial default connection
////////////////////////////////////////////////////////////////////////////////

V8ClientConnection* ClientConnection = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief max size body size (used for imports)
////////////////////////////////////////////////////////////////////////////////

static uint64_t MaxUploadSize = 1024 * 1024;

////////////////////////////////////////////////////////////////////////////////
/// @brief quote character(s)
////////////////////////////////////////////////////////////////////////////////

static string Quote = "\"";

////////////////////////////////////////////////////////////////////////////////
/// @brief separator
////////////////////////////////////////////////////////////////////////////////

static string Separator = ",";

////////////////////////////////////////////////////////////////////////////////
/// @brief file-name
////////////////////////////////////////////////////////////////////////////////

static string FileName = "";

////////////////////////////////////////////////////////////////////////////////
/// @brief collection-name
////////////////////////////////////////////////////////////////////////////////

static string CollectionName = "";

////////////////////////////////////////////////////////////////////////////////
/// @brief import type
////////////////////////////////////////////////////////////////////////////////

static string TypeImport = "json";

////////////////////////////////////////////////////////////////////////////////
/// @brief create collection if necessary
////////////////////////////////////////////////////////////////////////////////

static bool CreateCollection = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief progress
////////////////////////////////////////////////////////////////////////////////

static bool Progress = false;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the program options
////////////////////////////////////////////////////////////////////////////////

static void ParseProgramOptions (int argc, char* argv[]) {
  ProgramOptionsDescription description("STANDARD options");

  description
    ("file", &FileName, "file name (\"-\" for STDIN)")
    ("collection", &CollectionName, "collection name")
    ("create-collection", &CreateCollection, "create collection if it does not yet exist")
    ("max-upload-size", &MaxUploadSize, "maximum size of import chunks (in bytes)")
    ("type", &TypeImport, "type of file (\"csv\", \"tsv\", or \"json\")")
    ("quote", &Quote, "quote character(s)")
    ("separator", &Separator, "separator")
    ("progress", &Progress, "show progress")
  ;

  BaseClient.setupGeneral(description);
  BaseClient.setupServer(description);

  vector<string> arguments;
  description.arguments(&arguments);

  ProgramOptions options;
  BaseClient.parse(options, description, argc, argv, "arangoimp.conf");

  if (FileName == "" && arguments.size() > 0) {
    FileName = arguments[0];
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup arangoimp
/// @{
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @brief startup and exit functions
////////////////////////////////////////////////////////////////////////////////

void* arangoimpResourcesAllocated = NULL;
static void arangoimpEntryFunction ();
static void arangoimpExitFunction (int, void*);

#ifdef _WIN32

// .............................................................................
// Call this function to do various initialistions for windows only
// .............................................................................
void arangoimpEntryFunction() {
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

static void arangoimpExitFunction(int exitCode, void* data) {
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

static void arangoimpEntryFunction() {
}

static void arangoimpExitFunction(int exitCode, void* data) {
}

#endif


////////////////////////////////////////////////////////////////////////////////
/// @brief main
////////////////////////////////////////////////////////////////////////////////

int main (int argc, char* argv[]) {

  int ret = EXIT_SUCCESS;

  arangoimpEntryFunction();

  TRIAGENS_C_INITIALISE(argc, argv);
  TRIAGENS_REST_INITIALISE(argc, argv);

  TRI_InitialiseLogging(false);

  BaseClient.setEndpointString(Endpoint::getDefaultEndpoint());

  // .............................................................................
  // parse the program options
  // .............................................................................

  ParseProgramOptions(argc, argv);

  // .............................................................................
  // set-up client connection
  // .............................................................................

  BaseClient.createEndpoint();

  if (BaseClient.endpointServer() == 0) {
    cerr << "invalid value for --server.endpoint ('" << BaseClient.endpointString() << "')" << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE,NULL);
  }

  ClientConnection = new V8ClientConnection(BaseClient.endpointServer(),
                                            BaseClient.username(),
                                            BaseClient.password(),
                                            BaseClient.requestTimeout(),
                                            BaseClient.connectTimeout(),
                                            ArangoClient::DEFAULT_RETRIES,
                                            false);

  if (! ClientConnection->isConnected() || ClientConnection->getLastHttpReturnCode() != SimpleHttpResult::HTTP_STATUS_OK) {
    cerr << "Could not connect to endpoint " << BaseClient.endpointServer()->getSpecification() << endl;
    cerr << "Error message: '" << ClientConnection->getErrorMessage() << "'" << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE,NULL);
  }

  // successfully connected
  cout << "Connected to ArangoDB '" << BaseClient.endpointServer()->getSpecification()
       << "' Version " << ClientConnection->getVersion() << endl;

  cout << "----------------------------------------" << endl;
  cout << "collection:       " << CollectionName << endl;
  cout << "create:           " << (CreateCollection ? "yes" : "no") << endl;
  cout << "file:             " << FileName << endl;
  cout << "type:             " << TypeImport << endl;

  if (TypeImport == "csv") {
    cout << "quote:            " << Quote << endl;
    cout << "separator:        " << Separator << endl;
  }

  cout << "connect timeout:  " << BaseClient.connectTimeout() << endl;
  cout << "request timeout:  " << BaseClient.requestTimeout() << endl;
  cout << "----------------------------------------" << endl;

  ImportHelper ih(ClientConnection->getHttpClient(), MaxUploadSize);

  // create colletion
  if (CreateCollection) {
    ih.setCreateCollection(true);
  }

  // quote
  if (Quote.length() <= 1) {
    ih.setQuote(Quote);
  }
  else {
    cerr << "Wrong length of quote character." << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE,NULL);
  }

  // separator
  if (Separator.length() == 1) {
    ih.setSeparator(Separator);
  }
  else {
    cerr << "Separator must be exactly one character." << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE,NULL);
  }

  // collection name
  if (CollectionName == "") {
    cerr << "collection name is missing." << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE,NULL);
  }

  // filename
  if (FileName == "") {
    cerr << "file name is missing." << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE,NULL);
  }

  if (FileName != "-" && ! FileUtils::isRegularFile(FileName)) {
    cerr << "file '" << FileName << "' is not a regular file." << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE,NULL);
  }

  // progress
  if (Progress) {
    ih.setProgress(true);
  }

  // import type
  bool ok = false;

  if (TypeImport == "csv") {
    cout << "Starting CSV import..." << endl;
    ok = ih.importDelimited(CollectionName, FileName, ImportHelper::CSV);
  }

  else if (TypeImport == "tsv") {
    cout << "Starting TSV import..." << endl;
    ih.setQuote("");
    ih.setSeparator("\\t");
    ok = ih.importDelimited(CollectionName, FileName, ImportHelper::TSV);
  }

  else if (TypeImport == "json") {
    cout << "Starting JSON import..." << endl;
    ok = ih.importJson(CollectionName, FileName);
  }

  else {
    cerr << "Wrong type '" << TypeImport << "'." << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE,NULL);
  }

  cout << endl;

  // give information about import
  if (ok) {
    cout << "created:          " << ih.getImportedLines() << endl;
    cout << "errors:           " << ih.getErrorLines() << endl;
    cout << "total:            " << ih.getReadLines() << endl;
  }
  else {
    cerr << "error message:    " << ih.getErrorMessage() << endl;
  }

  // calling dispose in V8 3.10.x causes a segfault. the v8 docs says its not necessary to call it upon program termination
  // v8::V8::Dispose();

  TRIAGENS_REST_SHUTDOWN;

  arangoimpExitFunction(ret, NULL);

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
