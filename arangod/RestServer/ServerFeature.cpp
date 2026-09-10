////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "ServerFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/ShutdownFeature.h"
#include "FeaturePhases/AqlFeaturePhase.h"
#include "RestServer/UpgradeFeature.h"
#include "Basics/application-exit.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/HeartbeatThread.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/DatabaseFeature.h"
#include "Scheduler/SchedulerFeature.h"
#ifdef USE_V8
#include "V8Server/V8DealerFeature.h"
#endif

using namespace arangodb::application_features;
using namespace arangodb::options;
using namespace arangodb::rest;

namespace arangodb {

ServerFeature::ServerFeature(ApplicationServer& server, int* res)
    : ServerFeature(server, res, ServerFeatureOptions{}) {}

ServerFeature::ServerFeature(ApplicationServer& server, int* res,
                             ServerFeatureOptions options)
    : ApplicationFeature{server, *this},
      _options(std::move(options)),
      _result(res) {
  setOptional(true);
  startsAfter<AqlFeaturePhase>();
  startsAfter<UpgradeFeature>();
}

void ServerFeature::prepare() {
  DatabaseFeature& db = server().getFeature<DatabaseFeature>();
  if (operationMode() == OperationMode::MODE_SERVER && !_options.restServer &&
      !db.upgrade() &&
      !server().options()->processingResult().touched("rocksdb.verify-sst")) {
    LOG_TOPIC("8daab", FATAL, arangodb::Logger::FIXME)
        << "need at least '--console', '--javascript.unit-tests' or"
        << "'--javascript.script if rest-server is disabled";
    FATAL_ERROR_EXIT();
  }

  bool supportsV8 = false;
#ifdef USE_V8
  V8DealerFeature& v8dealer = server().getFeature<V8DealerFeature>();

  if (v8dealer.isEnabled()) {
    if (operationMode() == OperationMode::MODE_SCRIPT) {
      v8dealer.setMinimumExecutors(2);
    } else {
      v8dealer.setMinimumExecutors(1);
    }
    supportsV8 = true;
  }
#endif
  if (!supportsV8 && operationMode() != OperationMode::MODE_SERVER) {
    LOG_TOPIC("a114b", FATAL, arangodb::Logger::FIXME)
        << "Options '--console', '--javascript.unit-tests'"
        << " or '--javascript.script' are not supported without V8";
    FATAL_ERROR_EXIT();
  }

#ifdef USE_V8
  if (operationMode() == OperationMode::MODE_CONSOLE) {
    v8dealer.setMinimumExecutors(2);
  }
#endif

  if (operationMode() == OperationMode::MODE_SERVER ||
      operationMode() == OperationMode::MODE_CONSOLE) {
    server().getFeature<ShutdownFeature>().disable();
  }

  // adjust global settings for UTF-8 string validation
  basics::VelocyPackHelper::strictRequestValidationOptions.validateUtf8Strings =
      _options.validateUtf8Strings;
}

void ServerFeature::start() {
  waitForHeartbeat();

  *_result = EXIT_SUCCESS;

  switch (operationMode()) {
    case OperationMode::MODE_SCRIPT:
    case OperationMode::MODE_CONSOLE:
      break;

    case OperationMode::MODE_SERVER:
      LOG_TOPIC("7031b", TRACE, Logger::STARTUP)
          << "server operation mode: SERVER";
      break;
  }

  // flush all log output before we go on... this is sensible because any
  // of the following options may print or prompt, and pending log entries
  // might overwrite that
  Logger::flush();

  if (!isConsoleMode()) {
    // install CTRL-C handlers
    server().registerStartupCallback([this]() {
      server().getFeature<SchedulerFeature>().buildControlCHandler();
    });
  }
}

void ServerFeature::beginShutdown() { _isStopping = true; }

void ServerFeature::waitForHeartbeat() {
  if (!ServerState::instance()->isCoordinator()) {
    // waiting for the heartbeart thread is necessary on coordinator only
    return;
  }

  if (!server().hasFeature<ClusterFeature>()) {
    return;
  }

  auto& cf = server().getFeature<ClusterFeature>();

  while (true) {
    auto heartbeatThread = cf.heartbeatThread();
    TRI_ASSERT(heartbeatThread != nullptr);
    if (heartbeatThread == nullptr || heartbeatThread->hasRunOnce()) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
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

}  // namespace arangodb
