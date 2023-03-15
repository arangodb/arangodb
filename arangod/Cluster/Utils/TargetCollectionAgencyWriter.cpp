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
#include "Basics/VelocyPackHelper.h"
#include "Cluster/AgencyCache.h"
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

inline auto pathCollectionNamesInTarget(std::string_view databaseName) {
  return paths::target()->collectionNames()->database(
      std::string{databaseName});
}

inline auto pathCollectionInTarget(std::string_view databaseName) {
  return paths::target()->collections()->database(std::string{databaseName});
}

inline auto pathCollectionGroupInTarget(std::string_view databaseName) {
  return paths::target()->collectionGroups()->database(
      std::string{databaseName});
}

inline auto pathDatabaseInTarget(std::string_view databaseName) {
  // TODO: Make this target, as soon as databases are moved
  return paths::plan()->databases()->database(std::string{databaseName});
}

inline auto pathCollectionGroupInCurrent(std::string_view databaseName) {
  return paths::current()->collectionGroups()->database(
      std::string{databaseName});
}

}  // namespace

TargetCollectionAgencyWriter::TargetCollectionAgencyWriter(
    std::vector<PlanCollectionEntryReplication2> collectionPlanEntries,
    std::unordered_map<std::string, std::shared_ptr<IShardDistributionFactory>>
        shardDistributionsUsed,
    replication2::CollectionGroupUpdates collectionGroups)
    : _collectionPlanEntries{std::move(collectionPlanEntries)},
      _shardDistributionsUsed{std::move(shardDistributionsUsed)},
      _collectionGroups{std::move(collectionGroups)} {}

std::shared_ptr<CurrentWatcher>
TargetCollectionAgencyWriter::prepareCurrentWatcher(
    std::string_view databaseName, bool waitForSyncReplication,
    AgencyCache& agencyCache) const {
  auto report = std::make_shared<CurrentWatcher>();

  auto const baseStatePath = pathCollectionGroupInCurrent(databaseName);
  auto modGroups = _collectionGroups.getAllModifiedGroups();

  auto registerWaitForSupervisionVersion = [&](std::string gid,
                                               uint64_t version) {
    auto const groupPath = baseStatePath->group(gid)->supervision();
    auto callback = [report, gid, version](velocypack::Slice slice) -> bool {
      if (report->hasReported(gid)) {
        // This replicatedLog has already reported
        return true;
      }
      if (slice.isNone()) {
        return false;
      }

      auto supervision = velocypack::deserialize<
          replication2::agency::CollectionGroupCurrentSpecification::
              Supervision>(slice);
      if (supervision.version.has_value() &&
          supervision.version.value() >= version) {
        // Right now there cannot be any error on replicated states.
        // SO if they show up in correct version we just take them.
        report->addReport(gid, TRI_ERROR_NO_ERROR);
      }
      return true;
    };
    report->addWatchPath(
        groupPath->str(arangodb::cluster::paths::SkipComponents(1)),
        std::move(gid), callback);
  };
  // One callback per New and one per Existing Group
  report->reserve(_collectionGroups.newGroups.size() + modGroups.size());
  for (auto const& group : _collectionGroups.newGroups) {
    registerWaitForSupervisionVersion(std::to_string(group.id.id()),
                                      group.version.value());
  }

  /*
   * So far our API do not have a code path updating two CollectionGroups in one
   * call. However doing this with the following code-path is possible it is
   * just not optimal yet. If we want to support this in an optimized way the
   * below logic should be changed to: 1) One Preflight request reading all
   * CollectionGroupVersions in one go. 2) Add Watchers on increments of above
   * Groups.
   */
  TRI_ASSERT(modGroups.size() < 2)
      << "Add collections to different groups in one request. Triggering "
         "non-optimal codepath";
  for (auto const& group : modGroups) {
    auto gid = std::to_string(group.id());
    auto const targetPath = pathCollectionGroupInTarget(databaseName);
    auto const targetGroupPath = targetPath->group(gid)->version();
    // First read set version (sorry we have to do this as the increment call to
    // version does not give us the new value back)
    VPackBuilder response;
    agencyCache.get(response, targetGroupPath);
    uint64_t versionToWaitFor =
        basics::VelocyPackHelper::getNumericValue(response.slice(), 0ull);
    // Then register a waits for Callback where the above version is increased
    // by one. This is best effort and not race-free, if someone updates the
    // same group in parallel one of the parallel requests would not wait
    // properly. This is however a very rare production case, and only a minor
    // inconvenience to users, if it really happens.
    registerWaitForSupervisionVersion(std::move(gid), versionToWaitFor + 1);
  }
  return report;
}

ResultT<VPackBufferUInt8>
TargetCollectionAgencyWriter::prepareCreateTransaction(
    std::string_view databaseName) const {
  auto const baseCollectionPath = pathCollectionInTarget(databaseName);
  auto const baseGroupPath = pathCollectionGroupInTarget(databaseName);
  auto const collectionNamePath = pathCollectionNamesInTarget(databaseName);

  VPackBufferUInt8 data;
  VPackBuilder builder(data);
  auto envelope = arangodb::agency::envelope::into_builder(builder);
  // Envelope is not use able after this point
  // We started a write transaction, and now need to add all operations
  auto writes = std::move(envelope).write();

  // Write All new Collection Groups
  for (auto const& g : _collectionGroups.newGroups) {
    writes = std::move(writes).emplace_object(
        baseGroupPath->group(std::to_string(g.id.id()))->str(),
        [&](VPackBuilder& builder) { velocypack::serialize(builder, g); });
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

  auto modGroups = _collectionGroups.getAllModifiedGroups();
  // Increase group versions
  for (auto const& g : modGroups) {
    writes = std::move(writes).inc(
        baseGroupPath->group(std::to_string(g.id()))->version()->str());
  }

  // Write all requested Collection entries
  for (std::size_t it(0); it < _collectionPlanEntries.size(); ++it) {
    auto const& entry = _collectionPlanEntries[it];

    writes = std::move(writes).emplace_object(
        baseCollectionPath->collection(entry.getCID())->str(),
        [&](VPackBuilder& builder) { velocypack::serialize(builder, entry); });

    // Insert an empty object, we basically want to occupy the key here for
    // preconditions
    writes = std::move(writes).emplace_object(
        collectionNamePath->collection(entry.getName())->str(),
        [](VPackBuilder& builder) {
          builder.openObject();
          builder.close();
        });
  }

  // Done with adding writes. Now add all preconditions
  // writes is not usable after this point
  auto preconditions = std::move(writes).precs();

  // Make sure we have not lost our database yet
  preconditions = std::move(preconditions)
                      .isNotEmpty(pathDatabaseInTarget(databaseName)->str());

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

  // Created preconditions that no one has stolen our collections id or name
  for (auto const& entry : _collectionPlanEntries) {
    preconditions =
        std::move(preconditions)
            .isEmpty(baseCollectionPath->collection(entry.getCID())->str());
    preconditions =
        std::move(preconditions)
            .isEmpty(collectionNamePath->collection(entry.getName())->str());
  }

  // We are complete, add our ID and close the transaction;
  std::move(preconditions).end().done();

  return data;
}

std::vector<std::string> TargetCollectionAgencyWriter::collectionNames() const {
  std::vector<std::string> names;
  names.reserve(_collectionPlanEntries.size());
  for (auto& entry : _collectionPlanEntries) {
    names.emplace_back(entry.getName());
  }
  return names;
}
