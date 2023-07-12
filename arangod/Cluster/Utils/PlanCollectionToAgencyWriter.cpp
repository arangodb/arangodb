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

#include "PlanCollectionToAgencyWriter.h"
#include "Agency/AgencyComm.h"
#include "Agency/AgencyPaths.h"
#include "Basics/Guarded.h"
#include "Cluster/Utils/CurrentWatcher.h"
#include "Cluster/Utils/CurrentCollectionEntry.h"
#include "Cluster/Utils/PlanCollectionEntry.h"
#include "Cluster/Utils/PlanShardToServerMappping.h"
#include "Inspection/VPack.h"

using namespace arangodb;

namespace {
// Copy paste from ClusterInfo
// TODO: make it dry
inline arangodb::AgencyOperation IncreaseVersion() {
  return arangodb::AgencyOperation{
      "Plan/Version", arangodb::AgencySimpleOperationType::INCREMENT_OP};
}

inline auto pathCollectioInPlan(std::string_view databaseName) {
  return cluster::paths::root()->arango()->plan()->collections()->database(
      std::string{databaseName});
}

inline auto pathCollectioInCurrent(std::string_view databaseName) {
  // Note: I cannot use the Path builder here, as the Callbacks do not start at
  // ROOT
  return "Current/Collections/" + std::string{databaseName} + "/";
  ;
}

}  // namespace

PlanCollectionToAgencyWriter::PlanCollectionToAgencyWriter(
    std::vector<arangodb::PlanCollectionEntry> collectionPlanEntries,
    std::unordered_map<std::string, std::shared_ptr<IShardDistributionFactory>>
        shardDistributionsUsed)
    : _collectionPlanEntries{std::move(collectionPlanEntries)},
      _shardDistributionsUsed{std::move(shardDistributionsUsed)} {}

std::shared_ptr<CurrentWatcher>
PlanCollectionToAgencyWriter::prepareCurrentWatcher(
    std::string_view databaseName, bool waitForSyncReplication) const {
  auto report = std::make_shared<CurrentWatcher>();

  // One callback per collection
  report->reserve(_collectionPlanEntries.size());
  auto const baseCollectionPath = pathCollectioInCurrent(databaseName);
  for (auto const& entry : _collectionPlanEntries) {
    if (entry.requiresCurrentWatcher()) {
      auto const collectionPath = baseCollectionPath + entry.getCID();
      auto expectedShards = entry.getShardMapping();
      auto cid = entry.getCID();

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
              // If waitForSyncReplication is requested full server lists match
              // Let us return done!
              report->addReport(cid, Result{TRI_ERROR_NO_ERROR});
              return true;
            }
          }
        }
        // TODO: Maybe we want to do trace-logging if current cannot be parsed?

        return true;
      };
      report->addWatchPath(collectionPath, cid, callback);
    }
  }

  return report;
}

ResultT<AgencyWriteTransaction>
PlanCollectionToAgencyWriter::prepareStartBuildingTransaction(
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

  std::vector<AgencyOperation> opers{};
  std::vector<AgencyPrecondition> precs{};
  // One per collection, and the update on plan version
  opers.reserve(_collectionPlanEntries.size() + 1);

  // One per collection, the plan version, and the checks that servers
  // are not to be cleaned.
  precs.reserve(_collectionPlanEntries.size() + 3);

  // General Preconditions
  // TODO: beautify:
  // create a builder with just the version number for comparison
  auto versionBuilder = std::make_shared<VPackBuilder>();
  versionBuilder->add(VPackValue(planVersion));

  auto serversBuilder = std::make_shared<VPackBuilder>();
  {
    VPackArrayBuilder a(serversBuilder.get());
    for (auto const& i : serversPlanned) {
      serversBuilder->add(VPackValue(i));
    }
  }
  // * plan version unchanged
  precs.emplace_back(AgencyPrecondition("Plan/Version",
                                        AgencyPrecondition::Type::VALUE,
                                        std::move(versionBuilder)));
  // * not in to be cleaned server list
  precs.emplace_back(AgencyPrecondition(
      "Target/ToBeCleanedServers", AgencyPrecondition::Type::INTERSECTION_EMPTY,
      serversBuilder));
  // * not in cleaned server list
  precs.emplace_back(AgencyPrecondition(
      "Target/CleanedServers", AgencyPrecondition::Type::INTERSECTION_EMPTY,
      std::move(serversBuilder)));

  // TODO: End of to beatuify

  opers.emplace_back(IncreaseVersion());

  auto const baseCollectionPath = pathCollectioInPlan(databaseName);
  for (auto const& entry : _collectionPlanEntries) {
    auto const collectionPath = baseCollectionPath->collection(entry.getCID());
    // TODO: This is temporary
    auto builder = std::make_shared<VPackBuilder>(entry.toVPackDeprecated());
    // velocypack::serialize(*builder, _entry);

    // Create the operation to place our collection here
    opers.emplace_back(AgencyOperation{
        collectionPath, AgencyValueOperationType::SET, std::move(builder)});

    // Add a precondition that no other code has occupied the spot.
    precs.emplace_back(AgencyPrecondition{
        collectionPath, AgencyPrecondition::Type::EMPTY, true});
  }

  // TODO: Check ownership
  return AgencyWriteTransaction{opers, precs};
}

[[nodiscard]] AgencyWriteTransaction
PlanCollectionToAgencyWriter::prepareUndoTransaction(
    std::string_view databaseName) const {
  std::vector<AgencyOperation> opers{};
  std::vector<AgencyPrecondition> precs{};

  // One per Collection, and the PlanVersion
  opers.reserve(_collectionPlanEntries.size() + 1);
  // One per Collection
  precs.reserve(_collectionPlanEntries.size());

  opers.push_back(IncreaseVersion());

  auto const baseCollectionPath = pathCollectioInPlan(databaseName);
  for (auto& entry : _collectionPlanEntries) {
    auto const collectionPath = baseCollectionPath->collection(entry.getCID());

    // Add a precondition that we are still building
    precs.emplace_back(AgencyPrecondition{
        collectionPath->isBuilding(), AgencyPrecondition::Type::EMPTY, false});

    // Remove the entry
    opers.emplace_back(
        AgencyOperation{collectionPath, AgencySimpleOperationType::DELETE_OP});
  }

  // TODO: Check ownership
  return AgencyWriteTransaction{opers, precs};
}

[[nodiscard]] AgencyWriteTransaction
PlanCollectionToAgencyWriter::prepareCompletedTransaction(
    std::string_view databaseName) {
  std::vector<AgencyOperation> opers{};
  std::vector<AgencyPrecondition> precs{};

  // One per Collection, and the PlanVersion
  opers.reserve(_collectionPlanEntries.size() + 1);
  // One per Collection
  precs.reserve(_collectionPlanEntries.size());

  opers.push_back(IncreaseVersion());

  auto const baseCollectionPath = pathCollectioInPlan(databaseName);
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

std::vector<std::string> PlanCollectionToAgencyWriter::collectionNames() const {
  std::vector<std::string> names;
  names.reserve(_collectionPlanEntries.size());
  for (auto& entry : _collectionPlanEntries) {
    names.emplace_back(entry.getName());
  }
  return names;
}

/*
PlanCollectionToAgencyWriter::PlanCollectionToAgencyWriter(
    CreateCollectionBody col, ShardDistribution shardDistribution)
    : _entry(std::move(col), std::move(shardDistribution)) {}

[[nodiscard]] AgencyOperation PlanCollectionToAgencyWriter::prepareOperation(
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
