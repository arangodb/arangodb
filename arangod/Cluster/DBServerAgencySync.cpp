////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
#include "Cluster/ClusterInfo.h"
#include "Cluster/FollowerInfo.h"
#include "Cluster/HeartbeatThread.h"
#include "Cluster/Maintenance.h"
#include "Cluster/MaintenanceFeature.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "RestServer/DatabaseFeature.h"
#include "Utils/DatabaseGuard.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/V8Context.h"
#include "V8Server/V8DealerFeature.h"
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

Result getLocalCollections(VPackBuilder& collections) {

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
    auto vocbase = dbfeature->lookupDatabase(database);
    if (vocbase == nullptr) {
      continue;
    }

    try {
      DatabaseGuard guard(*vocbase);      
    } catch (std::exception const& e) {
      return Result(
        TRI_ERROR_INTERNAL,
        std::string("Failed to guard database ") +  database + ": " + e.what());
    }
    
    collections.add(VPackValue(database));
    VPackObjectBuilder db(&collections);
    auto cols = vocbase->collections(false);

    std::sort(cols.begin(), cols.end(),
              [](LogicalCollection* lhs, LogicalCollection* rhs) -> bool {
                return StringUtils::tolower(lhs->name()) <
                StringUtils::tolower(rhs->name());
              });
    
    for (auto const& collection : cols) {
      collections.add(VPackValue(collection->name()));
      VPackObjectBuilder col(&collections);
      collection->toVelocyPack(collections,true,false);
      collections.add(
        "theLeader", VPackValue(collection->followers()->getLeader()));
      
    }    
    
  }

  return Result();
  
}

DBServerAgencySyncResult DBServerAgencySync::execute() {
  // default to system database

  using namespace std::chrono;
  using clock = std::chrono::steady_clock;
  
  LOG_TOPIC(DEBUG, Logger::HEARTBEAT) << "DBServerAgencySync::execute starting";
  DatabaseFeature* dbfeature = 
    ApplicationServer::getFeature<DatabaseFeature>("Database");
  MaintenanceFeature* mfeature = 
    ApplicationServer::getFeature<MaintenanceFeature>("Maintenance");
  TRI_vocbase_t* const vocbase = dbfeature->systemDatabase();
  
  DBServerAgencySyncResult result;

  if (vocbase == nullptr) {
    LOG_TOPIC(DEBUG, Logger::HEARTBEAT)
      << "DBServerAgencySync::execute no vocbase";
    return result;
  }
  
  Result tmp;
  VPackBuilder rb;
  auto clusterInfo = ClusterInfo::instance();
  auto plan = clusterInfo->getPlan();
  auto current = clusterInfo->getCurrent();
  auto serverId = arangodb::ServerState::instance()->getId();

  DatabaseGuard guard(*vocbase);
  VPackBuilder local;
  Result glc = getLocalCollections(local);
#warning can fail or be empty

  auto start = clock::now();
  try {
    // in previous life handlePlanChange
    tmp = arangodb::maintenance::handleChange(
      plan->slice(), current->slice(), local.slice(), serverId, *mfeature, rb);
  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, Logger::HEARTBEAT)
      << "Failed to handle plan change: " << e.what();
  }
  auto report = rb.slice();
  
  result = DBServerAgencySyncResult(
    tmp.ok(),
    report.get("Plan").get("Version").getNumber<uint64_t>(),
    report.get("Current").get("Version").getNumber<uint64_t>());
  
  auto took = duration<double>(clock::now()-start).count();
  if (took > 30.0) {
    LOG_TOPIC(WARN, Logger::HEARTBEAT) << "DBServerAgencySync::execute "
      "took " << took << " s to execute handlePlanChange";
  }

  return result;
}
