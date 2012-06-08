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
#include "MRuby/MRLoader.h"
#include "MRuby/mr-actions.h"
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
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-shell.h"
#include "V8/v8-utils.h"
#include "V8Server/v8-actions.h"
#include "V8Server/v8-query.h"
#include "V8Server/v8-vocbase.h"

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
#include "mruby.h"
#include "mruby/compile.h"
#include "mruby/data.h"
#include "mruby/proc.h"
#include "mruby/variable.h"

#include "mr/common/bootstrap/mr-error.h"
#include "mr/server/mr-server.h"
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief vocbase
////////////////////////////////////////////////////////////////////////////////

static TRI_vocbase_t* Vocbase;

////////////////////////////////////////////////////////////////////////////////
/// @brief V8 gc interval
////////////////////////////////////////////////////////////////////////////////

static uint64_t GcIntervalJS;

////////////////////////////////////////////////////////////////////////////////
/// @brief V8 module path
////////////////////////////////////////////////////////////////////////////////

static string StartupModulesJS;

////////////////////////////////////////////////////////////////////////////////
/// @brief V8 startup loader
////////////////////////////////////////////////////////////////////////////////

static JSLoader StartupLoaderJS;

////////////////////////////////////////////////////////////////////////////////
/// @brief V8 action loader
////////////////////////////////////////////////////////////////////////////////

static JSLoader ActionLoaderJS;

////////////////////////////////////////////////////////////////////////////////
/// @brief allowed client actions
////////////////////////////////////////////////////////////////////////////////

static set<string> AllowedClientActions;

////////////////////////////////////////////////////////////////////////////////
/// @brief allowed admin actions
////////////////////////////////////////////////////////////////////////////////

static set<string> AllowedAdminActions;

////////////////////////////////////////////////////////////////////////////////
/// @brief startup loader
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MRUBY
static MRLoader StartupLoaderMR;
#endif

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
/// @brief JavaScript action dispatcher thread creator
////////////////////////////////////////////////////////////////////////////////

static DispatcherThread* ClientActionDispatcherThreadCreatorJS (DispatcherQueue* queue) {
  return new ActionDispatcherThread(queue,
                                    Vocbase,
                                    GcIntervalJS,
                                    "CLIENT-JAVASCRIPT",
                                    AllowedClientActions,
                                    StartupModulesJS,
                                    &StartupLoaderJS,
                                    &ActionLoaderJS);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript system action dispatcher thread creator
////////////////////////////////////////////////////////////////////////////////

static DispatcherThread* SystemActionDispatcherThreadCreatorJS (DispatcherQueue* queue) {
  return new ActionDispatcherThread(queue,
                                    Vocbase,
                                    GcIntervalJS,
                                    "SYSTEM-JAVASCRIPT",
                                    AllowedAdminActions,
                                    StartupModulesJS,
                                    &StartupLoaderJS,
                                    &ActionLoaderJS);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript action dispatcher thread creator
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MRUBY

static DispatcherThread* ClientActionDispatcherThreadCreatorMR (DispatcherQueue* queue) {
  return new ActionDispatcherThread(queue,
                                    Vocbase,
                                    GcIntervalJS,
                                    "CLIENT-RUBY",
                                    AllowedClientActions,
                                    StartupModulesJS,
                                    &StartupLoaderJS,
                                    &ActionLoaderJS);
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript system action dispatcher thread creator
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MRUBY

static DispatcherThread* SystemActionDispatcherThreadCreatorMR (DispatcherQueue* queue) {
  return new ActionDispatcherThread(queue,
                                    Vocbase,
                                    GcIntervalJS,
                                    "SYSTEM-RUBY",
                                    AllowedAdminActions,
                                    StartupModulesJS,
                                    &StartupLoaderJS,
                                    &ActionLoaderJS);
}

#endif

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
/// @brief constructor
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
    _dispatcherThreads(8),
    _startupPathJS(),
    _startupModulesJS("js/modules"),
    _actionPathJS(),
    _actionThreadsJS(8),
    _gcIntervalJS(1000),
#ifdef TRI_ENABLE_MRUBY
    _startupPathMR(),
    _actionThreadsMR(8),
#endif
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
    _actionPathJS = _binaryPath + "/../share/arango/js/actions/system";
    _startupModulesJS = _binaryPath + "/../share/arango/js/server/modules"
              + ";" + _binaryPath + "/../share/arango/js/common/modules";
    _databasePath = _binaryPath + "/../var/arango";

#else

  // .............................................................................
  // use relative development paths
  // .............................................................................

#ifdef TRI_ENABLE_RELATIVE_DEVEL

#ifdef TRI_SYSTEM_ACTION_PATH
    _actionPathJS = TRI_SYSTEM_ACTION_PATH;
#else
    _actionPathJS = _binaryPath + "/../js/actions/system";
#endif

#ifdef TRI_STARTUP_MODULES_PATH
    _startupModulesJS = TRI_STARTUP_MODULES_PATH;
#else
    _startupModulesJS = _binaryPath + "/../js/server/modules"
              + ";" + _binaryPath + "/../js/common/modules";
#endif

#else

  // .............................................................................
  // use absolute paths
  // .............................................................................

    _workingDirectory = "/var/tmp";

#ifdef _PKGDATADIR_
    _actionPathJS = string(_PKGDATADIR_) + "/js/actions/system";
    _startupModulesJS = string(_PKGDATADIR_) + "/js/server/modules"
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
  _applicationAdminServer->allowAdminDirectory(_binaryPath + "/../html/admin");
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

  additional["DIRECTORY Options:help-admin"]
    ("database.directory", &_databasePath, "path to the database directory")
  ;

  additional["DATABASE Options:help-admin"]
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
    ("javascript.gc-interval", &_gcIntervalJS, "JavaScript garbage collection interval (each x requests)")
  ;

  additional["DIRECTORY Options:help-admin"]
    ("javascript.action-directory", &_actionPathJS, "path to the JavaScript action directory")
    ("javascript.startup-directory", &_startupPathJS, "path to the directory containing alternate JavaScript startup scripts")
    ("javascript.modules-path", &_startupModulesJS, "one or more directories separated by (semi-) colons")
  ;

  additional["THREAD Options:help-admin"]
    ("javascript.action-threads", &_actionThreadsJS, "number of threads for JavaScript actions")
  ;

  // .............................................................................
  // JavaScript options
  // .............................................................................

#ifdef TRI_ENABLE_MRUBY

  additional["DIRECTORY Options:help-admin"]
    ("ruby.startup-directory", &_startupPathMR, "path to the directory containing alternate MRuby startup scripts")
  ;

  additional["THREAD Options:help-admin"]
    ("ruby.action-threads", &_actionThreadsMR, "number of threads for MRuby actions")
  ;

#endif

  // .............................................................................
  // database options
  // .............................................................................

  additional["Server Options:help-admin"]
    ("server.admin-port", &_adminPort, "http server:port for ADMIN requests")
  ;

  additional["THREAD Options:help-admin"]
    ("server.threads", &_dispatcherThreads, "number of threads for basic operations")
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

  if (_startupPathJS.empty()) {
    LOGGER_INFO << "using built-in JavaScript startup files";
    StartupLoaderJS.defineScript("common/bootstrap/modules.js", JS_common_bootstrap_modules);
    StartupLoaderJS.defineScript("common/bootstrap/print.js", JS_common_bootstrap_print);
    StartupLoaderJS.defineScript("common/bootstrap/errors.js", JS_common_bootstrap_errors);
    StartupLoaderJS.defineScript("server/ahuacatl.js", JS_server_ahuacatl);
    StartupLoaderJS.defineScript("server/server.js", JS_server_server);
  }
  else {
    LOGGER_INFO << "using JavaScript startup files at '" << _startupPathJS << "'";
    StartupLoaderJS.setDirectory(_startupPathJS);
  }

  if (! _actionPathJS.empty()) {
    ActionLoaderJS.setDirectory(_actionPathJS);
    LOGGER_INFO << "using action files at '" << _actionPathJS << "'";
  }
  else {
    LOGGER_INFO << "actions are disabled, empty system action path";
  }

#ifdef TRI_ENABLE_MRUBY

  if (_startupPathMR.empty()) {
    LOGGER_INFO << "using built-in MRuby startup files";
    StartupLoaderMR.defineScript("common/bootstrap/error.rb", MR_common_bootstrap_error);
    StartupLoaderMR.defineScript("server/server.rb", MR_server_server);
  }
  else {
    LOGGER_INFO << "using MRuby startup files at '" << _startupPathMR << "'";
    StartupLoaderMR.setDirectory(_startupPathMR);
  }

#endif

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

  LOGGER_INFO << "using JavaScript modules path '" << _startupModulesJS << "'";

  Vocbase = _vocbase;
  StartupModulesJS = _startupModulesJS;
  GcIntervalJS = _gcIntervalJS; 

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

  if (_actionThreadsJS < 1) {
    _actionThreadsJS = 1;
  }

  // if we share a the server port for admin and client, only create a CLIENT queue
  if (0 < _actionThreadsJS) {
    safe_cast<DispatcherImpl*>(dispatcher)->addQueue("CLIENT-JAVASCRIPT", ClientActionDispatcherThreadCreatorJS, _actionThreadsJS);
  }

#if TRI_ENABLE_MRUBY
  if (0 < _actionThreadsMR) {
    safe_cast<DispatcherImpl*>(dispatcher)->addQueue("CLIENT-RUBY", ClientActionDispatcherThreadCreatorMR, _actionThreadsMR);
  }
#endif

  // use a separate queue for administrator requests
  if (! shareAdminPort) {
    if (useAdminPort) {
      safe_cast<DispatcherImpl*>(dispatcher)->addQueue("SYSTEM-JAVASCRIPT", SystemActionDispatcherThreadCreatorJS, 2);

#if TRI_ENABLE_MRUBY
      safe_cast<DispatcherImpl*>(dispatcher)->addQueue("SYSTEM-RUBY", SystemActionDispatcherThreadCreatorMR, 2);
#endif
    }
  }

  // .............................................................................
  // create a http server and http handler factory
  // .............................................................................

  Scheduler* scheduler = _applicationServer->scheduler();

  RestActionHandler::action_options_t httpOptions;
  httpOptions._vocbase = _vocbase;
  httpOptions._queue = "CLIENT-JAVASCRIPT";

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
  adminOptions._queue = "SYSTEM-JAVASCRIPT";

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

  LOGGER_INFO << "using JavaScript modules path '" << _startupModulesJS << "'";

  TRI_v8_global_t* v8g = TRI_InitV8VocBridge(context, _vocbase);
  TRI_InitV8Queries(context);
  TRI_InitV8Conversions(context);
  TRI_InitV8Utils(context, _startupModulesJS);
  TRI_InitV8Shell(context);

  // load all init files
  for (i = 0;  i < sizeof(files) / sizeof(files[0]);  ++i) {
    ok = StartupLoaderJS.loadScript(context, files[i]);

    if (ok) {
      LOGGER_TRACE << "loaded JavaScript file '" << files[i] << "'";
    }
    else {
      LOGGER_FATAL << "cannot load JavaScript file '" << files[i] << "'";
      TRI_FlushLogging();
      return EXIT_FAILURE;
    }
  }

  // run the shell
  printf("ArangoDB JavaScript shell [V8 version %s, DB version %s]\n", v8::V8::GetVersion(), TRIAGENS_VERSION);

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
/// @brief executes the MRuby emergency console
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


int ArangoServer::executeRubyShell () {
  struct mrb_parser_state* p;
  size_t i;
  char const* files[] = { "common/bootstrap/error.rb",
                          "server/server.rb"
  };

  // only simple logging
  TRI_ShutdownLogging();
  TRI_InitialiseLogging(false);
  TRI_CreateLogAppenderFile("+");

  // open the database
  openDatabase();

  // create a new ruby shell
  MR_state_t* mrs = MR_OpenShell();

  TRI_InitMRUtils(mrs);
  TRI_InitMRActions(mrs);

  // load all init files
  for (i = 0;  i < sizeof(files) / sizeof(files[0]);  ++i) {
    bool ok = StartupLoaderMR.loadScript(&mrs->_mrb, files[i]);

    if (ok) {
      LOGGER_TRACE << "loaded ruby file '" << files[i] << "'";
    }
    else {
      LOGGER_FATAL << "cannot load ruby file '" << files[i] << "'";
      TRI_FlushLogging();
      return EXIT_FAILURE;
    }
  }

  // create a line editor
  printf("ArangoDB MRuby shell [DB version %s]\n", TRIAGENS_VERSION);

  MRLineEditor* console = new MRLineEditor(mrs, ".arango-mrb");

  console->open(false);
  
  while (true) {
    char* input = console->prompt("arangod> ");

    if (input == 0) {
      printf("<ctrl-D>\nBye Bye! Auf Wiedersehen! До свидания! さようなら\n");
      break;
    }

    if (*input == '\0') {
      TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, input);
      continue;
    }

    console->addHistory(input);

    p = mrb_parse_string(&mrs->_mrb, input);
    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, input);

    if (p == 0 || p->tree == 0 || 0 < p->nerr) {
      LOGGER_ERROR << "failed to compile input";
      continue;
    }

    int n = mrb_generate_code(&mrs->_mrb, p->tree);

    if (n < 0) {
      LOGGER_ERROR << "failed to execute Ruby bytecode";
      continue;
    }

    mrb_value result = mrb_run(&mrs->_mrb,
                               mrb_proc_new(&mrs->_mrb, mrs->_mrb.irep[n]),
                               mrb_top_self(&mrs->_mrb));

    if (mrs->_mrb.exc) {
      LOGGER_ERROR << "caught Ruby exception";
      mrb_p(&mrs->_mrb, mrb_obj_value(mrs->_mrb.exc));
      mrs->_mrb.exc = 0;
    }
    else if (! mrb_nil_p(result)) {
      mrb_p(&mrs->_mrb, result);
    }
  }
  
  // close the console
  console->close();
  delete console;

  // close the database
  closeDatabase();

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

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
