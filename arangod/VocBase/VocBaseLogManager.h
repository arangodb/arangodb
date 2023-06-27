////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "Basics/Guarded.h"
#include "Basics/ResultT.h"
#include "Cluster/CallbackGuard.h"
#include "Cluster/ClusterTypes.h"
#include "Replication2/LoggerContext.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "RestServer/arangod.h"

struct TRI_vocbase_t;

namespace arangodb {
class LogicalCollection;
namespace replication2 {
class LogId;
struct LogIndex;
struct LogTerm;
struct LogPayload;
namespace agency {
struct LogPlanSpecification;
struct LogPlanTermSpecification;
struct ParticipantsConfig;
}  // namespace agency
namespace maintenance {
struct LogStatus;
}
namespace replicated_log {
struct ILogLeader;
struct ILogFollower;
struct ILogParticipant;
struct LogStatus;
struct QuickLogStatus;
struct ReplicatedLog;
}  // namespace replicated_log
namespace replicated_state {
struct ReplicatedStateBase;
struct StateStatus;
struct PersistedStateInfo;
struct IStorageEngineMethods;
}  // namespace replicated_state
}  // namespace replication2

struct VocBaseLogManager {
  VocBaseLogManager(TRI_vocbase_t& vocbase, DatabaseID database);

  [[nodiscard]] auto getReplicatedStateById(replication2::LogId id) -> ResultT<
      std::shared_ptr<replication2::replicated_state::ReplicatedStateBase>>;

  void registerReplicatedState(
      replication2::LogId id,
      std::unique_ptr<arangodb::replication2::storage::IStorageEngineMethods>
          methods);

  void resignAll() noexcept;

  auto updateReplicatedState(
      arangodb::replication2::LogId id,
      arangodb::replication2::agency::LogPlanTermSpecification const& term,
      arangodb::replication2::agency::ParticipantsConfig const& config)
      -> Result;

  [[nodiscard]] auto dropReplicatedState(arangodb::replication2::LogId id)
      -> arangodb::Result;

  [[nodiscard]] auto getReplicatedLogsStatusMap() const
      -> std::unordered_map<arangodb::replication2::LogId,
                            arangodb::replication2::maintenance::LogStatus>;

  [[nodiscard]] auto getReplicatedStatesStatus() const
      -> std::unordered_map<arangodb::replication2::LogId,
                            arangodb::replication2::replicated_log::LogStatus>;

  auto createReplicatedState(replication2::LogId id, std::string_view type,
                             VPackSlice parameter)
      -> ResultT<
          std::shared_ptr<replication2::replicated_state::ReplicatedStateBase>>;

  ArangodServer& _server;
  TRI_vocbase_t& _vocbase;
  LoggerContext const _logContext;

  // During startup this map contains a mapping from logs to shards. Not
  // valid after startup.
  std::unordered_multimap<replication2::LogId,
                          std::shared_ptr<LogicalCollection>>
      _initCollections;

  struct GuardedData {
    struct StateAndLog {
      cluster::CallbackGuard rebootTrackerGuard;
      std::shared_ptr<arangodb::replication2::replicated_log::ReplicatedLog>
          log;
      std::shared_ptr<
          arangodb::replication2::replicated_state::ReplicatedStateBase>
          state;
      arangodb::replication2::replicated_log::ReplicatedLogConnection
          connection;
    };
    std::map<arangodb::replication2::LogId, StateAndLog> statesAndLogs;
    bool resignAllWasCalled{false};

    auto buildReplicatedStateWithMethods(
        replication2::LogId id, std::string_view type, VPackSlice parameters,
        replication2::replicated_state::ReplicatedStateAppFeature& feature,
        LoggerContext const& logContext, ArangodServer& server,
        TRI_vocbase_t& vocbase,
        std::unique_ptr<arangodb::replication2::storage::IStorageEngineMethods>
            storage)
        -> ResultT<std::shared_ptr<
            replication2::replicated_state::ReplicatedStateBase>>;

    auto buildReplicatedState(
        replication2::LogId id, std::string_view type, VPackSlice parameters,
        replication2::replicated_state::ReplicatedStateAppFeature& feature,
        LoggerContext const& logContext, ArangodServer& server,
        TRI_vocbase_t& vocbase)
        -> ResultT<std::shared_ptr<
            replication2::replicated_state::ReplicatedStateBase>>;
  };
  Guarded<GuardedData> _guardedData;
};

}  // namespace arangodb
