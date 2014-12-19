////////////////////////////////////////////////////////////////////////////////
/// @brief simple arango importer
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
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "ArangoShell/ArangoClient.h"
#include "Basics/FileUtils.h"
#include "Basics/ProgramOptions.h"
#include "Basics/ProgramOptionsDescription.h"
#include "Basics/StringUtils.h"
#include "Basics/files.h"
#include "Basics/init.h"
#include "Basics/logging.h"
#include "Basics/tri-strings.h"
#include "Basics/terminal-utils.h"
#include "ImportHelper.h"
#include "Rest/Endpoint.h"
#include "Rest/InitialiseRest.h"
#include "Rest/HttpResponse.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "V8Client/V8ClientConnection.h"


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
/// @brief base class for clients
////////////////////////////////////////////////////////////////////////////////

ArangoClient BaseClient("arangoimp");

////////////////////////////////////////////////////////////////////////////////
/// @brief the initial default connection
////////////////////////////////////////////////////////////////////////////////

V8ClientConnection* ClientConnection = nullptr;

////////////////////////////////////////////////////////////////////////////////
/// @brief max size body size (used for imports)
////////////////////////////////////////////////////////////////////////////////

static uint64_t ChunkSize = 1024 * 1024 * 16;

////////////////////////////////////////////////////////////////////////////////
/// @brief quote character(s)
////////////////////////////////////////////////////////////////////////////////

static string Quote = "\"";

////////////////////////////////////////////////////////////////////////////////
/// @brief separator
////////////////////////////////////////////////////////////////////////////////

static string Separator = ",";

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not backslashes can be used to escape quotes
////////////////////////////////////////////////////////////////////////////////

static bool UseBackslash = false;

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
/// @brief whether or not to overwrite existing data in a collection
////////////////////////////////////////////////////////////////////////////////

static bool Overwrite = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief progress
////////////////////////////////////////////////////////////////////////////////

static bool Progress = true;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the program options
////////////////////////////////////////////////////////////////////////////////

static void ParseProgramOptions (int argc, char* argv[]) {
  ProgramOptionsDescription deprecatedOptions("DEPRECATED options");

  deprecatedOptions
    ("max-upload-size", &ChunkSize, "size for individual data batches (in bytes)")
  ;

  ProgramOptionsDescription description("STANDARD options");

  description
    ("file", &FileName, "file name (\"-\" for STDIN)")
    ("backslash-escape", &UseBackslash, "use backslash as the escape character for quotes, used for csv")
    ("batch-size", &ChunkSize, "size for individual data batches (in bytes)")
    ("collection", &CollectionName, "collection name")
    ("create-collection", &CreateCollection, "create collection if it does not yet exist")
    ("type", &TypeImport, "type of file (\"csv\", \"tsv\", or \"json\")")
    ("overwrite", &Overwrite, "overwrite collection if it exist (WARNING: this will remove any data from the collection)")
    ("quote", &Quote, "quote character(s), used for csv")
    ("separator", &Separator, "field separator, used for csv")
    ("progress", &Progress, "show progress")
    (deprecatedOptions, true)
  ;

  BaseClient.setupGeneral(description);
  BaseClient.setupServer(description);

  vector<string> arguments;
  description.arguments(&arguments);

  ProgramOptions options;
  BaseClient.parse(options, description, "--file <file> --type <type> --collection <collection>", argc, argv, "arangoimp.conf");

  if (FileName == "" && arguments.size() > 0) {
    FileName = arguments[0];
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief startup and exit functions
////////////////////////////////////////////////////////////////////////////////

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
    exit(1);
  }

  exit(exitCode);
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

  if (BaseClient.endpointServer() == nullptr) {
    cerr << "invalid value for --server.endpoint ('" << BaseClient.endpointString() << "')" << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  ClientConnection = new V8ClientConnection(BaseClient.endpointServer(),
                                            BaseClient.databaseName(),
                                            BaseClient.username(),
                                            BaseClient.password(),
                                            BaseClient.requestTimeout(),
                                            BaseClient.connectTimeout(),
                                            ArangoClient::DEFAULT_RETRIES,
                                            BaseClient.sslProtocol(),
                                            false);

  if (! ClientConnection->isConnected() ||
      ClientConnection->getLastHttpReturnCode() != HttpResponse::OK) {
    cerr << "Could not connect to endpoint '" << BaseClient.endpointServer()->getSpecification()
         << "', database: '" << BaseClient.databaseName() << "'" << endl;
    cerr << "Error message: '" << ClientConnection->getErrorMessage() << "'" << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  // successfully connected
  cout << "Connected to ArangoDB '" << BaseClient.endpointServer()->getSpecification()
       << "', version " << ClientConnection->getVersion() << ", database: '"
       << BaseClient.databaseName() << "', username: '" << BaseClient.username() << "'" << endl;

  cout << "----------------------------------------" << endl;
  cout << "database:         " << BaseClient.databaseName() << endl;
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

  ImportHelper ih(ClientConnection->getHttpClient(), ChunkSize);

  // create colletion
  if (CreateCollection) {
    ih.setCreateCollection(true);
  }

  ih.setOverwrite(Overwrite);
  ih.useBackslash(UseBackslash);

  // quote
  if (Quote.length() <= 1) {
    ih.setQuote(Quote);
  }
  else {
    cerr << "Wrong length of quote character." << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  // separator
  if (Separator.length() == 1) {
    ih.setSeparator(Separator);
  }
  else {
    cerr << "Separator must be exactly one character." << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  // collection name
  if (CollectionName == "") {
    cerr << "Collection name is missing." << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  // filename
  if (FileName == "") {
    cerr << "File name is missing." << endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  if (FileName != "-" && ! FileUtils::isRegularFile(FileName)) {
    if (! FileUtils::exists(FileName)) {
      cerr << "Cannot open file '" << FileName << "'. File not found." << endl;
    }
    else if (FileUtils::isDirectory(FileName)) {
      cerr << "Specified file '" << FileName << "' is a directory. Please use a regular file." << endl;
    }
    else {
      cerr << "Cannot open '" << FileName << "'. Invalid file type." << endl;
    }

    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  // progress
  if (Progress) {
    ih.setProgress(true);
  }

  try {
    bool ok = false;

    // import type
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
      TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
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
  }
  catch (std::exception const& ex) {
    cerr << "Caught exception " << ex.what() << " during import" << endl;
  }
  catch (...) {
    cerr << "Got an unknown exception during import" << endl;
  }

  delete ClientConnection;

  TRIAGENS_REST_SHUTDOWN;

  arangoimpExitFunction(ret, nullptr);

  return ret;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
