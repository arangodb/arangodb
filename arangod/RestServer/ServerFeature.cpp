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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "ServerFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/ShutdownFeature.h"
#include "FeaturePhases/AqlFeaturePhase.h"
#include "RestServer/UpgradeFeature.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/HeartbeatThread.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Scheduler/SchedulerFeature.h"

using namespace arangodb::application_features;
using namespace arangodb::options;

namespace arangodb {

ServerFeature::ServerFeature(ApplicationServer& server, int* res)
    : ApplicationFeature{server, *this}, _result(res) {
  setOptional(true);
  startsAfter<AqlFeaturePhase>();
  startsAfter<UpgradeFeature>();
}

void ServerFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("server", "server features");

  options->addObsoleteOption(
      "--server.rest-server",
      "Has no effect; the REST API is always available except during "
      "database upgrade, initialization, and version check.",
      true);
  options->addObsoleteOption(
      "--no-server",
      "Has no effect; use --database.auto-upgrade, --database.check-version, "
      "or --database.init-database as appropriate.",
      false);

  options->addOption(
      "--server.validate-utf8-strings",
      "Perform UTF-8 string validation for incoming JSON and VelocyPack "
      "data.",
      new BooleanParameter(&_options.validateUtf8Strings),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));
}

void ServerFeature::validateOptions(std::shared_ptr<ProgramOptions>) {
  server().getFeature<ShutdownFeature>().disable();
}

void ServerFeature::prepare() {
  // adjust global settings for UTF-8 string validation
  basics::VelocyPackHelper::strictRequestValidationOptions.validateUtf8Strings =
      _options.validateUtf8Strings;
}

void ServerFeature::start() {
  waitForHeartbeat();

  *_result = EXIT_SUCCESS;

  // flush all log output before we go on... this is sensible because any
  // of the following options may print or prompt, and pending log entries
  // might overwrite that
  Logger::flush();

  // install CTRL-C handlers
  server().registerStartupCallback([this]() {
    server().getFeature<SchedulerFeature>().buildControlCHandler();
  });
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

}  // namespace arangodb
