////////////////////////////////////////////////////////////////////////////////
/// @brief MRuby shell
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

#include <mruby.h>
#include <mruby/proc.h>
#include <mruby/data.h>
#include <mruby/variable.h>
#include <mruby/compile.h>

#include <stdio.h>
#include <fstream>

#include "build.h"

#include "ArangoShell/ArangoClient.h"
#include "Basics/FileUtils.h"
#include "Basics/ProgramOptions.h"
#include "Basics/ProgramOptionsDescription.h"
#include "Basics/StringUtils.h"
#include "BasicsC/csv.h"
#include "BasicsC/files.h"
#include "BasicsC/init.h"
#include "BasicsC/logging.h"
#include "BasicsC/shell-colors.h"
#include "BasicsC/tri-strings.h"
#include "BasicsC/terminal-utils.h"
#include "Logger/Logger.h"
#include "MRClient/MRubyClientConnection.h"
#include "MRuby/MRLineEditor.h"
#include "MRuby/MRLoader.h"
#include "MRuby/mr-utils.h"
#include "Rest/Endpoint.h"
#include "Rest/InitialiseRest.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::httpclient;
using namespace triagens::arango;
using namespace triagens::mrclient;

#include "mr/common/bootstrap/mr-error.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup MRShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief base class for clients
////////////////////////////////////////////////////////////////////////////////

ArangoClient BaseClient;

////////////////////////////////////////////////////////////////////////////////
/// @brief the initial default connection
////////////////////////////////////////////////////////////////////////////////

MRubyClientConnection* ClientConnection = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief startup MR files
////////////////////////////////////////////////////////////////////////////////

static MRLoader StartupLoader;

////////////////////////////////////////////////////////////////////////////////
/// @brief path for Ruby modules files
////////////////////////////////////////////////////////////////////////////////

static string StartupModules = "";

////////////////////////////////////////////////////////////////////////////////
/// @brief path for Ruby bootstrap files
////////////////////////////////////////////////////////////////////////////////

static string StartupPath = "";

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    ruby functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup MRShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "httpGet"
////////////////////////////////////////////////////////////////////////////////

static mrb_value ClientConnection_httpGet (mrb_state* mrb, mrb_value self) {
  char* url;
  /* int res; */
  size_t l;
  struct RData* rdata;
  MRubyClientConnection* connection;

 /* res = */ mrb_get_args(mrb, "s", &url, &l);

  if (url == 0) {
    return self;
  }

  // looking at "mruby.h" I assume that is the way to unwrap the pointer
  rdata = (struct RData*) mrb_object(self);
  connection = (MRubyClientConnection*) rdata->data;

  if (connection == NULL) {
    printf("unknown connection (TODO raise error)\n");
    return self;
  }

  // check header fields
  map<string, string> headerFields;

  // and execute
  return connection->getData(url, headerFields);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup MRShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief return a new client connection instance
////////////////////////////////////////////////////////////////////////////////

static MRubyClientConnection* createConnection (mrb_state* mrb) {
  return new MRubyClientConnection(mrb,
                                   BaseClient.endpointServer(),
                                   BaseClient.username(),
                                   BaseClient.password(),
                                   BaseClient.requestTimeout(),
                                   BaseClient.connectTimeout(),
                                   ArangoClient::DEFAULT_RETRIES,
                                   false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the program options
////////////////////////////////////////////////////////////////////////////////

static void ParseProgramOptions (int argc, char* argv[]) {
  ProgramOptionsDescription description("STANDARD options");
  ProgramOptionsDescription ruby("RUBY options");

  ruby
    ("ruby.directory", &StartupPath, "startup paths containing the Ruby files; multiple directories can be separated by cola")
    ("ruby.modules-path", &StartupModules, "one or more directories separated by cola")
  ;

  description
    (ruby, false)
  ;

  // fill in used options
  BaseClient.setupGeneral(description);
  BaseClient.setupServer(description);

  // and parse the command line and config file
  ProgramOptions options;
  BaseClient.parse(options, description, argc, argv, "arangoirb.conf");

  // check module path
  if (StartupModules.empty()) {
    LOGGER_FATAL_AND_EXIT("module path not known, please use '--ruby.modules-path'");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set-up the connection functions
////////////////////////////////////////////////////////////////////////////////

static void MR_ArangoConnection_Free (mrb_state* mrb, void* p) {
  printf("free of ArangoCollection called\n");
}

static const struct mrb_data_type MR_ArangoConnection_Type = {
  "ArangoConnection", MR_ArangoConnection_Free
};

static void InitMRClientConnection (mrb_state* mrb, MRubyClientConnection* connection) {
  struct RClass *rcl;

  // .............................................................................
  // arango client connection
  // .............................................................................

  rcl = mrb_define_class(mrb, "ArangoConnection", mrb->object_class);

  mrb_define_method(mrb, rcl, "get", ClientConnection_httpGet, ARGS_REQ(1));

  // create the connection variable
  mrb_value arango = mrb_obj_value(Data_Wrap_Struct(mrb, rcl, &MR_ArangoConnection_Type, (void*) connection));
  mrb_gv_set(mrb, mrb_intern(mrb, "$arango"), arango);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the shell
////////////////////////////////////////////////////////////////////////////////

static void RunShell (mrb_state* mrb) {
  MRLineEditor console(mrb, ".arangoirb");

  console.open(false /*! NoAutoComplete*/);

  while (true) {
    char* input = console.prompt("arangoirb> ");

    if (input == 0) {
      break;
    }

    if (*input == '\0') {
      TRI_FreeString(TRI_CORE_MEM_ZONE, input);
      continue;
    }

    console.addHistory(input);

    struct mrb_parser_state* p = mrb_parse_nstring(mrb, input, strlen(input), NULL);
    TRI_FreeString(TRI_CORE_MEM_ZONE, input);

    if (p == 0 || p->tree == 0 || 0 < p->nerr) {
      cout << "UPPS!\n";
      continue;
    }

    int n = mrb_generate_code(mrb, p);

    if (n < 0) {
      cout << "UPPS: " << n << " returned by mrb_generate_code\n";
      continue;
    }

    mrb_value result = mrb_run(mrb,
                               mrb_proc_new(mrb, mrb->irep[n]),
                               mrb_top_self(mrb));

    if (mrb->exc) {
      cout << "Caught exception:\n";
      mrb_p(mrb, mrb_obj_value(mrb->exc));
      mrb->exc = 0;
    }
    else if (! mrb_nil_p(result)) {
      mrb_p(mrb, result);
    }
  }

  console.close();

  cout << endl;

  BaseClient.printByeBye();
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup MRShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief main
////////////////////////////////////////////////////////////////////////////////

int main (int argc, char* argv[]) {
  TRIAGENS_C_INITIALISE(argc, argv);
  TRIAGENS_REST_INITIALISE(argc, argv);

  TRI_InitialiseLogging(false);
  int ret = EXIT_SUCCESS;

  BaseClient.setEndpointString(Endpoint::getDefaultEndpoint());

  // .............................................................................
  // parse the program options
  // .............................................................................

  ParseProgramOptions(argc, argv);

  // .............................................................................
  // set-up MRuby objects
  // .............................................................................

  // create a new ruby shell
  mrb_state* mrb = MR_OpenShell();

  TRI_InitMRUtils(mrb);

  // .............................................................................
  // set-up client connection
  // .............................................................................

  // check if we want to connect to a server
  bool useServer = (BaseClient.endpointString() != "none");

  if (useServer) {
    BaseClient.createEndpoint();

    if (BaseClient.endpointServer() == 0) {
      LOGGER_FATAL_AND_EXIT("invalid value for --server.endpoint ('" << BaseClient.endpointString() << "')");
    }

    ClientConnection = createConnection(mrb);
    InitMRClientConnection(mrb, ClientConnection);
  }

  // .............................................................................
  // banner
  // .............................................................................

  // http://www.network-science.de/ascii/   Font: ogre
  if (! BaseClient.quiet()) {
    char const* g = TRI_SHELL_COLOR_GREEN;
    char const* r = TRI_SHELL_COLOR_RED;
    char const* z = TRI_SHELL_COLOR_RESET;

    if (! BaseClient.colors()) {
      g = "";
      r = "";
      z = "";
    }

    printf("%s                                  %s _      _     %s\n", g, r, z);
    printf("%s  __ _ _ __ __ _ _ __   __ _  ___ %s(_)_ __| |__  %s\n", g, r, z);
    printf("%s / _` | '__/ _` | '_ \\ / _` |/ _ \\%s| | '__| '_ \\ %s\n", g, r, z);
    printf("%s| (_| | | | (_| | | | | (_| | (_) %s| | |  | |_) |%s\n", g, r, z);
    printf("%s \\__,_|_|  \\__,_|_| |_|\\__, |\\___/%s|_|_|  |_.__/ %s\n", g, r, z);
    printf("%s                       |___/      %s              %s\n", g, r, z);

    cout << endl << "Welcome to arangosh " << TRIAGENS_VERSION << ". Copyright (c) 2012 triAGENS GmbH" << endl;

#ifdef TRI_V8_VERSION
    cout << "Using MRUBY " << TRI_MRUBY_VERSION << " engine. Copyright (c) 2012 mruby developers." << endl;
#endif

#ifdef TRI_READLINE_VERSION
    cout << "Using READLINE " << TRI_READLINE_VERSION << endl;
#endif

    cout << endl;

    BaseClient.printWelcomeInfo();

    if (useServer) {
      if (ClientConnection->isConnected()) {
        if (! BaseClient.quiet()) {
          cout << "Connected to ArangoDB '" << BaseClient.endpointServer()->getSpecification()
               << "' Version " << ClientConnection->getVersion() << endl;
        }
      }
      else {
        cerr << "Could not connect to endpoint '" << BaseClient.endpointString() << "'" << endl;
        cerr << "Error message '" << ClientConnection->getErrorMessage() << "'" << endl;
      }
    }
  }

  // .............................................................................
  // read files
  // .............................................................................

  // load java script from js/bootstrap/*.h files
  if (StartupPath.empty()) {
    StartupLoader.defineScript("common/bootstrap/error.rb", MR_common_bootstrap_error);
  }
  else {
    LOGGER_DEBUG("using Ruby startup files at '" << StartupPath << "'");
    StartupLoader.setDirectory(StartupPath);
  }

  // load all init files
  char const* files[] = {
    "common/bootstrap/error.rb"
  };

  for (size_t i = 0;  i < sizeof(files) / sizeof(files[0]);  ++i) {
    bool ok = StartupLoader.loadScript(mrb, files[i]);

    if (ok) {
      LOGGER_TRACE("loaded ruby file '" << files[i] << "'");
    }
    else {
      LOGGER_FATAL_AND_EXIT("cannot load ruby file '" << files[i] << "'");
    }
  }

  // .............................................................................
  // run normal shell
  // .............................................................................

  RunShell(mrb);

  TRIAGENS_REST_SHUTDOWN;

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
