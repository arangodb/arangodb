!////////////////////////////////////////////////////////////////////////////////
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
/// @author Kaveh Vahedipour
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#include "SynchronizeShard.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/FollowerInfo.h"
#include "Cluster/MaintenanceFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Databases.h"

#include <velocypack/Compare.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>


using namespace arangodb::application_features;
using namespace arangodb::maintenance;
using namespace arangodb::methods;

constexpr auto WAIT_FOR_SYNC_REPL = "waitForSyncReplication";
constexpr auto ENF_REPL_FACT = "enforceReplicationFactor";
constexpr auto REPL_HOLD_READ_LOCK = "/_api/replication/holdReadLockCollection";

std::string const READ_LOCK_TIMEOUT("startReadLockOnLeader: giving up");
std::string const DB("/_db/");

SynchronizeShard::SynchronizeShard(ActionDescription const& d) :
  ActionBase(d, arangodb::maintenance::FOREGROUND) {
/*  TRI_ASSERT(d.has(COLLECTION));
  TRI_ASSERT(d.has(DATABASE));
  TRI_ASSERT(d.has(ID));
  TRI_ASSERT(d.has(LEADER));
  TRI_ASSERT(d.properties().hasKey(TYPE));
  TRI_ASSERT(d.properties().get(TYPE).isInteger());  */
}

arangodb::Result getReadLockid (
  std::string const& endpoint, std::string const& database, double timeout,
  uint64_t& id) {

  std::string error("startReadLockOnLeader: Failed to get read lock - ");
  
  auto comm = arangodb::ClusterComm::instance();
  if (cc == nullptr) { // nullptr only happens during controlled shutdown
    return arangodb::Result(
      TRI_ERROR_SHUTTING_DOWN, "startReadLockOnLeader: Shutting down");
  }
  
  auto comres = cc->syncRequest(
    std::hash(_description), 1, endpoint, rest::RequestType::GET,
    DB + database + REPL_HOLD_READ_LOCK, std::string(),
    std::unordered_map<std::string, std::string>(), timeout);
  
  auto result = comres->result;
  if (result != nullptr) { 
    if (result->getHttpReturnCode() == rest::ResponseCode::OK) {
      auto const idv = comres->result->getBodyVelocyPack();
      auto const& idSlice = idv->slice();
      TRI_ASSERT(idSlice.isObject());
      TRI_ASSERT(idSlice.hasKey(ID));
      try {
        id = idSlice.get(ID)->getNumber<uint64_t>();
      } catch (std::exception const& e) {
        error += " expecting id to be int64_t ";
        error += idSlice.toJson();
        return arangodb::Result(TRI_ERROR_INTERNAL, error);
      }
    } else {
      error += result->getHttpReturnMessage();
      return arangodb::Result(TRI_ERROR_INTERNAL, error);
    }
  } else {
    error += "NULL result"
      return arangodb::Result(TRI_ERROR_INTERNAL, error);
  }
  
  return arangodb::Result();
  
}


arangodb::Result getReadLock(
  std::string const& endpoint, std::string const& database,
  uint64_t rlid) {

  auto start = std::chrono::steady_clock::now();

  auto cc = arangodb::ClusterComm::instance();
  if (cc == nullptr) { // nullptr only happens during controlled shutdown
    return arangodb::Result(
      TRI_ERROR_SHUTTING_DOWN, "startReadLockOnLeader: Shutting down");
  }

  VPackBuilder b;
  { VPackObjectBuilder(&b);
    b.add(ID, VPackValue(rlid));
    b.add(COLLECTION, collection);
    b.add(TTL, elapsed);
  }

  auto hf =
    std::make_unique<std::unordered_map<std::string, std::string>>();

  auto url = DB + database + REPL_HOLD_READ_LOCK;
  
  auto postres = cc->asyncRequest(
    std::hash(_description), 2, endpoint, rest::RequestType::POST,
    url, b.toJson(), hf, std::make_shared<SynchronizeShardCallback>(this), 1.0,
    true, 0.5);
  
  // Intentionally do not look at the outcome, even in case of an error
  // we must make sure that the read lock on the leader is not active!
  // This is done automatically below.
  
  decltype(postres) putres;
  size_t count = 0;
  while (++count < 20) { // wait for some time until read lock established:
    // Now check that we hold the read lock:
    putres = cc->syncRequest(
      std::hash(_description), 1, endpoint, rest::RequestType::PUT, url,
      b.toJson(), std::unordered_map<std::string, std::string>(), timeout);
    
    auto result = putres->result; 
    if (result->getHttpReturnCode() == rest::ResponseCode::OK) {
      auto const vp = putres->result->getBodyVelocyPack();
      auto const& slice = vp->slice();
      TRI_ASSERT(slice.isObject());
      if (slice.hasKey("lockHeld") && slice.get("lockHeld").isBoolean()) {
        if (slice.get("lockHeld").getBool()) {
          return arangodb::Result();
        }
      }
      LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
        << "startReadLockOnLeader: Lock not yet acquired...";
    } else {
      LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
        << "startReadLockOnLeader: Do not see read lock yet...";
    }

    std::this_thread::sleep_for(std::chrono::duration(.5));
    if ((std::chrono::steady_clock::now() - start).count() > timeout) {
      LOG_TOPIC(ERR, Logger::MAINTENANCE) << READ_LOCK_TIMEOUT;
      return arangodb::Result(TRI_ERROR_CLUSTER_TIMEOUT, READ_LOCK_TIMEOUT);
    }
    
  }
}

bool isStopping() {
  return application_features::ApplicationServer::isStopping();
}

arangodb::Result startReadLockOnLeader(
  std::string const& endpoint, std::string const& database,
  std::string const& collection, double timeout = 120.0) {

  auto start = std::chrono::steady_clock::now();

  // Read lock id
  uint64_t rlid = 0;
  arangodb::Result result = getReadLockId(endpoint, database, rlid);
  if (!result.successful()) {
    LOG_TOPIC(ERR, Logger::MAINTENANCE) << result.errorMessage();
    return result;
  }

  result = getReadLock(endpoint, database);
  
  auto elapsed = (std::chrono::steady_clock::now() - start).count();

  return result;
  
}


arangodb::Result terminateAndStartOther() {
  return arangodb::Result;
}


arangodb::Result synchroniseOneShard(
  std::string const& database, std::string const& shard,
  std::string const& planId, std::string const& leader) {

  auto* clusterInfo = ClusterInfo::instance();
  auto const ourselves = clusterInfo
  auto const startTime = std::chrono::system_clock::now();
  
  while(true) {

    if (isStopping()) {
      terminateAndStartOther();
      return arangodb::Result();
    }

    std::vector<std::string> planned;
    auto result = clusterInfo->shards(shard, planned);
    if (!result.successful() ||
        std::find(planned.begin(), planned.end(), ourselves) != planned.end() ||
        planned.front() != leader) {
      // Things have changed again, simply terminate:
      terminateAndStartOther();
      auto const endTime = std::chrono::system_clock::now();
      LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
        << "synchronizeOneShard: cancelled, " << database << "/" << shard
        << ", " << database << "/" << planId << ", started "
        << TimepointToString(startTime) << ", ended " << TimepointToString(endTime);
      return arangodb::Result(ERROR_FAILED, "synchronizeOneShard: cancelled");
    }

    std::shared_ptr<LogicalCollection> ci =
      clusterInfo->getCollection(database, planId);
    TRI_ASSERT(ci != nullptr);

    std::string const cid = ci->cid_as_string();
    std::string const& name = ci->name();
    std::shared_ptr<CollectionInfoCurrent> cic =
      ClusterInfo::instance()->getCollectionCurrent(database, cid);
    auto const current = cic->servers(shardID);

    TRI_ASSERT(!current.empty());
    if (current.front() == leader) {
      if (std::find(current.begin(), current.end(), ourselves) == current.end()) {
        break; // start synchronization work
      }
      // We are already there, this is rather strange, but never mind:
      terminateAndStartOther();
      auto const endTime = std::chrono::system_clock::now();
      LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
        << "synchronizeOneShard: already done, " << database << "/" << shard
        << ", " << database << "/" << planId << ", started "
        << TimepointToString(startTime) << ", ended " << TimepointToString(endTime);
      return arangodb::Result(ERROR_FAILED, "synchronizeOneShard: cancelled");
    }

    std::this_thread::wait_for(std::chrono::duration(0.2));
    
  }

  // Once we get here, we know that the leader is ready for sync, so
  // we give it a try:

  bool ok = false;
  auto ep = clusterInfo->getServerEndpoint(leader);
  if (db._collection(shard).count() === 0) {
    // We try a short cut:
    console.topic('heartbeat=debug', "synchronizeOneShard: trying short cut to synchronize local shard '%s/%s' for central '%s/%s'", database, shard, database, planId);
    try {
      let ok = addShardFollower(ep, database, shard);
      if (ok) {
        terminateAndStartOther();
        let endTime = new Date();
        console.topic('heartbeat=debug', 'synchronizeOneShard: shortcut worked, done, %s/%s, %s/%s, started: %s, ended: %s',
          database, shard, database, planId, startTime.toString(), endTime.toString());
        db._collection(shard).setTheLeader(leader);
        return;
      }
    } catch (dummy) { }
  }

  
}

SynchronizeShard::~SynchronizeShard() {};

arangodb::Result SynchronizeShard::run(
  std::chrono::duration<double> const&, bool& finished) {

  arangodb::Result res;
  return res;
}

arangodb::Result SynchronizeShard::kill(Signal const& signal) {
  return actionError(
    TRI_ERROR_ACTION_OPERATION_UNABORTABLE, "Cannot kill SynchronizeShard action");
}

arangodb::Result SynchronizeShard::progress(double& progress) {
  progress = 0.5;
  return arangodb::Result(TRI_ERROR_NO_ERROR);
}


