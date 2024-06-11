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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "ClusterCollectionMethods.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Agency/AsyncAgencyComm.h"
#include "Agency/AgencyPaths.h"
#include "Agency/TransactionBuilder.h"
#include "Cluster/AgencyCache.h"
#include "Basics/ScopeGuard.h"
#include "Cluster/AgencyCallback.h"
#include "Cluster/AgencyCallbackRegistry.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/Utils/CurrentWatcher.h"
#include "Cluster/Utils/DistributeShardsLike.h"
#include "Cluster/Utils/EvenDistribution.h"
#include "Cluster/Utils/IShardDistributionFactory.h"
#include "Cluster/Utils/PlanCollectionEntry.h"
#include "Cluster/Utils/PlanCollectionEntryReplication2.h"
#include "Cluster/Utils/PlanCollectionToAgencyWriter.h"
#include "Cluster/Utils/TargetCollectionAgencyWriter.h"
#include "Cluster/Utils/SatelliteDistribution.h"
#include "Logger/LogMacros.h"
#include "Replication2/AgencyCollectionSpecification.h"
#include "Rest/GeneralResponse.h"
#include "Sharding/ShardingInfo.h"
#include "VocBase/LogicalCollection.h"
#include "StorageEngine/PhysicalCollection.h"
#include "VocBase/Properties/CreateCollectionBody.h"
#include "VocBase/vocbase.h"

#include <velocypack/Collection.h>

using namespace arangodb;
namespace paths = arangodb::cluster::paths::aliases;

namespace {

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

auto reactToPreconditions(AsyncAgencyCommResult&& agencyRes)
    -> ResultT<uint64_t> {
  // We ordered the creation of collection, if this was not
  // successful we may try again, if it was, we continue with next
  // step.
  if (auto res = agencyRes.asResult(); res.fail()) {
    return res;
  }

  // extract raft index
  auto slice = agencyRes.slice().get("results");
  TRI_ASSERT(slice.isArray());
  TRI_ASSERT(!slice.isEmptyArray());
  return slice.at(slice.length() - 1).getNumericValue<uint64_t>();
}

auto reactToPreconditionsCreate(AsyncAgencyCommResult&& agencyRes)
    -> ResultT<uint64_t> {
  // We ordered the creation of collection, if this was not
  // successful we may try again, if it was, we continue with next
  // step.
  if (auto res = agencyRes.asResult(); res.fail()) {
    if (res.is(TRI_ERROR_HTTP_PRECONDITION_FAILED)) {
      // Unfortunately, we cannot know which precondition failed.
      // We have two possible options here, either our name is used, or someone
      // in parallel dropped the leading collection / collectionGroup.
      // As the later is highly unlikely we will always report the first here.
      return {TRI_ERROR_ARANGO_DUPLICATE_NAME};
    }
    return res;
  }

  // extract raft index
  auto slice = agencyRes.slice().get("results");
  TRI_ASSERT(slice.isArray());
  TRI_ASSERT(!slice.isEmptyArray());
  return slice.at(slice.length() - 1).getNumericValue<uint64_t>();
}

auto waitForOperationRoundtrip(ClusterInfo& ci,
                               ResultT<uint64_t>&& agencyRaftIndex) {
  // Got the Plan version while building.
  // Let us wait for it
  if (agencyRaftIndex.fail()) {
    return agencyRaftIndex.result();
  }
  return ci.waitForPlan(agencyRaftIndex.get()).waitAndGet();
}

auto waitForCurrentToCatchUp(
    ArangodServer& server, std::shared_ptr<CurrentWatcher>& callbackInfos,
    std::vector<std::pair<std::shared_ptr<AgencyCallback>, std::string>>&
        callbackList,
    double pollInterval) {
  // We waited on the buildingPlan to be loaded in the local cache
  // Now let us watch for CURRENT to check if all required changes
  // have been applied
  TRI_IF_FAILURE("ClusterInfo::createCollectionsCoordinator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  // NOTE: LOGID was 98bca before, duplicate from below
  LOG_TOPIC("98bc9", DEBUG, Logger::CLUSTER)
      << "createCollectionCoordinator, Plan changed, waiting for "
         "success...";

  // Now "busy-loop"
  while (!server.isStopping()) {
    auto maybeFinalResult = callbackInfos->getResultIfAllReported();
    if (maybeFinalResult.has_value()) {
      // We have a final result. we are complete
      return maybeFinalResult.value();
    }

    // We do not have a final result. Let's wait for more input
    // Wait for the next incomplete callback
    for (auto& [cb, cid] : callbackList) {
      if (!callbackInfos->hasReported(cid)) {
        // We do not have result for this collection, wait for it.
        bool gotTimeout;
        {
          // This one has not responded, wait for it.
          std::unique_lock guard(cb->_cv.mutex);
          gotTimeout = cb->executeByCallbackOrTimeout(pollInterval);
        }
        if (gotTimeout) {
          // We got woken up by waittime, not by  callback.
          // Let us check if we skipped other callbacks as well
          for (auto& [cb2, cid2] : callbackList) {
            if (callbackInfos->hasReported(cid2)) {
              // Only re check those where we have not yet found a
              // result.
              cb2->refetchAndUpdate(true, false);
            }
          }
        }
        // Break the callback loop,
        // continue on the check if we completed loop
        break;
      }
    }
  }

  // If we get here we are not allowed to retry.
  // The loop above does not contain a break
  TRI_ASSERT(server.isStopping());
  return Result{TRI_ERROR_SHUTTING_DOWN};
}

Result impl(ClusterInfo& ci, ArangodServer& server,
            std::string_view databaseName, PlanCollectionToAgencyWriter& writer,
            bool waitForSyncReplication) {
  AgencyComm ac(server);
  double pollInterval = ci.getPollInterval();
  AgencyCallbackRegistry& callbackRegistry = ci.agencyCallbackRegistry();

  // TODO Timeout?
  std::vector<std::string> collectionNames = writer.collectionNames();
  while (true) {
    // TODO: Is this necessary?
    ci.loadCurrentDBServers();
    auto planVersion =
        ci.checkDataSourceNamesAvailable(databaseName, collectionNames);
    if (planVersion.fail()) {
      return planVersion.result();
    }
    std::vector<ServerID> availableServers = ci.getCurrentDBServers();

    auto buildingTransaction = writer.prepareStartBuildingTransaction(
        databaseName, planVersion.get(), availableServers);
    if (buildingTransaction.fail()) {
      return buildingTransaction.result();
    }
    auto callbackInfos =
        writer.prepareCurrentWatcher(databaseName, waitForSyncReplication);

    std::vector<std::pair<std::shared_ptr<AgencyCallback>, std::string>>
        callbackList;
    auto unregisterCallbacksGuard =
        scopeGuard([&callbackList, &callbackRegistry]() noexcept {
          try {
            for (auto& [cb, _] : callbackList) {
              callbackRegistry.unregisterCallback(cb);
            }
          } catch (std::exception const& ex) {
            LOG_TOPIC("cc912", ERR, Logger::CLUSTER)
                << "Failed to unregister agency callback: " << ex.what();
          } catch (...) {
            // Should never be thrown, we only throw exceptions
          }
        });

    // First register all callbacks
    for (auto const& [path, identifier, cb] :
         callbackInfos->getCallbackInfos()) {
      auto agencyCallback =
          std::make_shared<AgencyCallback>(server, path, cb, true, false);
      Result r = callbackRegistry.registerCallback(agencyCallback);
      if (r.fail()) {
        return r;
      }
      callbackList.emplace_back(
          std::make_pair(std::move(agencyCallback), identifier));
    }
    callbackInfos->clearCallbacks();

    // Then send the transaction
    auto res = ac.sendTransactionWithFailover(buildingTransaction.get());
    if (res.successful()) {
      // Collections ordered
      // Prepare do undo if something fails now
      auto undoCreationGuard =
          scopeGuard([&writer, &databaseName, &ci, &server, &ac]() noexcept {
            try {
              auto undoTrx = writer.prepareUndoTransaction(databaseName);

              // Retry loop to remove the collection
              using namespace std::chrono;
              using namespace std::chrono_literals;
              auto const begin = steady_clock::now();
              // After a shutdown, the supervision will clean the collections
              // either due to the coordinator going into FAIL, or due to it
              // changing its rebootId. Otherwise we must under no
              // circumstance give up here, because noone else will clean this
              // up.
              while (!server.isStopping()) {
                auto res = ac.sendTransactionWithFailover(undoTrx);
                // If the collections were removed (res.ok()), we may abort.
                // If we run into precondition failed, the collections were
                // successfully created, so we're fine too.
                if (res.successful()) {
                  if (VPackSlice resultsSlice = res.slice().get("results");
                      resultsSlice.length() > 0) {
                    // Wait for updated plan to be loaded
                    [[maybe_unused]] Result r =
                        ci.waitForPlan(resultsSlice[0].getNumber<uint64_t>())
                            .waitAndGet();
                  }
                  return;
                } else if (res.httpCode() ==
                           rest::ResponseCode::PRECONDITION_FAILED) {
                  return;
                }

                // exponential backoff, just to be safe,
                auto const durationSinceStart = steady_clock::now() - begin;
                auto constexpr maxWaitTime = 2min;
                auto const waitTime =
                    std::min<std::common_type_t<decltype(durationSinceStart),
                                                decltype(maxWaitTime)>>(
                        durationSinceStart, maxWaitTime);
                std::this_thread::sleep_for(waitTime);
              }

            } catch (std::exception const& ex) {
              LOG_TOPIC("57486", ERR, Logger::CLUSTER)
                  << "Failed to delete collection during rollback: "
                  << ex.what();
            } catch (...) {
              // Just we do not crash, no one knowingly throws non exceptions.
            }
          });

      // Let us wait until we have locally seen the plan
      // TODO: Why? Can we just skip this?
      if (VPackSlice resultsSlice = res.slice().get("results");
          resultsSlice.length() > 0) {
        Result r =
            ci.waitForPlan(resultsSlice[0].getNumber<uint64_t>()).waitAndGet();
        if (r.fail()) {
          return r;
        }

        TRI_IF_FAILURE("ClusterInfo::createCollectionsCoordinator") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        LOG_TOPIC("98bca", DEBUG, Logger::CLUSTER)
            << "createCollectionCoordinator, Plan changed, waiting for "
               "success...";

        // Now "busy-loop"
        while (!server.isStopping()) {
          auto maybeFinalResult = callbackInfos->getResultIfAllReported();
          if (maybeFinalResult.has_value()) {
            // We have a final result. we are complete
            auto const& finalResult = maybeFinalResult.value();
            if (finalResult.fail()) {
              // Oh noes, something bad has happend.
              // Abort
              return finalResult;
            }

            // Collection Creation worked.
            LOG_TOPIC("98bcb", DEBUG, Logger::CLUSTER)
                << "createCollectionCoordinator, collections ok, removing "
                   "isBuilding...";

            // Let us remove the isBuilding flags.
            auto removeIsBuilding =
                writer.prepareCompletedTransaction(databaseName);

            // This is a best effort, in the worst case the collection stays,
            // but will be cleaned out by deleteCollectionGuard respectively
            // the supervision. This removes *all* isBuilding flags from all
            // collections. This is important so that the creation of all
            // collections is atomic, and the deleteCollectionGuard relies on
            // it, too.
            auto removeBuildingResult =
                ac.sendTransactionWithFailover(removeIsBuilding);

            LOG_TOPIC("98bcc", DEBUG, Logger::CLUSTER)
                << "createCollectionCoordinator, isBuilding removed, waiting "
                   "for new "
                   "Plan...";

            TRI_IF_FAILURE(
                "ClusterInfo::createCollectionsCoordinatorRemoveIsBuilding") {
              removeBuildingResult.set(rest::ResponseCode::PRECONDITION_FAILED,
                                       "Failed to mark collection ready");
            }

            if (removeBuildingResult.successful()) {
              // We do not want to undo from here, cancel the guard
              undoCreationGuard.cancel();

              // Wait for Plan to updated
              // TODO: Why?
              if (resultsSlice = removeBuildingResult.slice().get("results");
                  resultsSlice.length() > 0) {
                r = ci.waitForPlan(resultsSlice[0].getNumber<uint64_t>())
                        .waitAndGet();
                if (r.fail()) {
                  return r;
                }
                LOG_TOPIC("98764", DEBUG, Logger::CLUSTER)
                    << "Finished createCollectionsCoordinator for "
                    << collectionNames.size() << " collections in database "
                    << databaseName
                    << " first collection name: " << collectionNames[0]
                    << " result: " << TRI_ERROR_NO_ERROR;
                return TRI_ERROR_NO_ERROR;
              }

            } else {
              LOG_TOPIC("98675", WARN, Logger::CLUSTER)
                  << "Failed createCollectionsCoordinator for "
                  << collectionNames.size() << " collections in database "
                  << databaseName
                  << " first collection name: " << collectionNames[0]
                  << " result: " << removeBuildingResult;
              return {TRI_ERROR_HTTP_SERVICE_UNAVAILABLE,
                      "A cluster backend which was required for the operation "
                      "could not be reached"};
            }
          }
          // We do not have a final result. Let's wait for more input
          // Wait for the next incomplete callback
          for (auto& [cb, cid] : callbackList) {
            if (!callbackInfos->hasReported(cid)) {
              // We do not have result for this collection, wait for it.
              bool gotTimeout;
              {
                // This one has not responded, wait for it.
                std::unique_lock guard(cb->_cv.mutex);
                gotTimeout = cb->executeByCallbackOrTimeout(pollInterval);
              }
              if (gotTimeout) {
                // We got woken up by waittime, not by  callback.
                // Let us check if we skipped other callbacks as well
                for (auto& [cb2, cid2] : callbackList) {
                  if (callbackInfos->hasReported(cid2)) {
                    // Only re check those where we have not yet found a
                    // result.
                    cb2->refetchAndUpdate(true, false);
                  }
                }
              }
              // Break the callback loop,
              // continue on the check if we completed loop
              break;
            }
          }
        }
        // If we get here we are not allowed to retry.
        // The loop above does not contain a break
        TRI_ASSERT(server.isStopping());
        return Result{TRI_ERROR_SHUTTING_DOWN};
      }
    } else {
      // We can just retry here.
      // Most of our preconditions are protections against dead servers and
      // changes in plan version Those are recomputed in this loop. The only
      // thing that we cannot retry: The CollectionName is taken. That is
      // checked at the beginning of this retry loop
    }
  }
}

Result impl(ClusterInfo& ci, ArangodServer& server,
            std::string_view databaseName, TargetCollectionAgencyWriter& writer,
            bool waitForSyncReplication) {
  std::vector<ServerID> availableServers = ci.getCurrentDBServers();

  // TODO Timeout?
  auto buildingTransaction =
      writer.prepareCreateTransaction(databaseName, availableServers);
  if (buildingTransaction.fail()) {
    return buildingTransaction.result();
  }
  auto& agencyCache = server.getFeature<ClusterFeature>().agencyCache();
  auto callbackInfos = writer.prepareCurrentWatcher(
      databaseName, waitForSyncReplication, agencyCache);

  std::vector<std::pair<std::shared_ptr<AgencyCallback>, std::string>>
      callbackList;
  AgencyCallbackRegistry& callbackRegistry = ci.agencyCallbackRegistry();
  auto unregisterCallbacksGuard =
      scopeGuard([&callbackList, &callbackRegistry]() noexcept {
        try {
          for (auto& [cb, _] : callbackList) {
            callbackRegistry.unregisterCallback(cb);
          }
        } catch (std::exception const& ex) {
          LOG_TOPIC("cc911", ERR, Logger::CLUSTER)
              << "Failed to unregister agency callback: " << ex.what();
        } catch (...) {
          // Should never be thrown, we only throw exceptions
        }
      });

  // First register all callbacks
  for (auto const& [path, identifier, cb] : callbackInfos->getCallbackInfos()) {
    auto agencyCallback =
        std::make_shared<AgencyCallback>(server, path, cb, true, false);
    Result r = callbackRegistry.registerCallback(agencyCallback);
    if (r.fail()) {
      return r;
    }
    callbackList.emplace_back(
        std::make_pair(std::move(agencyCallback), identifier));
  }
  callbackInfos->clearCallbacks();
  using namespace std::chrono_literals;
  AsyncAgencyComm aac;
  // TODO do we need to handle Error message (thenError?)

  double pollInterval = ci.getPollInterval();
  auto res =
      aac.withSkipScheduler(true)
          .sendWriteTransaction(120s, std::move(buildingTransaction.get()))
          .thenValue(::reactToPreconditionsCreate)
          .waitAndGet();

  if (auto r = waitForOperationRoundtrip(ci, std::move(res)); r.fail()) {
    // TODO: TRIGGER_CLEANUP
    return r;
  }

  if (auto r = waitForCurrentToCatchUp(server, callbackInfos, callbackList,
                                       pollInterval);
      r.fail()) {
    return r;
  }
  // get current raft index; this is at least as high as the one we just waited
  // for in waitForCurrentToCatchUp
  auto const index = agencyCache.index();
  // wait for cluster info to catch up
  auto futCurrent = ci.waitForCurrent(index);
  auto futPlan = ci.waitForPlan(index);
  if (auto r = futCurrent.waitAndGet(); r.fail()) {
    return r;
  }
  if (auto r = futPlan.waitAndGet(); r.fail()) {
    return r;
  }
  return {};
}

template<replication::Version ReplicationVersion>
[[nodiscard]] arangodb::ResultT<std::vector<std::shared_ptr<LogicalCollection>>>
createCollectionsOnCoordinatorImpl(
    TRI_vocbase_t& vocbase, std::vector<CreateCollectionBody> collections,
    bool ignoreDistributeShardsLikeErrors, bool waitForSyncReplication,
    bool enforceReplicationFactor, bool isNewDatabase) {
  using EntryType =
      typename std::conditional<ReplicationVersion == replication::Version::TWO,
                                PlanCollectionEntryReplication2,
                                PlanCollectionEntry>::type;
  using WriterType =
      typename std::conditional<ReplicationVersion == replication::Version::TWO,
                                TargetCollectionAgencyWriter,
                                PlanCollectionToAgencyWriter>::type;

  auto& feature = vocbase.server().getFeature<ClusterFeature>();
  // List of all sharding prototypes.
  // We retain a reference here ourselfs in case we need to retry due to server
  // failure, this way we can just to create the shards on other servers.
  std::unordered_map<std::string, std::shared_ptr<IShardDistributionFactory>>
      shardDistributionList;

  std::vector<EntryType> collectionPlanEntries{};
  collectionPlanEntries.reserve(collections.size());

  /*
 NEED TO ACTIVATE THE FOLLOWING CODE

   if (warnAboutReplicationFactor) {
LOG_TOPIC("e16ec", WARN, Logger::CLUSTER)
   << "createCollectionCoordinator: replicationFactor is "
   << "too large for the number of DBservers";
}

   */
  auto groups = std::invoke(
      [&]() -> ResultT<arangodb::replication2::CollectionGroupUpdates> {
        if constexpr (ReplicationVersion == replication::Version::TWO) {
          return arangodb::ClusterCollectionMethods::prepareCollectionGroups(
              feature.clusterInfo(), vocbase.name(), collections);

        } else {
          arangodb::replication2::CollectionGroupUpdates groups{};
          return groups;
        }
      });

  if (groups.fail()) {
    return groups.result();
  }

  auto serverState = ServerState::instance();
  AgencyIsBuildingFlags buildingFlags;
  buildingFlags.coordinatorName = serverState->getId();
  buildingFlags.rebootId = serverState->getRebootId();
  for (auto& c : collections) {
    auto shards = ClusterCollectionMethods::generateShardNames(
        feature.clusterInfo(), c.numberOfShards.value());

    // Temporarily add shardsR2 here. This is going to be done by the
    // supervision in the future.
    c.shardsR2 = shards;

    auto distributionType = ClusterCollectionMethods::selectDistributeType(
        feature.clusterInfo(), vocbase.name(), c, enforceReplicationFactor,
        shardDistributionList);
    if constexpr (ReplicationVersion == replication::Version::TWO) {
      collectionPlanEntries.emplace_back(
          ClusterCollectionMethods::toPlanEntryReplication2(
              std::move(c), std::move(shards), distributionType,
              buildingFlags));
    } else if constexpr (ReplicationVersion == replication::Version::ONE) {
      collectionPlanEntries.emplace_back(ClusterCollectionMethods::toPlanEntry(
          std::move(c), std::move(shards), distributionType, buildingFlags));
    }
  }
  // Protection, all entries have been moved
  collections.clear();

  WriterType writer = std::invoke([&]() {
    if constexpr (ReplicationVersion == replication::Version::TWO) {
      return WriterType{std::move(collectionPlanEntries),
                        std::move(shardDistributionList),
                        std::move(groups.get())};
    } else if constexpr (ReplicationVersion == replication::Version::ONE) {
      return WriterType{std::move(collectionPlanEntries),
                        std::move(shardDistributionList)};
    }
  });
  auto res =
      ::impl(feature.clusterInfo(), vocbase.server(),
             std::string_view{vocbase.name()}, writer, waitForSyncReplication);
  if (res.fail()) {
    // Something went wrong, let's report
    return res;
  }

  // Everything all right, collections shall now be there

  std::vector<std::shared_ptr<LogicalCollection>> results;
  auto collectionNamesToLoad = writer.collectionNames();
  results.reserve(collectionNamesToLoad.size());

  auto& ci = feature.clusterInfo();
  if (isNewDatabase) {
    // Call dangerous method on ClusterInfo to generate only collection stubs to
    // use here.
    auto lookupList = ci.generateCollectionStubs(vocbase);
    for (auto const& name : collectionNamesToLoad) {
      try {
        auto c = lookupList.at(name);
        TRI_ASSERT(c.get() != nullptr) << "Collection created as nullptr. "
                                          "Should be detected in ClusterInfo.";
        TRI_ASSERT(!c->isSmartChild())
            << "For now we do not have SmartGraphs during database creation, "
               "if that ever changes remove this assertion.";

        // NOTE: The if is not strictly necessary now, see above assertion, just
        // future proof.
        if (!c->isSmartChild()) {
          // Smart Child collections should not be visible after create.
          results.emplace_back(std::move(c));
        }
      } catch (...) {
        TRI_ASSERT(false) << "Collection " << name
                          << " was not created during Database creation.";
        return Result{TRI_ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE,
                      "Required Collection " + name + " could not be created."};
      }
    }
  } else {
    for (auto const& name : collectionNamesToLoad) {
      auto c = ci.getCollection(vocbase.name(), name);
      TRI_ASSERT(c.get() != nullptr);
      // We never get a nullptr here because an exception is thrown if the
      // collection does not exist. Also, the createCollection should have
      // failed before.
      if (!c->isSmartChild()) {
        // Smart Child collections should be visible after create.
        results.emplace_back(std::move(c));
      }
    }
  }

  return {std::move(results)};
}
}  // namespace

[[nodiscard]] auto ClusterCollectionMethods::toPlanEntry(
    CreateCollectionBody col, std::vector<ShardID> shardNames,
    std::shared_ptr<IShardDistributionFactory> distributeType,
    AgencyIsBuildingFlags buildingFlags) -> PlanCollectionEntry {
  return {std::move(col),
          ShardDistribution{std::move(shardNames), std::move(distributeType)},
          std::move(buildingFlags)};
}

[[nodiscard]] auto ClusterCollectionMethods::toPlanEntryReplication2(
    CreateCollectionBody col, std::vector<ShardID> shardNames,
    std::shared_ptr<IShardDistributionFactory> distributeType,
    AgencyIsBuildingFlags buildingFlags) -> PlanCollectionEntryReplication2 {
  return PlanCollectionEntryReplication2{std::move(col)};
}

[[nodiscard]] auto ClusterCollectionMethods::generateShardNames(
    ClusterInfo& ci, uint64_t numberOfShards) -> std::vector<ShardID> {
  if (numberOfShards == 0) {
    // If we do not have shards, we do only need an empty vector and no ids.
    return {};
  }
  // Reserve ourselves the next numberOfShards many ids to use them for
  // shardNames
  uint64_t const id = ci.uniqid(numberOfShards);
  std::vector<ShardID> shardNames;
  shardNames.reserve(numberOfShards);
  for (uint64_t i = 0; i < numberOfShards; ++i) {
    shardNames.emplace_back("s" + basics::StringUtils::itoa(id + i));
  }
  return shardNames;
}

[[nodiscard]] auto ClusterCollectionMethods::prepareCollectionGroups(
    ClusterInfo& ci, std::string_view databaseName,
    std::vector<CreateCollectionBody>& collections)
    -> ResultT<replication2::CollectionGroupUpdates> {
  arangodb::replication2::CollectionGroupUpdates groups;
  std::unordered_map<std::string, replication2::agency::CollectionGroupId>
      selfCreatedGroups;
  for (auto& col : collections) {
    if (col.distributeShardsLike.has_value()) {
      auto const& leadingName = col.distributeShardsLike.value();
      if (selfCreatedGroups.contains(leadingName)) {
        auto groupId = selfCreatedGroups.at(leadingName);
        groups.addToNewGroup(groupId, col.id);
        col.groupId = groupId;
      } else {
        auto c = ci.getCollection(databaseName, leadingName);
        TRI_ASSERT(c.get() != nullptr);
        // We never get a nullptr here because an exception is thrown if the
        // collection does not exist. Also, the createCollection should have
        // failed before.
        auto groupId = c->groupID();
        groups.addToExistingGroup(groupId, col.id);
        col.groupId = groupId;
      }
    } else {
      // Create a new CollectionGroup
      auto groupId = groups.addNewGroup(col, [&ci]() { return ci.uniqid(); });
      // Remember it for reuse
      selfCreatedGroups.emplace(col.name, groupId);
      col.groupId = groupId;
    }
  }
  return groups;
}

[[nodiscard]] auto ClusterCollectionMethods::selectDistributeType(
    ClusterInfo& ci, std::string_view databaseName,
    CreateCollectionBody const& col, bool enforceReplicationFactor,
    std::unordered_map<std::string, std::shared_ptr<IShardDistributionFactory>>&
        allUsedDistributions) -> std::shared_ptr<IShardDistributionFactory> {
  if (col.distributeShardsLike.has_value()) {
    auto distLike = col.distributeShardsLike.value();
    // Empty value has to be rejected by invariants beforehand, assert here just
    // in case.
    TRI_ASSERT(!distLike.empty());
    if (allUsedDistributions.contains(distLike)) {
      // We are already set, use the other one.
      return allUsedDistributions.at(distLike);
    }
    // Follow the given distribution
    auto distribution = std::make_shared<DistributeShardsLike>(
        [&ci, databaseName,
         distLike]() -> ResultT<std::vector<ResponsibleServerList>> {
          // We need the lookup in the callback, as it will be called on retry.
          // So time has potentially passed, and shards could be moved
          // meanwhile.
          auto c = ci.getCollectionNT(databaseName, distLike);
          if (c == nullptr) {
            return Result{TRI_ERROR_CLUSTER_UNKNOWN_DISTRIBUTESHARDSLIKE,
                          "Collection not found: " + distLike +
                              " in database " + std::string(databaseName)};
          }
          auto shardingInfo = c->shardingInfo();
          // Every collection has shards.
          TRI_ASSERT(shardingInfo != nullptr);

          auto shardNames = shardingInfo->shardListAsShardID();
          std::vector<ResponsibleServerList> result{};
          auto shardIds = shardingInfo->shardIds();
          result.reserve(shardIds->size());
          for (auto const& s : shardNames) {
            auto servers = shardIds->find(s);
            result.emplace_back(ResponsibleServerList{servers->second});
          }
          return result;
        });
    // Add the Leader to the distribution List
    allUsedDistributions.emplace(distLike, distribution);
    return distribution;
  } else if (col.isSatellite()) {
    // We are a Satellite collection, use Satellite sharding
    auto distribution = std::make_shared<SatelliteDistribution>();
    allUsedDistributions.emplace(col.name, distribution);
    return distribution;
  } else {
    // Just distribute evenly
    auto distribution = std::make_shared<EvenDistribution>(
        col.numberOfShards.value(), col.replicationFactor.value(),
        col.avoidServers, enforceReplicationFactor);
    allUsedDistributions.emplace(col.name, distribution);
    return distribution;
  }
}

[[nodiscard]] arangodb::ResultT<std::vector<std::shared_ptr<LogicalCollection>>>
ClusterCollectionMethods::createCollectionsOnCoordinator(
    TRI_vocbase_t& vocbase, std::vector<CreateCollectionBody> collections,
    bool ignoreDistributeShardsLikeErrors, bool waitForSyncReplication,
    bool enforceReplicationFactor, bool isNewDatabase) {
  TRI_IF_FAILURE("ClusterInfo::requiresWaitForReplication") {
    if (waitForSyncReplication) {
      return {TRI_ERROR_DEBUG};
    } else {
      TRI_ASSERT(false) << "We required to have waitForReplication, but it "
                           "was set to false";
    }
  }

  TRI_ASSERT(!collections.empty());
  if (collections.empty()) {
    return Result{
        TRI_ERROR_INTERNAL,
        "Trying to create an empty list of collections on coordinator."};
  }

  if (vocbase.replicationVersion() == replication::Version::TWO) {
    return createCollectionsOnCoordinatorImpl<replication::Version::TWO>(
        vocbase, std::move(collections), ignoreDistributeShardsLikeErrors,
        waitForSyncReplication, enforceReplicationFactor, isNewDatabase);
  } else {
    return createCollectionsOnCoordinatorImpl<replication::Version::ONE>(
        vocbase, std::move(collections), ignoreDistributeShardsLikeErrors,
        waitForSyncReplication, enforceReplicationFactor, isNewDatabase);
  }
}
////////////////////////////////////////////////////////////////////////////////
/// @brief set collection properties in coordinator
////////////////////////////////////////////////////////////////////////////////

[[nodiscard]] auto ClusterCollectionMethods::updateCollectionProperties(
    TRI_vocbase_t& vocbase, LogicalCollection const& col) -> Result {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  AsyncAgencyComm aac;

  auto const& databaseName = vocbase.name();
  auto const& collectionID = std::to_string(col.id().id());
  auto const& server = vocbase.server();
  auto& ci = server.getFeature<ClusterFeature>().clusterInfo();
  auto& agencyCache = server.getFeature<ClusterFeature>().agencyCache();

  if (vocbase.replicationVersion() == replication::Version::TWO) {
    VPackBufferUInt8 data;
    VPackBuilder builder(data);
    auto envelope = arangodb::agency::envelope::into_builder(builder);
    // NOTE: We could do this better with partial updates.
    // e.g. we do not need to update the group if only the schema of the
    // collection is modified

    replication2::agency::CollectionGroupTargetSpecification::Attributes::
        MutableAttributes groupUpdate;
    {
      groupUpdate.writeConcern = col.writeConcern();
      groupUpdate.replicationFactor = col.replicationFactor();
      groupUpdate.waitForSync = col.waitForSync();
    }

    auto groupBase = pathCollectionGroupInTarget(databaseName)
                         ->group(std::to_string(col.groupID().id()));
    auto groupPath = groupBase->str();
    auto groupMutablesPath = groupBase->attributes()->mutables()->str();
    auto colBase =
        pathCollectionInTarget(databaseName)->collection(collectionID);
    auto colPath = colBase->str();

    auto writes = std::move(envelope).write();
    // Update Mutable Properties of the collection
    {
      writes = std::move(writes).emplace_object(
          colBase->schema()->str(),
          [&](VPackBuilder& builder) { col.schemaToVelocyPack(builder); });

      writes = std::move(writes).emplace_object(
          colBase->cacheEnabled()->str(), [&](VPackBuilder& builder) {
            builder.add(VPackValue(col.cacheEnabled()));
          });

      writes = std::move(writes).emplace_object(
          colBase->computedValues()->str(), [&](VPackBuilder& builder) {
            col.computedValuesToVelocyPack(builder);
          });
    }

    // Update Mutable Properties of the group
    writes = std::move(writes).emplace_object(
        groupMutablesPath, [&](VPackBuilder& builder) {
          velocypack::serialize(builder, groupUpdate);
        });
    // Increment the Group Version
    writes = std::move(writes).inc(groupBase->version()->str());

    // First read set version (sorry we have to do this as the increment call to
    // version does not give us the new value back)
    VPackBuilder response;
    agencyCache.get(response, groupBase->version()->str(
                                  arangodb::cluster::paths::SkipComponents{1}));
    uint64_t versionToWaitFor =
        basics::VelocyPackHelper::getNumericValue(response.slice(), 0ull);
    // We may get 0 here if the Target Group Entry does not exist anymore.
    // This indicates that the Group does not exist anymore. If this happens the
    // below preconditions will catch this.

    // Preconditions: Database exists, Collection exists. Group exists.
    auto preconditions = std::move(writes).precs();
    preconditions = std::move(preconditions)
                        .isNotEmpty(pathDatabaseInTarget(databaseName)->str());
    preconditions = std::move(preconditions).isNotEmpty(colPath);
    preconditions = std::move(preconditions).isNotEmpty(groupPath);
    std::move(preconditions).end().done();

    // Now data contains the transaction;
    using namespace std::chrono_literals;
    auto res = aac.sendWriteTransaction(120s, std::move(data))
                   .thenValue(::reactToPreconditions)
                   .waitAndGet();

    if (res.fail()) {
      return res.result();
    }
    // Now wait for the change to be happening
    AgencyCallbackRegistry& callbackRegistry = ci.agencyCallbackRegistry();
    auto currentGroup = pathCollectionGroupInCurrent(databaseName)
                            ->group(std::to_string(col.groupID().id()))
                            ->supervision()
                            ->str(arangodb::cluster::paths::SkipComponents(1));
    auto waitForSuccess = callbackRegistry.waitFor(
        currentGroup, [versionToWaitFor](VPackSlice slice) {
          TRI_ASSERT(versionToWaitFor > 0)
              << "We have found a CollectionGroup without a current version";
          if (slice.isNone()) {
            // TODO: Should this actual set an "error"? It indicates
            // that the collection is dropped if i am not mistaken
            return false;
          }
          auto groupSupervision = velocypack::deserialize<
              replication2::agency::CollectionGroupCurrentSpecification::
                  Supervision>(slice);
          // We need to wait for a version that is greater than the one we
          // started with
          return groupSupervision.version.has_value() &&
                 groupSupervision.version.value() > versionToWaitFor;
        });
    // WE do not really need the change to be appied.
    // We just have to wait for DBServers to apply it.
    std::ignore = waitForSuccess.waitAndGet();
    // Everything works as expected.
    return TRI_ERROR_NO_ERROR;
  } else {
    AgencyPrecondition databaseExists("Plan/Databases/" + databaseName,
                                      AgencyPrecondition::Type::EMPTY, false);
    AgencyOperation incrementVersion("Plan/Version",
                                     AgencySimpleOperationType::INCREMENT_OP);

    auto [acb, index] =
        agencyCache.read(std::vector<std::string>{AgencyCommHelper::path(
            "Plan/Collections/" + databaseName + "/" + collectionID)});

    velocypack::Slice collection =
        acb->slice()[0].get(std::initializer_list<std::string_view>{
            AgencyCommHelper::path(), "Plan", "Collections", databaseName,
            collectionID});

    if (!collection.isObject()) {
      return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    }

    VPackBuilder temp;
    temp.openObject();
    temp.add(StaticStrings::WaitForSyncString, VPackValue(col.waitForSync()));
    if (col.isSatellite()) {
      temp.add(StaticStrings::ReplicationFactor,
               VPackValue(StaticStrings::Satellite));
    } else {
      temp.add(StaticStrings::ReplicationFactor,
               VPackValue(col.replicationFactor()));
    }
    temp.add(StaticStrings::MinReplicationFactor,
             VPackValue(col.writeConcern()));  // deprecated in 3.6
    temp.add(StaticStrings::WriteConcern, VPackValue(col.writeConcern()));
    temp.add(StaticStrings::UsesRevisionsAsDocumentIds,
             VPackValue(col.usesRevisionsAsDocumentIds()));
    temp.add(StaticStrings::SyncByRevision, VPackValue(col.syncByRevision()));
    temp.add(VPackValue(StaticStrings::ComputedValues));
    col.computedValuesToVelocyPack(temp);
    temp.add(VPackValue(StaticStrings::Schema));
    col.schemaToVelocyPack(temp);
    col.getPhysical()->getPropertiesVPack(temp);
    temp.close();

    VPackBuilder builder =
        VPackCollection::merge(collection, temp.slice(), false);

    AgencyOperation setColl(
        "Plan/Collections/" + databaseName + "/" + collectionID,
        AgencyValueOperationType::SET, builder.slice());
    AgencyPrecondition oldValue(
        "Plan/Collections/" + databaseName + "/" + collectionID,
        AgencyPrecondition::Type::VALUE, collection);

    AgencyWriteTransaction trans({setColl, incrementVersion},
                                 {databaseExists, oldValue});

    using namespace std::chrono_literals;
    auto res = aac.withSkipScheduler(true)
                   .sendTransaction(120s, trans)
                   .thenValue(::reactToPreconditions)
                   .waitAndGet();
    return waitForOperationRoundtrip(ci, std::move(res));
  }
}
