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
/// @author Kaveh Vahedipour
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#include "SynchronizeShard.h"

#include "Agency/TimeString.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ActionDescription.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/CollectionLockState.h"
#include "Cluster/FollowerInfo.h"
#include "Cluster/MaintenanceFeature.h"
#include "Cluster/ServerState.h"
#include "RestServer/DatabaseFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
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
using namespace arangodb;

constexpr auto WAIT_FOR_SYNC_REPL = "waitForSyncReplication";
constexpr auto ENF_REPL_FACT = "enforceReplicationFactor";
constexpr auto REPL_HOLD_READ_LOCK = "/_api/replication/holdReadLockCollection";
constexpr auto REPL_ADD_FOLLOWER = "/_api/replication/addFollower";
constexpr auto REPL_REM_FOLLOWER = "/_api/replication/removeFollower";

std::string const READ_LOCK_TIMEOUT ("startReadLockOnLeader: giving up");
std::string const DB ("/_db/");
std::string const SYSTEM ("/_db/_system");
std::string const TTL ("ttl");
std::string const REPL_BARRIER_API ("/_api/replication/barrier/");

SynchronizeShard::SynchronizeShard(
  MaintenanceFeature& feature, ActionDescription const& desc) :
  ActionBase(feature, desc) {
  TRI_ASSERT(desc.has(COLLECTION));
  TRI_ASSERT(desc.has(DATABASE));
  TRI_ASSERT(desc.has(ID));
  TRI_ASSERT(desc.has(LEADER));
  TRI_ASSERT(properties().hasKey(TYPE));
  TRI_ASSERT(properties().get(TYPE).isInteger()); 
}

class SynchronizeShardCallback  : public arangodb::ClusterCommCallback {
public:
  SynchronizeShardCallback(SynchronizeShard* callie) : _callie(callie) {};
  virtual bool operator()(arangodb::ClusterCommResult*) override final {
    return true;
  }
private:
  std::shared_ptr<SynchronizeShard> _callie;
};

arangodb::Result getReadLockId (
  std::string const& endpoint, std::string const& database,
  std::string const& clientId, double timeout, uint64_t& id) {

  std::string error("startReadLockOnLeader: Failed to get read lock - ");
  
  auto cc = arangodb::ClusterComm::instance();
  if (cc == nullptr) { // nullptr only happens during controlled shutdown
    return arangodb::Result(
      TRI_ERROR_SHUTTING_DOWN, "startReadLockOnLeader: Shutting down");
  }
  
  auto comres = cc->syncRequest(
    clientId, 1, endpoint, rest::RequestType::GET,
    DB + database + REPL_HOLD_READ_LOCK, std::string(),
    std::unordered_map<std::string, std::string>(), timeout);
  
  auto result = comres->result;
  if (result != nullptr) { 
    if (result->getHttpReturnCode() == 200) {
      auto const idv = comres->result->getBodyVelocyPack();
      auto const& idSlice = idv->slice();
      TRI_ASSERT(idSlice.isObject());
      TRI_ASSERT(idSlice.hasKey(ID));
      try {
        id = idSlice.get(ID).getNumber<uint64_t>();
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
    error += "NULL result";
    return arangodb::Result(TRI_ERROR_INTERNAL, error);
  }
  
  return arangodb::Result();
  
}


arangodb::Result count(
  std::shared_ptr<arangodb::LogicalCollection> const col, uint64_t& c) {

  std::string collectionName(col->name());
  auto ctx = std::make_shared<transaction::StandaloneContext>(col->vocbase());
  SingleCollectionTransaction trx(
    ctx, collectionName, AccessMode::Type::READ);

  if (CollectionLockState::_noLockHeaders != nullptr) {
    if (CollectionLockState::_noLockHeaders->find(collectionName) !=
        CollectionLockState::_noLockHeaders->end()) {
      trx.addHint(transaction::Hints::Hint::LOCK_NEVER);
    }
  }
  
  Result res = trx.begin();
  if (!res.ok()) {
    LOG_TOPIC(ERR, Logger::MAINTENANCE)
      << "Failed to start count transaction: " << res;
    return res;
  }

  OperationResult opResult = trx.count(collectionName, false); 
  res = trx.finish(opResult.result);

  if (res.fail()) {
    LOG_TOPIC(ERR, Logger::MAINTENANCE)
      << "Failed to finish count transaction: " << res;
    return res;
  }

  VPackSlice s = opResult.slice();
  TRI_ASSERT(s.isNumber());
  c = s.getNumber<uint64_t>();

  return opResult.result;
  
}

arangodb::Result addShardFollower (
  std::string const& endpoint, std::string const& database,
  std::string const& shard, std::string const& lockJobId,
  std::string const& clientId, double timeout = 120.0 ) {

  auto cc = arangodb::ClusterComm::instance();
  if (cc == nullptr) { // nullptr only happens during controlled shutdown
    return arangodb::Result(
      TRI_ERROR_SHUTTING_DOWN, "startReadLockOnLeader: Shutting down");
  }

  auto vocbase = Databases::lookup(database);
  if (vocbase == nullptr) {
    std::string errorMsg(
      "SynchronizeShard::addShardFollower: Failed to lookup database ");
    errorMsg += database;
    LOG_TOPIC(ERR, Logger::MAINTENANCE) << errorMsg;
    return arangodb::Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, errorMsg);
  }

  auto collection = vocbase->lookupCollection(shard);
  if (collection == nullptr) {
    std::string errorMsg(
      "SynchronizeShard::addShardFollower: Failed to lookup collection ");
    errorMsg += shard;
    LOG_TOPIC(ERR, Logger::MAINTENANCE) << errorMsg;
    return arangodb::Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, errorMsg);    
  }

  size_t c;
  count(collection, c);
  VPackBuilder body;
  { VPackObjectBuilder b(&body);
    body.add("followerId", VPackValue(arangodb::ServerState::instance()->getId()));
    body.add("shard", VPackValue(shard));
    body.add("checksum", VPackValue(c));
    if (!lockJobId.empty()) {
      body.add("readLockId", VPackValue(lockJobId));
    }}
  
  auto comres = cc->syncRequest(
    clientId, 1, endpoint, rest::RequestType::PUT,
    DB + database + REPL_ADD_FOLLOWER, body.toJson(),
    std::unordered_map<std::string, std::string>(), timeout);  
  
  auto result = comres->result;
  if (result->getHttpReturnCode() != 200) {
    std::string errorMessage (
      "addShardFollower: could not add us to the leader's follower list. ");
    if (!lockJobId.empty()) {
      errorMessage += comres->stringifyErrorMessage();
      LOG_TOPIC(ERR, Logger::MAINTENANCE) << errorMessage;
    } else {
      errorMessage += "with shortcut.";
      LOG_TOPIC(DEBUG, Logger::MAINTENANCE) << errorMessage;
    }
    return arangodb::Result(TRI_ERROR_INTERNAL, errorMessage);
  }
  
  LOG_TOPIC(DEBUG, Logger::MAINTENANCE) << "cancelReadLockOnLeader: success";
  return arangodb::Result();
  
}


arangodb::Result removeShardFollower (
  std::string const& endpoint, std::string const& database,
  std::string const& shard, std::string const& clientId, double timeout = 120.0) {

  LOG_TOPIC(DEBUG, Logger::MAINTENANCE) <<
    "removeShardFollower: tell the leader to take us off the follower list...";
  
  auto cc = arangodb::ClusterComm::instance();
  if (cc == nullptr) { // nullptr only happens during controlled shutdown
    return arangodb::Result(
      TRI_ERROR_SHUTTING_DOWN, "startReadLockOnLeader: Shutting down");
  }

  VPackBuilder body;
  { VPackObjectBuilder b(&body);
    body.add("shard", VPackValue(shard));
    body.add("followerId",
             VPackValue(arangodb::ServerState::instance()->getId())); }

  // Note that we always use the _system database here because the actual
  // database might be gone already on the leader and we need to cancel
  // the read lock under all circumstances.
  auto comres = cc->syncRequest(
    clientId, 1, endpoint, rest::RequestType::PUT,
    DB + database + REPL_REM_FOLLOWER, body.toJson(),
    std::unordered_map<std::string, std::string>(), timeout);
  
  auto result = comres->result;
  if (result->getHttpReturnCode() != 200) {
    std::string errorMessage(
      "removeShardFollower: could not remove us from the leader's follower list: ");
    errorMessage += result->getHttpReturnCode();
    errorMessage += comres->stringifyErrorMessage();
    LOG_TOPIC(ERR, Logger::MAINTENANCE) << errorMessage;
    return arangodb::Result(TRI_ERROR_INTERNAL, errorMessage);
  }

  LOG_TOPIC(DEBUG, Logger::MAINTENANCE) << "removeShardFollower: success" ;
  return arangodb::Result();

}


arangodb::Result cancelReadLockOnLeader (
  std::string const& endpoint, std::string const& database,
  std::string const& lockJobId, std::string const& clientId,
  double timeout = 120.0) {

  auto cc = arangodb::ClusterComm::instance();
  if (cc == nullptr) { // nullptr only happens during controlled shutdown
    return arangodb::Result(
      TRI_ERROR_SHUTTING_DOWN, "startReadLockOnLeader: Shutting down");
  }

  VPackBuilder body;
  { VPackObjectBuilder b(&body);
    body.add("id", VPackValue(lockJobId)); }

  // Note that we always use the _system database here because the actual
  // database might be gone already on the leader and we need to cancel
  // the read lock under all circumstances.
  auto comres = cc->syncRequest(
    clientId, 1, endpoint, rest::RequestType::DELETE_REQ,
    SYSTEM + REPL_HOLD_READ_LOCK, body.toJson(),
    std::unordered_map<std::string, std::string>(), timeout);

  auto result = comres->result;
  if (result->getHttpReturnCode() != 200) {
    auto errorMessage = comres->stringifyErrorMessage();
    LOG_TOPIC(ERR, Logger::MAINTENANCE)
      << "cancelReadLockOnLeader: exception caught for " << body.toJson()
      << ": " << errorMessage;
    return arangodb::Result(TRI_ERROR_INTERNAL, errorMessage);
  }
  
  LOG_TOPIC(DEBUG, Logger::MAINTENANCE) << "cancelReadLockOnLeader: success";
  return arangodb::Result();
  
}


arangodb::Result cancelBarrier(
  std::string const& endpoint, std::string const& database,
  std::string const& barrierId, std::string const& clientId,
  double timeout = 120.0) {

  auto cc = arangodb::ClusterComm::instance();
  if (cc == nullptr) { // nullptr only happens during controlled shutdown
    return arangodb::Result(
      TRI_ERROR_SHUTTING_DOWN, "startReadLockOnLeader: Shutting down");
  }

  auto comres = cc->syncRequest(
    clientId, 1, endpoint, rest::RequestType::DELETE_REQ,
    DB + database + REPL_BARRIER_API + barrierId, std::string(),
    std::unordered_map<std::string, std::string>(), timeout);

  auto result = comres->result;
  if (result->getHttpReturnCode() != 200 &&
      result->getHttpReturnCode() != 204) {
    auto errorMessage = comres->stringifyErrorMessage();
    LOG_TOPIC(ERR, Logger::MAINTENANCE)
        << "CancelBarrier: error" << errorMessage;
    return arangodb::Result(TRI_ERROR_INTERNAL, errorMessage);
  }
  
  LOG_TOPIC(DEBUG, Logger::MAINTENANCE) << "cancelBarrier: success";
  return arangodb::Result();
  
}


arangodb::Result getReadLock(
  std::string const& endpoint, std::string const& database,
  std::string const& collection, std::string const& clientId,
  uint64_t rlid, SynchronizeShard* s, double timeout) {

  auto start = std::chrono::steady_clock::now();

  auto cc = arangodb::ClusterComm::instance();
  if (cc == nullptr) { // nullptr only happens during controlled shutdown
    return arangodb::Result(
      TRI_ERROR_SHUTTING_DOWN, "startReadLockOnLeader: Shutting down");
  }

  VPackBuilder b;
  { VPackObjectBuilder o(&b);
    b.add(ID, VPackValue(rlid));
    b.add(COLLECTION, VPackValue(collection));
    b.add(TTL, VPackValue(timeout)); }

  auto url = DB + database + REPL_HOLD_READ_LOCK;
  
  cc->asyncRequest(
    clientId, 2, endpoint, rest::RequestType::POST, url,
    std::make_shared<std::string>(b.toJson()),
    std::unordered_map<std::string, std::string>(),
    std::make_shared<SynchronizeShardCallback>(s), 1.0, true, 0.5);
  
  // Intentionally do not look at the outcome, even in case of an error
  // we must make sure that the read lock on the leader is not active!
  // This is done automatically below.
  
  size_t count = 0;
  while (++count < 20) { // wait for some time until read lock established:
    // Now check that we hold the read lock:
    auto putres = cc->syncRequest(
      clientId, 1, endpoint, rest::RequestType::PUT, url, b.toJson(),
      std::unordered_map<std::string, std::string>(), timeout);
    
    auto result = putres->result; 
    if (result->getHttpReturnCode() == 200) {
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

    // std::hash<ActionDescription>(_description)
    std::this_thread::sleep_for(std::chrono::duration<double>(.5));
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
  std::string const& collection, std::string const& clientId,
  SynchronizeShard* s,  double timeout = 120.0) {

  auto start = std::chrono::steady_clock::now();

  // Read lock id
  uint64_t rlid = 0;
  arangodb::Result result =
    getReadLockId(endpoint, database, clientId, timeout, rlid);
  if (!result.ok()) {
    LOG_TOPIC(ERR, Logger::MAINTENANCE) << result.errorMessage();
    return result;
  }

  result = getReadLock(endpoint, database, collection, clientId, rlid, s, timeout);
  
  auto elapsed = (std::chrono::steady_clock::now() - start).count();

  return result;
  
}


arangodb::Result terminateAndStartOther() {
  return arangodb::Result();
}


arangodb::Result synchroniseOneShard(
  std::string const& database, std::string const& shard,
  std::string const& planId, std::string const& leader) {

  auto* clusterInfo = ClusterInfo::instance();
  auto const ourselves = arangodb::ServerState::instance()->getId();
  auto const startTime = std::chrono::system_clock::now();
  
  while(true) {

    if (isStopping()) {
      terminateAndStartOther();
      return arangodb::Result();
    }

    std::vector<std::string> planned;
    auto result = clusterInfo->getShardServers(shard, planned);
    if (!result.ok() ||
        std::find(planned.begin(), planned.end(), ourselves) != planned.end() ||
        planned.front() != leader) {
      // Things have changed again, simply terminate:
      terminateAndStartOther();
      auto const endTime = std::chrono::system_clock::now();
      LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
        << "synchronizeOneShard: cancelled, " << database << "/" << shard
        << ", " << database << "/" << planId << ", started "
        << timepointToString(startTime) << ", ended " << timepointToString(endTime);
      return arangodb::Result(TRI_ERROR_FAILED, "synchronizeOneShard: cancelled");
    }

    std::shared_ptr<LogicalCollection> ci =
      clusterInfo->getCollection(database, planId);
    TRI_ASSERT(ci != nullptr);

    std::string const cid = std::to_string(ci->id());
    std::string const& name = ci->name();
    std::shared_ptr<CollectionInfoCurrent> cic =
      ClusterInfo::instance()->getCollectionCurrent(database, cid);
    std::vector<std::string> current;
    auto curres = cic->servers(shard);

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
        << timepointToString(startTime) << ", ended " << timepointToString(endTime);
      return arangodb::Result(TRI_ERROR_FAILED, "synchronizeOneShard: cancelled");
    }

    std::this_thread::sleep_for(std::chrono::duration<double>(0.2));
    
  }
  
  // Once we get here, we know that the leader is ready for sync, so
  // we give it a try:
  
  bool ok = false;
  auto ep = clusterInfo->getServerEndpoint(leader);
  
  //if (db._collection(shard).count() === 0) {
  // We try a short cut:
  /*console.topic('heartbeat=debug', "synchronizeOneShard: trying short cut to synchronize local shard '%s/%s' for central '%s/%s'", database, shard, database, planId);
    try {
    bool ok = addShardFollower(ep, database, shard);
    if (ok) {
    terminateAndStartOther();
    let endTime = new Date();
    console.topic('heartbeat=debug', 'synchronizeOneShard: shortcut worked, done, %s/%s, %s/%s, started: %s, ended: %s',
    database, shard, database, planId, startTime.toString(), endTime.toString());
    db._collection(shard).setTheLeader(leader);
    return;
    }
    } catch (std::exception const& e) { }*/
}

SynchronizeShard::~SynchronizeShard() {};

bool SynchronizeShard::first() {
  return true;
}

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


