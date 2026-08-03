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

#include "Replication/ReplicationOptionsProvider.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {

using namespace arangodb::options;

void ReplicationOptionsProvider::declareOptionsImpl(
    std::shared_ptr<ProgramOptions> opts, ReplicationOptions& options) {
  opts->addSection("replication", "replication");
  opts->addObsoleteOption(
      "--replication.auto-start",
      "Enable or disable the automatic start of replication appliers.", true);

  opts->addOldOption("server.disable-replication-applier",
                     "replication.auto-start");
  opts->addOldOption("database.replication-applier", "replication.auto-start");

  opts->addObsoleteOption(
      "--replication.active-failover",
      "Enable active-failover during asynchronous replication.", false);
  opts->addOldOption("--replication.automatic-failover",
                     "--replication.active-failover");

  opts->addOption(
      "--replication.max-parallel-tailing-invocations",
      "The maximum number of concurrently allowed WAL tailing invocations "
      "(0 = unlimited).",
      new UInt64Parameter(&options.maxParallelTailingInvocations),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  opts->addOption("--replication.connect-timeout",
                  "The default timeout value for replication connection "
                  "attempts (in seconds).",
                  new DoubleParameter(&options.connectTimeout));
  opts->addOption("--replication.request-timeout",
                  "The default timeout value for replication requests "
                  "(in seconds).",
                  new DoubleParameter(&options.requestTimeout));

  opts->addOption(
      "--replication.quick-keys-limit",
      "Limit at which 'quick' calls to the replication keys API return "
      "only the document count for the second run.",
      new UInt64Parameter(&options.quickKeysLimit),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  opts->addOption(
      "--replication.sync-by-revision",
      "Whether to use the newer revision-based replication protocol.",
      new BooleanParameter(&options.syncByRevision));

  opts->addOption("--replication.auto-repair-revision-trees",
                  "Whether to automatically repair revision trees of shards "
                  "after too many shard synchronization failures.",
                  new BooleanParameter(&options.autoRepairRevisionTrees),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer))
      .setIntroducedIn(31006);

  opts->addObsoleteOption(
      "--replication.active-failover-leader-grace-period",
      "The amount of time (in seconds) for which the current leader will "
      "continue to assume its leadership even if it lost connection to the "
      "agency (0 = unlimited)",
      true);
}

void ReplicationOptionsProvider::processOptionsImpl(
    std::shared_ptr<ProgramOptions> opts, ReplicationOptions& options) {
  if (options.connectTimeout < 1.0) {
    options.connectTimeout = 1.0;
  }
  if (opts->processingResult().touched("--replication.connect-timeout")) {
    options.forceConnectTimeout = true;
  }

  if (options.requestTimeout < 3.0) {
    options.requestTimeout = 3.0;
  }
  if (opts->processingResult().touched("--replication.request-timeout")) {
    options.forceRequestTimeout = true;
  }
}

void ReplicationOptionsProvider::validateOptionsImpl(
    std::shared_ptr<ProgramOptions> /*opts*/,
    ReplicationOptions const& /*options*/) {}

}  // namespace arangodb
