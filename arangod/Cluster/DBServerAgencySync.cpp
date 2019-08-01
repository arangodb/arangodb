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

#include "Basics/ScopeGuard.h"
#include "Basics/StringUtils.h"
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
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Databases.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::methods;
using namespace arangodb::rest;

DBServerAgencySync::DBServerAgencySync(HeartbeatThread* heartbeat)
    : _heartbeat(heartbeat) {}

void DBServerAgencySync::work() {
  LOG_TOPIC("57898", TRACE, Logger::CLUSTER) << "starting plan update handler";

  _heartbeat->setReady();

  DBServerAgencySyncResult result = execute();
  _heartbeat->dispatchedJobResult(result);
}

Result DBServerAgencySync::getLocalCollections(VPackBuilder& collections) {
  TRI_ASSERT(ServerState::instance()->isDBServer());

  using namespace arangodb::basics;
  Result result;
  DatabaseFeature* dbfeature = nullptr;

  try {
    dbfeature = ApplicationServer::getFeature<DatabaseFeature>("Database");
  } catch (...) {
  }

  if (dbfeature == nullptr) {
    LOG_TOPIC("d0ef2", ERR, Logger::HEARTBEAT)
        << "Failed to get feature database";
    return Result(TRI_ERROR_INTERNAL, "Failed to get feature database");
  }

  VPackObjectBuilder c(&collections);

  dbfeature->enumerateDatabases([&](TRI_vocbase_t& vocbase) {
    if (!vocbase.use()) {
      return;
    }
    auto unuse = scopeGuard([&vocbase] { vocbase.release(); });

    collections.add(VPackValue(vocbase.name()));

    VPackObjectBuilder db(&collections);
    auto cols = vocbase.collections(false);

    for (auto const& collection : cols) {
      if (!collection->system()) {
        std::string const colname = collection->name();

        collections.add(VPackValue(colname));

        VPackObjectBuilder col(&collections);

        // generate a collection definition identical to that which would be
        // persisted in the case of SingleServer
        collection->properties(collections,
                               LogicalDataSource::makeFlags(LogicalDataSource::Serialize::Detailed,
                                                            LogicalDataSource::Serialize::ForPersistence));

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
          folls->injectFollowerInfo(collections);
        }
      }
    }
  });

  return Result();
}

DBServerAgencySyncResult DBServerAgencySync::execute() {
  // default to system database

  TRI_ASSERT(AgencyCommManager::isEnabled());
  AgencyComm comm;

  using namespace std::chrono;
  using clock = std::chrono::steady_clock;

  LOG_TOPIC("62fd8", DEBUG, Logger::MAINTENANCE)
      << "DBServerAgencySync::execute starting";
  DBServerAgencySyncResult result;
  auto* sysDbFeature =
      application_features::ApplicationServer::lookupFeature<SystemDatabaseFeature>();
  MaintenanceFeature* mfeature =
      ApplicationServer::getFeature<MaintenanceFeature>("Maintenance");
  if (mfeature == nullptr) {
    LOG_TOPIC("3a1f7", ERR, Logger::MAINTENANCE)
        << "Could not load maintenance feature, can happen during shutdown.";
    result.success = false;
    result.errorMessage = "Could not load maintenance feature";
    return result;
  }
  arangodb::SystemDatabaseFeature::ptr vocbase =
      sysDbFeature ? sysDbFeature->use() : nullptr;

  if (vocbase == nullptr) {
    LOG_TOPIC("18d67", DEBUG, Logger::MAINTENANCE)
        << "DBServerAgencySync::execute no vocbase";
    result.errorMessage = "DBServerAgencySync::execute no vocbase";
    return result;
  }

  Result tmp;
  VPackBuilder rb;
  auto clusterInfo = ClusterInfo::instance();
  auto plan = clusterInfo->getPlan();
  auto serverId = arangodb::ServerState::instance()->getId();

  if (plan == nullptr) {
    // TODO increase log level, except during shutdown?
    LOG_TOPIC("0a6f2", DEBUG, Logger::MAINTENANCE)
        << "DBServerAgencySync::execute no plan";
    result.errorMessage = "DBServerAgencySync::execute no plan";
    return result;
  }

  VPackBuilder local;
  Result glc = getLocalCollections(local);
  if (!glc.ok()) {
    result.errorMessage = "Could not do getLocalCollections for phase 1: '";
    result.errorMessage.append(glc.errorMessage()).append("'");
    return result;
  }

  auto start = clock::now();
  try {
    // in previous life handlePlanChange

    VPackObjectBuilder o(&rb);

    auto startTimePhaseOne = std::chrono::steady_clock::now();
    LOG_TOPIC("19aaf", DEBUG, Logger::MAINTENANCE)
        << "DBServerAgencySync::phaseOne";
    tmp = arangodb::maintenance::phaseOne(plan->slice(), local.slice(),
                                          serverId, *mfeature, rb);
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

    auto current = clusterInfo->getCurrent();
    if (current == nullptr) {
      // TODO increase log level, except during shutdown?
      LOG_TOPIC("ab562", DEBUG, Logger::MAINTENANCE)
          << "DBServerAgencySync::execute no current";
      result.errorMessage = "DBServerAgencySync::execute no current";
      return result;
    }
    LOG_TOPIC("675fd", TRACE, Logger::MAINTENANCE)
        << "DBServerAgencySync::phaseTwo - current state: " << current->toJson();

    mfeature->increaseCurrentCounter();

    local.clear();
    glc = getLocalCollections(local);
    // We intentionally refetch local collections here, such that phase 2
    // can already see potential changes introduced by phase 1. The two
    // phases are sufficiently independent that this is OK.
    LOG_TOPIC("d15b5", TRACE, Logger::MAINTENANCE)
        << "DBServerAgencySync::phaseTwo - local state: " << local.toJson();
    if (!glc.ok()) {
      result.errorMessage = "Could not do getLocalCollections for phase 2: '";
      result.errorMessage.append(glc.errorMessage()).append("'");
      return result;
    }

    LOG_TOPIC("652ff", DEBUG, Logger::MAINTENANCE)
        << "DBServerAgencySync::phaseTwo";

    tmp = arangodb::maintenance::phaseTwo(plan->slice(), current->slice(),
                                          local.slice(), serverId, *mfeature, rb);

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
      if (report.hasKey(path) && report.get(path).isObject()) {
        auto agency = report.get(path);
        LOG_TOPIC("9c099", DEBUG, Logger::MAINTENANCE)
            << "DBServerAgencySync reporting to Current: " << agency.toJson();

        // Report to current
        if (!agency.isEmptyObject()) {
          std::vector<AgencyOperation> operations;
          std::vector<AgencyPrecondition> preconditions;
          for (auto const& ao : VPackObjectIterator(agency)) {
            auto const key = ao.key.copyString();
            auto const op = ao.value.get("op").copyString();

            if (ao.value.hasKey("precondition")) {
              auto const precondition = ao.value.get("precondition");
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
          } else {
            LOG_TOPIC("9b0b3", DEBUG, Logger::MAINTENANCE)
                << "Invalidating current in ClusterInfo";
            clusterInfo->invalidateCurrent();
          }
        }
      }

      if (tmp.ok()) {
        result = DBServerAgencySyncResult(
            true,
            report.hasKey("Plan") ? report.get("Plan").get("Version").getNumber<uint64_t>() : 0,
            report.hasKey("Current")
                ? report.get("Current").get("Version").getNumber<uint64_t>()
                : 0);
      } else {
        // Report an error:
        result = DBServerAgencySyncResult(false, "Error in phase 2: " + tmp.errorMessage(),
                                          0, 0);
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

  auto took = duration<double>(clock::now() - start).count();
  if (took > 30.0) {
    LOG_TOPIC("83cb8", WARN, Logger::MAINTENANCE)
        << "DBServerAgencySync::execute "
           "took "
        << took << " s to execute handlePlanChange";
  }

  return result;
}
