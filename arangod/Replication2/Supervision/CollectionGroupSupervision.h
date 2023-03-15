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
#include "Replication2/AgencyCollectionSpecification.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/ParticipantsHealth.h"
#include <variant>
#include <map>

namespace arangodb::agency {
struct envelope;
}

namespace arangodb::replication2::document::supervision {

inline namespace actions {
struct UpdateReplicatedLogConfig {
  LogId logId;
  agency::LogTargetConfig config;
};
struct UpdateConvergedVersion {
  std::optional<std::uint64_t> version;
};
struct DropCollectionPlan {
  CollectionID cid;
};
struct DropCollectionGroup {
  agency::CollectionGroupId gid;
  std::vector<agency::CollectionGroupPlanSpecification::ShardSheaf> logs;
};
struct AddCollectionToPlan {
  CollectionID cid;
  agency::CollectionPlanSpecification spec;
};
struct AddCollectionGroupToPlan {
  agency::CollectionGroupPlanSpecification spec;
  std::unordered_map<LogId, agency::LogTarget> sheaves;
  std::unordered_map<CollectionID, agency::CollectionPlanSpecification>
      collections;
};
struct UpdateCollectionShardMap {
  CollectionID cid;
  PlanShardToServerMapping mapping;
};
struct AddParticipantToLog {
  LogId logId;
  ParticipantId participant;
};
struct RemoveParticipantFromLog {
  LogId logId;
  ParticipantId participant;
};
struct NoActionRequired {};
struct NoActionPossible {
  std::string reason;
};
}  // namespace actions

using Action =
    std::variant<NoActionRequired, NoActionPossible, UpdateReplicatedLogConfig,
                 UpdateConvergedVersion, DropCollectionPlan,
                 DropCollectionGroup, AddCollectionToPlan,
                 AddCollectionGroupToPlan, UpdateCollectionShardMap,
                 AddParticipantToLog, RemoveParticipantFromLog>;

struct CollectionGroup {
  agency::CollectionGroupTargetSpecification target;
  std::optional<agency::CollectionGroupPlanSpecification> plan;
  std::optional<agency::CollectionGroupCurrentSpecification> current;

  std::unordered_map<LogId, agency::Log> logs;
  // TODO Make one map with optional plan and current?
  std::unordered_map<arangodb::CollectionID,
                     agency::CollectionTargetSpecification>
      targetCollections;
  std::unordered_map<arangodb::CollectionID,
                     agency::CollectionPlanSpecification>
      planCollections;
  std::unordered_map<arangodb::CollectionID,
                     agency::CollectionCurrentSpecification>
      currentCollections;
};

struct UniqueIdProvider {
  virtual ~UniqueIdProvider() = default;
  [[nodiscard]] virtual auto next() noexcept -> std::uint64_t = 0;
};

auto checkCollectionGroup(DatabaseID const& database,
                          CollectionGroup const& group,
                          UniqueIdProvider& uniqid,
                          replicated_log::ParticipantsHealth const& health)
    -> Action;

auto executeCheckCollectionGroup(
    DatabaseID const& database, std::string const& logIdString,
    CollectionGroup const& group,
    replicated_log::ParticipantsHealth const& health, UniqueIdProvider& uniqid,
    arangodb::agency::envelope envelope) noexcept -> arangodb::agency::envelope;

}  // namespace arangodb::replication2::document::supervision
