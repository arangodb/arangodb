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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "ImportHelper.h"

#include <iostream>

#include "ArangoShell/ArangoClient.h"
#include "Basics/FileUtils.h"
#include "Basics/ProgramOptions.h"
#include "Basics/ProgramOptionsDescription.h"
#include "Basics/files.h"
#include "Basics/init.h"
#include "Basics/terminal-utils.h"
#include "Basics/tri-strings.h"
#include "Rest/Endpoint.h"
#include "Rest/HttpResponse.h"
#include "Rest/InitializeRest.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief base class for clients
////////////////////////////////////////////////////////////////////////////////

ArangoClient BaseClient("arangoimp");

////////////////////////////////////////////////////////////////////////////////
/// @brief max size body size (used for imports)
////////////////////////////////////////////////////////////////////////////////

static uint64_t ChunkSize = 1024 * 1024 * 16;

////////////////////////////////////////////////////////////////////////////////
/// @brief quote character(s)
////////////////////////////////////////////////////////////////////////////////

static std::string Quote = "\"";

////////////////////////////////////////////////////////////////////////////////
/// @brief separator
////////////////////////////////////////////////////////////////////////////////

static std::string Separator = ",";

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not backslashes can be used to escape quotes
////////////////////////////////////////////////////////////////////////////////

static bool UseBackslash = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief file-name
////////////////////////////////////////////////////////////////////////////////

static std::string FileName = "";

////////////////////////////////////////////////////////////////////////////////
/// @brief collection-name
////////////////////////////////////////////////////////////////////////////////

static std::string CollectionName = "";

////////////////////////////////////////////////////////////////////////////////
/// @brief import type
////////////////////////////////////////////////////////////////////////////////

static std::string TypeImport = "json";

////////////////////////////////////////////////////////////////////////////////
/// @brief create collection if necessary
////////////////////////////////////////////////////////////////////////////////

static bool CreateCollection = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection type if collection is to be created
////////////////////////////////////////////////////////////////////////////////

static std::string CreateCollectionType = "document";

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not to overwrite existing data in a collection
////////////////////////////////////////////////////////////////////////////////

static bool Overwrite = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief type of action to perform on duplicate _key
////////////////////////////////////////////////////////////////////////////////

static std::string OnDuplicateAction = "error";

////////////////////////////////////////////////////////////////////////////////
/// @brief progress
////////////////////////////////////////////////////////////////////////////////

static bool Progress = true;

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the program options
////////////////////////////////////////////////////////////////////////////////

static void ParseProgramOptions(int argc, char* argv[]) {
  ProgramOptionsDescription deprecatedOptions("DEPRECATED options");

  deprecatedOptions("max-upload-size", &ChunkSize,
                    "size for individual data batches (in bytes)");

  ProgramOptionsDescription description("STANDARD options");

  description("file", &FileName, "file name (\"-\" for STDIN)")(
      "backslash-escape", &UseBackslash,
      "use backslash as the escape character for quotes, used for csv")(
      "batch-size", &ChunkSize, "size for individual data batches (in bytes)")(
      "collection", &CollectionName, "collection name")(
      "create-collection", &CreateCollection,
      "create collection if it does not yet exist")(
      "create-collection-type", &CreateCollectionType,
      "type of collection if collection is created ('document' or 'edge')")(
      "type", &TypeImport, "type of file (\"csv\", \"tsv\", or \"json\")")(
      "overwrite", &Overwrite,
      "overwrite collection if it exist (WARNING: this will remove any data "
      "from the collection)")("quote", &Quote,
                              "quote character(s), used for csv")(
      "separator", &Separator, "field separator, used for csv")(
      "progress", &Progress, "show progress")(
      "on-duplicate", &OnDuplicateAction,
      "action to perform when a unique key constraint "
      "violation occurs. Possible values: 'error', 'update', "
      "'replace', 'ignore')")(deprecatedOptions, true);

  BaseClient.setupGeneral(description);
  BaseClient.setupServer(description);

  std::vector<std::string> arguments;
  description.arguments(&arguments);

  ProgramOptions options;
  BaseClient.parse(options, description,
                   "--file <file> --type <type> --collection <collection>",
                   argc, argv, "arangoimp.conf");

  if (FileName == "" && arguments.size() > 0) {
    FileName = arguments[0];
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
/// @brief request location rewriter (injects database name)
////////////////////////////////////////////////////////////////////////////////

static std::string RewriteLocation(void* data, std::string const& location) {
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
    std::cerr << "invalid value for --server.endpoint ('"
              << BaseClient.endpointString() << "')" << std::endl;
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }

  // create a connection
  {
    std::unique_ptr<arangodb::httpclient::GeneralClientConnection> connection;

    connection.reset(GeneralClientConnection::factory(
        BaseClient.endpointServer(), BaseClient.requestTimeout(),
        BaseClient.connectTimeout(), ArangoClient::DEFAULT_RETRIES,
        BaseClient.sslProtocol()));

    if (connection == nullptr) {
      std::cerr << "out of memory" << std::endl;
      TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
    }

    // simple http client is only valid inside this scope
    {
      SimpleHttpClient client(connection.get(), BaseClient.requestTimeout(),
                              false);

      client.setLocationRewriter(nullptr, &RewriteLocation);
      client.setUserNamePassword("/", BaseClient.username(),
                                 BaseClient.password());

      // must stay here in order to establish the connection
      client.getServerVersion();

      if (!connection->isConnected()) {
        std::cerr << "Could not connect to endpoint '"
                  << BaseClient.endpointString() << "', database: '"
                  << BaseClient.databaseName() << "', username: '"
                  << BaseClient.username() << "'" << std::endl;
        std::cerr << "Error message: '" << client.getErrorMessage() << "'"
                  << std::endl;
        TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
      }

      // successfully connected
      std::cout << "Connected to ArangoDB '"
                << BaseClient.endpointServer()->getSpecification()
                << "', version " << client.getServerVersion() << ", database: '"
                << BaseClient.databaseName() << "', username: '"
                << BaseClient.username() << "'" << std::endl;

      std::cout << "----------------------------------------" << std::endl;
      std::cout << "database:         " << BaseClient.databaseName()
                << std::endl;
      std::cout << "collection:       " << CollectionName << std::endl;
      std::cout << "create:           " << (CreateCollection ? "yes" : "no")
                << std::endl;
      std::cout << "file:             " << FileName << std::endl;
      std::cout << "type:             " << TypeImport << std::endl;

      if (TypeImport == "csv") {
        std::cout << "quote:            " << Quote << std::endl;
        std::cout << "separator:        " << Separator << std::endl;
      }

      std::cout << "connect timeout:  " << BaseClient.connectTimeout()
                << std::endl;
      std::cout << "request timeout:  " << BaseClient.requestTimeout()
                << std::endl;
      std::cout << "----------------------------------------" << std::endl;

      arangodb::v8client::ImportHelper ih(&client, ChunkSize);

      // create colletion
      if (CreateCollection) {
        ih.setCreateCollection(true);
      }

      if (CreateCollectionType == "document" ||
          CreateCollectionType == "edge") {
        ih.setCreateCollectionType(CreateCollectionType);
      }

      ih.setOverwrite(Overwrite);
      ih.useBackslash(UseBackslash);

      // quote
      if (Quote.length() <= 1) {
        ih.setQuote(Quote);
      } else {
        std::cerr << "Wrong length of quote character." << std::endl;
        TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
      }

      // separator
      if (Separator.length() == 1) {
        ih.setSeparator(Separator);
      } else {
        std::cerr << "Separator must be exactly one character." << std::endl;
        TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
      }

      // collection name
      if (CollectionName == "") {
        std::cerr << "Collection name is missing." << std::endl;
        TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
      }

      // filename
      if (FileName == "") {
        std::cerr << "File name is missing." << std::endl;
        TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
      }

      if (FileName != "-" && !FileUtils::isRegularFile(FileName)) {
        if (!FileUtils::exists(FileName)) {
          std::cerr << "Cannot open file '" << FileName << "'. File not found."
                    << std::endl;
        } else if (FileUtils::isDirectory(FileName)) {
          std::cerr << "Specified file '" << FileName
                    << "' is a directory. Please use a regular file."
                    << std::endl;
        } else {
          std::cerr << "Cannot open '" << FileName << "'. Invalid file type."
                    << std::endl;
        }

        TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
      }

      // progress
      if (Progress) {
        ih.setProgress(true);
      }

      if (OnDuplicateAction != "error" && OnDuplicateAction != "update" &&
          OnDuplicateAction != "replace" && OnDuplicateAction != "ignore") {
        std::cerr
            << "Invalid value for '--on-duplicate'. Possible values: 'error', "
               "'update', 'replace', 'ignore'." << std::endl;
        TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
      }

      ih.setOnDuplicateAction(OnDuplicateAction);

      try {
        bool ok = false;

        // import type
        if (TypeImport == "csv") {
          std::cout << "Starting CSV import..." << std::endl;
          ok = ih.importDelimited(CollectionName, FileName,
                                  arangodb::v8client::ImportHelper::CSV);
        }

        else if (TypeImport == "tsv") {
          std::cout << "Starting TSV import..." << std::endl;
          ih.setQuote("");
          ih.setSeparator("\\t");
          ok = ih.importDelimited(CollectionName, FileName,
                                  arangodb::v8client::ImportHelper::TSV);
        }

        else if (TypeImport == "json") {
          std::cout << "Starting JSON import..." << std::endl;
          ok = ih.importJson(CollectionName, FileName);
        }

        else {
          std::cerr << "Wrong type '" << TypeImport << "'." << std::endl;
          TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
        }

        std::cout << std::endl;

        // give information about import
        if (ok) {
          std::cout << "created:          " << ih.getNumberCreated()
                    << std::endl;
          std::cout << "warnings/errors:  " << ih.getNumberErrors()
                    << std::endl;
          std::cout << "updated/replaced: " << ih.getNumberUpdated()
                    << std::endl;
          std::cout << "ignored:          " << ih.getNumberIgnored()
                    << std::endl;

          if (TypeImport == "csv" || TypeImport == "tsv") {
            std::cout << "lines read:       " << ih.getReadLines() << std::endl;
          }

        } else {
          std::cerr << "error message:    " << ih.getErrorMessage()
                    << std::endl;
        }
      } catch (std::exception const& ex) {
        std::cerr << "Caught exception " << ex.what() << " during import"
                  << std::endl;
      } catch (...) {
        std::cerr << "Got an unknown exception during import" << std::endl;
      }
    }
  }

  TRIAGENS_REST_SHUTDOWN;

  LocalExitFunction(ret, nullptr);

  return ret;
}
