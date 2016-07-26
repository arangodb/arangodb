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

  options->addSection("javascript", "Configure the Javascript engine");

  options->addHiddenOption("--javascript.unit-tests", "run unit-tests and exit",
                           new VectorParameter<StringParameter>(&_unitTests));

  options->addOption("--javascript.script", "run scripts and exit",
                     new VectorParameter<StringParameter>(&_scripts));
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
                                        "GeneralServer", "Scheduler",
                                        "SslServer", "Supervisor"});

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
    case OperationMode::MODE_SCRIPT:
    case OperationMode::MODE_CONSOLE:
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
