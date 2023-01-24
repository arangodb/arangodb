////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "TargetCollectionAgencyWriter.h"
#include "Agency/AgencyComm.h"
#include "Agency/AgencyPaths.h"
#include "Agency/TransactionBuilder.h"
#include "Basics/Guarded.h"
#include "Cluster/Utils/CurrentWatcher.h"
#include "Cluster/Utils/CurrentCollectionEntry.h"
#include "Cluster/Utils/PlanCollectionEntry.h"
#include "Cluster/Utils/PlanCollectionEntryReplication2.h"
#include "Inspection/VPack.h"
#include "Replication2/AgencyCollectionSpecification.h"
#include "Replication2/AgencyCollectionSpecificationInspectors.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/AgencySpecificationInspectors.h"
#include "VocBase/LogicalCollection.h"

#include "Logger/LogMacros.h"

using namespace arangodb;
namespace paths = arangodb::cluster::paths::aliases;

namespace {
// Copy paste from ClusterInfo
// TODO: make it dry
inline arangodb::AgencyOperation IncreaseVersion() {
  return arangodb::AgencyOperation{
      paths::plan()->version(),
      arangodb::AgencySimpleOperationType::INCREMENT_OP};
}

inline auto pathCollectionInPlan(std::string_view databaseName) {
  return cluster::paths::root()->arango()->plan()->collections()->database(
      std::string{databaseName});
}

inline auto pathReplicatedLogInTarget(std::string_view databaseName) {
  return paths::target()->replicatedLogs()->database(std::string{databaseName});
}

inline auto pathCollectionGroupInTarget(std::string_view databaseName) {
  return paths::target()->collectionGroups()->database(std::string{databaseName});
}

inline auto pathCollectionInCurrent(std::string_view databaseName) {
  return paths::current()->collections()->database(std::string{databaseName});
}

inline auto pathReplicatedLogInCurrent(std::string_view databaseName) {
  return paths::current()->replicatedLogs()->database(
      std::string{databaseName});
}

}  // namespace

TargetCollectionAgencyWriter::TargetCollectionAgencyWriter(
    std::vector<PlanCollectionEntryReplication2> collectionPlanEntries,
    std::unordered_map<std::string, std::shared_ptr<IShardDistributionFactory>>
        shardDistributionsUsed, replication2::CollectionGroupUpdates collectionGroups)
    : _collectionPlanEntries{std::move(collectionPlanEntries)},
      _shardDistributionsUsed{std::move(shardDistributionsUsed)},
      _collectionGroups{std::move(collectionGroups)} {}

std::shared_ptr<CurrentWatcher>
TargetCollectionAgencyWriter::prepareCurrentWatcher(
    std::string_view databaseName, bool waitForSyncReplication) const {
  auto report = std::make_shared<CurrentWatcher>();

  // One callback per collection
  report->reserve(_collectionPlanEntries.size());
  auto const baseCollectionPath = pathCollectionInCurrent(databaseName);
  auto const baseStatePath = pathReplicatedLogInCurrent(databaseName);
  for (auto const& entry : _collectionPlanEntries) {
    if (entry.requiresCurrentWatcher()) {
      // Add Shards Watcher
      auto expectedShards = entry.getShardMapping();
      {
        // Add the Watcher for Collection Shards
        auto cid = entry.getCID();
        auto const collectionPath = baseCollectionPath->collection(cid);
        auto callback = [cid, expectedShards, waitForSyncReplication,
                         report](VPackSlice result) -> bool {
          if (report->hasReported(cid)) {
            // This collection has already reported
            return true;
          }
          CurrentCollectionEntry state;
          auto status = velocypack::deserializeWithStatus(result, state);
          // We ignore parsing errors here, only react on positive case
          if (status.ok()) {
            if (state.haveAllShardsReported(expectedShards.shards.size())) {
              if (state.hasError()) {
                // At least on shard has reported an error.
                report->addReport(
                    cid, Result{TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION,
                                state.createErrorReport()});
                return true;
              } else if (!waitForSyncReplication ||
                         state.doExpectedServersMatch(expectedShards)) {
                // All servers reported back without error.
                // If waitForSyncReplication is requested full server lists
                // match Let us return done!
                report->addReport(cid, Result{TRI_ERROR_NO_ERROR});
                return true;
              }
            }
          }
          // TODO: Maybe we want to do trace-logging if current cannot be
          // parsed?

          return true;
        };
        // Note we use SkipComponents here, as callbacks to not have "arangod"
        // as prefix.
        report->addWatchPath(
            collectionPath->str(arangodb::cluster::paths::SkipComponents(1)),
            cid, callback);
      }

      for (auto const& [shardId, servers] : expectedShards.shards) {
        // NOTE: This is a bit too much work here, and could be narrowed down a
        // bit. But as we are waiting on Agency roundtrips here performance
        // should not matter too much.
        auto orderedSpec =
            entry.getReplicatedLogForTarget(shardId, servers, databaseName);
        auto const statePath =
            baseStatePath->log(orderedSpec.id)->supervision();
        TRI_ASSERT(orderedSpec.version.has_value());
        auto callback = [report,
                         id = basics::StringUtils::itoa(orderedSpec.id.id()),
                         version = orderedSpec.version.value()](
                            velocypack::Slice slice) -> bool {
          if (report->hasReported(id)) {
            // This replicatedLog has already reported
            return true;
          }
          if (slice.isNone()) {
            return false;
          }

          auto supervision = velocypack::deserialize<
              replication2::agency::LogCurrentSupervision>(slice);
          if (supervision.targetVersion.has_value() &&
              supervision.targetVersion >= version) {
            // Right now there cannot be any error on replicated states.
            // SO if they show up in correct version we just take them.
            report->addReport(id, TRI_ERROR_NO_ERROR);
          }
          return true;
        };
        report->addWatchPath(
            statePath->str(arangodb::cluster::paths::SkipComponents(1)),
            basics::StringUtils::itoa(orderedSpec.id.id()), callback);
      }
    }
  }

  return report;
}

ResultT<VPackBufferUInt8>
TargetCollectionAgencyWriter::prepareStartBuildingTransaction(
    std::string_view databaseName, uint64_t planVersion,
    std::vector<std::string> serversAvailable) const {
  // Distribute Shards onto servers
  std::unordered_set<ServerID> serversPlanned;
  for (auto& [_, dist] : _shardDistributionsUsed) {
    auto res = dist->planShardsOnServers(serversAvailable, serversPlanned);
    if (res.fail()) {
      return res;
    }
  }

  auto const baseCollectionPath = pathCollectionInPlan(databaseName);
  auto const baseReplicatedLogsPath = pathReplicatedLogInTarget(databaseName);
  auto const baseGroupPath = pathCollectionGroupInTarget(databaseName);

  VPackBufferUInt8 data;
  VPackBuilder builder(data);
  auto envelope = arangodb::agency::envelope::into_builder(builder);
  // Envelope is not use able after this point
  // We started a write transaction, and now need to add all operations
  auto writes = std::move(envelope).write();
  // Increase plan version
  writes = std::move(writes).inc(paths::plan()->version()->str());

  // Write All new Collection Groups
  for (auto const& g : _collectionGroups.newGroups) {
    writes = std::move(writes).emplace_object(
        baseGroupPath->group(std::to_string(g.id.id()))->str(),
        [&](VPackBuilder& builder) {
          velocypack::serialize(builder, g);
        });
  }

  // Inject entries into old CollectionGroups
  for (auto const& g : _collectionGroups.additionsToGroup) {
    writes = std::move(writes).emplace_object(
        baseGroupPath->group(std::to_string(g.id.id()))
            ->collections()
            ->collection(g.collectionId)
            ->str(),
        [](VPackBuilder& builder) {
          replication2::agency::CollectionGroup::Collection c;
          velocypack::serialize(builder, c);
        });
  }

  // Write all requested Collection entries
  for (auto const& entry : _collectionPlanEntries) {
    writes = std::move(writes).emplace_object(
        baseCollectionPath->collection(entry.getCID())->str(),
        [&](VPackBuilder& builder) {
          // Temporary. It should be replaced by velocypack::serialize(builder,
          // entry); This is not efficient we copy the builder twice
          auto b = entry.toVPackDeprecated();
          builder.add(b.slice());
        });

    // Create a replicated state for each shard.
    for (auto const& [shardId, serverIds] : entry.getShardMapping().shards) {
      auto spec =
          entry.getReplicatedLogForTarget(shardId, serverIds, databaseName);
      writes = std::move(writes).emplace_object(
          baseReplicatedLogsPath->log(spec.id)->str(),
          [&](VPackBuilder& builder) { velocypack::serialize(builder, spec); });
    }
  }

  // Done with adding writes. Now add all preconditions
  // writes is not usable after this point
  auto preconditions = std::move(writes).precs();
  preconditions = std::move(preconditions)
                      .isEqual(paths::plan()->version()->str(), planVersion);
  // Server should not be planned to be cleaned
  preconditions =
      std::move(preconditions)
          .isIntersectionEmpty(paths::target()->toBeCleanedServers()->str(),
                               serversPlanned);
  // Servers should not be in cleaned state
  preconditions =
      std::move(preconditions)
          .isIntersectionEmpty(paths::target()->cleanedServers()->str(),
                               serversPlanned);

  // Preconditions for Collection Groups
  for (auto const& g : _collectionGroups.newGroups) {
    // No one has stolen our new group id
    preconditions =
        std::move(preconditions)
            .isEmpty(baseGroupPath->group(std::to_string(g.id.id()))->str());
  }

  for (auto const& g : _collectionGroups.additionsToGroup) {
    // No one has meanwhile removed the group we want to participate in
    preconditions =
        std::move(preconditions)
            .isNotEmpty(baseGroupPath->group(std::to_string(g.id.id()))->str());
  }

  // Created preconditions that no one has stolen our id
  for (auto const& entry : _collectionPlanEntries) {
    auto const collectionPath = baseCollectionPath->collection(entry.getCID());
    preconditions =
        std::move(preconditions)
            .isEmpty(baseCollectionPath->collection(entry.getCID())->str());
  }

  // We are complete, add our ID and close the transaction;
  std::move(preconditions).end().done();

  return data;
}

[[nodiscard]] AgencyWriteTransaction
TargetCollectionAgencyWriter::prepareUndoTransaction(
    std::string_view databaseName) const {
  std::vector<AgencyOperation> opers{};
  std::vector<AgencyPrecondition> precs{};

  // One per Collection, and the PlanVersion
  opers.reserve(_collectionPlanEntries.size() + 1);
  // One per Collection
  precs.reserve(_collectionPlanEntries.size());

  opers.push_back(IncreaseVersion());

  auto const baseCollectionPath = pathCollectionInPlan(databaseName);
  auto const baseReplicatedLogsPath = pathReplicatedLogInTarget(databaseName);
  for (auto& entry : _collectionPlanEntries) {
    auto const collectionPath = baseCollectionPath->collection(entry.getCID());

    // Add a precondition that we are still building
    precs.emplace_back(AgencyPrecondition{
        collectionPath->isBuilding(), AgencyPrecondition::Type::EMPTY, false});

    // Remove the entry
    opers.emplace_back(
        AgencyOperation{collectionPath, AgencySimpleOperationType::DELETE_OP});

    // Delete the replicated state for each shard.
    for (auto const& [shardId, serverIds] : entry.getShardMapping().shards) {
      auto logId = LogicalCollection::shardIdToStateId(shardId);
      // Remove the ReplicatedState
      opers.emplace_back(AgencyOperation{baseReplicatedLogsPath->log(logId),
                                         AgencySimpleOperationType::DELETE_OP});
    }
  }

  // TODO: Check ownership
  return AgencyWriteTransaction{opers, precs};
}

[[nodiscard]] AgencyWriteTransaction
TargetCollectionAgencyWriter::prepareCompletedTransaction(
    std::string_view databaseName) {
  std::vector<AgencyOperation> opers{};
  std::vector<AgencyPrecondition> precs{};

  // One per Collection, and the PlanVersion
  opers.reserve(_collectionPlanEntries.size() + 1);
  // One per Collection
  precs.reserve(_collectionPlanEntries.size());

  opers.push_back(IncreaseVersion());

  auto const baseCollectionPath = pathCollectionInPlan(databaseName);
  for (auto& entry : _collectionPlanEntries) {
    auto const collectionPath = baseCollectionPath->collection(entry.getCID());
    {
      // TODO: This is temporary
      auto builder = std::make_shared<VPackBuilder>(entry.toVPackDeprecated());
      // velocypack::serialize(*builder, _entry);
      // Add a precondition that no other code has modified our collection.
      // Especially no failover has happend
      precs.emplace_back(AgencyPrecondition{
          collectionPath, AgencyPrecondition::Type::VALUE, std::move(builder)});
    }
    // Get out of is Building mode
    entry.removeBuildingFlags();
    {
      // TODO: This is temporary
      auto builder = std::make_shared<VPackBuilder>(entry.toVPackDeprecated());
      // velocypack::serialize(*builder, _entry);

      // Create the operation to place our collection here
      opers.emplace_back(AgencyOperation{
          collectionPath, AgencyValueOperationType::SET, std::move(builder)});
    }
  }

  // TODO: Check ownership
  return AgencyWriteTransaction{opers, precs};
}

std::vector<std::string> TargetCollectionAgencyWriter::collectionNames() const {
  std::vector<std::string> names;
  names.reserve(_collectionPlanEntries.size());
  for (auto& entry : _collectionPlanEntries) {
    names.emplace_back(entry.getName());
  }
  return names;
}

/*
TargetCollectionAgencyWriter::TargetCollectionAgencyWriter(
    CreateCollectionBody col, ShardDistribution shardDistribution)
    : _entry(std::move(col), std::move(shardDistribution)) {}

[[nodiscard]] AgencyOperation TargetCollectionAgencyWriter::prepareOperation(
    std::string const& databaseName) const {
  auto const collectionPath = cluster::paths::root()
                                  ->arango()
                                  ->plan()
                                  ->collections()
                                  ->database(databaseName)
                                  ->collection(_entry.getCID());
  // TODO: This is temporary
  auto builder = std::make_shared<VPackBuilder>(_entry.toVPackDeprecated());
  // velocypack::serialize(*builder, _entry);
  return AgencyOperation{collectionPath, AgencyValueOperationType::SET,
                         std::move(builder)};
}
*/
