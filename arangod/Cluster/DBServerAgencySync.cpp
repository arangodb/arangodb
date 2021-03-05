////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "DBServerAgencySync.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StringUtils.h"
#include "Basics/application-exit.h"
#include "Cluster/AgencyCache.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/FollowerInfo.h"
#include "Cluster/HeartbeatThread.h"
#include "Cluster/Maintenance.h"
#include "Cluster/MaintenanceFeature.h"
#include "Cluster/MaintenanceStrings.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Databases.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::methods;
using namespace arangodb::rest;

DBServerAgencySync::DBServerAgencySync(ApplicationServer& server, HeartbeatThread* heartbeat)
    : _server(server), _heartbeat(heartbeat) {}

void DBServerAgencySync::work() {
  LOG_TOPIC("57898", TRACE, Logger::CLUSTER) << "starting plan update handler";

  _heartbeat->setReady();

  DBServerAgencySyncResult result = execute();
  _heartbeat->dispatchedJobResult(result);
}

Result DBServerAgencySync::getLocalCollections(
  std::unordered_set<std::string> const& dirty,
  AgencyCache::databases_t& databases) {

  TRI_ASSERT(ServerState::instance()->isDBServer());

  using namespace arangodb::basics;

  if (!_server.hasFeature<DatabaseFeature>()) {
    LOG_TOPIC("d0ef2", ERR, Logger::HEARTBEAT)
        << "Failed to get feature database";
    return Result(TRI_ERROR_INTERNAL, "Failed to get feature database");
  }
  DatabaseFeature& dbfeature = _server.getFeature<DatabaseFeature>();

  for (auto const& dbname : dirty) {
    TRI_vocbase_t* tmp = dbfeature.useDatabase(dbname);
    if (tmp == nullptr) {
      continue;
    }
    TRI_vocbase_t& vocbase = *tmp;
    auto unuse = scopeGuard([&vocbase] { vocbase.release(); });

    auto [it, created] =
      databases.try_emplace(dbname, std::make_shared<VPackBuilder>());
    if (!created) {
      LOG_TOPIC("0e9d7", ERR, Logger::MAINTENANCE)
        << "Failed to emplace new entry in local collection cache";
      return Result(TRI_ERROR_INTERNAL, "Failed to emplace new entry in local collection cache");
    }

    auto& collections = *it->second;
    VPackObjectBuilder db(&collections);
    auto cols = vocbase.collections(false);

    for (auto const& collection : cols) {
      // note: system collections are ignored here, but the local parts of
      // smart edge collections are system collections, too. these are
      // included.
      if (!collection->system() || collection->isSmartChild()) {
        std::string const colname = collection->name();

        collections.add(VPackValue(colname));

        VPackObjectBuilder col(&collections);

        // generate a collection definition identical to that which would be
        // persisted in the case of SingleServer
        collection->properties(collections, LogicalDataSource::Serialization::Persistence);

        auto const& folls = collection->followers();
        std::string const theLeader = folls->getLeader();
        bool theLeaderTouched = folls->getLeaderTouched();

        // Note that whenever theLeader was set explicitly since the collection
        // object was created, we believe it. Otherwise, we do not accept
        // that we are the leader. This is to circumvent the problem that
        // after a restart we would implicitly be assumed to be the leader.
        collections.add("theLeader", VPackValue(theLeaderTouched ? theLeader : maintenance::LEADER_NOT_YET_KNOWN));
        collections.add("theLeaderTouched", VPackValue(theLeaderTouched));

        if (theLeader.empty() && theLeaderTouched) {
          // we are the leader ourselves
          // In this case we report our in-sync followers here in the format
          // of the agency: [ leader, follower1, follower2, ... ]
          folls->injectFollowerInfo(collections);
        }
      }
    }
  }

  return Result();
}

std::ostream& operator<<(std::ostream& o, std::shared_ptr<VPackBuilder> const& v) {
  o << v->toJson();
  return o;
}

DBServerAgencySyncResult DBServerAgencySync::execute() {
  // default to system database
  using namespace std::chrono;
  using clock = std::chrono::steady_clock;
  auto start = clock::now();

  AgencyComm comm(_server);

  LOG_TOPIC("62fd8", DEBUG, Logger::MAINTENANCE)
      << "DBServerAgencySync::execute starting";
  DBServerAgencySyncResult result;
  if (!_server.hasFeature<MaintenanceFeature>()) {
    LOG_TOPIC("3a1f7", ERR, Logger::MAINTENANCE)
        << "Could not load maintenance feature, can happen during shutdown.";
    result.success = false;
    result.errorMessage = "Could not load maintenance feature";
    return result;
  }
  MaintenanceFeature& mfeature = _server.getFeature<MaintenanceFeature>();
  arangodb::SystemDatabaseFeature::ptr vocbase =
      _server.hasFeature<SystemDatabaseFeature>()
          ? _server.getFeature<SystemDatabaseFeature>().use()
          : nullptr;

  if (vocbase == nullptr) {
    LOG_TOPIC("18d67", DEBUG, Logger::MAINTENANCE)
        << "DBServerAgencySync::execute no vocbase";
    result.errorMessage = "DBServerAgencySync::execute no vocbase";
    return result;
  }

  auto& clusterInfo = _server.getFeature<ClusterFeature>().clusterInfo();
  uint64_t planIndex = 0, currentIndex = 0;


  // The rationale is that even with 1000 databases and us waking up every 5 seconds,
  // we should approximately visit every database without being dirty once per hour.
  // That is why we divide by 720.
  auto moreDirt = mfeature.pickRandomDirty(
    static_cast<size_t>(std::ceil(static_cast<double>(mfeature.lastNumberOfDatabases()) / 720.)));
  auto dirty = mfeature.dirty();
  // Add `moreDirt` to `dirty` but remove from `moreDirt`, if already dirty anyway.
  // Then we can reasonably be surprised if we find anything in a database in `moreDirt`.
  for (auto it = moreDirt.begin(); it != moreDirt.end();) {
    if (dirty.find(*it) == dirty.end()) {
      dirty.insert(*it);
      it++;
    } else {
      it = moreDirt.erase(it);
    }
  }

  if (dirty.empty()) {
    LOG_TOPIC("0a62f", DEBUG, Logger::MAINTENANCE)
      << "DBServerAgencySync::execute no dirty collections";
    result.success = true;
    result.errorMessage = "DBServerAgencySync::execute no dirty databases";
    return result;
  }

  AgencyCache::databases_t plan = clusterInfo.getPlan(planIndex, dirty);

  auto serverId = arangodb::ServerState::instance()->getId();

  // It is crucial that the following happens before we do `getLocalCollections`!
  MaintenanceFeature::ShardActionMap currentShardLocks = mfeature.getShardLocks();

  AgencyCache::databases_t local;
  LOG_TOPIC("54261", TRACE, Logger::MAINTENANCE) << "Before getLocalCollections for phaseOne";
  Result glc = getLocalCollections(dirty, local);

  LOG_TOPIC("54262", TRACE, Logger::MAINTENANCE) << "After getLocalCollections for phaseOne";
  if (!glc.ok()) {
    result.errorMessage =
        StringUtils::concatT("Could not do getLocalCollections for phase 1: '",
                             glc.errorMessage(), "'");
    return result;
  }
  LOG_TOPIC("54263", TRACE, Logger::MAINTENANCE) << "local for phaseOne: " << local;

  VPackBuilder rb;
  Result tmp;
  try {
    // in previous life handlePlanChange

    VPackObjectBuilder o(&rb);

    auto startTimePhaseOne = std::chrono::steady_clock::now();
    LOG_TOPIC("19aaf", DEBUG, Logger::MAINTENANCE)
        << "DBServerAgencySync::phaseOne";

    tmp = arangodb::maintenance::phaseOne(
      plan, planIndex, dirty, moreDirt, local, serverId, mfeature, rb, currentShardLocks);

    auto endTimePhaseOne = std::chrono::steady_clock::now();
    LOG_TOPIC("93f83", DEBUG, Logger::MAINTENANCE)
        << "DBServerAgencySync::phaseOne done";

    if (endTimePhaseOne - startTimePhaseOne > std::chrono::milliseconds(200)) {
      // We take this as indication that many shards are in the system,
      // in this case: give some asynchronous jobs created in phaseOne a
      // chance to complete before we collect data for phaseTwo:
      LOG_TOPIC("ef730", DEBUG, Logger::MAINTENANCE)
          << "DBServerAgencySync::hesitating between phases 1 and 2 for "
             "0.1s...";
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    AgencyCache::databases_t current = clusterInfo.getCurrent(currentIndex, dirty);

    LOG_TOPIC("675fd", TRACE, Logger::MAINTENANCE)
        << "DBServerAgencySync::phaseTwo - current state: " << current;

    // It is crucial that the following happens before we do `getLocalCollections`!
    // We must lock a shard if an action for the shard is scheduled. We then unlock
    // the shard when this action has terminated. This unlock makes the database
    // dirty again and triggers a run of the maintenance thread. We want the outcome
    // of the completed action to be visible in what `getLocalCollections` sees, when
    // the dirtiness of the database is consumed. Therefore, we do it in exactly this
    // order: First get a snapshot of the locks (copy!) and ignore the shards which
    // have been locked *now*. Then `getLocalCollections`.
    currentShardLocks = mfeature.getShardLocks();

    local.clear();
    glc = getLocalCollections(dirty, local);
    // We intentionally refetch local collections here, such that phase 2
    // can already see potential changes introduced by phase 1. The two
    // phases are sufficiently independent that this is OK.
    LOG_TOPIC("d15b5", TRACE, Logger::MAINTENANCE)
        << "DBServerAgencySync::phaseTwo - local state: " << local;
    if (!glc.ok()) {
      result.errorMessage = StringUtils::concatT(
          "Could not do getLocalCollections for phase 2: '", glc.errorMessage(),
          "'");
      return result;
    }

    LOG_TOPIC("652ff", DEBUG, Logger::MAINTENANCE)
        << "DBServerAgencySync::phaseTwo";

    tmp = arangodb::maintenance::phaseTwo(
      plan, current, currentIndex, dirty, local, serverId, mfeature, rb, currentShardLocks);

    LOG_TOPIC("dfc54", DEBUG, Logger::MAINTENANCE)
        << "DBServerAgencySync::phaseTwo done";

  } catch (std::exception const& e) {
    LOG_TOPIC("cd308", ERR, Logger::MAINTENANCE)
        << "Failed to handle plan change: " << e.what();
  }

  if (rb.isClosed()) {
    auto report = rb.slice();
    if (report.isObject()) {
      std::vector<std::string> path = {maintenance::PHASE_TWO, "agency"};
      auto agency = report.get(path);
      if (agency.isObject()) {
        LOG_TOPIC("9c099", DEBUG, Logger::MAINTENANCE)
            << "DBServerAgencySync reporting to Current: " << agency.toJson();

        // Report to current
        if (!agency.isEmptyObject()) {
          std::vector<AgencyOperation> operations;
          std::vector<AgencyPrecondition> preconditions;
          for (auto const& ao : VPackObjectIterator(agency)) {
            auto const key = ao.key.copyString();
            auto const op = ao.value.get("op").copyString();

            auto const precondition = ao.value.get("precondition");
            if (!precondition.isNone()) {
              // have a precondition
              preconditions.push_back(AgencyPrecondition(precondition.keyAt(0).copyString(),
                                                         AgencyPrecondition::Type::VALUE,
                                                         precondition.valueAt(0)));
            }

            if (op == "set") {
              auto const value = ao.value.get("payload");
              operations.push_back(AgencyOperation(key, AgencyValueOperationType::SET, value));
            } else if (op == "delete") {
              operations.push_back(AgencyOperation(key, AgencySimpleOperationType::DELETE_OP));
            }
          }
          operations.push_back(AgencyOperation("Current/Version",
                                               AgencySimpleOperationType::INCREMENT_OP));

          AgencyWriteTransaction currentTransaction(operations, preconditions);
          AgencyCommResult r = comm.sendTransactionWithFailover(currentTransaction);
          if (!r.successful()) {
            LOG_TOPIC("d73b8", INFO, Logger::MAINTENANCE)
                << "Error reporting to agency: _statusCode: " << r.errorCode()
                << " message: " << r.errorMessage() << ". This can be ignored, since it will be retried automatically.";
          }
        }
      }

      if (tmp.ok()) {
        VPackSlice planSlice = report.get("Plan");
        VPackSlice currentSlice = report.get("Current");
        result = DBServerAgencySyncResult(
          true, planSlice.isObject() ? planSlice.get("Index").getNumber<uint64_t>() : 0,
          currentSlice.isObject() ? currentSlice.get("Index").getNumber<uint64_t>() : 0);
      } else {
        // Report an error:
        result = DBServerAgencySyncResult(
            false, StringUtils::concatT("Error in phase 2: ", tmp.errorMessage()), 0, 0);
      }
    } else {
      // This code should never run, it is only there to debug problems if
      // we mess up in other places.
      result.errorMessage = "Report from phase 1 and 2 was no object.";
      try {
        std::string json = report.toJson();
        LOG_TOPIC("65fde", WARN, Logger::MAINTENANCE)
            << "Report from phase 1 and 2 was: " << json;
      } catch (std::exception const& exc) {
        LOG_TOPIC("54de2", WARN, Logger::MAINTENANCE)
            << "Report from phase 1 and 2 could not be dumped to JSON, error: "
            << exc.what() << ", head byte:" << report.head();
        uint64_t l = 0;
        try {
          l = report.byteSize();
          LOG_TOPIC("54dda", WARN, Logger::MAINTENANCE)
              << "Report from phase 1 and 2, byte size: " << l;
          LOG_TOPIC("67421", WARN, Logger::MAINTENANCE)
              << "Bytes: "
              << arangodb::basics::StringUtils::encodeHex((char const*)report.start(), l);
        } catch (...) {
          LOG_TOPIC("76124", WARN, Logger::MAINTENANCE)
              << "Report from phase 1 and 2, byte size throws.";
        }
      }
    }
  } else {
    result.errorMessage = "Report from phase 1 and 2 was not closed.";
  }

  auto end = clock::now();
  auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  mfeature._agency_sync_total_runtime_msec->get().count(total_ms);
  mfeature._agency_sync_total_accum_runtime_msec->get().count(total_ms);
  auto took = duration<double>(end - start).count();
  if (took > 30.0) {
    LOG_TOPIC("83cb8", WARN, Logger::MAINTENANCE)
        << "DBServerAgencySync::execute "
           "took "
        << took << " s to execute handlePlanChange";
  }

  return result;
}
