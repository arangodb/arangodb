////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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

#include "Cluster/MaintenanceOptionsProvider.h"

#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {

using namespace arangodb::options;

void MaintenanceOptionsProvider::declareOptionsImpl(
    std::shared_ptr<ProgramOptions> opts, MaintenanceOptions& options) {
  opts->addOption(
      "--server.maintenance-threads",
      "The maximum number of threads available for maintenance actions.",
      new UInt32Parameter(&options.maintenanceThreadsMax),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnDBServer,
          arangodb::options::Flags::Uncommon,
          arangodb::options::Flags::Dynamic));

  opts->addOption(
          "--server.maximal-number-sync-shard-actions",
          "The maximum number of SynchronizeShard actions which may be queued "
          "at any given time.",
          new UInt64Parameter(&options.maximalNumberOfSyncShardActionsQueued, 1,
                              1, std::numeric_limits<uint64_t>::max()),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::Uncommon))
      .setIntroducedIn(31205);

  opts->addOption("--server.maintenance-slow-threads",
                  "The maximum number of threads available for slow "
                  "maintenance actions (long SynchronizeShard and long "
                  "EnsureIndex).",
                  new UInt32Parameter(&options.maintenanceThreadsSlowMax),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::Dynamic))
      .setIntroducedIn(30803);

  opts->addOption(
      "--server.maintenance-actions-block",
      "The minimum number of seconds finished actions block duplicates.",
      new Int32Parameter(&options.secondsActionsBlock),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnDBServer,
          arangodb::options::Flags::Uncommon));

  opts->addOption(
      "--server.maintenance-actions-linger",
      "The minimum number of seconds finished actions remain in the deque.",
      new Int32Parameter(&options.secondsActionsLinger),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnDBServer,
          arangodb::options::Flags::Uncommon));

  opts->addOption(
      "--cluster.resign-leadership-on-shutdown",
      "Create a resign leader ship job for this DB-Server on shutdown.",
      new BooleanParameter(&options.resignLeadershipOnShutdown),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnDBServer,
          arangodb::options::Flags::Uncommon));
}

void MaintenanceOptionsProvider::processOptionsImpl(
    std::shared_ptr<ProgramOptions> opts, MaintenanceOptions& options) {
  // There must always be at least 3 maintenance threads.
  // The first one only does actions which are labelled "fast track".
  // The next few threads do "slower" actions, but never work on very slow
  // actions which came into being by rescheduling. If they stumble on
  // an actions which seems to take long, they give up and reschedule
  // with a special slow priority, such that the action is eventually
  // executed by the slow threads. We can configure both the total number
  // of threads as well as the number of slow threads. The number of slow
  // threads must always be at most N-2 if N is the total number of threads.
  // The default for the slow threads is N/2, unless the user has used
  // an override.
  constexpr uint32_t minThreadLimit = 3;
  constexpr uint32_t maxThreadLimit = 64;

  if (options.maintenanceThreadsMax < minThreadLimit) {
    LOG_TOPIC("37726", WARN, Logger::MAINTENANCE)
        << "Need at least" << minThreadLimit << "maintenance-threads";
    options.maintenanceThreadsMax = minThreadLimit;
  } else if (options.maintenanceThreadsMax > maxThreadLimit) {
    LOG_TOPIC("8fb0e", WARN, Logger::MAINTENANCE)
        << "maintenance-threads limited to " << maxThreadLimit;
    options.maintenanceThreadsMax = maxThreadLimit;
  }
  if (!opts->processingResult().touched("server.maintenance-slow-threads")) {
    options.maintenanceThreadsSlowMax = options.maintenanceThreadsMax / 2;
  }
  if (options.maintenanceThreadsSlowMax + 2 > options.maintenanceThreadsMax) {
    options.maintenanceThreadsSlowMax = options.maintenanceThreadsMax - 2;
    LOG_TOPIC("54251", WARN, Logger::MAINTENANCE)
        << "maintenance-slow-threads limited to "
        << options.maintenanceThreadsSlowMax;
  }
  if (options.maintenanceThreadsSlowMax == 0) {
    options.maintenanceThreadsSlowMax = 1;
    LOG_TOPIC("54252", WARN, Logger::MAINTENANCE)
        << "maintenance-slow-threads raised to "
        << options.maintenanceThreadsSlowMax;
  }
}

void MaintenanceOptionsProvider::validateOptionsImpl(
    std::shared_ptr<ProgramOptions> /*opts*/,
    MaintenanceOptions const& /*options*/) {}

}  // namespace arangodb
