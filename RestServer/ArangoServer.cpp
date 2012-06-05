////////////////////////////////////////////////////////////////////////////////
/// @brief arango server
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

#include "ArangoServer.h"

#include <v8.h>

#include "build.h"

#include "Admin/RestHandlerCreator.h"
#include "Basics/FileUtils.h"
#include "Basics/ProgramOptions.h"
#include "Basics/ProgramOptionsDescription.h"
#include "Basics/Random.h"
#include "Basics/safe_cast.h"
#include "BasicsC/files.h"
#include "BasicsC/init.h"
#include "BasicsC/logging.h"
#include "BasicsC/strings.h"
#include "Dispatcher/ApplicationServerDispatcher.h"
#include "Dispatcher/DispatcherImpl.h"
#include "HttpServer/HttpHandlerFactory.h"
#include "HttpServer/RedirectHandler.h"
#include "Logger/Logger.h"

#ifdef TRI_ENABLE_MRUBY
#include "MRuby/MRLineEditor.h"
#endif

#include "Rest/Initialise.h"
#include "RestHandler/RestActionHandler.h"
#include "RestHandler/RestDocumentHandler.h"
#include "RestHandler/RestEdgeHandler.h"
#include "RestHandler/RestImportHandler.h"
#include "RestServer/ActionDispatcherThread.h"
#include "RestServer/ArangoHttpServer.h"
#include "UserManager/ApplicationUserManager.h"
#include "V8/JSLoader.h"
#include "V8/V8LineEditor.h"
#include "V8/v8-actions.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-shell.h"
#include "V8/v8-utils.h"
#include "V8/v8-query.h"
#include "V8/v8-vocbase.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::admin;
using namespace triagens::arango;

#include "js/common/bootstrap/js-modules.h"
#include "js/common/bootstrap/js-print.h"
#include "js/common/bootstrap/js-errors.h"
#include "js/server/js-ahuacatl.h"
#include "js/server/js-server.h"

#ifdef TRI_ENABLE_MRUBY
extern "C" {
#include "compile.h"
#include "mruby.h"
#include "mruby/data.h"
#include "mruby/proc.h"
#include "mruby/variable.h"
}
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief startup loader
////////////////////////////////////////////////////////////////////////////////

static JSLoader StartupLoader;

////////////////////////////////////////////////////////////////////////////////
/// @brief action loader
////////////////////////////////////////////////////////////////////////////////

static JSLoader ActionLoader;

////////////////////////////////////////////////////////////////////////////////
/// @brief allowed client actions
////////////////////////////////////////////////////////////////////////////////

static set<string> AllowedClientActions;

////////////////////////////////////////////////////////////////////////////////
/// @brief allowed admin actions
////////////////////////////////////////////////////////////////////////////////

static set<string> AllowedAdminActions;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief action dispatcher thread creator
////////////////////////////////////////////////////////////////////////////////

static DispatcherThread* ClientActionDispatcherThreadCreator (DispatcherQueue* queue) {
  return new ActionDispatcherThread(queue, "CLIENT", AllowedClientActions, &ActionLoader);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief system action dispatcher thread creator
////////////////////////////////////////////////////////////////////////////////

static DispatcherThread* SystemActionDispatcherThreadCreator (DispatcherQueue* queue) {
  return new ActionDispatcherThread(queue, "SYSTEM", AllowedAdminActions, &ActionLoader);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief define "_api" handlers
////////////////////////////////////////////////////////////////////////////////

static void DefineApiHandlers (HttpHandlerFactory* factory,
                               ApplicationAdminServer* admin, 
                               TRI_vocbase_t* vocbase) {

  // add "/version" handler
  admin->addBasicHandlers(factory, "/_api");

  // add "/document" handler
  factory->addPrefixHandler(RestVocbaseBaseHandler::DOCUMENT_PATH,
                            RestHandlerCreator<RestDocumentHandler>::createData<TRI_vocbase_t*>,
                            vocbase);

  // add "/edge" handler
  factory->addPrefixHandler(RestVocbaseBaseHandler::EDGE_PATH,
                            RestHandlerCreator<RestEdgeHandler>::createData<TRI_vocbase_t*>,
                            vocbase);

  // add import handler
  factory->addPrefixHandler(RestVocbaseBaseHandler::DOCUMENT_IMPORT_PATH,
                            RestHandlerCreator<RestImportHandler>::createData<TRI_vocbase_t*>,
                            vocbase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief define "admin" handlers
////////////////////////////////////////////////////////////////////////////////

static void DefineAdminHandlers (HttpHandlerFactory* factory,
                                 ApplicationAdminServer* admin,
                                 ApplicationUserManager* user,
                                 TRI_vocbase_t* vocbase) {

  // add "/version" handler
  admin->addBasicHandlers(factory, "/_admin");

  // add admin handlers
  admin->addHandlers(factory, "/_admin");
  user->addHandlers(factory, "/_admin");

}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               class ArangoServer
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief UnviversalVoc constructor
////////////////////////////////////////////////////////////////////////////////

ArangoServer::ArangoServer (int argc, char** argv)
  : _argc(argc),
    _argv(argv),
    _binaryPath(),
    _applicationAdminServer(0),
    _applicationHttpServer(0),
    _httpServer(0),
    _adminHttpServer(0),
    _httpPort("127.0.0.1:8529"),
    _adminPort(),
    _dispatcherThreads(1),
    _startupPath(),
    _startupModules("js/modules"),
    _systemActionPath(),
    _actionThreads(8),
    _gcInterval(1000),
    _databasePath("/var/lib/arango"),
    _removeOnDrop(true),
    _removeOnCompacted(true),
    _defaultMaximalSize(TRI_JOURNAL_DEFAULT_MAXIMAL_SIZE),
    _vocbase(0) {
  char* p;

  p = TRI_LocateBinaryPath(argv[0]);
  _binaryPath = p;

  TRI_FreeString(TRI_CORE_MEM_ZONE, p);

  // .............................................................................
  // use relative system paths
  // .............................................................................

#ifdef TRI_ENABLE_RELATIVE_SYSTEM
  
    _workingDirectory = _binaryPath + "/../tmp";
    _systemActionPath = _binaryPath + "/../share/arango/js/actions/system";
    _startupModules = _binaryPath + "/../share/arango/js/server/modules"
              + ";" + _binaryPath + "/../share/arango/js/common/modules";
    _databasePath = _binaryPath + "/../var/arango";

#else

  // .............................................................................
  // use relative development paths
  // .............................................................................

#ifdef TRI_ENABLE_RELATIVE_DEVEL

#ifdef TRI_SYSTEM_ACTION_PATH
    _systemActionPath = TRI_SYSTEM_ACTION_PATH;
#else
    _systemActionPath = _binaryPath + "/js/actions/system";
#endif

#ifdef TRI_STARTUP_MODULES_PATH
    _startupModules = TRI_STARTUP_MODULES_PATH;
#else
    _startupModules = _binaryPath + "/js/server/modules"
              + ";" + _binaryPath + "/js/common/modules";
#endif

#else

  // .............................................................................
  // use absolute paths
  // .............................................................................

    _workingDirectory = "/var/tmp";

#ifdef _PKGDATADIR_
    _systemActionPath = string(_PKGDATADIR_) + "/js/actions/system";
    _startupModules = string(_PKGDATADIR_) + "/js/server/modules"
              + ";" + string(_PKGDATADIR_) + "/js/common/modules";
#endif

#ifdef _DATABASEDIR_
    _databasePath = _DATABASEDIR_;
#endif

#endif
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 AnyServer methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ArangoServer::buildApplicationServer () {
  _applicationServer = ApplicationServerDispatcher::create("[<options>] <database-directory>", TRIAGENS_VERSION);

  _applicationServer->setUserConfigFile(".arango/arango.conf");

  // .............................................................................
  // allow multi-threading scheduler
  // .............................................................................

  _applicationServer->allowMultiScheduler(true);

  // .............................................................................
  // and start a simple admin server
  // .............................................................................

  _applicationAdminServer = ApplicationAdminServer::create(_applicationServer);
  _applicationServer->addFeature(_applicationAdminServer);

  _applicationAdminServer->allowLogViewer();
  _applicationAdminServer->allowVersion("arango", TRIAGENS_VERSION);

  // .............................................................................
  // build the application user manager
  // .............................................................................

  _applicationUserManager = ApplicationUserManager::create(_applicationServer);
  _applicationServer->addFeature(_applicationUserManager);

  // create manager role
  vector<right_t> rightsManager;
  rightsManager.push_back(RIGHT_TO_MANAGE_USER);
  rightsManager.push_back(RIGHT_TO_MANAGE_ADMIN);
      
  _applicationUserManager->createRole("manager", rightsManager, 0);
      
  // create admin role
  vector<right_t> rightsAdmin;
  rightsAdmin.push_back(RIGHT_TO_MANAGE_USER);
  rightsAdmin.push_back(RIGHT_TO_BE_DELETED);
  
  _applicationUserManager->createRole("admin", rightsAdmin, RIGHT_TO_MANAGE_ADMIN);
      
  // create user role
  vector<right_t> rightsUser;
  rightsUser.push_back(RIGHT_TO_BE_DELETED);
      
  _applicationUserManager->createRole("user", rightsUser, RIGHT_TO_MANAGE_USER);
      
  // create a standard user
  _applicationUserManager->createUser("manager", "manager");
      
  // added a anonymous right for session which are not logged in
  vector<right_t> rightsAnonymous;
  rightsAnonymous.push_back(RIGHT_TO_LOGIN);
      
  _applicationUserManager->setAnonymousRights(rightsAnonymous);
      
  // .............................................................................
  // use relative system paths
  // .............................................................................

#ifdef TRI_ENABLE_RELATIVE_SYSTEM
  
  _applicationServer->setSystemConfigFile("arango.conf", _binaryPath + "/../etc");
  _applicationAdminServer->allowAdminDirectory(_binaryPath + "/../share/arango/html/admin");

#else

  // .............................................................................
  // use relative development paths
  // .............................................................................

#ifdef TRI_ENABLE_RELATIVE_DEVEL

#ifdef TRI_HTML_ADMIN_PATH
  _applicationAdminServer->allowAdminDirectory(TRI_HTML_ADMIN_PATH);
#else
  _applicationAdminServer->allowAdminDirectory(_binaryPath + "/html/admin");
#endif

#else

  // .............................................................................
  // use absolute paths
  // .............................................................................

  _applicationServer->setSystemConfigFile("arango.conf");
  _applicationAdminServer->allowAdminDirectory(string(_PKGDATADIR_) + "/html/admin");

#endif
#endif

  // .............................................................................
  // a http server
  // .............................................................................

  _applicationHttpServer = ApplicationHttpServer::create(_applicationServer);
  _applicationServer->addFeature(_applicationHttpServer);

  // .............................................................................
  // daemon and supervisor mode
  // .............................................................................

  map<string, ProgramOptionsDescription> additional;

  additional[ApplicationServer::OPTIONS_CMDLINE]
    ("console", "do not start as server, start a JavaScript emergency console instead")
#ifdef TRI_ENABLE_MRUBY
    ("ruby-console", "do not start as server, start a Ruby emergency console instead")
#endif
    ("unit-tests", &_unitTests, "do not start as server, run unit tests instead")
    ("jslint", &_jslint, "do not start as server, run js lint instead")
  ;

  additional[ApplicationServer::OPTIONS_CMDLINE + ":help-extended"]
    ("daemon", "run as daemon")
    ("supervisor", "starts a supervisor and runs as daemon")
    ("pid-file", &_pidFile, "pid-file in daemon mode")
    ("working directory", &_workingDirectory, "working directory in daemon mode")
  ;

  // .............................................................................
  // for this server we display our own options such as port to use
  // .............................................................................

  _applicationHttpServer->showPortOptions(false);

  additional["PORT Options"]
    ("server.http-port", &_httpPort, "port for client access")
  ;

  additional[ApplicationServer::OPTIONS_HIDDEN]
    ("port", &_httpPort, "port for client access")
  ;

  // .............................................................................
  // database options
  // .............................................................................

  additional["DATABASE Options:help-admin"]
    ("database.directory", &_databasePath, "path to the database directory (use this option in configuration files instead of passing it via the command line)")
    ("database.remove-on-drop", &_removeOnDrop, "wipe a collection from disk after dropping")
    ("database.maximal-journal-size", &_defaultMaximalSize, "default maximal journal size, can be overwritten when creating a collection")
  ;

  additional["DATABASE Options:help-devel"]
    ("database.remove-on-compacted", &_removeOnCompacted, "wipe a datafile from disk after compaction")
  ;

  // .............................................................................
  // JavaScript options
  // .............................................................................

  additional["JAVASCRIPT Options:help-admin"]
    ("startup.directory", &_startupPath, "path to the directory containing alternate startup scripts")
    ("startup.modules-path", &_startupModules, "one or more directories separated by cola")
    ("gc.interval", &_gcInterval, "garbage collection interval (each x requests)")
  ;

  additional["JAVASCRIPT Options:help-admin"]
    ("action.system-directory", &_systemActionPath, "path to the system action directory")
    ("action.threads", &_actionThreads, "threads for actions")
  ;

  // .............................................................................
  // database options
  // .............................................................................

  additional["Server Options:help-admin"]
    ("server.admin-port", &_adminPort, "http server:port for ADMIN requests")
    ("dispatcher.threads", &_dispatcherThreads, "number of dispatcher threads")
  ;

  // .............................................................................
  // parse the command line options - exit if there is a parse error
  // .............................................................................

  if (! _applicationServer->parse(_argc, _argv, additional)) {
    exit(EXIT_FAILURE);
  }

  // .............................................................................
  // set directories and scripts
  // .............................................................................

  vector<string> arguments = _applicationServer->programArguments();

  if (1 < arguments.size()) {
    LOGGER_FATAL << "expected at most one database directory, got " << arguments.size();
    exit(EXIT_FAILURE);
  }
  else if (1 == arguments.size()) {
    _databasePath = arguments[0];
  }

  if (_startupPath.empty()) {
    LOGGER_INFO << "using built-in JavaScript startup files";
    StartupLoader.defineScript("common/bootstrap/modules.js", JS_common_bootstrap_modules);
    StartupLoader.defineScript("common/bootstrap/print.js", JS_common_bootstrap_print);
    StartupLoader.defineScript("common/bootstrap/errors.js", JS_common_bootstrap_errors);
    StartupLoader.defineScript("server/ahuacatl.js", JS_server_ahuacatl);
    StartupLoader.defineScript("server/server.js", JS_server_server);
  }
  else {
    LOGGER_INFO << "using JavaScript startup files at '" << _startupPath << "'";
    StartupLoader.setDirectory(_startupPath);
  }

  if (! _systemActionPath.empty()) {
    ActionLoader.setDirectory(_systemActionPath);
    LOGGER_INFO << "using action files at '" << _systemActionPath << "'";
  }
  else {
    LOGGER_INFO << "actions are disabled, empty system action path";
  }

  // .............................................................................
  // in shell mode ignore the rest
  // .............................................................................

  if (_applicationServer->programOptions().has("console")) {
    int res = executeShell(MODE_CONSOLE);
    exit(res);
  }
  else if (! _unitTests.empty()) {
    int res = executeShell(MODE_UNITTESTS);
    exit(res);
  }
  else if (_applicationServer->programOptions().has("jslint")) {
    int res = executeShell(MODE_JSLINT);
    exit(res);
  }


#ifdef TRI_ENABLE_MRUBY
  if (_applicationServer->programOptions().has("ruby-console")) {
    int res = executeRubyShell();
    exit(res);
  }
#endif

  // .............................................................................
  // sanity checks
  // .............................................................................

  if (_applicationServer->programOptions().has("daemon")) {
    _daemonMode = true;
  }

  if (_applicationServer->programOptions().has("supervisor")) {
    _supervisorMode = true;
  }

  if (_daemonMode) {
    if (_pidFile.empty()) {
      LOGGER_FATAL << "no pid-file defined, but daemon mode requested";
      cerr << "no pid-file defined, but daemon mode requested\n";
      LOGGER_INFO << "please use the '--pid-file' option";
      exit(EXIT_FAILURE);
    }
  }

  if (_databasePath.empty()) {
    LOGGER_FATAL << "no database path has been supplied, giving up";
    cerr << "no database path has been supplied, giving up\n";
    LOGGER_INFO << "please use the '--database.directory' option";
    exit(EXIT_FAILURE);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

int ArangoServer::startupServer () {
  v8::HandleScope handle_scope;

  bool useHttpPort = ! _httpPort.empty();
  bool useAdminPort = ! _adminPort.empty() && _adminPort != "-";
  bool shareAdminPort = useHttpPort && _adminPort.empty();

  // .............................................................................
  // open the database
  // .............................................................................

  openDatabase();

  // .............................................................................
  // create the action dispatcher thread infor
  // .............................................................................

  LOGGER_INFO << "using JavaScript modules path '" << _startupModules << "'";

  ActionDispatcherThread::_startupLoader = &StartupLoader;
  ActionDispatcherThread::_vocbase = _vocbase;
  ActionDispatcherThread::_startupModules = _startupModules;
  ActionDispatcherThread::_gcInterval = _gcInterval; 

  // .............................................................................
  // create the various parts of the Arango server
  // .............................................................................

  _applicationServer->buildScheduler();
  _applicationServer->buildSchedulerReporter();
  _applicationServer->buildControlCHandler();

  safe_cast<ApplicationServerDispatcher*>(_applicationServer)->buildDispatcher();
  safe_cast<ApplicationServerDispatcher*>(_applicationServer)->buildDispatcherReporter();
  safe_cast<ApplicationServerDispatcher*>(_applicationServer)->buildStandardQueue(_dispatcherThreads);

  Dispatcher* dispatcher = safe_cast<ApplicationServerDispatcher*>(_applicationServer)->dispatcher();

  if (_actionThreads < 1) {
    _actionThreads = 1;
  }

  // if we share a the server port for admin and client, only create a SYSTEM queue
  if (shareAdminPort) {
    safe_cast<DispatcherImpl*>(dispatcher)->addQueue("CLIENT", ClientActionDispatcherThreadCreator, _actionThreads);
  }

  // use a separate queue for administrator requests
  else {
    safe_cast<DispatcherImpl*>(dispatcher)->addQueue("CLIENT", ClientActionDispatcherThreadCreator, _actionThreads);

    if (useAdminPort) {
      safe_cast<DispatcherImpl*>(dispatcher)->addQueue("SYSTEM", SystemActionDispatcherThreadCreator, 2);
    }
  }

  // .............................................................................
  // create a http server and http handler factory
  // .............................................................................

  Scheduler* scheduler = _applicationServer->scheduler();

  RestActionHandler::action_options_t httpOptions;
  httpOptions._vocbase = _vocbase;
  httpOptions._queue = "CLIENT";

  if (useHttpPort) {
    HttpHandlerFactory* factory = new HttpHandlerFactory();

    AllowedClientActions.insert("user");
    AllowedClientActions.insert("api");

    vector<AddressPort> ports;
    ports.push_back(AddressPort(_httpPort));

    DefineApiHandlers(factory, _applicationAdminServer, _vocbase);

    if (shareAdminPort) {
      DefineAdminHandlers(factory, _applicationAdminServer, _applicationUserManager, _vocbase);
      AllowedClientActions.insert("admin");
    }

    // add action handler
    factory->addPrefixHandler("/", 
                              RestHandlerCreator<RestActionHandler>::createData<RestActionHandler::action_options_t*>,
                              (void*) &httpOptions);

    _httpServer = _applicationHttpServer->buildServer(new ArangoHttpServer(scheduler, dispatcher), factory, ports);
  }

  // .............................................................................
  // create a http server and http handler factory
  // .............................................................................

  RestActionHandler::action_options_t adminOptions;
  adminOptions._vocbase = _vocbase;
  adminOptions._queue = "SYSTEM";

  if (useAdminPort) {
    HttpHandlerFactory* factory = new HttpHandlerFactory();

    AllowedAdminActions.insert("api");
    AllowedAdminActions.insert("admin");

    vector<AddressPort> adminPorts;
    adminPorts.push_back(AddressPort(_adminPort));

    DefineApiHandlers(factory, _applicationAdminServer, _vocbase);
    DefineAdminHandlers(factory, _applicationAdminServer, _applicationUserManager, _vocbase);

    // add action handler
    factory->addPrefixHandler("/", 
                              RestHandlerCreator<RestActionHandler>::createData<RestActionHandler::action_options_t*>,
                              (void*) &adminOptions);

    _adminHttpServer = _applicationHttpServer->buildServer(new ArangoHttpServer(scheduler, dispatcher), factory, adminPorts);
  }

  // .............................................................................
  // start the main event loop
  // .............................................................................

  LOGGER_INFO << "ArangoDB (version " << TRIAGENS_VERSION << ") is ready for business";

  if (useHttpPort) {
    if (shareAdminPort) {
      LOGGER_INFO << "HTTP client/admin port: " << _httpPort;
    }
    else {
      LOGGER_INFO << "HTTP client port: " << _httpPort;
    }
  }
  else {
    LOGGER_WARNING << "HTTP client port not defined, maybe you want to use the 'server.http-port' option?";
  }

  if (useAdminPort) {
    LOGGER_INFO << "HTTP admin port: " << _adminPort;
  }
  else if (! shareAdminPort) {
    LOGGER_INFO << "HTTP admin port not defined, maybe you want to use the 'server.admin-port' option?";
  }

  LOGGER_INFO << "Have Fun!";

  _applicationServer->start();
  _applicationServer->wait();

  // .............................................................................
  // and cleanup
  // .............................................................................

  closeDatabase();
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the JavaScript emergency console
////////////////////////////////////////////////////////////////////////////////

int ArangoServer::executeShell (shell_operation_mode_e mode) {
  v8::Isolate* isolate;
  v8::Persistent<v8::Context> context;
  bool ok;
  char const* files[] = { "common/bootstrap/modules.js",
                          "common/bootstrap/print.js",
                          "common/bootstrap/errors.js",
                          "server/ahuacatl.js",
                          "server/server.js"
  };
  size_t i;

  // only simple logging
  TRI_ShutdownLogging();
  TRI_InitialiseLogging(false);
  TRI_CreateLogAppenderFile("+");

  // open the database
  openDatabase();

  // enter a new isolate
  isolate = v8::Isolate::New();
  isolate->Enter();

  // global scope
  v8::HandleScope globalScope;

  // create the context
  context = v8::Context::New(0);

  if (context.IsEmpty()) {
    LOGGER_FATAL << "cannot initialize V8 engine";
    TRI_FlushLogging();
    return EXIT_FAILURE;
  }

  context->Enter();

  LOGGER_INFO << "using JavaScript modules path '" << _startupModules << "'";

  TRI_v8_global_t* v8g = TRI_InitV8VocBridge(context, _vocbase);
  TRI_InitV8Queries(context);
  TRI_InitV8Conversions(context);
  TRI_InitV8Utils(context, _startupModules);
  TRI_InitV8Shell(context);

  // load all init files
  for (i = 0;  i < sizeof(files) / sizeof(files[0]);  ++i) {
    ok = StartupLoader.loadScript(context, files[i]);

    if (ok) {
      LOGGER_TRACE << "loaded js file '" << files[i] << "'";
    }
    else {
      LOGGER_FATAL << "cannot load js file '" << files[i] << "'";
      TRI_FlushLogging();
      return EXIT_FAILURE;
    }
  }

  // run the shell
  printf("ArangoDB shell [V8 version %s, DB version %s]\n", v8::V8::GetVersion(), TRIAGENS_VERSION);

  v8::Local<v8::String> name(v8::String::New("(arango)"));
  v8::Context::Scope contextScope(context);

  ok = true;

  switch (mode) {
    case MODE_UNITTESTS: {
      // run all unit tests
      v8::HandleScope scope;
      v8::TryCatch tryCatch;

      // set-up unit tests array
      v8::Handle<v8::Array> sysTestFiles = v8::Array::New();

      for (size_t i = 0;  i < _unitTests.size();  ++i) {
        sysTestFiles->Set((uint32_t) i, v8::String::New(_unitTests[i].c_str()));
      }

      context->Global()->Set(v8::String::New("SYS_UNIT_TESTS"), sysTestFiles);
      context->Global()->Set(v8::String::New("SYS_UNIT_TESTS_RESULT"), v8::True());

      // run tests
      char const* input = "require(\"jsunity\").runCommandLineTests();";
      TRI_ExecuteJavaScriptString(context, v8::String::New(input), name, true);
      
      if (tryCatch.HasCaught()) {
        cout << TRI_StringifyV8Exception(&tryCatch);
        ok = false;
      }
      else {
        ok = TRI_ObjectToBoolean(context->Global()->Get(v8::String::New("SYS_UNIT_TESTS_RESULT")));
      }

      break;
    }

    case MODE_JSLINT: {
      // run jslint
      v8::HandleScope scope;
      v8::TryCatch tryCatch;
      
      // set-up tests files array
      v8::Handle<v8::Array> sysTestFiles = v8::Array::New();
      for (size_t i = 0;  i < _jslint.size();  ++i) {
        sysTestFiles->Set((uint32_t) i, v8::String::New(_jslint[i].c_str()));
      }
      
      context->Global()->Set(v8::String::New("SYS_UNIT_TESTS"), sysTestFiles);
      context->Global()->Set(v8::String::New("SYS_UNIT_TESTS_RESULT"), v8::True());

      char const* input = "require(\"jslint\").runCommandLineTests({ });";
      TRI_ExecuteJavaScriptString(context, v8::String::New(input), name, true);
      if (tryCatch.HasCaught()) {
        cout << TRI_StringifyV8Exception(&tryCatch);
        ok = false;
      }
      else {
        ok = TRI_ObjectToBoolean(context->Global()->Get(v8::String::New("SYS_UNIT_TESTS_RESULT")));
      }

      break;
    }

    case MODE_CONSOLE: {
      // run a shell
      V8LineEditor* console = new V8LineEditor(context, ".arango");

      console->open(true);

      while (true) {
        while(! v8::V8::IdleNotification()) {
        }

        char* input = console->prompt("arangod> ");
 
        if (input == 0) {
          printf("<ctrl-D>\nBye Bye! Auf Wiedersehen! До свидания! さようなら\n");
          break;
        }

        if (*input == '\0') {
          TRI_FreeString(TRI_CORE_MEM_ZONE, input);
          continue;
        }

        console->addHistory(input);

        v8::HandleScope scope;
        v8::TryCatch tryCatch;

        TRI_ExecuteJavaScriptString(context, v8::String::New(input), name, true);
        TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, input);
      
        if (tryCatch.HasCaught()) {
          cout << TRI_StringifyV8Exception(&tryCatch);
        }
      }

      console->close();

      delete console;

      break;
    }

  }
                      

  // and return from the context and isolate
  context->Exit();
  isolate->Exit();

  if (v8g) {
    delete v8g;
  }

  // close the database
  closeDatabase();
  Random::shutdown();

  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the ruby emergency console
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MRUBY

struct RClass* ArangoDatabaseClass;
struct RClass* ArangoEdgesClass;
struct RClass* ArangoCollectionClass;
struct RClass* ArangoEdgesCollectionClass;

mrb_value MR_ArangoDatabase_Inialize (mrb_state* mrb, mrb_value exc) {
  printf("initializer of ArangoDatabase called\n");
  return exc;
}

static void MR_ArangoDatabase_Free (mrb_state* mrb, void* p) {
  printf("free of ArangoDatabase called\n");
}

static const struct mrb_data_type MR_ArangoDatabase_Type = {
  "ArangoDatabase", MR_ArangoDatabase_Free
};

static void MR_ArangoCollection_Free (mrb_state* mrb, void* p) {
  printf("free of ArangoCollection called\n");
}

static const struct mrb_data_type MR_ArangoCollection_Type = {
  "ArangoDatabase", MR_ArangoCollection_Free
};

mrb_value MR_ArangoDatabase_Collection (mrb_state* mrb, mrb_value self) {
  char* name;
  TRI_vocbase_t* vocbase;
  TRI_vocbase_col_t* collection;
  struct RData* rdata;

  // check "class.c" to see how to specify the arguments
  mrb_get_args(mrb, "s", &name);

  if (name == 0) {
    return self;
  }

  // check 

  printf("using collection '%s'\n", name);

  // looking at "mruby.h" I assume that is the way to unwrap the pointer
  rdata = (struct RData*) mrb_object(self);
  vocbase = (TRI_vocbase_t*) rdata->data;
  collection = TRI_FindCollectionByNameVocBase(vocbase, name, false);

  if (collection == NULL) {
    printf("unknown collection (TODO raise error)\n");
    return self;
  }
  
  return mrb_obj_value(Data_Wrap_Struct(mrb, ArangoCollectionClass, &MR_ArangoCollection_Type, (void*) collection));
}


int ArangoServer::executeRubyShell () {
  struct mrb_parser_state* p;
  MR_state_t mrs;
  mrb_state* mrb;
  int n;

  // only simple logging
  TRI_ShutdownLogging();
  TRI_InitialiseLogging(false);
  TRI_CreateLogAppenderFile("+");

  // open the database
  openDatabase();

  // create a new ruby shell
  mrb = mrb_open();

  memcpy(&mrs, mrb, sizeof(mrb_state));

  // create a line editor
  MRLineEditor* console = new MRLineEditor(&mrs, ".arango-mrb");

  // setup the classes
#if 0
  struct RClass* ArangoDatabaseClass = mrb_define_class(mrb, "ArangoDatabase", mrb->object_class);
  struct RClass* ArangoEdgesClass = mrb_define_class(mrb, "ArangoEdges", mrb->object_class);
  struct RClass* ArangoCollectionClass = mrb_define_class(mrb, "ArangoCollection", mrb->object_class);
  struct RClass* ArangoEdgesCollectionClass = mrb_define_class(mrb, "ArangoEdgesCollection", mrb->object_class);

  // add an initializer (for TESTING only)
  mrb_define_method(mrb, ArangoDatabaseClass, "initialize", MR_ArangoDatabase_Inialize, ARGS_ANY());

  // add a method to extract the collection
  mrb_define_method(mrb, ArangoDatabaseClass, "_collection", MR_ArangoDatabase_Collection, ARGS_ANY());

  // create the database variable
  mrb_value db = mrb_obj_value(Data_Wrap_Struct(mrb, ArangoDatabaseClass, &MR_ArangoDatabase_Type, (void*) _vocbase));

  mrb_gv_set(mrb, mrb_intern(mrb, "$db"), db);

  // read-eval-print loop
  mrb_define_const(mrb, "$db", db);
#endif

  console->open(true);
  
  while (true) {
    char* input = console->prompt("avocmrb> ");

    if (input == 0) {
      printf("\nBye Bye! Auf Wiedersehen! さようなら\n");
      break;
    }

    if (*input == '\0') {
      TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, input);
      continue;
    }

    console->addHistory(input);

    p = mrb_parse_string(mrb, input);
    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, input);

    if (p == 0 || p->tree == 0 || 0 < p->nerr) {
      cout << "UPPS!\n";
      continue;
    }

    n = mrb_generate_code(mrb, p->tree);

    if (n < 0) {
      cout << "UPPS 2: " << n << "\n";
      continue;
    }

    mrb_value result = mrb_run(mrb, mrb_proc_new(mrb, mrb->irep[n]), mrb_nil_value());

    if (mrb->exc) {
      cout << "OUTPUT EXCEPTION\n";
      mrb_funcall(mrb, mrb_nil_value(), "p", 1, mrb_obj_value(mrb->exc));
      cout << "\nOUTPUT END\n";
    }
    else {
      mrb_funcall(mrb, mrb_nil_value(), "p", 1, result);
    }
  }
  
  Random::shutdown();

  return EXIT_SUCCESS;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief opens the database
////////////////////////////////////////////////////////////////////////////////

void ArangoServer::openDatabase () {
  _vocbase = TRI_OpenVocBase(_databasePath.c_str());

  if (! _vocbase) {
    LOGGER_FATAL << "cannot open database '" << _databasePath << "'";
    LOGGER_INFO << "please use the '--database.directory' option";
    TRI_FlushLogging();
  
    ApplicationUserManager::unloadUsers();
    ApplicationUserManager::unloadRoles();
    exit(EXIT_FAILURE);
  }

  _vocbase->_removeOnDrop = _removeOnDrop;
  _vocbase->_removeOnCompacted = _removeOnCompacted;
  _vocbase->_defaultMaximalSize = _defaultMaximalSize;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes the database
////////////////////////////////////////////////////////////////////////////////

void ArangoServer::closeDatabase () {
  ApplicationUserManager::unloadUsers();
  ApplicationUserManager::unloadRoles();

  TRI_DestroyVocBase(_vocbase);
  _vocbase = 0;
  LOGGER_INFO << "ArangoDB has been shut down";
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
