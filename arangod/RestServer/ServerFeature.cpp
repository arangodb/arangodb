////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#include "ServerFeature.h"

#include "Aql/RestAqlHandler.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/messages.h"
#include "Basics/process-utils.h"
#include "Cluster/RestShardHandler.h"
#include "HttpServer/HttpHandlerFactory.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Rest/HttpRequest.h"
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
#include "RestServer/DatabaseFeature.h"
#include "Scheduler/SchedulerFeature.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8Server/V8DealerFeature.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::options;
using namespace arangodb::rest;

ServerFeature::ServerFeature(application_features::ApplicationServer* server,
                             std::string const& authenticationRealm, int* res)
    : ApplicationFeature(server, "Server"),
      _defaultApiCompatibility(Version::getNumericServerVersion()),
      _allowMethodOverride(false),
      _console(false),
      _restServer(true),
      _authentication(false),
      _authenticationRealm(authenticationRealm),
      _result(res),
      _handlerFactory(nullptr),
      _jobManager(nullptr),
      _operationMode(OperationMode::MODE_SERVER),
      _consoleThread(nullptr) {
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("Cluster");
  startsAfter("Database");
  startsAfter("Dispatcher");
  startsAfter("Scheduler");
  startsAfter("WorkMonitor");
}

void ServerFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::collectOptions";

  options->addOption("--console", "start a JavaScript emergency console",
                     new BooleanParameter(&_console, false));

  options->addSection("server", "Server features");

  options->addHiddenOption("--server.default-api-compatibility",
                           "default API compatibility version",
                           new Int32Parameter(&_defaultApiCompatibility));

  options->addHiddenOption("--server.rest-server", "start a rest-server",
                           new BooleanParameter(&_restServer));

#warning TODO
#if 0
  // other options
      "start-service", "used to start as windows service")(
      "no-server", "do not start the server, if console is requested")(
      "use-thread-affinity", &_threadAffinity,
      "try to set thread affinity (0=disable, 1=disjunct, 2=overlap, "
      "3=scheduler, 4=dispatcher)");

(
      "javascript.script-parameter", &_scriptParameters, "script parameter");

(
              "server.hide-product-header", &HttpResponse::HIDE_PRODUCT_HEADER,
              "do not expose \"Server: ArangoDB\" header in HTTP responses")

              "server.session-timeout", &VocbaseContext::ServerSessionTtl,
              "timeout of web interface server sessions (in seconds)");


  additional["Server Options:help-admin"](
      "server.authenticate-system-only", &_authenticateSystemOnly,
      "use HTTP authentication only for requests to /_api and /_admin")

    (
      "server.disable-authentication", &_disableAuthentication,
      "disable authentication for ALL client requests")

#ifdef ARANGODB_HAVE_DOMAIN_SOCKETS
      ("server.disable-authentication-unix-sockets",
       &_disableAuthenticationUnixSockets,
       "disable authentication for requests via UNIX domain sockets")
#endif
#endif

  options->addSection("http", "HttpServer features");

  options->addHiddenOption("--http.allow-method-override",
                           "allow HTTP method override using special headers",
                           new BooleanParameter(&_allowMethodOverride));

  options->addSection("javascript", "Configure the Javascript engine");

  options->addHiddenOption("--javascript.unit-tests", "run unit-tests and exit",
                           new VectorParameter<StringParameter>(&_unitTests));

  options->addOption("--javascript.script", "run scripts and exit",
                     new VectorParameter<StringParameter>(&_scripts));

  options->addOption("--javascript.script-parameter", "script parameter",
                     new VectorParameter<StringParameter>(&_scriptParameters));
}

void ServerFeature::validateOptions(std::shared_ptr<ProgramOptions>) {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::validateOptions";

  if (_defaultApiCompatibility < HttpRequest::MIN_COMPATIBILITY) {
    LOG(FATAL) << "invalid value for --server.default-api-compatibility. "
                  "minimum allowed value is "
               << HttpRequest::MIN_COMPATIBILITY;
    FATAL_ERROR_EXIT();
  }

  LOG(DEBUG) << "using default API compatibility: "
             << (long int)_defaultApiCompatibility;

  int count = 0;

  if (_console) {
    _operationMode = OperationMode::MODE_CONSOLE;
    ++count;
  }

  if (!_unitTests.empty()) {
    _operationMode = OperationMode::MODE_UNITTESTS;
    ++count;
  }

  if (!_scripts.empty()) {
    _operationMode = OperationMode::MODE_SCRIPT;
    ++count;
  }

  if (1 < count) {
    LOG(FATAL) << "cannot combine '--console', '--javascript.unit-tests' and "
               << "'--javascript.script'";
    FATAL_ERROR_EXIT();
  }

  if (_operationMode == OperationMode::MODE_SERVER && !_restServer) {
    LOG(FATAL) << "need at least '--console', '--javascript.unit-tests' or"
               << "'--javascript.script if rest-server is disabled";
    FATAL_ERROR_EXIT();
  }

  if (!_restServer) {
    ApplicationServer::disableFeatures(
        {"Daemon", "Dispatcher", "Endpoint", "Scheduler", "Ssl", "Supervisor"});

    DatabaseFeature* database = dynamic_cast<DatabaseFeature*>(
        ApplicationServer::lookupFeature("Database"));
    database->disableReplicationApplier();

    TRI_ENABLE_STATISTICS = false;
  }

  V8DealerFeature* v8dealer = dynamic_cast<V8DealerFeature*>(
      ApplicationServer::lookupFeature("V8Dealer"));

  if (_operationMode == OperationMode::MODE_SCRIPT ||
      _operationMode == OperationMode::MODE_UNITTESTS) {
    _authentication = false;
    v8dealer->setMinimumContexts(2);
  } else {
    v8dealer->setMinimumContexts(1);
  }

  if (_operationMode == OperationMode::MODE_CONSOLE) {
    ApplicationServer::disableFeatures({"Daemon", "Supervisor"});
    v8dealer->increaseContexts();
  }

  if (_operationMode == OperationMode::MODE_SERVER ||
      _operationMode == OperationMode::MODE_CONSOLE) {
    ApplicationServer::lookupFeature("Shutdown")->disable();
  }
}

void ServerFeature::prepare() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::prepare";

  buildHandlerFactory();
  HttpHandlerFactory::setMaintenance(true);
}

void ServerFeature::start() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::start";

  auto vocbase = DatabaseFeature::DATABASE->vocbase();
  V8DealerFeature::DEALER->loadJavascript(vocbase, "server/server.js");
  _httpOptions._vocbase = vocbase;

  if (_operationMode != OperationMode::MODE_CONSOLE) {
    auto scheduler = dynamic_cast<SchedulerFeature*>(
        ApplicationServer::lookupFeature("Scheduler"));

    if (scheduler != nullptr) {
      scheduler->buildControlCHandler();
    }
  }

  defineHandlers();
  HttpHandlerFactory::setMaintenance(false);

#warning TODO
#if 0
  // disabled maintenance mode
  waitForHeartbeat();

  // just wait until we are signalled
  _applicationServer->wait();
#endif

#warning TODO
#if 0
  _jobManager = new AsyncJobManager(ClusterCommRestCallback);
#endif

  if (!_authentication) {
    LOG(INFO) << "Authentication is turned off";
  }

  LOG(INFO) << "ArangoDB (version " << ARANGODB_VERSION_FULL
            << ") is ready for business. Have fun!";

  *_result = EXIT_SUCCESS;

  if (_operationMode == OperationMode::MODE_CONSOLE) {
    startConsole();
  } else if (_operationMode == OperationMode::MODE_UNITTESTS) {
    *_result = runUnitTests();
  } else if (_operationMode == OperationMode::MODE_SCRIPT) {
    *_result = runScript();
  }
}

void ServerFeature::beginShutdown() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::shutdown";

  std::string msg =
      ArangoGlobalContext::CONTEXT->binaryName() + " [shutting down]";
  TRI_SetProcessTitle(msg.c_str());
}

void ServerFeature::stop() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::stop";

  _httpOptions._vocbase = nullptr;

  if (_operationMode == OperationMode::MODE_CONSOLE) {
    stopConsole();
  }
}

std::vector<std::string> ServerFeature::httpEndpoints() {
#warning TODO
  return {};
}

void ServerFeature::startConsole() {
  DatabaseFeature* database = dynamic_cast<DatabaseFeature*>(
      ApplicationServer::lookupFeature("Database"));

  _consoleThread.reset(new ConsoleThread(server(), database->vocbase()));
  _consoleThread->start();
}

void ServerFeature::stopConsole() {
  _consoleThread->userAbort();
  _consoleThread->beginShutdown();

  int iterations = 0;

  while (_consoleThread->isRunning() && ++iterations < 30) {
    usleep(100 * 1000);  // spin while console is still needed
  }

  std::cout << std::endl << TRI_BYE_MESSAGE << std::endl;
}

int ServerFeature::runUnitTests() {
  DatabaseFeature* database = dynamic_cast<DatabaseFeature*>(
      ApplicationServer::lookupFeature("Database"));
  V8Context* context =
      V8DealerFeature::DEALER->enterContext(database->vocbase(), true);

  auto isolate = context->_isolate;

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

  V8DealerFeature::DEALER->exitContext(context);

  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int ServerFeature::runScript() {
  bool ok = false;

  DatabaseFeature* database = dynamic_cast<DatabaseFeature*>(
      ApplicationServer::lookupFeature("Database"));
  V8Context* context =
      V8DealerFeature::DEALER->enterContext(database->vocbase(), true);

  auto isolate = context->_isolate;

  {
    v8::HandleScope globalScope(isolate);

    auto localContext = v8::Local<v8::Context>::New(isolate, context->_context);
    localContext->Enter();
    {
      v8::Context::Scope contextScope(localContext);
      for (auto script : _scripts) {
        bool r = TRI_ExecuteGlobalJavaScriptFile(isolate, script.c_str(), true);

        if (!r) {
          LOG(FATAL) << "cannot load script '" << script << "', giving up";
          FATAL_ERROR_EXIT();
        }
      }

      v8::TryCatch tryCatch;
      // run the garbage collection for at most 30 seconds
      TRI_RunGarbageCollectionV8(isolate, 30.0);

      // parameter array
      v8::Handle<v8::Array> params = v8::Array::New(isolate);

      params->Set(0, TRI_V8_STD_STRING(_scripts[_scripts.size() - 1]));

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

  V8DealerFeature::DEALER->exitContext(context);

  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

static TRI_vocbase_t* LookupDatabaseFromRequest(HttpRequest* request,
                                                TRI_server_t* server) {
  // get database name from request
  std::string dbName = request->databaseName();

  if (dbName.empty()) {
    // if no databases was specified in the request, use system database name
    // as a fallback
    dbName = TRI_VOC_SYSTEM_DATABASE;
    request->setDatabaseName(dbName);
  }

  if (ServerState::instance()->isCoordinator()) {
    return TRI_UseCoordinatorDatabaseServer(server, dbName.c_str());
  }

  return TRI_UseDatabaseServer(server, dbName.c_str());
}

static bool SetRequestContext(HttpRequest* request, void* data) {
  TRI_server_t* server = static_cast<TRI_server_t*>(data);
  TRI_vocbase_t* vocbase = LookupDatabaseFromRequest(request, server);

  // invalid database name specified, database not found etc.
  if (vocbase == nullptr) {
    return false;
  }

  // database needs upgrade
  if (vocbase->_state == (sig_atomic_t)TRI_VOCBASE_STATE_FAILED_VERSION) {
    request->setRequestPath("/_msg/please-upgrade");
    return false;
  }

  VocbaseContext* ctx = new arangodb::VocbaseContext(request, server, vocbase);
  request->setRequestContext(ctx, true);

  // the "true" means the request is the owner of the context
  return true;
}

void ServerFeature::buildHandlerFactory() {
  _handlerFactory.reset(new HttpHandlerFactory(
      _authenticationRealm, _defaultApiCompatibility, _allowMethodOverride,
      &SetRequestContext, nullptr));
}

void ServerFeature::defineHandlers() {
#warning TODO
#if 0
  // ...........................................................................
  // /_msg
  // ...........................................................................

  _handlerFactory->addPrefixHandler(
      "/_msg/please-upgrade",
      RestHandlerCreator<RestPleaseUpgradeHandler>::createNoData);

  // ...........................................................................
  // /_api
  // ...........................................................................

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::BATCH_PATH,
      RestHandlerCreator<RestBatchHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::CURSOR_PATH,
      RestHandlerCreator<RestCursorHandler>::createData<
          std::pair<ApplicationV8*, aql::QueryRegistry*>*>,
      _pairForAqlHandler);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::DOCUMENT_PATH,
      RestHandlerCreator<RestDocumentHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::EDGE_PATH,
      RestHandlerCreator<RestEdgeHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::EDGES_PATH,
      RestHandlerCreator<RestEdgesHandler>::createNoData);

#warning TODO
#if 0
  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::EXPORT_PATH,
      RestHandlerCreator<RestExportHandler>::createNoData);
#endif

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::IMPORT_PATH,
      RestHandlerCreator<RestImportHandler>::createNoData);

#warning TODO
#if 0
  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::REPLICATION_PATH,
      RestHandlerCreator<RestReplicationHandler>::createNoData);
#endif

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::SIMPLE_QUERY_ALL_PATH,
      RestHandlerCreator<RestSimpleQueryHandler>::createData<
          std::pair<ApplicationV8*, aql::QueryRegistry*>*>,
      _pairForAqlHandler);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::SIMPLE_LOOKUP_PATH,
      RestHandlerCreator<RestSimpleHandler>::createData<
          std::pair<ApplicationV8*, aql::QueryRegistry*>*>,
      _pairForAqlHandler);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::SIMPLE_REMOVE_PATH,
      RestHandlerCreator<RestSimpleHandler>::createData<
          std::pair<ApplicationV8*, aql::QueryRegistry*>*>,
      _pairForAqlHandler);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::UPLOAD_PATH,
      RestHandlerCreator<RestUploadHandler>::createNoData);

#warning TODO
#if 0
  _handlerFactory->addPrefixHandler(
      "/_api/shard-comm",
      RestHandlerCreator<RestShardHandler>::createNoData);
#endif

  _handlerFactory->addPrefixHandler(
      "/_api/aql", RestHandlerCreator<aql::RestAqlHandler>::createData<
                       std::pair<ApplicationV8*, aql::QueryRegistry*>*>,
      _pairForAqlHandler);

  _handlerFactory->addPrefixHandler(
      "/_api/query",
      RestHandlerCreator<RestQueryHandler>::createData<ApplicationV8*>,
      _applicationV8);

  _handlerFactory->addPrefixHandler(
      "/_api/query-cache",
      RestHandlerCreator<RestQueryCacheHandler>::createNoData);

  // And now some handlers which are registered in both /_api and /_admin
  _handlerFactory->addPrefixHandler(
      "/_api/job", RestHandlerCreator<arangodb::RestJobHandler>::createData<
                       std::pair<Dispatcher*, AsyncJobManager*>*>,
      _pairForJobHandler);

  _handlerFactory->addHandler(
      "/_api/version", RestHandlerCreator<RestVersionHandler>::createNoData,
      nullptr);

  // ...........................................................................
  // /_admin
  // ...........................................................................

  _handlerFactory->addPrefixHandler(
      "/_admin/job", RestHandlerCreator<arangodb::RestJobHandler>::createData<
                         std::pair<Dispatcher*, AsyncJobManager*>*>,
      _pairForJobHandler);

  _handlerFactory->addHandler(
      "/_admin/version", RestHandlerCreator<RestVersionHandler>::createNoData,
      nullptr);

  // further admin handlers
  _handlerFactory->addHandler(
      "/_admin/log",
      RestHandlerCreator<arangodb::RestAdminLogHandler>::createNoData, nullptr);

  _handlerFactory->addPrefixHandler(
      "/_admin/work-monitor",
      RestHandlerCreator<WorkMonitorHandler>::createNoData, nullptr);

// This handler is to activate SYS_DEBUG_FAILAT on DB servers
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  _handlerFactory->addPrefixHandler(
      "/_admin/debug", RestHandlerCreator<RestDebugHandler>::createNoData,
      nullptr);
#endif

#warning TODO
#if 0
  _handlerFactory->addPrefixHandler(
      "/_admin/shutdown",
      RestHandlerCreator<arangodb::RestShutdownHandler>::createData<void*>,
      static_cast<void*>(_applicationServer));
#endif

  // ...........................................................................
  // /_admin
  // ...........................................................................

  _handlerFactory->addPrefixHandler(
      "/", RestHandlerCreator<RestActionHandler>::createData<
               RestActionHandler::action_options_t*>,
      (void*)&httpOptions);
#endif
}

#warning TODO
#if 0

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

  // .............................................................................
  // try to figure out the thread affinity
  // .............................................................................

  size_t n = TRI_numberProcessors();

  if (n > 2 && _threadAffinity > 0) {
    size_t ns = _applicationScheduler->numberOfThreads();
    size_t nd = _applicationDispatcher->numberOfThreads();

    if (ns != 0 && nd != 0) {
      LOG(INFO) << "the server has " << n << " (hyper) cores, using " << ns
                << " scheduler threads, " << nd << " dispatcher threads";
    } else {
      _threadAffinity = 0;
    }

    switch (_threadAffinity) {
      case 1:
        if (n < ns + nd) {
          ns = static_cast<size_t>(round(1.0 * n * ns / (ns + nd)));
          nd = static_cast<size_t>(round(1.0 * n * nd / (ns + nd)));

          if (ns < 1) {
            ns = 1;
          }
          if (nd < 1) {
            nd = 1;
          }

          while (n < ns + nd) {
            if (1 < ns) {
              ns -= 1;
            } else if (1 < nd) {
              nd -= 1;
            } else {
              ns = 1;
              nd = 1;
            }
          }
        }

        break;

      case 2:
        if (n < ns) {
          ns = n;
        }

        if (n < nd) {
          nd = n;
        }

        break;

      case 3:
        if (n < ns) {
          ns = n;
        }

        nd = 0;

        break;

      case 4:
        if (n < nd) {
          nd = n;
        }

        ns = 0;

        break;

      default:
        _threadAffinity = 0;
        break;
    }

    if (_threadAffinity > 0) {
      TRI_ASSERT(ns <= n);
      TRI_ASSERT(nd <= n);

      std::vector<size_t> ps;
      std::vector<size_t> pd;

      for (size_t i = 0; i < ns; ++i) {
        ps.push_back(i);
      }

      for (size_t i = 0; i < nd; ++i) {
        pd.push_back(n - i - 1);
      }

      if (0 < ns) {
        _applicationScheduler->setProcessorAffinity(ps);
      }

      if (0 < nd) {
        _applicationDispatcher->setProcessorAffinity(pd);
      }

      if (0 < ns) {
        LOG(INFO) << "scheduler cores: " << ToString(ps);
      }
      if (0 < nd) {
        LOG(INFO) << "dispatcher cores: " << ToString(pd);
      }
    } else {
      LOG(INFO) << "the server has " << n << " (hyper) cores";
    }
  }

#endif
