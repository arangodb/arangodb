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
#include "Basics/ArangoGlobalContext.h"
#include "Basics/application-exit.h"
#include "Basics/process-utils.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/HeartbeatThread.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Replication/ReplicationFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "Scheduler/SchedulerFeature.h"
#include "Statistics/StatisticsFeature.h"

using namespace arangodb::application_features;
using namespace arangodb::options;
using namespace arangodb::rest;

namespace arangodb {

ServerFeature::ServerFeature(Server& server, int* res)
    : ArangodFeature{server, *this}, _result(res) {
  setOptional(true);
  startsAfter<AqlFeaturePhase>();
  startsAfter<UpgradeFeature>();
}

void ServerFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("server", "server features");

  options->addOption(
      "--server.rest-server", "Start a REST server.",
      new BooleanParameter(&_restServer),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options->addOption(
      "--server.validate-utf8-strings",
      "Perform UTF-8 string validation for incoming JSON and VelocyPack "
      "data.",
      new BooleanParameter(&_validateUtf8Strings),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  // add obsolete MMFiles WAL options (obsoleted in 3.7)
  options->addSection("wal", "WAL of the MMFiles engine", "", true, true);
  options->addObsoleteOption(
      "--wal.allow-oversize-entries",
      "allow entries that are bigger than '--wal.logfile-size'", false);
  options->addObsoleteOption("--wal.use-mlock",
                             "mlock WAL logfiles in memory (may require "
                             "elevated privileges or limits)",
                             false);
  options->addObsoleteOption("--wal.directory", "logfile directory", true);
  options->addObsoleteOption(
      "--wal.historic-logfiles",
      "maximum number of historic logfiles to keep after collection", true);
  options->addObsoleteOption(
      "--wal.ignore-logfile-errors",
      "ignore logfile errors. this will read recoverable data from corrupted "
      "logfiles but ignore any unrecoverable data",
      false);
  options->addObsoleteOption(
      "--wal.ignore-recovery-errors",
      "continue recovery even if re-applying operations fails", false);
  options->addObsoleteOption("--wal.flush-timeout",
                             "flush timeout (in milliseconds)", true);
  options->addObsoleteOption("--wal.logfile-size",
                             "size of each logfile (in bytes)", true);
  options->addObsoleteOption("--wal.open-logfiles",
                             "maximum number of parallel open logfiles", true);
  options->addObsoleteOption("--wal.reserve-logfiles",
                             "maximum number of reserve logfiles to maintain",
                             true);
  options->addObsoleteOption("--wal.slots", "number of logfile slots to use",
                             true);
  options->addObsoleteOption(
      "--wal.sync-interval",
      "interval for automatic, non-requested disk syncs (in milliseconds)",
      true);
  options->addObsoleteOption(
      "--wal.throttle-when-pending",
      "throttle writes when at least this many operations are waiting for "
      "collection (set to 0 to deactivate write-throttling)",
      true);
  options->addObsoleteOption(
      "--wal.throttle-wait",
      "maximum wait time per operation when write-throttled (in milliseconds)",
      true);
}

void ServerFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  DatabaseFeature& db = server().getFeature<DatabaseFeature>();

  if (!_restServer && !db.upgrade() &&
      !options->processingResult().touched("rocksdb.verify-sst")) {
    LOG_TOPIC("8daab", FATAL, arangodb::Logger::FIXME)
        << "Rest server cannot be disabled any more, because it is pointless.";
    FATAL_ERROR_EXIT();
  }

  auto disableDeamonAndSupervisor = [&]() {
    if constexpr (Server::contains<DaemonFeature>()) {
      server().disableFeatures(std::array{Server::id<DaemonFeature>()});
    }
    if constexpr (Server::contains<SupervisorFeature>()) {
      server().disableFeatures(std::array{Server::id<SupervisorFeature>()});
    }
  };

  if (!_restServer) {
    server().disableFeatures(std::array{
        Server::id<HttpEndpointProvider>(),
        Server::id<GeneralServerFeature>(),
        Server::id<SslServerFeature>(),
        Server::id<StatisticsFeature>(),
    });
    disableDeamonAndSupervisor();

    if (!options->processingResult().touched("replication.auto-start")) {
      // turn off replication applier when we do not have a rest server
      // but only if the config option is not explicitly set (the recovery
      // test want the applier to be enabled for testing it)
      ReplicationFeature& replicationFeature =
          server().getFeature<ReplicationFeature>();
      replicationFeature.disableReplicationApplier();
    }
  }

  server().getFeature<ShutdownFeature>().disable();
}

void ServerFeature::prepare() {
  // adjust global settings for UTF-8 string validation
  basics::VelocyPackHelper::strictRequestValidationOptions.validateUtf8Strings =
      _validateUtf8Strings;
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
