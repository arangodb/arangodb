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

#include "ArangoServer.h"

#ifdef TRI_HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include <v8.h>
#include <iostream>
#include <fstream>

#include "Actions/RestActionHandler.h"
#include "Actions/actions.h"
#include "ApplicationServer/ApplicationServer.h"
#include "Aql/Query.h"
#include "Aql/QueryCache.h"
#include "Aql/RestAqlHandler.h"
#include "Basics/FileUtils.h"
#include "Logger/Logger.h"
#include "Basics/Nonce.h"
// #include "Basics/ProgramOptions.h"
// #include "Basics/ProgramOptionsDescription.h"
#include "Basics/RandomGenerator.h"
#include "Basics/Utf8Helper.h"
#include "Basics/files.h"
#include "Basics/messages.h"
#include "Basics/process-utils.h"
#include "Basics/tri-strings.h"
#include "Cluster/ApplicationCluster.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/HeartbeatThread.h"
#include "Cluster/RestShardHandler.h"
// #include "Dispatcher/ApplicationDispatcher.h"
#include "Dispatcher/Dispatcher.h"
#include "HttpServer/ApplicationEndpointServer.h"
#include "HttpServer/AsyncJobManager.h"
#include "HttpServer/HttpHandlerFactory.h"
#include "Rest/InitializeRest.h"
#include "Rest/OperationMode.h"
#include "Rest/Version.h"
#include "RestHandler/RestAdminLogHandler.h"
#include "RestHandler/RestBatchHandler.h"
#include "RestHandler/RestCursorHandler.h"
#include "RestHandler/RestDebugHandler.h"
#include "RestHandler/RestDocumentHandler.h"
#include "RestHandler/RestEdgeHandler.h"
#include "RestHandler/RestEdgesHandler.h"
#include "RestHandler/RestExportHandler.h"
#include "RestHandler/RestHandlerCreator.h"
#include "RestHandler/RestImportHandler.h"
#include "RestHandler/RestJobHandler.h"
#include "RestHandler/RestPleaseUpgradeHandler.h"
#include "RestHandler/RestQueryCacheHandler.h"
#include "RestHandler/RestQueryHandler.h"
#include "RestHandler/RestReplicationHandler.h"
#include "RestHandler/RestShutdownHandler.h"
#include "RestHandler/RestSimpleHandler.h"
#include "RestHandler/RestSimpleQueryHandler.h"
#include "RestHandler/RestUploadHandler.h"
#include "RestHandler/RestVersionHandler.h"
#include "RestHandler/WorkMonitorHandler.h"
#include "RestServer/ConsoleThread.h"
#include "RestServer/VocbaseContext.h"
#include "Statistics/statistics.h"
#include "V8/V8LineEditor.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "VocBase/auth.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

bool ALLOW_USE_DATABASE_IN_REST_ACTIONS;

bool IGNORE_DATAFILE_ERRORS;

ArangoServer::ArangoServer(int argc, char** argv)
    : _mode(ServerMode::MODE_STANDALONE),
#warning TODO
      // _applicationServer(nullptr),
      _argc(argc),
      _argv(argv),
      _tempPath(),
      _applicationEndpointServer(nullptr),
      _applicationCluster(nullptr),
      _jobManager(nullptr),
      _applicationV8(nullptr),
      _authenticateSystemOnly(false),
      _disableAuthentication(false),
      _disableAuthenticationUnixSockets(false),
      _v8Contexts(8),
      _indexThreads(2),
      _databasePath(),
      _ignoreDatafileErrors(false),
      _disableReplicationApplier(false),
      _disableQueryTracking(false),
      _throwCollectionNotLoadedError(false),
      _foxxQueues(true),
      _foxxQueuesPollInterval(1.0),
      _server(nullptr),
      _queryRegistry(nullptr),
      _pairForAqlHandler(nullptr),
      _pairForJobHandler(nullptr),
      _indexPool(nullptr),
      _threadAffinity(0) {
#ifndef TRI_HAVE_THREAD_AFFINITY
  _threadAffinity = 0;
#endif
}

ArangoServer::~ArangoServer() {
  delete _jobManager;
  delete _server;

  Nonce::destroy();

#warning TODO
  // delete _applicationServer;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the server
////////////////////////////////////////////////////////////////////////////////

int ArangoServer::start() {
#warning TODO
#if 0
  if (_applicationServer == nullptr) {
    buildApplicationServer();
  }

  if (_supervisorMode) {
    return startupSupervisor();
  } else if (_daemonMode) {
    return startupDaemon();
  } else {
    InitializeWorkMonitor();
    _applicationServer->setupLogging(true, false, false);

    if (!_pidFile.empty()) {
      CheckPidFile(_pidFile);
      WritePidFile(_pidFile, Thread::currentProcessId());
    }

    int res = startupServer();

    if (!_pidFile.empty()) {
      if (!FileUtils::remove(_pidFile)) {
        LOG(DEBUG) << "cannot remove pid file '" << _pidFile << "'";
      }
    }

    return res;
  }
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief begins shutdown sequence
////////////////////////////////////////////////////////////////////////////////

void ArangoServer::beginShutdown() {
#warning TODO
#if 0
  if (_applicationServer != nullptr) {
    _applicationServer->beginShutdown();
  }
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts list of size_t to string
////////////////////////////////////////////////////////////////////////////////

template <typename T>
static std::string ToString(std::vector<T> const& v) {
  std::string result = "";
  std::string sep = "[";

  for (auto const& e : v) {
    result += sep + std::to_string(e);
    sep = ",";
  }

  return result + "]";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief define "_api" and "_admin" handlers
////////////////////////////////////////////////////////////////////////////////

void ArangoServer::buildApplicationServer() {
#warning TODO
#if 0

  // ...........................................................................
  // create QueryRegistry
  // ...........................................................................

  _queryRegistry = new aql::QueryRegistry();
  _server->_queryRegistry = _queryRegistry;

  // .............................................................................
  // V8 engine
  // .............................................................................

  // .............................................................................
  // define server options
  // .............................................................................

  std::map<std::string, ProgramOptionsDescription> additional;

  // command-line only options
  additional["General Options:help-default"](
      "console",
      "do not start as server, start a JavaScript emergency console instead")(
      "upgrade", "perform a database upgrade")(

  // .............................................................................
  // set language of default collator
  // .............................................................................

  // other options
  additional["Hidden Options"]("no-upgrade", "skip a database upgrade")(
      "start-service", "used to start as windows service")(
      "no-server", "do not start the server, if console is requested")(
      "use-thread-affinity", &_threadAffinity,
      "try to set thread affinity (0=disable, 1=disjunct, 2=overlap, "
      "3=scheduler, 4=dispatcher)");

  // javascript options
  // .............................................................................

  additional["Javascript Options:help-admin"](
      "javascript.script", &_scriptFile,
      "do not start as server, run script instead")(
      "javascript.script-parameter", &_scriptParameters, "script parameter");

  additional["Hidden Options"](
      "javascript.unit-tests", &_unitTests,
      "do not start as server, run unit tests instead");

  // .............................................................................
  // database options
  // .............................................................................

  additional["Database Options:help-admin"](
      "database.directory", &_databasePath, "path to the database directory")(
      "database.maximal-journal-size", &_defaultMaximalSize,
      "default maximal journal size, can be overwritten when creating a "
      "collection")("database.wait-for-sync", &_defaultWaitForSync,
                    "default wait-for-sync behavior, can be overwritten when "
                    "creating a collection")(
      "database.force-sync-properties", &_forceSyncProperties,
      "force syncing of collection properties to disk, will use waitForSync "
      "value of collection when turned off")(
      "database.ignore-datafile-errors", &_ignoreDatafileErrors,
      "load collections even if datafiles may contain errors")(
      "database.disable-query-tracking", &_disableQueryTracking,
      "turn off AQL query tracking by default")(
      "database.query-cache-mode", &_queryCacheMode,
      "mode for the AQL query cache (on, off, demand)")(
      "database.query-cache-max-results", &_queryCacheMaxResults,
      "maximum number of results in query cache per database")(
      "database.throw-collection-not-loaded-error",
      &_throwCollectionNotLoadedError,
      "throw an error when accessing a collection that is still loading");

  // .............................................................................
  // cluster options
  // .............................................................................

  _applicationCluster =
      new ApplicationCluster(_server);
  _applicationServer->addFeature(_applicationCluster);

  // .............................................................................
  // server options
  // .............................................................................

  // .............................................................................
  // for this server we display our own options such as port to use
  // .............................................................................

  additional["Server Options:help-admin"](
      "server.authenticate-system-only", &_authenticateSystemOnly,
      "use HTTP authentication only for requests to /_api and /_admin")(
      "server.disable-authentication", &_disableAuthentication,
      "disable authentication for ALL client requests")
#ifdef TRI_HAVE_LINUX_SOCKETS
      ("server.disable-authentication-unix-sockets",
       &_disableAuthenticationUnixSockets,
       "disable authentication for requests via UNIX domain sockets")
#endif
          ("server.disable-replication-applier", &_disableReplicationApplier,
           "start with replication applier turned off")(
              "server.allow-use-database", &ALLOW_USE_DATABASE_IN_REST_ACTIONS,
              "allow change of database in REST actions, only needed for "
              "unittests")(
              "server.additional-threads", &_additionalThreads,
              "number of threads in additional queues")(
              "server.hide-product-header", &HttpResponse::HideProductHeader,
              "do not expose \"Server: ArangoDB\" header in HTTP responses")(
              "server.foxx-queues", &_foxxQueues, "enable Foxx queues")(
              "server.foxx-queues-poll-interval", &_foxxQueuesPollInterval,
              "Foxx queue manager poll interval (in seconds)")(
              "server.session-timeout", &VocbaseContext::ServerSessionTtl,
              "timeout of web interface server sessions (in seconds)");

  bool disableStatistics = false;

  additional["Server Options:help-admin"]("server.disable-statistics",
                                          &disableStatistics,
                                          "turn off statistics gathering");


  // .............................................................................
  // endpoint server
  // .............................................................................

  _jobManager = new AsyncJobManager(ClusterCommRestCallback);

  _applicationEndpointServer = new ApplicationEndpointServer(
      _applicationServer,
      _jobManager, "arangodb", &SetRequestContext, (void*)_server);
  _applicationServer->addFeature(_applicationEndpointServer);

  // .............................................................................
  // parse the command line options - exit if there is a parse error
  // .............................................................................

  IGNORE_DATAFILE_ERRORS = _ignoreDatafileErrors;

  // .............................................................................
  // set language name
  // .............................................................................

  // ...........................................................................
  // init nonces
  // ...........................................................................

  uint32_t optionNonceHashSize = 0;

  if (optionNonceHashSize > 0) {
    LOG(DEBUG) << "setting nonce hash size to " << optionNonceHashSize;
    Nonce::create(optionNonceHashSize);
  }

  if (disableStatistics) {
    TRI_ENABLE_STATISTICS = false;
  }

  // validate journal size
  if (_defaultMaximalSize < TRI_JOURNAL_MINIMAL_SIZE) {
    LOG(FATAL) << "invalid value for '--database.maximal-journal-size'. "
                  "expected at least "
               << TRI_JOURNAL_MINIMAL_SIZE;
    FATAL_ERROR_EXIT();
  }

  // .............................................................................
  // set directories and scripts
  // .............................................................................

  std::vector<std::string> arguments = _applicationServer->programArguments();

  if (1 < arguments.size()) {
    LOG(FATAL) << "expected at most one database directory, got "
               << arguments.size();
    FATAL_ERROR_EXIT();
  } else if (1 == arguments.size()) {
    _databasePath = arguments[0];
  }

  // disable certain options in unittest or script mode
  OperationMode::server_operation_mode_e mode =
      OperationMode::determineMode(_applicationServer->programOptions());

  if (mode == OperationMode::MODE_SCRIPT ||
      mode == OperationMode::MODE_UNITTESTS) {
    // testing disables authentication
    _disableAuthentication = true;
  }

  TRI_SetThrowCollectionNotLoadedVocBase(nullptr,
                                         _throwCollectionNotLoadedError);

  // set global query tracking flag
  arangodb::aql::Query::DisableQueryTracking(_disableQueryTracking);

  // configure the query cache
  {
    std::pair<std::string, size_t> cacheProperties{_queryCacheMode,
                                                   _queryCacheMaxResults};
    arangodb::aql::QueryCache::instance()->setProperties(cacheProperties);
  }

  // .............................................................................
  // now run arangod
  // .............................................................................

  // dump version details
  LOG(INFO) << "" << rest::Version::getVerboseVersionString();


  // if we got here, then we are in server mode

  // .............................................................................
  // sanity checks
  // .............................................................................

    OperationMode::server_operation_mode_e mode =
        OperationMode::determineMode(_applicationServer->programOptions());
    if (mode != OperationMode::MODE_SERVER) {
      LOG(FATAL) << "invalid mode. must not specify --console together with "
                    "--daemon or --supervisor";
      FATAL_ERROR_EXIT();
    }

#endif
}

int ArangoServer::startupServer() {
#warning TODO
#if 0
  TRI_InitializeStatistics();

  OperationMode::server_operation_mode_e mode =
      OperationMode::determineMode(_applicationServer->programOptions());
  bool startServer = true;

  if (_applicationServer->programOptions().has("no-server")) {
    startServer = false;
    TRI_ENABLE_STATISTICS = false;
    // --no-server disables all replication appliers
    _disableReplicationApplier = true;
  }

  // initialize V8
  if (!_applicationServer->programOptions().has("javascript.v8-contexts")) {
    // the option was added recently so it's not always set
    // the behavior in older ArangoDB was to create one V8 context per
    // dispatcher thread
    _v8Contexts = _dispatcherThreads;
  }

  if (_v8Contexts < 1) {
    _v8Contexts = 1;
  }

  if (mode == OperationMode::MODE_CONSOLE) {
    // one V8 instance is taken by the console
    if (startServer) {
      ++_v8Contexts;
    }
  } else if (mode == OperationMode::MODE_UNITTESTS ||
             mode == OperationMode::MODE_SCRIPT) {
    if (_v8Contexts == 1) {
      // at least two to allow both the test-runner and the scheduler to use a
      // V8 instance
      _v8Contexts = 2;
    }
  }

  // .............................................................................
  // prepare everything
  // .............................................................................

  if (!startServer) {
    _applicationEndpointServer->disable();
  }

  // prepare scheduler and dispatcher
  _applicationServer->prepare();

  auto const role = ServerState::instance()->getRole();

  // and finish prepare
  _applicationServer->prepare2();

  _pairForAqlHandler = new std::pair<ApplicationV8*, aql::QueryRegistry*>(
      _applicationV8, _queryRegistry);
  _pairForJobHandler = new std::pair<Dispatcher*, AsyncJobManager*>(
      _applicationDispatcher->dispatcher(), _jobManager);

  // ...........................................................................
  // create endpoints and handlers
  // ...........................................................................

  // we pass the options by reference, so keep them until shutdown

  // active deadlock detection in case we're not running in cluster mode
  if (!arangodb::ServerState::instance()->isRunningInCluster()) {
    TRI_EnableDeadlockDetectionDatabasesServer(_server);
  }

  // .............................................................................
  // start the main event loop
  // .............................................................................

  _applicationServer->start();

  // for a cluster coordinator, the users are loaded at a later stage;
  // the kickstarter will trigger a bootstrap process
  //
  if (role != ServerState::ROLE_COORDINATOR &&
      role != ServerState::ROLE_PRIMARY &&
      role != ServerState::ROLE_SECONDARY) {
    // if the authentication info could not be loaded, but authentication is
    // turned on,
    // then we refuse to start
    if (!vocbase->_authInfoLoaded && !_disableAuthentication) {
      LOG(FATAL) << "could not load required authentication information";
      FATAL_ERROR_EXIT();
    }
  }

  if (_disableAuthentication) {
    LOG(INFO) << "Authentication is turned off";
  }

  LOG(INFO) << "ArangoDB (version " << ARANGODB_VERSION_FULL
            << ") is ready for business. Have fun!";

  int res;

  if (mode == OperationMode::MODE_CONSOLE) {
    res = runConsole(vocbase);
  } else if (mode == OperationMode::MODE_UNITTESTS) {
    res = runUnitTests(vocbase);
  } else if (mode == OperationMode::MODE_SCRIPT) {
    res = runScript(vocbase);
  } else {
    res = runServer(vocbase);
  }

  // stop the replication appliers so all replication transactions can end
  TRI_StopReplicationAppliersServer(_server);

  _applicationServer->stop();

  _server->_queryRegistry = nullptr;

  delete _queryRegistry;
  _queryRegistry = nullptr;
  delete _pairForAqlHandler;
  _pairForAqlHandler = nullptr;
  delete _pairForJobHandler;
  _pairForJobHandler = nullptr;

  closeDatabases();

  if (mode == OperationMode::MODE_CONSOLE) {
    std::cout << std::endl << TRI_BYE_MESSAGE << std::endl;
  }

  TRI_ShutdownStatistics();
  ShutdownWorkMonitor();

  return res;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wait for the heartbeat thread to run
/// before the server responds to requests, the heartbeat thread should have
/// run at least once
////////////////////////////////////////////////////////////////////////////////

void ArangoServer::waitForHeartbeat() {
  if (!ServerState::instance()->isCoordinator()) {
    // waiting for the heartbeart thread is necessary on coordinator only
    return;
  }

  while (true) {
    if (HeartbeatThread::hasRunOnce()) {
      break;
    }
    usleep(100 * 1000);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs the server
////////////////////////////////////////////////////////////////////////////////

int ArangoServer::runServer(TRI_vocbase_t* vocbase) {
#warning TODO
#if 0
  waitForHeartbeat();

  // just wait until we are signalled
  _applicationServer->wait();

  return EXIT_SUCCESS;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the JavaScript emergency console
////////////////////////////////////////////////////////////////////////////////

int ArangoServer::runConsole(TRI_vocbase_t* vocbase) {
#warning TODO
#if 0
  ConsoleThread console(_applicationServer, _applicationV8, vocbase);
  console.start();

#ifdef __APPLE__
  if (_applicationServer->programOptions().has("voice")) {
    system("say -v zarvox 'welcome to ArangoDB' &");
  }
#endif

  // disabled maintenance mode
  waitForHeartbeat();
  HttpHandlerFactory::setMaintenance(false);

  // just wait until we are signalled
  _applicationServer->wait();

#ifdef __APPLE__
  if (_applicationServer->programOptions().has("voice")) {
    system("say -v zarvox 'good-bye' &");
  }
#endif

  // .............................................................................
  // and cleanup
  // .............................................................................

  console.userAbort();
  console.beginShutdown();

  int iterations = 0;

  while (console.isRunning() && ++iterations < 30) {
    usleep(100 * 1000);  // spin while console is still needed
  }

  return EXIT_SUCCESS;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs unit tests
////////////////////////////////////////////////////////////////////////////////

int ArangoServer::runUnitTests(TRI_vocbase_t* vocbase) {
#warning TODO
#if 0
  ApplicationV8::V8Context* context =
      _applicationV8->enterContext(vocbase, true);

  auto isolate = context->isolate;

  bool ok = false;
  {
    v8::HandleScope scope(isolate);
    v8::TryCatch tryCatch;

    auto localContext = v8::Local<v8::Context>::New(isolate, context->_context);
    localContext->Enter();
    {
      v8::Context::Scope contextScope(localContext);
      // set-up unit tests array
      v8::Handle<v8::Array> sysTestFiles = v8::Array::New(isolate);

      for (size_t i = 0; i < _unitTests.size(); ++i) {
        sysTestFiles->Set((uint32_t)i, TRI_V8_STD_STRING(_unitTests[i]));
      }

      localContext->Global()->Set(TRI_V8_ASCII_STRING("SYS_UNIT_TESTS"),
                                  sysTestFiles);
      localContext->Global()->Set(TRI_V8_ASCII_STRING("SYS_UNIT_TESTS_RESULT"),
                                  v8::True(isolate));

      v8::Local<v8::String> name(
          TRI_V8_ASCII_STRING(TRI_V8_SHELL_COMMAND_NAME));

      // run tests
      auto input = TRI_V8_ASCII_STRING(
          "require(\"@arangodb/testrunner\").runCommandLineTests();");
      TRI_ExecuteJavaScriptString(isolate, localContext, input, name, true);

      if (tryCatch.HasCaught()) {
        if (tryCatch.CanContinue()) {
          std::cerr << TRI_StringifyV8Exception(isolate, &tryCatch);
        } else {
          // will stop, so need for v8g->_canceled = true;
          TRI_ASSERT(!ok);
        }
      } else {
        ok = TRI_ObjectToBoolean(localContext->Global()->Get(
            TRI_V8_ASCII_STRING("SYS_UNIT_TESTS_RESULT")));
      }
    }
    localContext->Exit();
  }

  _applicationV8->exitContext(context);

  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs a script
////////////////////////////////////////////////////////////////////////////////

int ArangoServer::runScript(TRI_vocbase_t* vocbase) {
#warning TODO
#if 0
  bool ok = false;
  ApplicationV8::V8Context* context =
      _applicationV8->enterContext(vocbase, true);
  auto isolate = context->isolate;

  {
    v8::HandleScope globalScope(isolate);

    auto localContext = v8::Local<v8::Context>::New(isolate, context->_context);
    localContext->Enter();
    {
      v8::Context::Scope contextScope(localContext);
      for (size_t i = 0; i < _scriptFile.size(); ++i) {
        bool r =
            TRI_ExecuteGlobalJavaScriptFile(isolate, _scriptFile[i].c_str());

        if (!r) {
          LOG(FATAL) << "cannot load script '" << _scriptFile[i]
                     << "', giving up";
          FATAL_ERROR_EXIT();
        }
      }

      v8::TryCatch tryCatch;
      // run the garbage collection for at most 30 seconds
      TRI_RunGarbageCollectionV8(isolate, 30.0);

      // parameter array
      v8::Handle<v8::Array> params = v8::Array::New(isolate);

      params->Set(0, TRI_V8_STD_STRING(_scriptFile[_scriptFile.size() - 1]));

      for (size_t i = 0; i < _scriptParameters.size(); ++i) {
        params->Set((uint32_t)(i + 1), TRI_V8_STD_STRING(_scriptParameters[i]));
      }

      // call main
      v8::Handle<v8::String> mainFuncName = TRI_V8_ASCII_STRING("main");
      v8::Handle<v8::Function> main = v8::Handle<v8::Function>::Cast(
          localContext->Global()->Get(mainFuncName));

      if (main.IsEmpty() || main->IsUndefined()) {
        LOG(FATAL) << "no main function defined, giving up";
        FATAL_ERROR_EXIT();
      } else {
        v8::Handle<v8::Value> args[] = {params};

        try {
          v8::Handle<v8::Value> result = main->Call(main, 1, args);

          if (tryCatch.HasCaught()) {
            if (tryCatch.CanContinue()) {
              TRI_LogV8Exception(isolate, &tryCatch);
            } else {
              // will stop, so need for v8g->_canceled = true;
              TRI_ASSERT(!ok);
            }
          } else {
            ok = TRI_ObjectToDouble(result) == 0;
          }
        } catch (arangodb::basics::Exception const& ex) {
          LOG(ERR) << "caught exception " << TRI_errno_string(ex.code()) << ": "
                   << ex.what();
          ok = false;
        } catch (std::bad_alloc const&) {
          LOG(ERR) << "caught exception "
                   << TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY);
          ok = false;
        } catch (...) {
          LOG(ERR) << "caught unknown exception";
          ok = false;
        }
      }
    }

    localContext->Exit();
  }

  _applicationV8->exitContext(context);
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
#endif
}
