////////////////////////////////////////////////////////////////////////////////
/// @brief avocado server
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

#include "AvocadoServer.h"

#include <v8.h>

#include "build.h"

#include "Admin/RestHandlerCreator.h"
#include "Basics/ProgramOptions.h"
#include "Basics/ProgramOptionsDescription.h"
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
#include "Rest/Initialise.h"
#include "RestHandler/RestActionHandler.h"
#include "RestHandler/RestDocumentHandler.h"
#include "RestHandler/RestSystemActionHandler.h"
#include "RestServer/ActionDispatcherThread.h"
#include "RestServer/AvocadoHttpServer.h"
#include "RestServer/JSLoader.h"
#include "RestServer/SystemActionDispatcherThread.h"
#include "V8/v8-actions.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-line-editor.h"
#include "V8/v8-shell.h"
#include "V8/v8-utils.h"
#include "V8/v8-vocbase.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::admin;
using namespace triagens::avocado;

#include "js/bootstrap/js-print.h"
#include "js/bootstrap/js-modules.h"
#include "js/server/js-modules.h"
#include "js/server/js-actions.h"
#include "js/server/js-aql.h"
#include "js/server/js-json.h"
#include "js/server/js-shell.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoDB
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
/// @brief system action loader
////////////////////////////////////////////////////////////////////////////////

static JSLoader SystemActionLoader;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief action dispatcher thread creator
////////////////////////////////////////////////////////////////////////////////

static DispatcherThread* ActionDisptacherThreadCreator (DispatcherQueue* queue) {
  return new ActionDisptacherThread(queue);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief system action dispatcher thread creator
////////////////////////////////////////////////////////////////////////////////

static DispatcherThread* SystemActionDisptacherThreadCreator (DispatcherQueue* queue) {
  return new SystemActionDisptacherThread(queue);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               class AvocadoServer
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief UnviversalVoc constructor
////////////////////////////////////////////////////////////////////////////////

AvocadoServer::AvocadoServer (int argc, char** argv)
  : _argc(argc),
    _argv(argv),
    _applicationAdminServer(0),
    _applicationHttpServer(0),
    _httpServer(0),
    _adminHttpServer(0),
    _httpPort("localhost:8529"),
    _adminPort("localhost:8530"),
    _dispatcherThreads(1),
    _startupPath(),
    _startupModules("js/modules"),
    _actionPath(),
    _systemActionPath(),
    _actionThreads(1),
    _databasePath("/var/lib/avocado"),
    _vocbase(0) {
  _workingDirectory = "/var/tmp";

#ifdef _PKGDATADIR_
  _systemActionPath = string(_PKGDATADIR_) + "/js/system";
  _startupModules = string(_PKGDATADIR_) + "/js/modules";
#endif

#ifdef _DATABASEDIR_
  _databasePath = _DATABASEDIR_;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 AnyServer methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void AvocadoServer::buildApplicationServer () {
  _applicationServer = ApplicationServerDispatcher::create("[<options>] <database-directory>", TRIAGENS_VERSION);
  _applicationServer->setSystemConfigFile("avocado.conf");
  _applicationServer->setUserConfigFile(".avocado/avocado.conf");

  // .............................................................................
  // allow multi-threading scheduler
  // .............................................................................

  _applicationServer->allowMultiScheduler(true);

  // .............................................................................
  // and start a simple admin server
  // .............................................................................

  _applicationAdminServer = ApplicationAdminServer::create(_applicationServer);
  _applicationServer->addFeature(_applicationAdminServer);

  _applicationAdminServer->allowAdminDirectory();
  _applicationAdminServer->allowLogViewer();
  _applicationAdminServer->allowVersion("avocado", TRIAGENS_VERSION);

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
    ("shell", "do not start as server, start in shell mode instead")
  ;

  additional[ApplicationServer::OPTIONS_CMDLINE + ":help-extended"]
    ("daemon", "run as daemon")
    ("supervisor", "starts a supervisor and runs as daemon")
    ("pid-file", &_pidFile, "pid-file in daemon mode")
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

  additional["DATABASE Options:help-extended"]
    ("database.directory", &_databasePath, "path to the database directory (use this option in configuration files instead of passing it via the command line)")
  ;

  // .............................................................................
  // JavaScript options
  // .............................................................................

  additional["JAVASCRIPT Options:help-admin"]
    ("startup.directory", &_startupPath, "path to the directory containing alternate startup scripts")
    ("startup.modules-path", &_startupModules, "one or more directories separated by semicolon")
    ("action.directory", &_actionPath, "path to the action directory, defaults to <database.directory>/_ACTIONS")
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
    StartupLoader.defineScript("bootstrap/modules.js", JS_bootstrap_modules);
    StartupLoader.defineScript("bootstrap/print.js", JS_bootstrap_print);
    StartupLoader.defineScript("server/modules.js", JS_server_modules);
    StartupLoader.defineScript("server/actions.js", JS_server_actions);
    StartupLoader.defineScript("server/aql.js", JS_server_aql);
    StartupLoader.defineScript("server/json.js", JS_server_json);
    StartupLoader.defineScript("server/shell.js", JS_server_shell);
  }
  else {
    LOGGER_INFO << "using JavaScript startup files at '" << _startupPath << "'";
    StartupLoader.setDirectory(_startupPath);
  }

  if (_actionPath.empty()) {
    string path = TRI_Concatenate2File(_databasePath.c_str(), "_ACTIONS");

    if (! TRI_IsDirectory(path.c_str())) {
      bool ok = TRI_ExistsFile(path.c_str());

      if (ok) {
        LOGGER_FATAL << "action directory '" << path << "' must be a directory";
        cerr << "action directory '" << path << "' must be a directory\n";
        LOGGER_INFO << "please use the '--database.directory' option";
        exit(EXIT_FAILURE);
      }

      ok = TRI_CreateDirectory(path.c_str());

      if (! ok) {
        LOGGER_FATAL << "cannot create action directory '" << path << "': " << TRI_last_error();
        cerr << "cannot create action directory '" << path << "': " << TRI_last_error() << "\n";
        LOGGER_INFO << "please use the '--database.directory' option";
        exit(EXIT_FAILURE);
      }
    }

    ActionLoader.setDirectory(path);

    LOGGER_INFO << "using database action files at '" << path << "'";
  }
  else {
    ActionLoader.setDirectory(_actionPath);
    LOGGER_INFO << "using alternate action files at '" << _actionPath << "'";
  }

  if (! _systemActionPath.empty()) {
    SystemActionLoader.setDirectory(_systemActionPath);
    LOGGER_INFO << "using system action files at '" << _systemActionPath << "'";
  }
  else {
    LOGGER_INFO << "system actions are disabled, empty system action path";
  }

  // .............................................................................
  // in shell mode ignore the rest
  // .............................................................................

  if (_applicationServer->programOptions().has("shell")) {
    executeShell();
    exit(EXIT_SUCCESS);
  }

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

int AvocadoServer::startupServer () {
  v8::HandleScope handle_scope;

  // .............................................................................
  // open the database
  // .............................................................................

  openDatabase();

  // .............................................................................
  // create the action dispatcher thread infor
  // .............................................................................

  LOGGER_INFO << "using JavaScript modules path '" << _startupModules << "'";

  ActionDisptacherThread::_actionLoader = &ActionLoader;
  ActionDisptacherThread::_startupLoader = &StartupLoader;
  ActionDisptacherThread::_vocbase = _vocbase;
  ActionDisptacherThread::_startupModules = _startupModules;

  SystemActionDisptacherThread::_actionLoader = &SystemActionLoader;

  // .............................................................................
  // create the various parts of the Avocado server
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

  safe_cast<DispatcherImpl*>(dispatcher)->addQueue("ACTION", ActionDisptacherThreadCreator, _actionThreads);
  safe_cast<DispatcherImpl*>(dispatcher)->addQueue("SYSTEM ACTION", SystemActionDisptacherThreadCreator, 2);

  // .............................................................................
  // create a http server and http handler factory
  // .............................................................................

  Scheduler* scheduler = _applicationServer->scheduler();

  if (! _httpPort.empty()) {
    HttpHandlerFactory* factory = new HttpHandlerFactory();

    vector<AddressPort> ports;
    ports.push_back(AddressPort(_httpPort));

    _applicationAdminServer->addBasicHandlers(factory);

    factory->addPrefixHandler(RestVocbaseBaseHandler::DOCUMENT_PATH, RestHandlerCreator<RestDocumentHandler>::createData<TRI_vocbase_t*>, _vocbase);
    factory->addPrefixHandler(RestVocbaseBaseHandler::ACTION_PATH, RestHandlerCreator<RestActionHandler>::createData<TRI_vocbase_t*>, _vocbase);

    _httpServer = _applicationHttpServer->buildServer(new AvocadoHttpServer(scheduler, dispatcher), factory, ports);
  }

  // .............................................................................
  // create a http server and http handler factory
  // .............................................................................

  if (! _adminPort.empty()) {
    HttpHandlerFactory* adminFactory = new HttpHandlerFactory();

    vector<AddressPort> adminPorts;
    adminPorts.push_back(AddressPort(_adminPort));

    _applicationAdminServer->addBasicHandlers(adminFactory);
    _applicationAdminServer->addHandlers(adminFactory, "/admin");

    adminFactory->addPrefixHandler(RestVocbaseBaseHandler::DOCUMENT_PATH, RestHandlerCreator<RestDocumentHandler>::createData<TRI_vocbase_t*>, _vocbase);
    adminFactory->addPrefixHandler(RestVocbaseBaseHandler::ACTION_PATH, RestHandlerCreator<RestActionHandler>::createData<TRI_vocbase_t*>, _vocbase);
    adminFactory->addPrefixHandler(RestVocbaseBaseHandler::SYSTEM_ACTION_PATH, RestHandlerCreator<RestSystemActionHandler>::createData<TRI_vocbase_t*>, _vocbase);

    adminFactory->addHandler("/", RedirectHandler::create, (void*) "/admin/index.html");

    _adminHttpServer = _applicationHttpServer->buildServer(adminFactory, adminPorts);
  }

  // .............................................................................
  // start the main event loop
  // .............................................................................

  LOGGER_INFO << "AvocadoDB (version " << TRIAGENS_VERSION << ") is ready for business";

  if (_httpPort.empty()) {
    LOGGER_WARNING << "HTTP client port not defined, maybe you want to use the 'server.http-port' option?";
  }
  else {
    LOGGER_INFO << "HTTP client port: " << _httpPort;
  }

  if (_adminPort.empty()) {
    LOGGER_INFO << "HTTP admin port not defined, maybe you want to use the 'server.admin-port' option?";
  }
  else {
    LOGGER_INFO << "HTTP admin port: " << _adminPort;
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
/// @addtogroup AvocadoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the shell
////////////////////////////////////////////////////////////////////////////////

void AvocadoServer::executeShell () {
  v8::Isolate* isolate;
  v8::Persistent<v8::Context> context;
  bool ok;
  char const* files[] = { "bootstrap/modules.js",
                          "bootstrap/print.js",
                          "server/modules.js",
                          "server/json.js",
                          "server/aql.js",
                          "server/shell.js"
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
    cerr << "cannot initialize V8 engine\n";
    exit(EXIT_FAILURE);
  }

  context->Enter();

  LOGGER_INFO << "using JavaScript modules path '" << _startupModules << "'";

  TRI_InitV8VocBridge(context, _vocbase);
  TRI_InitV8Conversions(context);
  TRI_InitV8Utils(context, _startupModules);
  TRI_InitV8Shell(context);

  // load all init files
  for (i = 0;  i < sizeof(files) / sizeof(files[0]);  ++i) {
    ok = StartupLoader.loadScript(context, files[i]);

    if (ok) {
      LOGGER_TRACE << "loaded json file '" << files[i] << "'";
    }
    else {
      LOGGER_FATAL << "cannot load json file '" << files[i] << "'";
      cerr << "cannot load json file '" << files[i] << "'\n";
      exit(EXIT_FAILURE);
    }
  }

  // run the shell
  printf("AvocadoDB shell [V8 version %s, DB version %s]\n", v8::V8::GetVersion(), TRIAGENS_VERSION);

  v8::Context::Scope contextScope(context);
  v8::Local<v8::String> name(v8::String::New("(avocado)"));

  V8LineEditor* console = new V8LineEditor(context, ".avocado");

  console->open();

  while (true) {
    while(! v8::V8::IdleNotification()) {
    }

    char* input = console->prompt("avocado> ");

    if (input == 0) {
      printf("bye...\n");
      break;
    }

    if (*input == '\0') {
      TRI_FreeString(input);
      continue;
    }

    console->addHistory(input);

    v8::HandleScope scope;

    TRI_ExecuteStringVocBase(context, v8::String::New(input), name, true, true);

    TRI_FreeString(input);
  }
  console->close();

  // and return from the context and isolate
  context->Exit();
  isolate->Exit();

  // close the database
  closeDatabase();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens the database
////////////////////////////////////////////////////////////////////////////////

void AvocadoServer::openDatabase () {
  _vocbase = TRI_OpenVocBase(_databasePath.c_str());

  if (_vocbase == 0) {
    LOGGER_FATAL << "cannot open database '" << _databasePath << "'";
    cerr << "cannot open database '" << _databasePath << "'\n";
    LOGGER_INFO << "please use the '--database.directory' option";
    exit(EXIT_FAILURE);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes the database
////////////////////////////////////////////////////////////////////////////////

void AvocadoServer::closeDatabase () {
  TRI_CloseVocBase(_vocbase);
  _vocbase = 0;
  LOGGER_INFO << "AvocadoDB has been shut down";
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
