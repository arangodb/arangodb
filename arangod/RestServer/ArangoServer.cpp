////////////////////////////////////////////////////////////////////////////////
/// @brief arango server
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
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

#ifdef TRI_ENABLE_MRUBY
#include "mruby.h"
#include "mruby/compile.h"
#include "mruby/data.h"
#include "mruby/proc.h"
#include "mruby/variable.h"
#endif

#include "build.h"

#include "Actions/RestActionHandler.h"
#include "Actions/actions.h"
#include "Admin/ApplicationAdminServer.h"
#include "Admin/RestHandlerCreator.h"
#include "Basics/ProgramOptions.h"
#include "Basics/ProgramOptionsDescription.h"
#include "Basics/Random.h"
#include "BasicsC/files.h"
#include "BasicsC/init.h"
#include "BasicsC/strings.h"
#include "Dispatcher/ApplicationDispatcher.h"
#include "Dispatcher/Dispatcher.h"
#include "HttpServer/ApplicationHttpServer.h"
#include "HttpServer/HttpHandlerFactory.h"
#include "HttpServer/RedirectHandler.h"

#ifdef TRI_OPENSSL_VERSION
#include "HttpsServer/ApplicationHttpsServer.h"
#include "HttpsServer/HttpsServer.h"
#endif

#include "Logger/Logger.h"
#include "Rest/Initialise.h"
#include "RestHandler/ConnectionStatisticsHandler.h"
#include "RestHandler/RequestStatisticsHandler.h"
#include "RestHandler/RestBatchHandler.h"
#include "RestHandler/RestDocumentHandler.h"
#include "RestHandler/RestEdgeHandler.h"
#include "RestHandler/RestImportHandler.h"
#include "Scheduler/ApplicationScheduler.h"
#include "UserManager/ApplicationUserManager.h"
#include "V8/V8LineEditor.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8Server/ApplicationV8.h"
#include "VocBase/auth.h"

#ifdef TRI_ENABLE_MRUBY
#include "MRServer/ApplicationMR.h"
#include "MRServer/mr-actions.h"
#include "MRuby/MRLineEditor.h"
#include "MRuby/MRLoader.h"
#endif

#ifdef TRI_ENABLE_ZEROMQ
#include "ZeroMQ/ApplicationZeroMQ.h"
#endif

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
#include "mr/common/bootstrap/mr-error.h"
#include "mr/server/mr-server.h"
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

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
  
  // add batch handler
  factory->addPrefixHandler(RestVocbaseBaseHandler::BATCH_PATH,
                            RestHandlerCreator<RestBatchHandler>::createData<TRI_vocbase_t*>,
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

  // add statistics
  factory->addHandler("/_admin/connection-statistics",
                      RestHandlerCreator<ConnectionStatisticsHandler>::createNoData,
                      0);

  factory->addHandler("/_admin/request-statistics",
                      RestHandlerCreator<RequestStatisticsHandler>::createNoData,
                      0);
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
    _applicationScheduler(0),
    _applicationDispatcher(0),
    _applicationHttpServer(0),
    _applicationHttpsServer(0),
    _applicationAdminServer(0),
    _applicationUserManager(0),
    _httpServer(0),
    _httpsServer(0),
    _endpoints(),
    _disableAuthentication(false),
    _dispatcherThreads(8),
    _databasePath("/var/lib/arango"),
    _removeOnDrop(true),
    _removeOnCompacted(true),
    _defaultMaximalSize(TRI_JOURNAL_DEFAULT_MAXIMAL_SIZE),
    _defaultWaitForSync(false),
    _vocbase(0) {
  char* p;

  p = TRI_LocateBinaryPath(argv[0]);
  _binaryPath = p;

  TRI_FreeString(TRI_CORE_MEM_ZONE, p);

#ifdef TRI_ENABLE_RELATIVE_SYSTEM
    _workingDirectory = _binaryPath + "/../tmp";
    _databasePath = _binaryPath + "/../var/arango";
#endif

    _workingDirectory = "/var/tmp";

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
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ArangoServer::buildApplicationServer () {
  _applicationServer = new ApplicationServer("[<options>] <database-directory>", TRIAGENS_VERSION);
  _applicationServer->setUserConfigFile(".arango/arangod.conf");

  // .............................................................................
  // multi-threading scheduler and dispatcher
  // .............................................................................

  _applicationScheduler = new ApplicationScheduler(_applicationServer);
  _applicationScheduler->allowMultiScheduler(true);
  _applicationServer->addFeature(_applicationScheduler);

  _applicationDispatcher = new ApplicationDispatcher(_applicationScheduler);
  _applicationServer->addFeature(_applicationDispatcher);

  // .............................................................................
  // V8 engine
  // .............................................................................

  _applicationV8 = new ApplicationV8(_binaryPath);
  _applicationServer->addFeature(_applicationV8);

  // .............................................................................
  // MRuby engine
  // .............................................................................

#ifdef TRI_ENABLE_MRUBY

  _applicationMR = new ApplicationMR(_binaryPath);
  _applicationServer->addFeature(_applicationMR);

#endif

  // .............................................................................
  // ZeroMQ
  // .............................................................................

#ifdef TRI_ENABLE_ZEROMQ

  _applicationZeroMQ = new ApplicationZeroMQ(_applicationServer);
  _applicationServer->addFeature(_applicationZeroMQ);

#endif

  // .............................................................................
  // and start a simple admin server
  // .............................................................................

  _applicationAdminServer = new ApplicationAdminServer();
  _applicationServer->addFeature(_applicationAdminServer);

  _applicationAdminServer->allowLogViewer();
  _applicationAdminServer->allowVersion("arango", TRIAGENS_VERSION);

  // .............................................................................
  // build the application user manager
  // .............................................................................

  _applicationUserManager = new ApplicationUserManager();
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

  _applicationServer->setSystemConfigFile("arangod.conf", _binaryPath + "/../etc");
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

  _applicationServer->setSystemConfigFile("arangod.conf");
  _applicationAdminServer->allowAdminDirectory(string(_PKGDATADIR_) + "/html/admin");

#endif
#endif
  
  // .............................................................................
  // define server options
  // .............................................................................

  // .............................................................................
  // daemon and supervisor mode
  // .............................................................................

  map<string, ProgramOptionsDescription> additional;

  additional[ApplicationServer::OPTIONS_CMDLINE]
    ("console", "do not start as server, start a JavaScript emergency console instead")
  ;

#ifdef TRI_ENABLE_MRUBY
  additional[ApplicationServer::OPTIONS_CMDLINE]
    ("ruby-console", "do not start as server, start a Ruby emergency console instead")
  ;
#endif

  additional[ApplicationServer::OPTIONS_CMDLINE + ":help-extended"]
    ("daemon", "run as daemon")
    ("pid-file", &_pidFile, "pid-file in daemon mode")
    ("supervisor", "starts a supervisor and runs as daemon")
    ("working directory", &_workingDirectory, "working directory in daemon mode")
  ;
  
  // .............................................................................
  // javascript options
  // .............................................................................

  additional["JAVASCRIPT Options:help-admin"]
    ("javascript.script", &_scriptFile, "do not start as server, run script instead")
    ("javascript.script-parameter", &_scriptParameters, "script parameter")
    ("jslint", &_jslint, "do not start as server, run js lint instead")
    ("javascript.unit-tests", &_unitTests, "do not start as server, run unit tests instead")
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
    ("database.wait-for-sync", &_defaultWaitForSync, "default wait-for-sync behavior, can be overwritten when creating a collection")
  ;

  additional["DATABASE Options:help-devel"]
    ("database.remove-on-compacted", &_removeOnCompacted, "wipe a datafile from disk after compaction")
  ;

  // .............................................................................
  // server options
  // .............................................................................
  
  // .............................................................................
  // for this server we display our own options such as port to use
  // .............................................................................

  additional["ENDPOINT Options"]
    ("server.disable-auth", &_disableAuthentication, "disable authentication for ALL client requests")
    ("server.endpoint", &_endpoints, "endpoint for client HTTP requests")
  ;
  
  additional["Server Options:help-admin"]
    ("server.disable-admin-interface", "turn off the HTML admin interface")
  ;

  additional["THREAD Options:help-admin"]
    ("server.threads", &_dispatcherThreads, "number of threads for basic operations")
  ;
  
  
  // .............................................................................
  // a http server
  // .............................................................................

  _applicationHttpServer = new ApplicationHttpServer(_applicationServer,
                                                     _applicationScheduler,
                                                     _applicationDispatcher,
                                                     "arangodb",
                                                     TRI_CheckAuthenticationAuthInfo);
  _applicationServer->addFeature(_applicationHttpServer);

#ifdef TRI_OPENSSL_VERSION
  // .............................................................................
  // an https server
  // .............................................................................

  _applicationHttpsServer = new ApplicationHttpsServer(_applicationServer,
                                                       _applicationScheduler, 
                                                       _applicationDispatcher,
                                                       "arangodb",
                                                       TRI_CheckAuthenticationAuthInfo);
  // this will add options --ssl.*
  _applicationServer->addFeature(_applicationHttpsServer);
#endif


  // .............................................................................
  // parse the command line options - exit if there is a parse error
  // .............................................................................

  if (! _applicationServer->parse(_argc, _argv, additional)) {
    exit(EXIT_FAILURE);
  }
  
  // .............................................................................
  // disable access to the HTML admin interface
  // .............................................................................

  if (_applicationServer->programOptions().has("server.disable-admin-interface")) {
    _applicationAdminServer->allowAdminDirectory(false);
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

  // .............................................................................
  // in shell mode ignore the rest
  // .............................................................................

  if (_applicationServer->programOptions().has("console")) {
    int res = executeConsole(MODE_CONSOLE);
    exit(res);
  }
  else if (_applicationServer->programOptions().has("javascript.unit-tests")) {
    int res = executeConsole(MODE_UNITTESTS);
    exit(res);
  }
  else if (_applicationServer->programOptions().has("jslint")) {
    int res = executeConsole(MODE_JSLINT);
    exit(res);
  }
  else if (_applicationServer->programOptions().has("javascript.script")) {
    int res = executeConsole(MODE_SCRIPT);
    exit(res);
  }


#ifdef TRI_ENABLE_MRUBY
  if (_applicationServer->programOptions().has("ruby-console")) {
    int res = executeRubyConsole();
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

  if (0 == _endpoints.size()) {
    LOGGER_FATAL << "no endpoint has been specified, giving up";
    cerr << "no endpoint has been specified, giving up\n";
    LOGGER_INFO << "please use the '--server.endpoint' option";
    exit(EXIT_FAILURE);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

int ArangoServer::startupServer () {
  v8::HandleScope handle_scope;

  // .............................................................................
  // open the database
  // .............................................................................

  openDatabase();

  // .............................................................................
  // prepare the various parts of the Arango server
  // .............................................................................

  if (_dispatcherThreads < 1) {
    _dispatcherThreads = 1;
  }

  _applicationV8->setVocbase(_vocbase);
  _applicationV8->setConcurrency(_dispatcherThreads);

#if TRI_ENABLE_MRUBY
  _applicationMR->setVocbase(_vocbase);
  _applicationMR->setConcurrency(_dispatcherThreads);
#endif

  _applicationServer->prepare();

  // .............................................................................
  // create the dispatcher
  // .............................................................................

  _applicationDispatcher->buildStandardQueue(_dispatcherThreads);

  // .............................................................................
  // create a client http server and http handler factory
  // .............................................................................

  // add & validate endpoints
  for (vector<string>::const_iterator i = _endpoints.begin(); i != _endpoints.end(); ++i) {
    Endpoint* endpoint = Endpoint::serverFactory(*i);
  
    if (endpoint == 0) {
      LOGGER_FATAL << "invalid endpoint '" << *i << "'";
      cerr << "invalid endpoint '" << *i << "'\n";
      exit(EXIT_FAILURE);
    }

    assert(endpoint);

    bool ok = _endpointList.addEndpoint(endpoint->getProtocol(), endpoint->getEncryption(), endpoint);
    if (! ok) {
      LOGGER_FATAL << "invalid endpoint '" << *i << "'";
      cerr << "invalid endpoint '" << *i << "'\n";
      exit(EXIT_FAILURE);
    }
  }


  // dump used endpoints for user information
  _endpointList.dump();
    
  // we pass the options by reference, so keep them until shutdown
  RestActionHandler::action_options_t httpOptions;
  httpOptions._vocbase = _vocbase;
  httpOptions._queue = "STANDARD";

  // create the handlers
  httpOptions._contexts.insert("user");
  httpOptions._contexts.insert("api");
  httpOptions._contexts.insert("admin");

  // unencrypted endpoints
  if (_endpointList.count(Endpoint::PROTOCOL_HTTP, Endpoint::ENCRYPTION_NONE) > 0) {
    // create the http server
    _httpServer = _applicationHttpServer->buildServer(&_endpointList);

    DefineApiHandlers(_httpServer, _applicationAdminServer, _vocbase);

    DefineAdminHandlers(_httpServer, _applicationAdminServer, _applicationUserManager, _vocbase);

    // add action handler
    _httpServer->addPrefixHandler("/",
                                  RestHandlerCreator<RestActionHandler>::createData<RestActionHandler::action_options_t*>,
                                  (void*) &httpOptions);
  
    if (_disableAuthentication) {
      // turn off authentication
      _httpServer->setAuthenticationCallback(0);
    }
  }

#ifdef TRI_OPENSSL_VERSION
  // SSL endpoints
  if (_endpointList.count(Endpoint::PROTOCOL_HTTP, Endpoint::ENCRYPTION_SSL) > 0) {

    // create the https server
    _httpsServer = _applicationHttpsServer->buildServer(&_endpointList);
    DefineApiHandlers(_httpsServer, _applicationAdminServer, _vocbase);
    
    DefineAdminHandlers(_httpsServer, _applicationAdminServer, _applicationUserManager, _vocbase);

    // add action handler
    _httpsServer->addPrefixHandler("/",
                                   RestHandlerCreator<RestActionHandler>::createData<RestActionHandler::action_options_t*>,
                                   (void*) &httpOptions);
    
    if (_disableAuthentication) {
      // turn off authentication
      _httpsServer->setAuthenticationCallback(0);
    }
  }
#endif

  // .............................................................................
  // create a http handler factory for zeromq
  // .............................................................................

#ifdef TRI_ENABLE_ZEROMQ

  // we pass the options be reference, so keep them until shutdown
  RestActionHandler::action_options_t zeromqOptions;
  zeromqOptions._vocbase = _vocbase;
  zeromqOptions._queue = "CLIENT";

  // only construct factory if ZeroMQ is active
  if (_applicationZeroMQ->isActive()) {
    HttpHandlerFactory* factory = new HttpHandlerFactory();

    DefineApiHandlers(factory, _applicationAdminServer, _vocbase);

    DefineAdminHandlers(factory, _applicationAdminServer, _applicationUserManager, _vocbase);

    // add action handler
    factory->addPrefixHandler("/",
                              RestHandlerCreator<RestActionHandler>::createData<RestActionHandler::action_options_t*>,
                              (void*) &httpOptions);

    _applicationZeroMQ->setHttpHandlerFactory(factory);
  }

#endif
  
  if (_disableAuthentication) {
    LOGGER_INFO << "Authentication is turned off";
  }

  // .............................................................................
  // start the main event loop
  // .............................................................................

  _applicationServer->start();

  LOGGER_INFO << "ArangoDB (version " << TRIAGENS_VERSION << ") is ready for business";
  LOGGER_INFO << "Have Fun!";

  _applicationServer->wait();

  // .............................................................................
  // and cleanup
  // .............................................................................

  _applicationServer->stop();
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

int ArangoServer::executeConsole (server_operation_mode_e mode) {
  bool ok;

  // only simple logging
  TRI_ShutdownLogging();
  TRI_InitialiseLogging(false);
  TRI_CreateLogAppenderFile("+");

  // open the database
  openDatabase();

  // set-up V8 context
  _applicationV8->setVocbase(_vocbase);
  _applicationV8->setConcurrency(1);
  _applicationV8->disableActions();

  ok = _applicationV8->prepare();

  if (! ok) {
    LOGGER_FATAL << "cannot initialize V8 enigne";
    exit(EXIT_FAILURE);
  }

  _applicationV8->start();

  // enter V8 context
  ApplicationV8::V8Context* context = _applicationV8->enterContext();

  // .............................................................................
  // execute everything with a global scope
  // .............................................................................

  {
    v8::HandleScope globalScope;

    // run the shell
    if (mode != MODE_SCRIPT) {
      printf("ArangoDB JavaScript shell [V8 version %s, DB version %s]\n", v8::V8::GetVersion(), TRIAGENS_VERSION);
    }
    else {
      LOGGER_INFO << "V8 version " << v8::V8::GetVersion() << ", DB version " << TRIAGENS_VERSION;
    }

    v8::Local<v8::String> name(v8::String::New("(arango)"));
    v8::Context::Scope contextScope(context->_context);

    ok = true;

    switch (mode) {

      // .............................................................................
      // run all unit tests
      // .............................................................................

      case MODE_UNITTESTS: {
        v8::HandleScope scope;
        v8::TryCatch tryCatch;

        // set-up unit tests array
        v8::Handle<v8::Array> sysTestFiles = v8::Array::New();

        for (size_t i = 0;  i < _unitTests.size();  ++i) {
          sysTestFiles->Set((uint32_t) i, v8::String::New(_unitTests[i].c_str()));
        }

        context->_context->Global()->Set(v8::String::New("SYS_UNIT_TESTS"), sysTestFiles);
        context->_context->Global()->Set(v8::String::New("SYS_UNIT_TESTS_RESULT"), v8::True());

        // run tests
        char const* input = "require(\"jsunity\").runCommandLineTests();";
        TRI_ExecuteJavaScriptString(context->_context, v8::String::New(input), name, true);

        if (tryCatch.HasCaught()) {
          cout << TRI_StringifyV8Exception(&tryCatch);
          ok = false;
        }
        else {
          ok = TRI_ObjectToBoolean(context->_context->Global()->Get(v8::String::New("SYS_UNIT_TESTS_RESULT")));
        }

        break;
      }

      // .............................................................................
      // run jslint
      // .............................................................................

      case MODE_JSLINT: {
        v8::HandleScope scope;
        v8::TryCatch tryCatch;

        // set-up tests files array
        v8::Handle<v8::Array> sysTestFiles = v8::Array::New();

        for (size_t i = 0;  i < _jslint.size();  ++i) {
          sysTestFiles->Set((uint32_t) i, v8::String::New(_jslint[i].c_str()));
        }

        context->_context->Global()->Set(v8::String::New("SYS_UNIT_TESTS"), sysTestFiles);
        context->_context->Global()->Set(v8::String::New("SYS_UNIT_TESTS_RESULT"), v8::True());

        char const* input = "require(\"jslint\").runCommandLineTests({ });";
        TRI_ExecuteJavaScriptString(context->_context, v8::String::New(input), name, true);

        if (tryCatch.HasCaught()) {
          cout << TRI_StringifyV8Exception(&tryCatch);
          ok = false;
        }
        else {
          ok = TRI_ObjectToBoolean(context->_context->Global()->Get(v8::String::New("SYS_UNIT_TESTS_RESULT")));
        }

        break;
      }

      // .............................................................................
      // run console
      // .............................................................................

      case MODE_SCRIPT: {
        v8::TryCatch tryCatch;

        for (size_t i = 0;  i < _scriptFile.size();  ++i) {
          bool r = TRI_LoadJavaScriptFile(context->_context, _scriptFile[i].c_str());

          if (! r) {
            LOGGER_FATAL << "cannot load script '" << _scriptFile[i] << ", giving up";
            ok = false;
            break;
          }
        }

        if (ok) {

          // parameter array
          v8::Handle<v8::Array> params = v8::Array::New();

          params->Set(0, v8::String::New(_scriptFile[_scriptFile.size() - 1].c_str()));

          for (size_t i = 0;  i < _scriptParameters.size();  ++i) {
            params->Set((uint32_t) (i + 1), v8::String::New(_scriptParameters[i].c_str()));
          }

          // call main
          v8::Handle<v8::String> mainFuncName = v8::String::New("main");
          v8::Handle<v8::Function> main = v8::Handle<v8::Function>::Cast(context->_context->Global()->Get(mainFuncName));

          if (main.IsEmpty() || main->IsUndefined()) {
            LOGGER_FATAL << "no main function defined, giving up";
            ok = false;
          }
          else {
            v8::Handle<v8::Value> args[] = { params };

            v8::Handle<v8::Value> result = main->Call(main, 1, args);

            if (tryCatch.HasCaught()) {
              TRI_LogV8Exception(&tryCatch);
              ok = false;
            }
            else {
              ok = TRI_ObjectToDouble(result) == 0;
            }
          }
        }

        break;
      }

      // .............................................................................
      // run console
      // .............................................................................

      case MODE_CONSOLE: {
        V8LineEditor* console = new V8LineEditor(context->_context, ".arango");

        console->open(true);

        while (true) {
          while(! v8::V8::IdleNotification()) {
          }

          char* input = console->prompt("arangod> ");

          if (input == 0) {
            printf("<ctrl-D>\nBye Bye! Arrivederci! Auf Wiedersehen! До свидания! さようなら\n");
            break;
          }

          if (*input == '\0') {
            TRI_FreeString(TRI_CORE_MEM_ZONE, input);
            continue;
          }

          console->addHistory(input);

          v8::HandleScope scope;
          v8::TryCatch tryCatch;

          TRI_ExecuteJavaScriptString(context->_context, v8::String::New(input), name, true);
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
  }

  // ............................................................................. 
  // and return from the context and isolate
  // ............................................................................. 

  _applicationV8->exitContext(context);

  _applicationV8->close();
  _applicationV8->stop();

  
  closeDatabase();
  Random::shutdown();

  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the MRuby emergency shell
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


int ArangoServer::executeRubyConsole () {
  bool ok;

  // only simple logging
  TRI_ShutdownLogging();
  TRI_InitialiseLogging(false);
  TRI_CreateLogAppenderFile("+");

  // open the database
  openDatabase();

  // set-up MRuby context
  _applicationMR->setVocbase(_vocbase);
  _applicationMR->setConcurrency(1);
  _applicationMR->disableActions();

  ok = _applicationMR->prepare();

  if (! ok) {
    LOGGER_FATAL << "cannot initialize MRuby enigne";
    exit(EXIT_FAILURE);
  }

  _applicationMR->start();

  // enter MR context
  ApplicationMR::MRContext* context = _applicationMR->enterContext();

  // create a line editor
  printf("ArangoDB MRuby shell [DB version %s]\n", TRIAGENS_VERSION);

  MRLineEditor* console = new MRLineEditor(context->_mrs, ".arango-mrb");

  console->open(false);

  while (true) {
    char* input = console->prompt("arangod> ");

    if (input == 0) {
      printf("<ctrl-D>\nBye Bye! Arrivederci! Auf Wiedersehen! До свидания! さようなら\n");
      break;
    }

    if (*input == '\0') {
      TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, input);
      continue;
    }

    console->addHistory(input);

    struct mrb_parser_state* p = mrb_parse_string(&context->_mrs->_mrb, input);
    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, input);

    if (p == 0 || p->tree == 0 || 0 < p->nerr) {
      LOGGER_ERROR << "failed to compile input";
      continue;
    }

    int n = mrb_generate_code(&context->_mrs->_mrb, p->tree);

    if (n < 0) {
      LOGGER_ERROR << "failed to execute Ruby bytecode";
      continue;
    }

    mrb_value result = mrb_run(&context->_mrs->_mrb,
                               mrb_proc_new(&context->_mrs->_mrb, context->_mrs->_mrb.irep[n]),
                               mrb_top_self(&context->_mrs->_mrb));

    if (context->_mrs->_mrb.exc) {
      LOGGER_ERROR << "caught Ruby exception";
      mrb_p(&context->_mrs->_mrb, mrb_obj_value(context->_mrs->_mrb.exc));
      context->_mrs->_mrb.exc = 0;
    }
    else if (! mrb_nil_p(result)) {
      mrb_p(&context->_mrs->_mrb, result);
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
  _vocbase->_defaultWaitForSync = _defaultWaitForSync;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes the database
////////////////////////////////////////////////////////////////////////////////

void ArangoServer::closeDatabase () {
  ApplicationUserManager::unloadUsers();
  ApplicationUserManager::unloadRoles();

  TRI_CleanupActions();
  TRI_DestroyVocBase(_vocbase);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, _vocbase);
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
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:
