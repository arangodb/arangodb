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

#include "ClusterIndexMethods.h"

#include "Agency/AgencyComm.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/system-functions.h"
#include "Basics/voc-errors.h"
#include "Cluster/AgencyCache.h"
#include "Cluster/AgencyCallback.h"
#include "Cluster/AgencyCallbackRegistry.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Logger/LogMacros.h"
#include "Indexes/Index.h"
#include "Random/RandomGenerator.h"
#include "Utils/Events.h"
#include "VocBase/Identifiers/IndexId.h"
#include "VocBase/vocbase.h"

#include <chrono>
#include <memory>

using namespace arangodb;

namespace {
static double getTimeout(double timeout) {
  if (timeout == 0.0) {
    return 24.0 * 3600.0;
  }
  return timeout;
}

constexpr double getPollInterval() {
  return 5.0;
  ;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the JSON returns an error
////////////////////////////////////////////////////////////////////////////////

static inline bool hasError(VPackSlice slice) {
  return arangodb::basics::VelocyPackHelper::getBooleanValue(
      slice, StaticStrings::Error, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the error message from a JSON
////////////////////////////////////////////////////////////////////////////////

static std::string extractErrorMessage(std::string_view shardId,
                                       VPackSlice slice) {
  std::string msg = " shardID:";
  msg += shardId;
  msg += ": ";

  // add error message text
  msg += arangodb::basics::VelocyPackHelper::getStringValue(
      slice, StaticStrings::ErrorMessage, "");

  // add error number
  if (slice.hasKey(StaticStrings::ErrorNum)) {
    VPackSlice const errorNum = slice.get(StaticStrings::ErrorNum);
    if (errorNum.isNumber()) {
      msg += " (errNum=" +
             arangodb::basics::StringUtils::itoa(
                 errorNum.getNumericValue<uint32_t>()) +
             ")";
    }
  }

  return msg;
}

// Read the collection from Plan; this is an object to have a valid VPack
// around to read from and to not have to carry around vpack builders.
// Might want to do the error handling with throw/catch?
class PlanCollectionReader {
 public:
  PlanCollectionReader(PlanCollectionReader const&&) = delete;
  PlanCollectionReader(PlanCollectionReader const&) = delete;
  explicit PlanCollectionReader(LogicalCollection const& collection) {
    std::string databaseName = collection.vocbase().name();
    std::string collectionID = std::to_string(collection.id().id());
    std::vector<std::string> path{AgencyCommHelper::path(
        "Plan/Collections/" + databaseName + "/" + collectionID)};

    auto& agencyCache = collection.vocbase()
                            .server()
                            .getFeature<ClusterFeature>()
                            .agencyCache();
    consensus::index_t idx = 0;
    std::tie(_read, idx) = agencyCache.read(path);

    if (!_read->slice().isArray()) {
      _state = Result(TRI_ERROR_CLUSTER_READING_PLAN_AGENCY,
                      "Could not retrieve " + path.front() +
                          " from agency cache: " + _read->toJson());
      return;
    }

    _collection = _read->slice()[0];

    std::initializer_list<std::string_view> const vpath{
        AgencyCommHelper::path(), "Plan", "Collections", databaseName,
        collectionID};

    if (!_collection.hasKey(vpath)) {
      _state = Result(TRI_ERROR_CLUSTER_READING_PLAN_AGENCY,
                      "Could not retrieve " + path.front() +
                          " from agency in version " + std::to_string(idx));
      return;
    }

    _collection = _collection.get(vpath);

    if (!_collection.isObject()) {
      _state = Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
      return;
    }

    _state = Result();
  }

  VPackSlice indexes() {
    VPackSlice res = _collection.get("indexes");
    if (res.isNone()) {
      return VPackSlice::emptyArraySlice();
    } else {
      TRI_ASSERT(res.isArray());
      return res;
    }
  }

  VPackSlice slice() { return _collection; }
  Result state() { return _state; }

 private:
  consensus::query_t _read;
  Result _state;
  velocypack::Slice _collection;
};

// make sure a collection is still in Plan
// we are only going from *assuming* that it is present
// to it being changed to not present.
class CollectionWatcher
    : public std::enable_shared_from_this<CollectionWatcher> {
 public:
  CollectionWatcher(CollectionWatcher const&) = delete;
  CollectionWatcher(AgencyCallbackRegistry* agencyCallbackRegistry,
                    LogicalCollection const& collection)
      : _agencyCallbackRegistry(agencyCallbackRegistry), _present(true) {
    std::string databaseName = collection.vocbase().name();
    std::string collectionID = std::to_string(collection.id().id());
    std::string where = "Plan/Collections/" + databaseName + "/" + collectionID;

    _agencyCallback = std::make_shared<AgencyCallback>(
        collection.vocbase().server(), where,
        [self = weak_from_this()](VPackSlice result) {
          auto watcher = self.lock();
          if (result.isNone() && watcher) {
            watcher->_present.store(false);
          }
          return true;
        },
        true, false);
    Result res = _agencyCallbackRegistry->registerCallback(_agencyCallback);
    if (res.fail()) {
      THROW_ARANGO_EXCEPTION(res);
    }
  }

  ~CollectionWatcher() {
    try {
      _agencyCallbackRegistry->unregisterCallback(_agencyCallback);
    } catch (std::exception const& ex) {
      LOG_TOPIC("42af2", WARN, Logger::CLUSTER)
          << "caught unexpected exception in CollectionWatcher: " << ex.what();
    }
  }

  bool isPresent() {
    // Make sure we did not miss a callback
    _agencyCallback->refetchAndUpdate(true, false);
    return _present.load();
  }

 private:
  AgencyCallbackRegistry* _agencyCallbackRegistry;
  std::shared_ptr<AgencyCallback> _agencyCallback;

  // TODO: this does not really need to be atomic: We only write to it
  //       in the callback, and we only read it in `isPresent`; it does
  //       not actually matter whether this value is "correct".
  std::atomic<bool> _present;
};

Result dropIndexCoordinatorInner(LogicalCollection const& col, IndexId iid,
                                 double endTime, AgencyComm& agencyComm) {
  std::string const idString = arangodb::basics::StringUtils::itoa(iid.id());
  double const interval = getPollInterval();
  auto const& vocbase = col.vocbase();
  auto const& databaseName = vocbase.name();
  auto collectionID = std::to_string(col.id().id());
  auto& server = vocbase.server();

  std::string const planCollKey =
      "Plan/Collections/" + databaseName + "/" + collectionID;
  std::string const planIndexesKey = planCollKey + "/indexes";

  auto& agencyCache = server.getFeature<ClusterFeature>().agencyCache();
  auto& clusterInfo = server.getFeature<ClusterFeature>().clusterInfo();
  AgencyCallbackRegistry& callbackRegistry =
      clusterInfo.agencyCallbackRegistry();
  auto [acb, index] = agencyCache.read(
      std::vector<std::string>{AgencyCommHelper::path(planCollKey)});
  auto previous = acb->slice();

  if (!previous.isArray() || previous.length() == 0) {
    return Result(TRI_ERROR_CLUSTER_READING_PLAN_AGENCY);
  }
  velocypack::Slice collection =
      previous[0].get(std::initializer_list<std::string_view>{
          AgencyCommHelper::path(), "Plan", "Collections", databaseName,
          collectionID});
  if (!collection.isObject()) {
    return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }

  TRI_ASSERT(VPackObjectIterator(collection).size() > 0);
  size_t const numberOfShards = col.numberOfShards();

  VPackSlice indexes = collection.get("indexes");
  if (!indexes.isArray()) {
    LOG_TOPIC("63178", DEBUG, Logger::CLUSTER)
        << "Failed to find index " << databaseName << "/" << collectionID << "/"
        << iid.id();
    return Result(TRI_ERROR_ARANGO_INDEX_NOT_FOUND);
  }

  VPackSlice indexToRemove;

  for (VPackSlice indexSlice : VPackArrayIterator(indexes)) {
    auto idSlice = indexSlice.get(arangodb::StaticStrings::IndexId);
    auto typeSlice = indexSlice.get(arangodb::StaticStrings::IndexType);

    if (!idSlice.isString() || !typeSlice.isString()) {
      continue;
    }

    if (idSlice.isEqualString(idString)) {
      Index::IndexType type = Index::type(typeSlice.copyString());

      if (type == Index::TRI_IDX_TYPE_PRIMARY_INDEX ||
          type == Index::TRI_IDX_TYPE_EDGE_INDEX) {
        return Result(TRI_ERROR_FORBIDDEN);
      }

      indexToRemove = indexSlice;

      break;
    }
  }

  if (!indexToRemove.isObject()) {
    LOG_TOPIC("95fe6", DEBUG, Logger::CLUSTER)
        << "Failed to find index " << databaseName << "/" << collectionID << "/"
        << iid.id();
    return Result(TRI_ERROR_ARANGO_INDEX_NOT_FOUND);
  }

  std::string where =
      "Current/Collections/" + databaseName + "/" + collectionID;

  auto dbServerResult =
      std::make_shared<std::atomic<std::optional<ErrorCode>>>(std::nullopt);
  // We need explicit copies as this callback may run even after
  // this function returns. So let's keep all used variables
  // explicit here.
  std::function<bool(VPackSlice result)> dbServerChanged =
      [dbServerResult, idString, numberOfShards](VPackSlice current) {
        if (numberOfShards == 0) {
          return false;
        }

        if (!current.isObject()) {
          return true;
        }

        VPackObjectIterator shards(current);

        if (shards.size() == numberOfShards) {
          bool found = false;
          for (auto const& shard : shards) {
            VPackSlice const indexes = shard.value.get("indexes");

            if (indexes.isArray()) {
              for (VPackSlice v : VPackArrayIterator(indexes)) {
                if (v.isObject()) {
                  VPackSlice const k = v.get(StaticStrings::IndexId);
                  if (k.isString() && k.isEqualString(idString)) {
                    // still found the index in some shard
                    found = true;
                    break;
                  }
                }

                if (found) {
                  break;
                }
              }
            }
          }

          if (!found) {
            dbServerResult->store(TRI_ERROR_NO_ERROR,
                                  std::memory_order_release);
          }
        }
        return true;
      };

  // ATTENTION: The following callback calls the above closure in a
  // different thread. Nevertheless, the closure accesses some of our
  // local variables. Therefore we have to protect all accesses to them
  // by a mutex. We use the mutex of the condition variable in the
  // AgencyCallback for this.
  auto agencyCallback = std::make_shared<AgencyCallback>(
      server, where, dbServerChanged, true, false);
  {
    Result r = callbackRegistry.registerCallback(agencyCallback);
    if (r.fail()) {
      return r;
    }
  }

  auto cbGuard = scopeGuard([&]() noexcept {
    try {
      callbackRegistry.unregisterCallback(agencyCallback);
    } catch (std::exception const& ex) {
      LOG_TOPIC("ac2bf", ERR, Logger::CLUSTER)
          << "Failed to unregister agency callback: " << ex.what();
    }
  });

  AgencyOperation planErase(planIndexesKey, AgencyValueOperationType::ERASE,
                            indexToRemove);
  AgencyOperation incrementVersion("Plan/Version",
                                   AgencySimpleOperationType::INCREMENT_OP);
  AgencyPrecondition prec(planCollKey, AgencyPrecondition::Type::VALUE,
                          collection);
  AgencyWriteTransaction trx({planErase, incrementVersion}, prec);
  AgencyCommResult result = agencyComm.sendTransactionWithFailover(trx, 0.0);

  if (!result.successful()) {
    if (result.httpCode() ==
        arangodb::rest::ResponseCode::PRECONDITION_FAILED) {
      // Retry loop is outside!
      return Result(TRI_ERROR_HTTP_PRECONDITION_FAILED);
    }

    return Result(
        TRI_ERROR_CLUSTER_COULD_NOT_DROP_INDEX_IN_PLAN,
        basics::StringUtils::concatT(" Failed to execute ", trx.toJson(),
                                     " ResultCode: ", result.errorCode()));
  }
  if (VPackSlice resultSlice = result.slice().get("results");
      resultSlice.length() > 0) {
    Result r =
        clusterInfo.waitForPlan(resultSlice[0].getNumber<uint64_t>()).get();
    if (r.fail()) {
      return r;
    }
  }

  if (numberOfShards == 0) {  // smart "dummy" collection has no shards
    TRI_ASSERT(collection.get(StaticStrings::IsSmart).getBool());

    return Result(TRI_ERROR_NO_ERROR);
  }

  {
    while (true) {
      auto const tmpRes = dbServerResult->load();
      if (tmpRes.has_value()) {
        cbGuard.fire();  // unregister cb
        events::DropIndex(databaseName, collectionID, idString, *tmpRes);

        return Result(*tmpRes);
      }

      if (TRI_microtime() > endTime) {
        return Result(TRI_ERROR_CLUSTER_TIMEOUT);
      }

      {
        std::lock_guard locker{agencyCallback->_cv.mutex};
        agencyCallback->executeByCallbackOrTimeout(interval);
      }

      if (server.isStopping()) {
        return Result(TRI_ERROR_SHUTTING_DOWN);
      }
    }
  }
}

// The following function does the actual work of index creation: Create
// in Plan, watch Current until all dbservers for all shards have done their
// bit. If this goes wrong with a timeout, the creation operation is rolled
// back. If the `create` flag is false, this is actually a lookup operation.
// In any case, no rollback has to happen in the calling function
// ClusterInfo::ensureIndexCoordinator. Note that this method here
// sets the `isBuilding` attribute to `true`, which leads to the fact
// that the index is not yet used by queries. There is code in the
// Agency Supervision which deletes this flag once everything has been
// built successfully. This is a more robust and self-repairing solution
// than if we would take out the `isBuilding` here, since it survives a
// coordinator crash and failover operations.
// Finally note that the retry loop for the case of a failed precondition
// is outside this function here in `ensureIndexCoordinator`.
Result ensureIndexCoordinatorInner(LogicalCollection const& collection,
                                   std::string_view idString, VPackSlice slice,
                                   bool create, VPackBuilder& resultBuilder,
                                   double timeout, ArangodServer& server) {
  using namespace std::chrono;

  double const realTimeout = getTimeout(timeout);
  double const endTime = TRI_microtime() + realTimeout;
  double const interval = getPollInterval();

  auto& clusterInfo = server.getFeature<ClusterFeature>().clusterInfo();
  AgencyCallbackRegistry& callbackRegistry =
      clusterInfo.agencyCallbackRegistry();

  TRI_ASSERT(resultBuilder.isEmpty());

  auto type = slice.get(arangodb::StaticStrings::IndexType);
  if (!type.isString()) {
    return Result(TRI_ERROR_INTERNAL,
                  "expecting string value for \"type\" attribute");
  }

  const size_t numberOfShards = collection.numberOfShards();

  // Get the current entry in Plan for this collection
  PlanCollectionReader collectionFromPlan(collection);
  if (!collectionFromPlan.state().ok()) {
    return collectionFromPlan.state();
  }

  auto& engine = server.getFeature<EngineSelectorFeature>().engine();
  VPackSlice indexes = collectionFromPlan.indexes();
  for (auto const& other : VPackArrayIterator(indexes)) {
    TRI_ASSERT(other.isObject());
    if (arangodb::Index::Compare(engine, slice, other,
                                 collection.vocbase().name())) {
      {  // found an existing index... Copy over all elements in slice.
        VPackObjectBuilder b(&resultBuilder);
        resultBuilder.add(VPackObjectIterator(other));
        resultBuilder.add("isNewlyCreated", VPackValue(false));
      }
      return Result(TRI_ERROR_NO_ERROR);
    }

    if (arangodb::Index::CompareIdentifiers(slice, other)) {
      // found an existing index with a same identifier (i.e. name)
      // but different definition, throw an error
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      LOG_TOPIC("e547d", WARN, Logger::CLUSTER)
          << "attempted to create index '" << slice.toJson()
          << "' but found conflicting index '" << other.toJson() << "'";
#endif
      return Result(TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER,
                    "duplicate value for `" + arangodb::StaticStrings::IndexId +
                        "` or `" + arangodb::StaticStrings::IndexName + "`");
    }
  }

  // no existing index found.
  if (!create) {
    TRI_ASSERT(resultBuilder.isEmpty());
    return Result(TRI_ERROR_NO_ERROR);
  }

  // will contain the error number and message
  auto dbServerResult =
      std::make_shared<std::atomic<std::optional<ErrorCode>>>(std::nullopt);
  std::shared_ptr<std::string> errMsg = std::make_shared<std::string>();

  // We need explicit copies as this callback may run even after
  // this function returns. So let's keep all used variables
  // explicit here.
  std::function<bool(VPackSlice result)> dbServerChanged =  // for format
      [dbServerResult, errMsg, numberOfShards,
       idString = std::string{idString}](VPackSlice result) {
        if (!result.isObject() || result.length() != numberOfShards) {
          return true;
        }

        size_t found = 0;
        for (auto const& shard : VPackObjectIterator(result)) {
          VPackSlice const slice = shard.value;
          if (slice.hasKey("indexes")) {
            VPackSlice const indexes = slice.get("indexes");
            if (!indexes.isArray()) {
              break;  // no list, so our index is not present. we can abort
                      // searching
            }

            for (VPackSlice v : VPackArrayIterator(indexes)) {
              VPackSlice const k = v.get(StaticStrings::IndexId);
              if (!k.isString() || idString != k.stringView()) {
                continue;  // this is not our index
              }

              // check for errors
              if (hasError(v)) {
                // Note that this closure runs with the mutex in the condition
                // variable of the agency callback, which protects writing
                // to *errMsg:
                *errMsg = extractErrorMessage(shard.key.stringView(), v);
                *errMsg = "Error during index creation: " + *errMsg;
                // Returns the specific error number if set, or the general
                // error otherwise
                auto errNum =
                    arangodb::basics::VelocyPackHelper::getNumericValue<
                        ErrorCode>(v, StaticStrings::ErrorNum,
                                   TRI_ERROR_ARANGO_INDEX_CREATION_FAILED);
                dbServerResult->store(errNum, std::memory_order_release);
                return true;
              }

              found++;  // found our index
              break;
            }
          }
        }

        if (found == static_cast<size_t>(numberOfShards)) {
          dbServerResult->store(TRI_ERROR_NO_ERROR, std::memory_order_release);
        }

        return true;
      };

  VPackBuilder newIndexBuilder;
  {
    VPackObjectBuilder ob(&newIndexBuilder);
    // Add the new index ignoring "id"
    for (auto const& e : VPackObjectIterator(slice)) {
      TRI_ASSERT(e.key.isString());
      auto key = e.key.stringView();
      if (key != StaticStrings::IndexId &&
          key != StaticStrings::IndexIsBuilding) {
        ob->add(e.key);
        ob->add(e.value);
      }
    }
    if (numberOfShards > 0) {
      ob->add(StaticStrings::IndexIsBuilding, VPackValue(true));
      // add our coordinator id and reboot id
      ob->add(StaticStrings::AttrCoordinator,
              VPackValue(ServerState::instance()->getId()));
      ob->add(StaticStrings::AttrCoordinatorRebootId,
              VPackValue(ServerState::instance()->getRebootId().value()));
    }
    ob->add(StaticStrings::IndexId, VPackValue(idString));
  }

  // ATTENTION: The following callback calls the above closure in a
  // different thread. Nevertheless, the closure accesses some of our
  // local variables. Therefore we have to protect all accesses to them
  // by a mutex. We use the mutex of the condition variable in the
  // AgencyCallback for this.
  std::string databaseName = collection.vocbase().name();
  std::string collectionID = std::to_string(collection.id().id());

  std::string where =
      "Current/Collections/" + databaseName + "/" + collectionID;
  auto agencyCallback = std::make_shared<AgencyCallback>(
      server, where, dbServerChanged, true, false);

  {
    Result r = callbackRegistry.registerCallback(agencyCallback);
    if (r.fail()) {
      return r;
    }
  }

  auto cbGuard = scopeGuard([&]() noexcept {
    try {
      callbackRegistry.unregisterCallback(agencyCallback);
    } catch (std::exception const& ex) {
      LOG_TOPIC("7702e", ERR, Logger::CLUSTER)
          << "Failed to unregister agency callback: " << ex.what();
    }
  });

  std::string const planCollKey =
      "Plan/Collections/" + databaseName + "/" + collectionID;
  std::string const planIndexesKey = planCollKey + "/indexes";
  AgencyOperation newValue(planIndexesKey, AgencyValueOperationType::PUSH,
                           newIndexBuilder.slice());
  AgencyOperation incrementVersion("Plan/Version",
                                   AgencySimpleOperationType::INCREMENT_OP);

  AgencyPrecondition oldValue(planCollKey, AgencyPrecondition::Type::VALUE,
                              collectionFromPlan.slice());
  AgencyComm ac(server);

  AgencyWriteTransaction trx({newValue, incrementVersion}, oldValue);
  AgencyCommResult result = ac.sendTransactionWithFailover(trx, 0.0);

  if (result.successful()) {
    if (VPackSlice resultsSlice = result.slice().get("results");
        resultsSlice.length() > 0) {
      Result r =
          clusterInfo.waitForPlan(resultsSlice[0].getNumber<uint64_t>()).get();
      if (r.fail()) {
        return r;
      }
    }
  }

  // This object watches whether the collection is still present in Plan
  // It assumes that the collection *is* present and only changes state
  // if the collection disappears
  auto collectionWatcher =
      std::make_shared<CollectionWatcher>(&callbackRegistry, collection);

  if (!result.successful()) {
    if (result.httpCode() == rest::ResponseCode::PRECONDITION_FAILED) {
      // Retry loop is outside!
      return Result(TRI_ERROR_HTTP_PRECONDITION_FAILED);
    }

    return Result(TRI_ERROR_CLUSTER_COULD_NOT_CREATE_INDEX_IN_PLAN,
                  basics::StringUtils::concatT(
                      " Failed to execute ", trx.toJson(),
                      " ResultCode: ", result.errorCode(),
                      " HttpCode: ", static_cast<int>(result.httpCode()), " ",
                      __FILE__, ":", __LINE__));
  }

  // From here on we want to roll back the index creation if we run into
  // the timeout. If this coordinator crashes, the worst that can happen
  // is that the index stays in some state. In most cases, it will converge
  // against the planned state.
  if (numberOfShards == 0) {  // smart "dummy" collection has no shards
    TRI_ASSERT(collection.isSmart());

    {
      // Copy over all elements in slice.
      VPackObjectBuilder b(&resultBuilder);
      resultBuilder.add(StaticStrings::IsSmart, VPackValue(true));
    }

    return Result(TRI_ERROR_NO_ERROR);
  }

  {
    while (!server.isStopping()) {
      auto tmpRes = dbServerResult->load(std::memory_order_acquire);

      if (!tmpRes.has_value()) {
        // index has not shown up in Current yet,  follow up check to
        // ensure it is still in plan (not dropped between iterations)
        auto& cache = server.getFeature<ClusterFeature>().agencyCache();
        auto [acb, index] = cache.get(planIndexesKey);
        auto oldIndexes = acb->slice();

        bool found = false;
        if (oldIndexes.isArray()) {
          for (VPackSlice v : VPackArrayIterator(oldIndexes)) {
            VPackSlice const k = v.get(StaticStrings::IndexId);
            if (k.isString() && k.stringView() == idString) {
              // index is still here
              found = true;
              break;
            }
          }
        }

        if (!found) {
          return Result(TRI_ERROR_ARANGO_INDEX_CREATION_FAILED,
                        "index was dropped during creation");
        }
      }

      if (tmpRes.has_value() && tmpRes == TRI_ERROR_NO_ERROR) {
        // Finally, in case all is good, remove the `isBuilding` flag
        // check that the index has appeared. Note that we have to have
        // a precondition since the collection could have been deleted
        // in the meantime:
        VPackBuilder finishedPlanIndex;
        {
          VPackObjectBuilder o(&finishedPlanIndex);
          for (auto entry : VPackObjectIterator(newIndexBuilder.slice())) {
            auto const key = entry.key.stringView();
            // remove "isBuilding", "coordinatorId" and "rebootId", plus
            // "newlyCreated" from the final index
            if (key != StaticStrings::IndexIsBuilding &&
                key != StaticStrings::AttrCoordinator &&
                key != StaticStrings::AttrCoordinatorRebootId &&
                key != "isNewlyCreated") {
              finishedPlanIndex.add(entry.key.stringView(), entry.value);
            }
          }
        }

        AgencyWriteTransaction trx(
            {AgencyOperation(planIndexesKey, AgencyValueOperationType::REPLACE,
                             finishedPlanIndex.slice(),
                             newIndexBuilder.slice()),
             AgencyOperation("Plan/Version",
                             AgencySimpleOperationType::INCREMENT_OP)},
            AgencyPrecondition(planIndexesKey, AgencyPrecondition::Type::EMPTY,
                               false));
        IndexId indexId{arangodb::basics::StringUtils::uint64(
            newIndexBuilder.slice().get("id").copyString())};
        result = ac.sendTransactionWithFailover(trx, 0.0);
        if (!result.successful()) {
          // We just log the problem and move on, the Supervision will repair
          // things in due course:
          LOG_TOPIC("d9420", INFO, Logger::CLUSTER)
              << "Could not remove isBuilding flag in new index "
              << indexId.id() << ", this will be repaired automatically.";
        } else {
          if (VPackSlice resultsSlice = result.slice().get("results");
              resultsSlice.length() > 0) {
            Result r =
                clusterInfo.waitForPlan(resultsSlice[0].getNumber<uint64_t>())
                    .get();
            if (r.fail()) {
              return r;
            }
          }
        }

        if (!collectionWatcher->isPresent()) {
          return Result(TRI_ERROR_ARANGO_INDEX_CREATION_FAILED,
                        "Collection " + collectionID +
                            " has gone from database " + databaseName +
                            ". Aborting index creation");
        }

        {
          // Copy over all elements in slice.
          VPackObjectBuilder b(&resultBuilder);
          resultBuilder.add(VPackObjectIterator(finishedPlanIndex.slice()));
          resultBuilder.add("isNewlyCreated", VPackValue(true));
        }
        std::lock_guard locker{agencyCallback->_cv.mutex};

        return Result(*tmpRes, *errMsg);
      }

      if ((tmpRes.has_value() && *tmpRes != TRI_ERROR_NO_ERROR) ||
          TRI_microtime() > endTime) {
        // At this time the index creation has failed and we want to
        // roll back the plan entry, provided the collection still exists:
        AgencyWriteTransaction trx(
            std::vector<AgencyOperation>(
                {AgencyOperation(planIndexesKey,
                                 AgencyValueOperationType::ERASE,
                                 newIndexBuilder.slice()),
                 AgencyOperation("Plan/Version",
                                 AgencySimpleOperationType::INCREMENT_OP)}),
            AgencyPrecondition(planCollKey, AgencyPrecondition::Type::EMPTY,
                               false));

        int sleepFor = 50;
        auto rollbackEndTime = steady_clock::now() + std::chrono::seconds(10);

        while (true) {
          AgencyCommResult update = ac.sendTransactionWithFailover(trx, 0.0);

          if (update.successful()) {
            if (VPackSlice updateSlice = update.slice().get("results");
                updateSlice.length() > 0) {
              Result r =
                  clusterInfo.waitForPlan(updateSlice[0].getNumber<uint64_t>())
                      .get();
              if (r.fail()) {
                return r;
              }
            }

            if (!tmpRes.has_value()) {  // timeout
              return Result(
                  TRI_ERROR_CLUSTER_TIMEOUT,
                  "Index could not be created within timeout, giving up and "
                  "rolling back index creation.");
            }

            // The mutex in the condition variable protects the access to
            // *errMsg:
            std::lock_guard locker{agencyCallback->_cv.mutex};
            return Result(*tmpRes, *errMsg);
          }

          if (update._statusCode == rest::ResponseCode::PRECONDITION_FAILED) {
            // Collection was removed, let's break here and report outside
            break;
          }

          if (steady_clock::now() > rollbackEndTime) {
            LOG_TOPIC("db00b", ERR, Logger::CLUSTER)
                << "Couldn't roll back index creation of " << idString
                << ". Database: " << databaseName << ", Collection "
                << collectionID;

            if (!tmpRes.has_value()) {  // timeout
              return Result(
                  TRI_ERROR_CLUSTER_TIMEOUT,
                  "Timed out while trying to roll back index creation failure");
            }

            // The mutex in the condition variable protects the access to
            // *errMsg:
            std::lock_guard locker{agencyCallback->_cv.mutex};
            return Result(*tmpRes, *errMsg);
          }

          if (sleepFor <= 2500) {
            sleepFor *= 2;
          }

          std::this_thread::sleep_for(std::chrono::milliseconds(sleepFor));
        }
        // We only get here if the collection was dropped just in the moment
        // when we wanted to roll back the index creation.
      }

      if (!collectionWatcher->isPresent()) {
        return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                      "collection " + collectionID +
                          "appears to have been dropped from database " +
                          databaseName + " during ensureIndex");
      }

      {
        std::lock_guard locker{agencyCallback->_cv.mutex};
        agencyCallback->executeByCallbackOrTimeout(interval);
      }
    }
  }

  return Result(TRI_ERROR_SHUTTING_DOWN);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
/// @brief ensure an index in coordinator.
////////////////////////////////////////////////////////////////////////////////
Result ClusterIndexMethods::ensureIndexCoordinator(
    LogicalCollection const& collection, VPackSlice slice, bool create,
    VPackBuilder& resultBuilder, double timeout) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  // check index id
  IndexId iid = IndexId::none();
  VPackSlice const idSlice = slice.get(StaticStrings::IndexId);

  if (idSlice.isString()) {  // use predefined index id
    iid = IndexId{arangodb::basics::StringUtils::uint64(idSlice.copyString())};
  }

  auto& server = collection.vocbase().server();

  if (iid.empty()) {  // no id set, create a new one!
    auto& clusterInfo = server.getFeature<ClusterFeature>().clusterInfo();
    iid = IndexId{clusterInfo.uniqid()};
  }

  std::string const idString = arangodb::basics::StringUtils::itoa(iid.id());

  VPackSlice const typeSlice = slice.get(StaticStrings::IndexType);
  if (!typeSlice.isString() ||
      (typeSlice.isEqualString("geo1") || typeSlice.isEqualString("geo2"))) {
    // geo1 and geo2 are disallowed here. Only "geo" should be used
    return Result(TRI_ERROR_BAD_PARAMETER, "invalid index type");
  }

  Result res;

  try {
    auto start = std::chrono::steady_clock::now();

    // Keep trying for 2 minutes, if it's preconditions, which are stopping us
    do {
      resultBuilder.clear();
      res = ::ensureIndexCoordinatorInner(  // create index
          collection, idString, slice, create, resultBuilder, timeout, server);

      // Note that this function sets the errorMsg unless it is precondition
      // failed, in which case we retry, if this times out, we need to set
      // it ourselves, otherwise all is done!
      if (res.is(TRI_ERROR_HTTP_PRECONDITION_FAILED)) {
        auto diff = std::chrono::steady_clock::now() - start;

        if (diff < std::chrono::seconds(120)) {
          uint32_t wt = RandomGenerator::interval(static_cast<uint32_t>(1000));
          std::this_thread::sleep_for(std::chrono::steady_clock::duration(wt));
          continue;
        }

        res = Result(                                          // result
            TRI_ERROR_CLUSTER_COULD_NOT_CREATE_INDEX_IN_PLAN,  // code
            res.errorMessage()                                 // message
        );
      }

      break;
    } while (true);
  } catch (basics::Exception const& ex) {
    res = Result(ex.code(),
                 basics::StringUtils::concatT(TRI_errno_string(ex.code()),
                                              ", exception: ", ex.what()));
  } catch (...) {
    res = Result(TRI_ERROR_INTERNAL);
  }

  // We get here in any case eventually, regardless of whether we have
  //   - succeeded with lookup or index creation
  //   - failed because of a timeout and rollback
  //   - some other error
  // There is nothing more to do here.

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drop an index in coordinator.
////////////////////////////////////////////////////////////////////////////////

auto ClusterIndexMethods::dropIndexCoordinator(LogicalCollection const& col,
                                               IndexId iid, double timeout)
    -> Result {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  double const endTime = TRI_microtime() + getTimeout(timeout);
  std::string const idString = arangodb::basics::StringUtils::itoa(iid.id());
  auto const& vocbase = col.vocbase();
  auto collectionID = std::to_string(col.id().id());
  auto databaseName = vocbase.name();
  auto& server = vocbase.server();
  AgencyComm ac{server};

  Result res(TRI_ERROR_CLUSTER_TIMEOUT);
  do {
    res = ::dropIndexCoordinatorInner(col, iid, endTime, ac);

    if (res.ok()) {
      // success!
      break;
    }

    // check if we got a precondition failed error
    if (!res.is(TRI_ERROR_HTTP_PRECONDITION_FAILED)) {
      // no, different error. report it!
      break;
    }
    // precondition failed

    // apply a random wait time
    uint32_t wt = RandomGenerator::interval(static_cast<uint32_t>(1000));
    std::this_thread::sleep_for(std::chrono::steady_clock::duration(wt));
  } while (TRI_microtime() < endTime);

  events::DropIndex(databaseName, collectionID, idString, res.errorNumber());
  return res;
}
