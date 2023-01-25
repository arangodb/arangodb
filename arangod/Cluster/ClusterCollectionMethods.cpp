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

#include "ClusterCollectionMethods.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Agency/AsyncAgencyComm.h"
#include "Basics/ConditionLocker.h"
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
#include "Rest/GeneralResponse.h"
#include "Sharding/ShardingInfo.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Properties/CreateCollectionBody.h"
#include "VocBase/vocbase.h"

using namespace arangodb;

namespace {

template<class WriterType>
Result impl(ClusterInfo& ci, ArangodServer& server,
            std::string_view databaseName, WriterType& writer,
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
            LOG_TOPIC("cc911", ERR, Logger::CLUSTER)
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

    if constexpr (std::is_same_v<WriterType, TargetCollectionAgencyWriter>) {
      using namespace std::chrono_literals;
      AsyncAgencyComm aac;
      // TODO do we need to handle Error message (thenError?)

      // TODO INFO: AFAIK this waiting chain is different with the TARGET
      // based architecture on the agency. So right now it is teomprary to
      // mimic the original behaviour.

      bool shouldUndo = false;
      // Prepare do undo if something fails now
      auto undoCreationGuard = scopeGuard([&writer, &databaseName, &ci, &server,
                                           &ac, &shouldUndo]() noexcept {
        if (!shouldUndo) {
          // This is a flag to disable the guard.
          return;
        }
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
                    ci.waitForPlan(resultsSlice[0].getNumber<uint64_t>()).get();
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
          // NOTE Increased LOGID by one, duplicate from below
          LOG_TOPIC("57487", ERR, Logger::CLUSTER)
              << "Failed to delete collection during rollback: " << ex.what();
        } catch (...) {
          // Just we do not crash, no one knowingly throws non exceptions.
        }
      });

      auto future =
          aac.sendWriteTransaction(120s, std::move(buildingTransaction.get()))
              .thenValue([&shouldUndo](
                             AsyncAgencyCommResult&& res) -> ResultT<uint64_t> {
                // We ordered the creation of collection, if this was not
                // successful we may try again, if it was, we continue with next
                // step.
                if (res.fail()) {
                  if (GeneralResponse::responseCode(
                          network::fuerteStatusToArangoErrorCode(
                              res.statusCode())) ==
                      rest::ResponseCode::PRECONDITION_FAILED) {
                    // If the precondition failed,
                    // we can retry o the next Plan version
                    return {
                        TRI_ERROR_CLUSTER_CREATE_COLLECTION_PRECONDITION_FAILED};
                  }
                  return res.asResult();
                }
                // We have changed the plan, if something bad happens on the
                // way, trigger the UndoGuard.
                shouldUndo = true;

                // extract raft index
                auto slice = res.slice().get("results");
                TRI_ASSERT(slice.isArray());
                TRI_ASSERT(!slice.isEmptyArray());
                return slice.at(slice.length() - 1).getNumericValue<uint64_t>();
              })
              .thenValue(
                  [&ci](ResultT<uint64_t>&& buildingPlanversion) -> Result {
                    // Got the Plan version while building.
                    // Let us wait for it
                    if (buildingPlanversion.fail()) {
                      return buildingPlanversion.result();
                    }
                    // TODO: Is this step actually necessary?
                    // We are mostly intrested in the current here.
                    return ci.waitForPlan(buildingPlanversion.get()).get();
                  })
              .thenValue([&server, &callbackInfos, &callbackList,
                          &pollInterval](Result&& res) {
                // We waited on the buildingPlan to be loaded in the local cache
                // Now let us watch for CURRENT to check if all requried changes
                // ahve been applied
                if (res.fail()) {
                  // TODO: TRIGGER_CLEANUP
                  return res;
                }

                TRI_IF_FAILURE("ClusterInfo::createCollectionsCoordinator") {
                  THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
                }
                // NOTE: LOGID was 98bca before, deplicate from below
                LOG_TOPIC("98bc9", DEBUG, Logger::CLUSTER)
                    << "createCollectionCoordinator, Plan changed, waiting for "
                       "success...";

                // Now "busy-loop"
                while (!server.isStopping()) {
                  auto maybeFinalResult =
                      callbackInfos->getResultIfAllReported();
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
                        CONDITION_LOCKER(locker, cb->_cv);
                        gotTimeout =
                            cb->executeByCallbackOrTimeout(pollInterval);
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
              })
              .thenValue([&ac, &ci, &writer, &databaseName, &collectionNames,
                          &shouldUndo](Result&& res) -> Result {
                // All changes have been applied, next step drop the
                // isBuildingFlags
                if (res.fail()) {
                  // Oh noes, something bad has happened.
                  // Abort
                  return res;
                }

                // Collection Creation worked.
                // NOTE: LOGID was 98bcb before, duplicate from below
                LOG_TOPIC("98bcd", DEBUG, Logger::CLUSTER)
                    << "createCollectionCoordinator, collections ok, removing "
                       "isBuilding...";

                // TODO: This piece can be made asynchronous as well.
                // Right now we use the Synchronous Agency here.

                // Let us remove the isBuilding flags.
                auto removeIsBuilding =
                    writer.prepareCompletedTransaction(databaseName);

                // This is a best effort, in the worst case the collection
                // stays, but will be cleaned out by deleteCollectionGuard
                // respectively the supervision. This removes *all* isBuilding
                // flags from all collections. This is important so that the
                // creation of all collections is atomic, and the
                // deleteCollectionGuard relies on it, too.
                auto removeBuildingResult =
                    ac.sendTransactionWithFailover(removeIsBuilding);

                // NOTE: LOGID was 98bcc before, duplicate from below
                LOG_TOPIC("98bce", DEBUG, Logger::CLUSTER)
                    << "createCollectionCoordinator, isBuilding removed, "
                       "waiting "
                       "for new "
                       "Plan...";

                TRI_IF_FAILURE(
                    "ClusterInfo::"
                    "createCollectionsCoordinatorRemoveIsBuilding") {
                  removeBuildingResult.set(
                      rest::ResponseCode::PRECONDITION_FAILED,
                      "Failed to mark collection ready");
                }

                if (removeBuildingResult.successful()) {
                  // We do not want to undo from here, cancel the guard
                  shouldUndo = false;

                  // Wait for Plan to updated
                  // TODO: Why?
                  if (VPackSlice resultsSlice =
                          removeBuildingResult.slice().get("results");
                      resultsSlice.length() > 0) {
                    auto r =
                        ci.waitForPlan(resultsSlice[0].getNumber<uint64_t>())
                            .get();
                    if (r.fail()) {
                      return r;
                    }
                    // NOTE: LogID was 98764 before, duplicate from below
                    LOG_TOPIC("98766", DEBUG, Logger::CLUSTER)
                        << "Finished createCollectionsCoordinator for "
                        << collectionNames.size() << " collections in database "
                        << databaseName
                        << " first collection name: " << collectionNames[0]
                        << " result: " << TRI_ERROR_NO_ERROR;
                    return TRI_ERROR_NO_ERROR;
                  }
                  TRI_ASSERT(false);
                  return TRI_ERROR_INTERNAL;

                } else {
                  // NOTE: Increased LOGID by one, deplicate from below
                  LOG_TOPIC("98676", WARN, Logger::CLUSTER)
                      << "Failed createCollectionsCoordinator for "
                      << collectionNames.size() << " collections in database "
                      << databaseName
                      << " first collection name: " << collectionNames[0]
                      << " result: " << removeBuildingResult;
                  return {
                      TRI_ERROR_HTTP_SERVICE_UNAVAILABLE,
                      "A cluster backend which was required for the operation "
                      "could not be reached"};
                }
              });
      // Wait synchronously here.
      // We cannot easily return the future as the callbacks capture local
      // variables.
      Result finalResult = future.get();
      if (finalResult.isNot(
              TRI_ERROR_CLUSTER_CREATE_COLLECTION_PRECONDITION_FAILED)) {
        // If we do not end up in the special preconditionFailed phase, we have
        // to report here If necessary the return will trigger the undo guard.
        // Otherwise this will now either return success or non-retryable error.
        return finalResult;
      }
    } else {
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
                              .get();
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
              ci.waitForPlan(resultsSlice[0].getNumber<uint64_t>()).get();
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
                removeBuildingResult.set(
                    rest::ResponseCode::PRECONDITION_FAILED,
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
                          .get();
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
                return {
                    TRI_ERROR_HTTP_SERVICE_UNAVAILABLE,
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
                  CONDITION_LOCKER(locker, cb->_cv);
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
        // TODO: Clean this up should not return DEBUG here
        return {TRI_ERROR_DEBUG, res.errorMessage()};
      }
    }
  }
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
  auto groups = arangodb::ClusterCollectionMethods::prepareCollectionGroups(
      feature.clusterInfo(), vocbase.name(), collections);
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
  return {std::move(col),
          ShardDistribution{std::move(shardNames), std::move(distributeType)},
          std::move(buildingFlags)};
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
        // TODO: This code needs to look-up the CollectionID.
        // It is not yet added as part of the Collection Properties.
        auto c = ci.getCollection(databaseName, leadingName);
        TRI_ASSERT(c.get() != nullptr);
        // We never get a nullptr here because an exception is thrown if the
        // collection does not exist. Also, the createCollection should have
        // failed before.
        auto groupId = replication2::agency::CollectionGroupId{c->id().id()};
        groups.addToExistingGroup(groupId, col.id);
        col.groupId = groupId;
      }
    } else {
      // Create a new CollectionGroup
      auto groupId = groups.addNewGroup(col);
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
          TRI_ASSERT(shardNames != nullptr);
          std::vector<ResponsibleServerList> result{};
          auto shardIds = shardingInfo->shardIds();
          result.reserve(shardIds->size());
          for (auto const& s : *shardNames) {
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
