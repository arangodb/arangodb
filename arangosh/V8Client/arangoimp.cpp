////////////////////////////////////////////////////////////////////////////////
/// @brief simple arango importer
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <fstream>

#include "build.h"

#include "Basics/ProgramOptions.h"
#include "Basics/ProgramOptionsDescription.h"
#include "Basics/StringUtils.h"
#include "Basics/FileUtils.h"
#include "BasicsC/files.h"
#include "BasicsC/init.h"
#include "BasicsC/logging.h"
#include "BasicsC/strings.h"
#include "BasicsC/terminal-utils.h"
#include "ImportHelper.h"
#include "Logger/Logger.h"
#include "Rest/Endpoint.h"
#include "Rest/Initialise.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "V8ClientConnection.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::httpclient;
using namespace triagens::rest;
using namespace triagens::v8client;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief connection default values
////////////////////////////////////////////////////////////////////////////////

static int64_t DEFAULT_REQUEST_TIMEOUT = 300;
static size_t  DEFAULT_RETRIES = 2;
static int64_t DEFAULT_CONNECTION_TIMEOUT = 3;

////////////////////////////////////////////////////////////////////////////////
/// @brief endpoint to connect to
////////////////////////////////////////////////////////////////////////////////

static Endpoint* _endpoint = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief connect timeout (in s) 
////////////////////////////////////////////////////////////////////////////////

static int64_t _connectTimeout = DEFAULT_CONNECTION_TIMEOUT;

////////////////////////////////////////////////////////////////////////////////
/// @brief request timeout (in s) 
////////////////////////////////////////////////////////////////////////////////

static int64_t _requestTimeout = DEFAULT_REQUEST_TIMEOUT;

////////////////////////////////////////////////////////////////////////////////
/// @brief endpoint to connect to
////////////////////////////////////////////////////////////////////////////////

static string _endpointString;

////////////////////////////////////////////////////////////////////////////////
/// @brief user to send to endpoint
////////////////////////////////////////////////////////////////////////////////

static string _username = "root";

////////////////////////////////////////////////////////////////////////////////
/// @brief password to send to endpoint
////////////////////////////////////////////////////////////////////////////////

static string _password = "";

////////////////////////////////////////////////////////////////////////////////
/// @brief the initial default connection
////////////////////////////////////////////////////////////////////////////////

V8ClientConnection* clientConnection = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief max size body size (used for imports)
////////////////////////////////////////////////////////////////////////////////

static uint64_t _maxUploadSize = 500000;

static string QuoteChar = "\"";
static string SeparatorChar = ",";
static string FileName = "";
static string CollectionName = "";
static string TypeImport = "json";
static bool CreateCollection = false;
static bool UseIds = false;

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
  string level = "info";

  ProgramOptionsDescription description("STANDARD options");

  description
    ("help,h", "help message")
    ("log.level,l", &level,  "log level")
    ("file", &FileName, "file name (\"-\" for STDIN)")
    ("collection", &CollectionName, "collection name")
    ("create-collection", &CreateCollection, "create collection if it does not yet exist")
    ("use-ids", &UseIds, "re-use _id and _rev values found in document data")
    ("max-upload-size", &_maxUploadSize, "maximum size of import chunks (in bytes)")
    ("type", &TypeImport, "type of file (\"csv\" or \"json\")")
    ("quote", &QuoteChar, "quote character")
    ("separator", &SeparatorChar, "separator character")
    ("server.endpoint", &_endpointString, "endpoint to connect to")
    ("server.username", &_username, "username to use when connecting")
    ("server.password", &_password, "password to use when connecting (leave empty for prompt)")
    ("server.connect-timeout", &_connectTimeout, "connect timeout in seconds")
    ("server.request-timeout", &_requestTimeout, "request timeout in seconds")
  ;

  vector<string> myargs;
  description.arguments(&myargs);
  
  ProgramOptions options;

  if (! options.parse(description, argc, argv)) {
    cerr << options.lastError() << "\n";
    exit(EXIT_FAILURE);
  }

  if (FileName == "" && myargs.size() > 0) {
    FileName = myargs[0];
  }
    
  // check for help
  set<string> help = options.needHelp("help");

  if (! help.empty()) {
    cout << description.usage(help) << endl;
    exit(EXIT_SUCCESS);
  }

  // set the logging
  TRI_SetLogLevelLogging(level.c_str());
  TRI_CreateLogAppenderFile("-");
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
/// @brief main
////////////////////////////////////////////////////////////////////////////////

int main (int argc, char* argv[]) {
  TRIAGENS_C_INITIALISE(argc, argv);
  TRIAGENS_REST_INITIALISE(argc, argv);

  TRI_InitialiseLogging(false);

  _endpointString = Endpoint::getDefaultEndpoint();

  // parse the program options
  ParseProgramOptions(argc, argv);

  // check connection args
  if (_connectTimeout <= 0) {
    cerr << "invalid value for --server.connect-timeout" << endl;
    return EXIT_FAILURE;
  }

  if (_requestTimeout <= 0) {
    cerr << "invalid value for --server.request-timeout" << endl;
    return EXIT_FAILURE;
  }
  
  if (_username.size() == 0) {
    // must specify a user name
    cerr << "no value specified for --server.username" << endl;
    exit(EXIT_FAILURE);
  }
  
  if (_password.size() == 0) {
    // no password given on command-line
    cout << "Please specify a password:" << endl;
    // now prompt for it
#ifdef TRI_HAVE_TERMIOS
    TRI_SetStdinVisibility(false);
    getline(cin, _password);

    TRI_SetStdinVisibility(true);
#else
    getline(cin, _password);
#endif
  }

  if (_password.size() == 0) {
    cerr << "no value specified for --server.password" << endl;
    exit(EXIT_FAILURE);
  }


  _endpoint = Endpoint::clientFactory(_endpointString); 
  if (_endpoint == 0) {
    cerr << "invalid --server.endpoint specification." << endl;
    return EXIT_FAILURE;
  }

  assert(_endpoint);
  
  clientConnection = new V8ClientConnection(_endpoint, 
                                            _username, 
                                            _password,
                                            (double) _requestTimeout, 
                                            (double) _connectTimeout, 
                                            DEFAULT_RETRIES, 
                                            false);

  if (!clientConnection->isConnected()) {
    cerr << "Could not connect to endpoint " << _endpoint->getSpecification() << endl;
    cerr << "Error message: '" << clientConnection->getErrorMessage() << "'" << endl;

    return EXIT_FAILURE;
  }

  // successfully connected

  cout << "Connected to ArangoDB '" << _endpoint->getSpecification() << "' Version " << clientConnection->getVersion() << endl; 

  cout << "----------------------------------------" << endl;
  cout << "collection      : " << CollectionName << endl;
  cout << "create          : " << (CreateCollection ? "yes" : "no") << endl;
  cout << "reusing ids     : " << (UseIds ? "yes" : "no") << endl;
  cout << "file            : " << FileName << endl;
  cout << "type            : " << TypeImport << endl;
  cout << "quote           : " << QuoteChar << endl;
  cout << "separator       : " << SeparatorChar << endl;
  cout << "connect timeout : " << _connectTimeout << endl;
  cout << "request timeout : " << _requestTimeout << endl;
  cout << "----------------------------------------" << endl;

  ImportHelper ih(clientConnection->getHttpClient(), _maxUploadSize);

  if (CreateCollection) {
    ih.setCreateCollection(true);
  }

  if (UseIds) {
    ih.setUseIds(true);
  }

  if (QuoteChar.length() == 1) {
    ih.setQuote(QuoteChar[0]);      
  }
  else {
    cerr << "Wrong length of quote character." << endl;
    return EXIT_FAILURE;
  }

  if (SeparatorChar.length() == 1) {
    ih.setSeparator(SeparatorChar[0]);      
  }
  else {
    cerr << "Wrong length of separator character." << endl;
    return EXIT_FAILURE;
  }

  if (CollectionName == "") {
    cerr << "collection name is missing." << endl;
    return EXIT_FAILURE;
  }

  if (FileName == "") {
    cerr << "file name is missing." << endl;
    return EXIT_FAILURE;
  }

  if (FileName != "-" && !FileUtils::isRegularFile(FileName)) {
    cerr << "file '" << FileName << "' is not a regular file." << endl;
    return EXIT_FAILURE;      
  }

  bool ok;
  if (TypeImport == "csv") {
    cout << "Starting CSV import..." << endl;
    ok = ih.importCsv(CollectionName, FileName);
  }

  else if (TypeImport == "json") {
    cout << "Starting JSON import..." << endl;
    ok = ih.importJson(CollectionName, FileName);
  }

  else {
    cerr << "Wrong type." << endl;
    return EXIT_FAILURE;      
  }

  cout << endl;

  if (ok) {
    cout << "created       : " << ih.getImportedLines() << endl;
    cout << "errors        : " << ih.getErrorLines() << endl;
    cout << "total         : " << ih.getReadLines() << endl;
  }
  else {
    cerr << "error message : " << ih.getErrorMessage() << endl;      
  }

  TRIAGENS_REST_SHUTDOWN;

  return EXIT_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
