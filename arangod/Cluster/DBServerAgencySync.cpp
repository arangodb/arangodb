////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/MutexLocker.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/FollowerInfo.h"
#include "Cluster/HeartbeatThread.h"
#include "Cluster/Maintenance.h"
#include "Cluster/MaintenanceFeature.h"
#include "Cluster/MaintenanceStrings.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/vocbase.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Databases.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::methods;
using namespace arangodb::rest;

DBServerAgencySync::DBServerAgencySync(HeartbeatThread* heartbeat)
    : _heartbeat(heartbeat) {}

void DBServerAgencySync::work() {
  LOG_TOPIC(TRACE, Logger::CLUSTER) << "starting plan update handler";

  _heartbeat->setReady();

  DBServerAgencySyncResult result = execute();
  _heartbeat->dispatchedJobResult(result);
}

Result DBServerAgencySync::getLocalCollections(VPackBuilder& collections) {

  using namespace arangodb::basics;
  Result result;
  DatabaseFeature* dbfeature = nullptr;

  try {
    dbfeature = ApplicationServer::getFeature<DatabaseFeature>("Database");
  } catch (...) {}

  if (dbfeature == nullptr) {
    LOG_TOPIC(ERR, Logger::HEARTBEAT) << "Failed to get feature database";
    return Result(TRI_ERROR_INTERNAL, "Failed to get feature database");
  }

  VPackObjectBuilder c(&collections);

  for (auto const& database : Databases::list()) {

    try {
      DatabaseGuard guard(database);
      auto vocbase = &guard.database();

      collections.add(VPackValue(database));

      VPackObjectBuilder db(&collections);
      auto cols = vocbase->collections(false);

      for (auto const& collection : cols) {
        if (!collection->system()) {
          std::string const colname = collection->name();

          collections.add(VPackValue(colname));
          VPackObjectBuilder col(&collections);
          collection->properties(collections,true,false);

          auto const& folls = collection->followers();
          std::string const theLeader = folls->getLeader();
          bool theLeaderTouched = folls->getLeaderTouched();

          // Note that whenever theLeader was set explicitly since the collection
          // object was created, we believe it. Otherwise, we do not accept
          // that we are the leader. This is to circumvent the problem that
          // after a restart we would implicitly be assumed to be the leader.
          collections.add("theLeader", VPackValue(theLeaderTouched ? theLeader : "NOT_YET_TOUCHED"));
          collections.add("theLeaderTouched", VPackValue(theLeaderTouched));

          if (theLeader.empty() && theLeaderTouched) {
            // we are the leader ourselves
            // In this case we report our in-sync followers here in the format
            // of the agency: [ leader, follower1, follower2, ... ]
            collections.add(VPackValue("servers"));

            { VPackArrayBuilder guard(&collections);

              collections.add(VPackValue(arangodb::ServerState::instance()->getId()));
              std::shared_ptr<std::vector<ServerID> const> srvs = folls->get();
              for (auto const& s : *srvs) {
                collections.add(VPackValue(s));
              }
            }
          }
        }
      }
    } catch (std::exception const& e) {
      return Result(
        TRI_ERROR_INTERNAL,
        std::string("Failed to guard database ") +  database + ": " + e.what());
    }
  }

  return Result();
}

DBServerAgencySyncResult DBServerAgencySync::execute() {
  // default to system database

  TRI_ASSERT(AgencyCommManager::isEnabled());
  AgencyComm comm;

  using namespace std::chrono;
  using clock = std::chrono::steady_clock;

  LOG_TOPIC(DEBUG, Logger::MAINTENANCE) << "DBServerAgencySync::execute starting";

  auto* sysDbFeature = application_features::ApplicationServer::lookupFeature<
    SystemDatabaseFeature
  >();
  MaintenanceFeature* mfeature =
    ApplicationServer::getFeature<MaintenanceFeature>("Maintenance");
  arangodb::SystemDatabaseFeature::ptr vocbase =
      sysDbFeature ? sysDbFeature->use() : nullptr;
  DBServerAgencySyncResult result;

  if (vocbase == nullptr) {
    LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
      << "DBServerAgencySync::execute no vocbase";
    result.errorMessage = "DBServerAgencySync::execute no vocbase";
    return result;
  }

  Result tmp;
  VPackBuilder rb;
  auto clusterInfo = ClusterInfo::instance();
  auto plan = clusterInfo->getPlan();
  auto serverId = arangodb::ServerState::instance()->getId();

  VPackBuilder local;
  Result glc = getLocalCollections(local);
  if (!glc.ok()) {
    // FIXMEMAINTENANCE: if this fails here, then result is empty, is this
    // intended? I also notice that there is another Result object "tmp"
    // that is going to eat bad results in few lines later. Again, is
    // that the correct action? If so, how about supporting comments in
    // the code for both.
    result.errorMessage = "Could not do getLocalCollections for phase 1.";
    return result;
  }

  auto start = clock::now();
  try {
    // in previous life handlePlanChange

    VPackObjectBuilder o(&rb);

    LOG_TOPIC(DEBUG, Logger::MAINTENANCE) << "DBServerAgencySync::phaseOne";
    tmp = arangodb::maintenance::phaseOne(
      plan->slice(), local.slice(), serverId, *mfeature, rb);
    LOG_TOPIC(DEBUG, Logger::MAINTENANCE) << "DBServerAgencySync::phaseOne done";

    LOG_TOPIC(DEBUG, Logger::MAINTENANCE) << "DBServerAgencySync::phaseTwo";
    local.clear();
    glc = getLocalCollections(local);
    // We intentionally refetch local collections here, such that phase 2
    // can already see potential changes introduced by phase 1. The two
    // phases are sufficiently independent that this is OK.
    LOG_TOPIC(TRACE, Logger::MAINTENANCE) << "DBServerAgencySync::phaseTwo - local state: " << local.toJson();
    if (!glc.ok()) {
      result.errorMessage = "Could not do getLocalCollections for phase 2.";
      return result;
    }

    auto current = clusterInfo->getCurrent();
    LOG_TOPIC(TRACE, Logger::MAINTENANCE) << "DBServerAgencySync::phaseTwo - current state: " << current->toJson();

    tmp = arangodb::maintenance::phaseTwo(
      plan->slice(), current->slice(), local.slice(), serverId, *mfeature, rb);

    LOG_TOPIC(DEBUG, Logger::MAINTENANCE) << "DBServerAgencySync::phaseTwo done";

  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, Logger::MAINTENANCE)
      << "Failed to handle plan change: " << e.what();
  }

  if (rb.isClosed()) {

    auto report = rb.slice();
    if (report.isObject()) {

      std::vector<std::string> path = {maintenance::PHASE_TWO, "agency"};
      if (report.hasKey(path) && report.get(path).isObject()) {

        auto agency = report.get(path);
        LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
          << "DBServerAgencySync reporting to Current: " << agency.toJson();

        // Report to current
        if (!agency.isEmptyObject()) {

          std::vector<AgencyOperation> operations;
          for (auto const& ao : VPackObjectIterator(agency)) {
            auto const key = ao.key.copyString();
            auto const op = ao.value.get("op").copyString();
            if (op == "set") {
              auto const value = ao.value.get("payload");
              operations.push_back(
                AgencyOperation(key, AgencyValueOperationType::SET, value));
            } else if (op == "delete") {
              operations.push_back(
                AgencyOperation(key, AgencySimpleOperationType::DELETE_OP));
            }
          }
          operations.push_back(
            AgencyOperation(
              "Current/Version", AgencySimpleOperationType::INCREMENT_OP));
          AgencyWriteTransaction currentTransaction(operations);
          AgencyCommResult r = comm.sendTransactionWithFailover(currentTransaction);
          if (!r.successful()) {
            LOG_TOPIC(ERR, Logger::MAINTENANCE) << "Error reporting to agency";
          } else {
            LOG_TOPIC(DEBUG, Logger::MAINTENANCE) << "Invalidating current in ClusterInfo";
            clusterInfo->invalidateCurrent();
          }
        }

      }

      if (tmp.ok()) {
        result = DBServerAgencySyncResult(
          true,
          report.hasKey("Plan") ?
          report.get("Plan").get("Version").getNumber<uint64_t>() : 0,
          report.hasKey("Current") ?
          report.get("Current").get("Version").getNumber<uint64_t>() : 0);
      } else {
        // Report an error:
        result = DBServerAgencySyncResult(
          false, "Error in phase 2: " + tmp.errorMessage(), 0, 0);
      }
    }
  } else {
    result.errorMessage = "Report from phase 1 and 2 was not closed.";
  }

  auto took = duration<double>(clock::now() - start).count();
  if (took > 30.0) {
    LOG_TOPIC(WARN, Logger::MAINTENANCE) << "DBServerAgencySync::execute "
      "took " << took << " s to execute handlePlanChange";
  }

  return result;
}
