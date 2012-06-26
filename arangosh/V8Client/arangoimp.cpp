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
#include "Logger/Logger.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "ImportHelper.h"
#include "V8ClientConnection.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::httpclient;
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

static string DEFAULT_SERVER_NAME = "localhost";
static int    DEFAULT_SERVER_PORT = 8529;
static int64_t DEFAULT_REQUEST_TIMEOUT = 300;
static size_t DEFAULT_RETRIES = 5;
static int64_t DEFAULT_CONNECTION_TIMEOUT = 5;

////////////////////////////////////////////////////////////////////////////////
/// @brief server address
////////////////////////////////////////////////////////////////////////////////

static string ServerAddress = "127.0.0.1:8529";

////////////////////////////////////////////////////////////////////////////////
/// @brief the initial default connection
////////////////////////////////////////////////////////////////////////////////

V8ClientConnection* clientConnection = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief max size body size (used for imports)
////////////////////////////////////////////////////////////////////////////////

static uint64_t maxUploadSize = 500000;

////////////////////////////////////////////////////////////////////////////////
/// @brief connect timeout (in s) 
////////////////////////////////////////////////////////////////////////////////

static int64_t connectTimeout = DEFAULT_CONNECTION_TIMEOUT;

////////////////////////////////////////////////////////////////////////////////
/// @brief request timeout (in s) 
////////////////////////////////////////////////////////////////////////////////

static int64_t requestTimeout = DEFAULT_REQUEST_TIMEOUT;

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
/// @brief splits the address
////////////////////////////////////////////////////////////////////////////////

static bool splitServerAdress (string const& definition, string& address, int& port) {
  if (definition.empty()) {
    return false;
  }

  if (definition[0] == '[') {
    // ipv6 address
    size_t find = definition.find("]:", 1);

    if (find != string::npos && find + 2 < definition.size()) {
      address = definition.substr(1, find - 1);
      port = triagens::basics::StringUtils::uint32(definition.substr(find + 2));
      return true;
    }

  }

  int n = triagens::basics::StringUtils::numEntries(definition, ":");

  if (n == 1) {
    address = "";
    port = triagens::basics::StringUtils::uint32(definition);
    return true;
  }
  else if (n == 2) {
    address = triagens::basics::StringUtils::entry(1, definition, ":");
    port = triagens::basics::StringUtils::int32(triagens::basics::StringUtils::entry(2, definition, ":"));
    return true;
  }
  else {
    return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the program options
////////////////////////////////////////////////////////////////////////////////

static void ParseProgramOptions (int argc, char* argv[]) {
  string level = "info";

  ProgramOptionsDescription description("STANDARD options");

  description
    ("help,h", "help message")
    ("log.level,l", &level,  "log level")
    ("server", &ServerAddress, "server address and port")
    ("file", &FileName, "file name (\"-\" for STDIN)")
    ("collection", &CollectionName, "collection name")
    ("create-collection", &CreateCollection, "create collection if it does not yet exist")
    ("use-ids", &UseIds, "re-use _id and _rev values found in document data")
    ("type", &TypeImport, "type of file (\"csv\" or \"json\")")
    ("quote", &QuoteChar, "quote character")
    ("separator", &SeparatorChar, "separator character")
    ("max-upload-size", &maxUploadSize, "maximum size of import chunks")
    ("connect-timeout", &connectTimeout, "connect timeout in seconds")
    ("request-timeout", &requestTimeout, "request timeout in seconds")
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
  TRIAGENS_C_INITIALISE;
  TRI_InitialiseLogging(false);
  int ret = EXIT_SUCCESS;

  // parse the program options
  ParseProgramOptions(argc, argv);
  
  // check connection args
  if (connectTimeout <= 0) {
    cout << "invalid value for connect-timeout." << endl;
    return EXIT_FAILURE;
  }

  if (requestTimeout <= 0) {
    cout << "invalid value for request-timeout." << endl;
    return EXIT_FAILURE;
  }
    
  // processComandLineArguments(argc, argv);
  if (! splitServerAdress(ServerAddress, DEFAULT_SERVER_NAME, DEFAULT_SERVER_PORT)) {
    if (ServerAddress.length()) {
      printf("Could not split %s.\n", ServerAddress.c_str());                  
    }
  }
          
  clientConnection = new V8ClientConnection(
          DEFAULT_SERVER_NAME, 
          DEFAULT_SERVER_PORT, 
          (double) requestTimeout,
          DEFAULT_RETRIES, 
          (double) connectTimeout, 
          true);

  if (clientConnection->isConnected()) {
    printf("Connected to Arango DB %s:%d Version %s\n", 
            clientConnection->getHostname().c_str(), 
            clientConnection->getPort(), 
            clientConnection->getVersion().c_str());

    cout << "----------------------------------------" << endl;
    cout << "collection      : " << CollectionName << endl;
    cout << "create          : " << (CreateCollection ? "yes" : "no") << endl;
    cout << "reusing ids     : " << (UseIds ? "yes" : "no") << endl;
    cout << "file            : " << FileName << endl;
    cout << "type            : " << TypeImport << endl;
    cout << "quote           : " << QuoteChar << endl;
    cout << "separator       : " << SeparatorChar << endl;
    cout << "connect timeout : " << connectTimeout << endl;
    cout << "request timeout : " << requestTimeout << endl;
    cout << "----------------------------------------" << endl;
    
    ImportHelper ih(clientConnection->getHttpClient(), maxUploadSize);

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
      cout << "Wrong length of quote character." << endl;
      return EXIT_FAILURE;
    }

    if (SeparatorChar.length() == 1) {
      ih.setSeparator(SeparatorChar[0]);      
    }
    else {
      cout << "Wrong length of separator character." << endl;
      return EXIT_FAILURE;
    }
    
    if (CollectionName == "") {
      cout << "collection name is missing." << endl;
      return EXIT_FAILURE;
    }
    
    if (FileName == "") {
      cout << "file name is missing." << endl;
      return EXIT_FAILURE;
    }

    if (FileName != "-" && !FileUtils::isRegularFile(FileName)) {
      cout << "file '" << FileName << "' is not a regular file." << endl;
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
      cout << "Wrong type." << endl;
      return EXIT_FAILURE;      
    }

    cout << endl;
    
    if (ok) {
      cout << "created    : " << ih.getImportedLines() << endl;
      cout << "errors     : " << ih.getErrorLines() << endl;
      cout << "total      : " << ih.getReadLines() << endl;
    }
    else {
      cout << "error mesg : " << ih.getErrorMessage() << endl;      
    }
      
  }
  else {
    printf("Could not connect to server %s:%d\n", DEFAULT_SERVER_NAME.c_str(), DEFAULT_SERVER_PORT);
    printf("Error message '%s'\n", clientConnection->getErrorMessage().c_str());
  }

  return ret;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
