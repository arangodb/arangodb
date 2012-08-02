////////////////////////////////////////////////////////////////////////////////
/// @brief MRuby shell
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

#include <v8.h>

#include <stdio.h>
#include <fstream>

#include "build.h"

#include "BasicsC/csv.h"

#include "Basics/ProgramOptions.h"
#include "Basics/ProgramOptionsDescription.h"
#include "Basics/StringUtils.h"
#include "BasicsC/files.h"
#include "BasicsC/init.h"
#include "BasicsC/logging.h"
#include "BasicsC/strings.h"
#include "BasicsC/terminal-utils.h"
#include "Logger/Logger.h"
#include "Rest/Initialise.h"
#include "Rest/Endpoint.h"
#include "MRClient/MRubyClientConnection.h"
#include "MRuby/MRLineEditor.h"
#include "MRuby/MRLoader.h"
#include "MRuby/mr-utils.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

#include "mruby.h"
#include "mruby/proc.h"
#include "mruby/data.h"
#include "mruby/variable.h"
#include "mruby/compile.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::httpclient;
using namespace triagens::arango;
using namespace triagens::mrclient;

#include "mr/common/bootstrap/mr-error.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief connection default values
////////////////////////////////////////////////////////////////////////////////

static int64_t const DEFAULT_REQUEST_TIMEOUT = 300;
static size_t const  DEFAULT_RETRIES = 2;
static int64_t const DEFAULT_CONNECTION_TIMEOUT = 3;

////////////////////////////////////////////////////////////////////////////////
/// @brief colors for output
////////////////////////////////////////////////////////////////////////////////

static char const DEF_RED[6]         = "\x1b[31m";
// static char const DEF_BOLD_RED[8]    = "\x1b[1;31m";
static char const DEF_GREEN[6]       = "\x1b[32m";
// static char const DEF_BOLD_GREEN[8]  = "\x1b[1;32m";
// static char const DEF_BLUE[6]        = "\x1b[34m";
// static char const DEF_BOLD_BLUE[8]   = "\x1b[1;34m";
// static char const DEF_YELLOW[8]      = "\x1b[1;33m";
// static char const DEF_WHITE[6]       = "\x1b[37m";
// static char const DEF_BOLD_WHITE[8]  = "\x1b[1;37m";
// static char const DEF_BLACK[6]       = "\x1b[30m";
// static char const DEF_BOLD_BLACK[8]  = "\x1b[1;39m";
// static char const DEF_BLINK[5]       = "\x1b[5m";
// static char const DEF_BRIGHT[5]      = "\x1b[1m";
static char const DEF_RESET[5]       = "\x1b[0m";

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup MRShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a password was specified on the command line
////////////////////////////////////////////////////////////////////////////////

static bool _hasPassword = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief endpoint
////////////////////////////////////////////////////////////////////////////////

static Endpoint* _endpoint = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief the initial default connection
////////////////////////////////////////////////////////////////////////////////

MRubyClientConnection* _clientConnection = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief connect timeout (in s) 
////////////////////////////////////////////////////////////////////////////////

static int64_t _connectTimeout = DEFAULT_CONNECTION_TIMEOUT;

////////////////////////////////////////////////////////////////////////////////
/// @brief disable auto completion
////////////////////////////////////////////////////////////////////////////////

static bool NoAutoComplete = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief deactivate colors
////////////////////////////////////////////////////////////////////////////////

static bool NoColors = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief the output pager
////////////////////////////////////////////////////////////////////////////////

static string OutputPager = "less -X -R -F -L";

////////////////////////////////////////////////////////////////////////////////
/// @brief the pager FILE 
////////////////////////////////////////////////////////////////////////////////

// static FILE* PAGER = stdout;

////////////////////////////////////////////////////////////////////////////////
/// @brief use pretty print
////////////////////////////////////////////////////////////////////////////////

static bool PrettyPrint = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief quiet start
////////////////////////////////////////////////////////////////////////////////

static bool Quiet = false;

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
/// @brief use pager
////////////////////////////////////////////////////////////////////////////////

static bool UsePager = false;

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
/// @brief print to pager
////////////////////////////////////////////////////////////////////////////////

#if 0
static void InternalPrint (const char *format, const char *str = 0) {
  if (str) {
    fprintf(PAGER, format, str);    
  }
  else {
    fprintf(PAGER, "%s", format);    
  }
}
#endif

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
/// @brief starts pager
////////////////////////////////////////////////////////////////////////////////

#if 0
static void StartPager () {
  if (! UsePager || OutputPager == "" || OutputPager == "stdout") {
    PAGER = stdout;
    return;
  }
  
  PAGER = popen(OutputPager.c_str(), "w");

  if (PAGER == 0) {
    printf("popen() failed! defaulting PAGER to stdout!\n");
    PAGER = stdout;
    UsePager = false;
  }
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief stops pager
////////////////////////////////////////////////////////////////////////////////

#if 0
static void StopPager () {
  if (PAGER != stdout) {
    pclose(PAGER);
  }
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief return a new client connection instance
////////////////////////////////////////////////////////////////////////////////
  
static MRubyClientConnection* createConnection (MR_state_t* mrs) {
  return new MRubyClientConnection(mrs,
                                   _endpoint,
                                   _username,
                                   _password, 
                                   (double) _requestTimeout, 
                                   (double) _connectTimeout, 
                                   DEFAULT_RETRIES,
                                   false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the program options
////////////////////////////////////////////////////////////////////////////////

static void ParseProgramOptions (int argc, char* argv[]) {
  string level = "info";

  ProgramOptionsDescription description("STANDARD options");

  ProgramOptionsDescription hidden("HIDDEN options");

  hidden
    ("colors", "activate color support")
    ("no-pretty-print", "disable pretty printting")          
    ("auto-complete", "enable auto completion, use no-auto-complete to disable")
  ;

  description
    ("help,h", "help message")
    ("quiet,s", "no banner")
    ("log.level,l", &level,  "log level")
    ("startup.directory", &StartupPath, "startup paths containing the Ruby files; multiple directories can be separated by cola")
    ("startup.modules-path", &StartupModules, "one or more directories separated by cola")
    ("pager", &OutputPager, "output pager")
    ("server.endpoint", &_endpointString, "endpoint to connect to, use 'none' to start without a server")
    ("server.username", &_username, "username to use when connecting")
    ("server.password", &_password, "password to use when connecting (leave empty for prompt)")
    ("server.connect-timeout", &_connectTimeout, "connect timeout in seconds")
    ("server.request-timeout", &_requestTimeout, "request timeout in seconds")
    ("use-pager", "use pager")
    ("pretty-print", "pretty print values")          
    ("no-colors", "deactivate color support")
    ("no-auto-complete", "disable auto completion")
    // ("unit-tests", &UnitTests, "do not start as shell, run unit tests instead")
    (hidden, true)
  ;

  ProgramOptions options;

  if (! options.parse(description, argc, argv)) {
    cerr << options.lastError() << "\n";
    exit(EXIT_FAILURE);
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
  
  _hasPassword =  options.has("server.password");

  // set colors
  if (options.has("colors")) {
    NoColors = false;
  }

  if (options.has("no-colors")) {
    NoColors = true;
  }

  if (options.has("auto-complete")) {
    NoAutoComplete = false;
  }

  if (options.has("no-auto-complete")) {
    NoAutoComplete = true;
  }

  if (options.has("pretty-print")) {
    PrettyPrint = true;
  }

  if (options.has("no-pretty-print")) {
    PrettyPrint = false;
  }

  if (options.has("use-pager")) {
    UsePager = true;
  }

  if (options.has("quiet")) {
    Quiet = true;
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

static void RunShell (MR_state_t* mrs) {
  MRLineEditor* console = new MRLineEditor(mrs, ".arango-mrb");

  console->open(false /*! NoAutoComplete*/);

  while (true) {
    char* input = console->prompt("arangoirb> ");

    if (input == 0) {
      break;
    }

    if (*input == '\0') {
      TRI_FreeString(TRI_CORE_MEM_ZONE, input);
      continue;
    }

    console->addHistory(input);

    struct mrb_parser_state* p = mrb_parse_nstring(&mrs->_mrb, input, strlen(input));
    TRI_FreeString(TRI_CORE_MEM_ZONE, input);

    if (p == 0 || p->tree == 0 || 0 < p->nerr) {
      cout << "UPPS!\n";
      continue;
    }

    int n = mrb_generate_code(&mrs->_mrb, p->tree);

    if (n < 0) {
      cout << "UPPS: " << n << " returned by mrb_generate_code\n";
      continue;
    }

    mrb_value result = mrb_run(&mrs->_mrb,
                               mrb_proc_new(&mrs->_mrb, mrs->_mrb.irep[n]),
                               mrb_top_self(&mrs->_mrb));

    if (mrs->_mrb.exc) {
      cout << "Caught exception:\n";
      mrb_p(&mrs->_mrb, mrb_obj_value(mrs->_mrb.exc));
      mrs->_mrb.exc = 0;
    }
    else if (! mrb_nil_p(result)) {
      mrb_p(&mrs->_mrb, result);
    }
  }

  console->close();

  cout << endl;
  if (! Quiet) {
    cout << endl << "Bye Bye! Auf Wiedersehen! さようなら" << endl;
  }
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

  // .............................................................................
  // use relative system paths
  // .............................................................................

  {
    char* binaryPath = TRI_LocateBinaryPath(argv[0]);

#ifdef TRI_ENABLE_RELATIVE_SYSTEM
  
    StartupModules = string(binaryPath) + "/../share/arango/rb/client/modules"
             + ";" + string(binaryPath) + "/../share/arango/rb/common/modules";

#else

  // .............................................................................
  // use relative development paths
  // .............................................................................

#ifdef TRI_ENABLE_RELATIVE_DEVEL

#ifdef TRI_STARTUP_MODULES_PATH
    StartupModules = TRI_STARTUP_MODULES_PATH;
#else
    StartupModules = string(binaryPath) + "/rb/client/modules"
             + ";" + string(binaryPath) + "/rb/common/modules";
#endif

#else

  // .............................................................................
  // use absolute paths
  // .............................................................................

#ifdef _PKGDATADIR_

    StartupModules = string(_PKGDATADIR_) + "/rb/client/modules"
             + ";" + string(_PKGDATADIR_) + "/rb/common/modules";

#endif

#endif
#endif

    TRI_FreeString(TRI_CORE_MEM_ZONE, binaryPath);
  }

  // .............................................................................
  // parse the program options
  // .............................................................................
  
  _endpointString = Endpoint::getDefaultEndpoint();

  ParseProgramOptions(argc, argv);
  
  // check connection args
  if (_connectTimeout <= 0) {
    cerr << "invalid value for --server.connect-timeout" << endl;
    exit(EXIT_FAILURE);
  }

  if (_requestTimeout <= 0) {
    cerr << "invalid value for --server.request-timeout" << endl;
    exit(EXIT_FAILURE);
  }
  
  if (_username.size() == 0) {
    // must specify a user name
    cerr << "no value specified for --server.username" << endl;
    exit(EXIT_FAILURE);
  }

  if (! _hasPassword) {
    // no password given on command-line
    cout << "Please specify a password:" << endl;
    // now prompt for it
#ifdef TRI_HAVE_TERMIOS_H
    TRI_SetStdinVisibility(false);
    getline(cin, _password);

    TRI_SetStdinVisibility(true);
#else
    getline(cin, _password);
#endif
  }
  
  // .............................................................................
  // set-up MRuby objects
  // .............................................................................

  // create a new ruby shell
  MR_state_t* mrs = MR_OpenShell();

  TRI_InitMRUtils(mrs);

  
  // .............................................................................
  // set-up client connection
  // .............................................................................

  // check if we want to connect to a server
  bool useServer = (_endpointString != "none");

  if (useServer) {
    _endpoint = Endpoint::clientFactory(_endpointString);

    if (_endpoint == 0) {
      cerr << "invalid value for --server.endpoint ('" << _endpointString.c_str() << "')" << endl;
      exit(EXIT_FAILURE);
    }

    assert(_endpoint);
   
    _clientConnection = createConnection(mrs);
    InitMRClientConnection(&mrs->_mrb, _clientConnection);
  }

  // .............................................................................
  // banner
  // .............................................................................  

  // http://www.network-science.de/ascii/   Font: ogre
  if (! Quiet) {
    char const* g = DEF_GREEN;
    char const* r = DEF_RED;
    char const* z = DEF_RESET;

    if (NoColors) {
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

    if (UsePager) {
      cout << "Using pager '" << OutputPager << "' for output buffering." << endl;
    }
 
    if (PrettyPrint) {
      cout << "Pretty print values." << endl;    
    }

    if (useServer) {
      if (_clientConnection->isConnected()) {
        if (! Quiet) {
          cout << "Connected to ArangoDB '" << _endpoint->getSpecification() << "' Version " << _clientConnection->getVersion() << endl; 
        }
      }
      else {
        cerr << "Could not connect to endpoint '" << _endpointString << "'" << endl;
        cerr << "Error message '" << _clientConnection->getErrorMessage() << "'" << endl;
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
    LOGGER_DEBUG << "using JavaScript startup files at '" << StartupPath << "'";
    StartupLoader.setDirectory(StartupPath);
  }

  // load all init files
  char const* files[] = {
    "common/bootstrap/error.rb"
  };
  
  for (size_t i = 0;  i < sizeof(files) / sizeof(files[0]);  ++i) {
    bool ok = StartupLoader.loadScript(&mrs->_mrb, files[i]);
    
    if (ok) {
      LOGGER_TRACE << "loaded ruby file '" << files[i] << "'";
    }
    else {
      LOGGER_ERROR << "cannot load ruby file '" << files[i] << "'";
      exit(EXIT_FAILURE);
    }
  }
  
  // .............................................................................
  // run normal shell
  // .............................................................................

  RunShell(mrs);
  
  TRIAGENS_REST_SHUTDOWN;

  return ret;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
