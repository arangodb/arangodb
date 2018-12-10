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
#include "Replication/ReplicationFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/VocbaseContext.h"
#include "Scheduler/SchedulerFeature.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8Server/V8Context.h"
#include "V8Server/V8DealerFeature.h"

using namespace arangodb::application_features;
using namespace arangodb::options;
using namespace arangodb::rest;

namespace arangodb {

ServerFeature::ServerFeature(
    application_features::ApplicationServer& server,
    int* res
)
    : ApplicationFeature(server, "Server"),
      _vstMaxSize(1024 * 30),
      _result(res),
      _operationMode(OperationMode::MODE_SERVER)
#if _WIN32
      ,_codePage(65001), // default to UTF8
      _originalCodePage(UINT16_MAX)
#endif
  {
  setOptional(true);

  startsAfter("AQLPhase");
  startsAfter("Statistics");
  startsAfter("Upgrade");
}

void ServerFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addOption("--console", "start a JavaScript emergency console",
                     new BooleanParameter(&_console));

  options->addSection("server", "Server features");

  options->addOption("--server.rest-server", "start a rest-server",
                     new BooleanParameter(&_restServer),
                     arangodb::options::makeFlags(arangodb::options::Flags::Hidden));

  options->addObsoleteOption("--server.session-timeout",
                             "timeout of web interface server sessions (in seconds)",
                             true);

  options->addSection("javascript", "Configure the Javascript engine");

  options->addOption("--javascript.script", "run scripts and exit",
                     new VectorParameter<StringParameter>(&_scripts));

  options->addSection("vst", "Configure the VelocyStream protocol");

  options->addOption("--vst.maxsize",
                     "maximal size (in bytes) for a VelocyPack chunk",
                     new UInt32Parameter(&_vstMaxSize));
#if _WIN32
  options->addOption("--console.code-page", "Windows code page to use; defaults to UTF8",
                     new UInt16Parameter(&_codePage),
                     arangodb::options::makeFlags(arangodb::options::Flags::Hidden));
#endif
}

void ServerFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  int count = 0;

  if (_console) {
    _operationMode = OperationMode::MODE_CONSOLE;
    ++count;
  }

  if (!_scripts.empty()) {
    _operationMode = OperationMode::MODE_SCRIPT;
    ++count;
  }

  if (1 < count) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "cannot combine '--console', '--javascript.unit-tests' and "
               << "'--javascript.script'";
    FATAL_ERROR_EXIT();
  }

  if (_operationMode == OperationMode::MODE_SERVER && !_restServer) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "need at least '--console', '--javascript.unit-tests' or"
               << "'--javascript.script if rest-server is disabled";
    FATAL_ERROR_EXIT();
  }
  
  V8DealerFeature* v8dealer =
    ApplicationServer::getFeature<V8DealerFeature>("V8Dealer");
  
  if (v8dealer->isEnabled()) {
    if (_operationMode == OperationMode::MODE_SCRIPT) {
      v8dealer->setMinimumContexts(2);
    } else {
      v8dealer->setMinimumContexts(1);
    }
  } else if (_operationMode != OperationMode::MODE_SERVER) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "Options '--console', '--javascript.unit-tests'"
       << " or '--javascript.script' are not supported without V8";
    FATAL_ERROR_EXIT();
  }

  if (!_restServer) {
    ApplicationServer::disableFeatures({"Daemon", "Endpoint", "GeneralServer",
                                        "SslServer", "Statistics", "Supervisor"});

    if (!options->processingResult().touched("replication.auto-start")) {
      // turn off replication applier when we do not have a rest server
      // but only if the config option is not explicitly set (the recovery
      // test want the applier to be enabled for testing it)
      ReplicationFeature* replicationFeature =
          ApplicationServer::getFeature<ReplicationFeature>("Replication");
      replicationFeature->disableReplicationApplier();
    }
  }

  if (_operationMode == OperationMode::MODE_CONSOLE) {
    ApplicationServer::disableFeatures({"Daemon", "Supervisor"});
    v8dealer->setMinimumContexts(2);
  }

  if (_operationMode == OperationMode::MODE_SERVER ||
      _operationMode == OperationMode::MODE_CONSOLE) {
    ApplicationServer::getFeature<ApplicationFeature>("Shutdown")->disable();
  }
}

void ServerFeature::start() {
#if _WIN32
  _originalCodePage = GetConsoleOutputCP();
  if (IsValidCodePage(_codePage)) {
    SetConsoleOutputCP(_codePage);
  }
#endif

  waitForHeartbeat();

  *_result = EXIT_SUCCESS;

  switch (_operationMode) {
    case OperationMode::MODE_SCRIPT:
    case OperationMode::MODE_CONSOLE:
      break;

    case OperationMode::MODE_SERVER:
      LOG_TOPIC(TRACE, Logger::STARTUP) << "server operation mode: SERVER";
      break;
  }
  
  // flush all log output before we go on... this is sensible because any
  // of the following options may print or prompt, and pending log entries
  // might overwrite that
  Logger::flush();
 
  if (!isConsoleMode()) {
    // install CTRL-C handlers
    server()->registerStartupCallback([]() {
      ApplicationServer::getFeature<SchedulerFeature>("Scheduler")->buildControlCHandler();
    });
  }

}

void ServerFeature::stop() {
#if _WIN32
  if (IsValidCodePage(_originalCodePage)) {
    SetConsoleOutputCP(_originalCodePage);
  }
#endif
}

void ServerFeature::beginShutdown() {
  std::string msg =
      ArangoGlobalContext::CONTEXT->binaryName() + " [shutting down]";
  TRI_SetProcessTitle(msg.c_str());
  _isStopping = true;
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
    std::this_thread::sleep_for(std::chrono::microseconds(100 * 1000));
  }
}

std::string ServerFeature::operationModeString(OperationMode mode) {
  switch (mode) {
    case OperationMode::MODE_CONSOLE:
      return "console";
    case OperationMode::MODE_SCRIPT:
      return "script";
    case OperationMode::MODE_SERVER:
      return "server";
    default:
      return "unknown";
  }
}

} // arangodb
