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

#include "Basics/ArangoGlobalContext.h"
#include "Basics/process-utils.h"
#include "Cluster/HeartbeatThread.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Rest/HttpRequest.h"
#include "Rest/Version.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/VocbaseContext.h"
#include "Scheduler/SchedulerFeature.h"
#include "Statistics/StatisticsFeature.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8Server/V8Context.h"
#include "V8Server/V8DealerFeature.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::options;
using namespace arangodb::rest;

ServerFeature::ServerFeature(application_features::ApplicationServer* server,
                             int* res)
    : ApplicationFeature(server, "Server"),
      _console(false),
      _restServer(true),
      _result(res),
      _operationMode(OperationMode::MODE_SERVER) {
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("Cluster");
  startsAfter("Database");
  startsAfter("Dispatcher");
  startsAfter("Recovery");
  startsAfter("Scheduler");
  startsAfter("Statistics");
  startsAfter("Upgrade");
  startsAfter("V8Dealer");
  startsAfter("WorkMonitor");
}

void ServerFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addOption("--console", "start a JavaScript emergency console",
                     new BooleanParameter(&_console));

  options->addSection("server", "Server features");

  options->addHiddenOption("--server.rest-server", "start a rest-server",
                           new BooleanParameter(&_restServer));

  options->addOption("--server.session-timeout",
                     "timeout of web interface server sessions (in seconds)",
                     new DoubleParameter(&VocbaseContext::ServerSessionTtl));

//YYY #warning TODO
#if 0
  // other options
      "start-service", "used to start as windows service")
#endif

  options->addSection("javascript", "Configure the Javascript engine");

  options->addHiddenOption("--javascript.unit-tests", "run unit-tests and exit",
                           new VectorParameter<StringParameter>(&_unitTests));

  options->addOption("--javascript.script", "run scripts and exit",
                     new VectorParameter<StringParameter>(&_scripts));

  options->addOption("--javascript.script-parameter", "script parameter",
                     new VectorParameter<StringParameter>(&_scriptParameters));
}

void ServerFeature::validateOptions(std::shared_ptr<ProgramOptions>) {
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
    ApplicationServer::disableFeatures({"Daemon", "Dispatcher", "Endpoint",
                                        "RestServer", "Scheduler", "Ssl",
                                        "Supervisor"});

    DatabaseFeature* database = 
        ApplicationServer::getFeature<DatabaseFeature>("Database");
    database->disableReplicationApplier();

    StatisticsFeature* statistics = 
        ApplicationServer::getFeature<StatisticsFeature>("Statistics");
    statistics->disableStatistics();
  }

  V8DealerFeature* v8dealer = 
      ApplicationServer::getFeature<V8DealerFeature>("V8Dealer");

  if (_operationMode == OperationMode::MODE_SCRIPT ||
      _operationMode == OperationMode::MODE_UNITTESTS) {
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
    ApplicationServer::getFeature<ApplicationFeature>("Shutdown")->disable();
  }
}

void ServerFeature::start() {
  if (_operationMode != OperationMode::MODE_CONSOLE && _restServer) {
    auto scheduler = 
        ApplicationServer::getFeature<SchedulerFeature>("Scheduler");

    scheduler->buildControlCHandler();
  }

  waitForHeartbeat();

  *_result = EXIT_SUCCESS;

  // flush all log output before we go on... this is sensible because any
  // of the following options may print or prompt, and pending log entries
  // might overwrite that 
  Logger::flush();
   
  switch (_operationMode) {
    case OperationMode::MODE_UNITTESTS:
      LOG_TOPIC(TRACE, Logger::STARTUP) << "server about to run unit-tests";
      *_result = runUnitTests();
      break;

    case OperationMode::MODE_SCRIPT:
      LOG_TOPIC(TRACE, Logger::STARTUP) << "server about to run scripts";
      *_result = runScript();
      break;

    case OperationMode::MODE_CONSOLE:
      LOG_TOPIC(TRACE, Logger::STARTUP) << "server operation mode: CONSOLE";
      break;

    case OperationMode::MODE_SERVER:
      LOG_TOPIC(TRACE, Logger::STARTUP) << "server operation mode: SERVER";
      break;
  }
}

void ServerFeature::beginShutdown() {
  std::string msg =
      ArangoGlobalContext::CONTEXT->binaryName() + " [shutting down]";
  TRI_SetProcessTitle(msg.c_str());
}

void ServerFeature::waitForHeartbeat() {
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
  
std::string ServerFeature::operationModeString(OperationMode mode) {
  switch (mode) {
    case OperationMode::MODE_CONSOLE: 
      return "console";
    case OperationMode::MODE_UNITTESTS: 
      return "unittests";
    case OperationMode::MODE_SCRIPT: 
      return "script";
    case OperationMode::MODE_SERVER: 
      return "server";
    default: 
      return "unknown";
  }
}

int ServerFeature::runUnitTests() {
  DatabaseFeature* database = 
      ApplicationServer::getFeature<DatabaseFeature>("Database");
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

  DatabaseFeature* database = 
      ApplicationServer::getFeature<DatabaseFeature>("Database");
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
        LOG(TRACE) << "executing script '" << script << "'";
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
