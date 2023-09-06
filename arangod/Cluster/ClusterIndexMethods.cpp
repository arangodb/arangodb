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

Result dropIndexCoordinatorInner(LogicalCollection const& col, IndexId iid,
                                 double endTime, AgencyComm& agencyComm) {
  std::string const idString = arangodb::basics::StringUtils::itoa(iid.id());
  double const interval = 5.0;
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
        LOG_DEVEL << "Got callback on " << idString;
        if (numberOfShards == 0) {
          LOG_DEVEL << "No Shards";
          return false;
        }

        if (!current.isObject()) {
          LOG_DEVEL << "No Object";
          return true;
        }

        VPackObjectIterator shards(current);
        LOG_DEVEL << current.toJson() << " and we expect shards "
                  << numberOfShards;

        if (shards.size() == numberOfShards) {
          LOG_DEVEL << "Lets check";
          bool found = false;
          for (auto const& shard : shards) {
            VPackSlice const indexes = shard.value.get("indexes");

            if (indexes.isArray()) {
              for (VPackSlice v : VPackArrayIterator(indexes)) {
                if (v.isObject()) {
                  VPackSlice const k = v.get(StaticStrings::IndexId);
                  if (k.isString() && k.isEqualString(idString)) {
                    // still found the index in some shard
                    LOG_DEVEL << "Found the index in "
                              << shard.key.copyString();
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
            LOG_DEVEL << "We are good";
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
  Result r = callbackRegistry.registerCallback(agencyCallback);
  if (r.fail()) {
    return r;
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
}  // namespace

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