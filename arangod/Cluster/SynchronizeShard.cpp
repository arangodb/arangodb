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
#include "Cluster/FollowerInfo.h"
#include "Cluster/MaintenanceFeature.h"
#include "Cluster/ServerState.h"
#include "Replication/DatabaseTailingSyncer.h"
#include "Replication/DatabaseInitialSyncer.h"
#include "Replication/DatabaseReplicationApplier.h"
#include "Replication/GlobalInitialSyncer.h"
#include "Replication/GlobalReplicationApplier.h"
#include "Replication/ReplicationApplierConfiguration.h"
#include "Replication/ReplicationFeature.h"
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
using namespace arangodb::transaction;
using namespace arangodb;

constexpr auto REPL_HOLD_READ_LOCK = "/_api/replication/holdReadLockCollection";
constexpr auto REPL_ADD_FOLLOWER = "/_api/replication/addFollower";
constexpr auto REPL_REM_FOLLOWER = "/_api/replication/removeFollower";

std::string const READ_LOCK_TIMEOUT ("startReadLockOnLeader: giving up");
std::string const DB ("/_db/");
std::string const SYSTEM ("/_db/_system");
std::string const TTL ("ttl");
std::string const REPL_BARRIER_API ("/_api/replication/barrier/");
std::string const ENDPOINT("endpoint");
std::string const INCREMENTAL("incremental");
std::string const KEEP_BARRIER("keepBarrier");
std::string const LEADER_ID("leaderId");
std::string const SKIP_CREATE_DROP("skipCreateDrop");
std::string const COLLECTIONS("collections");
std::string const LAST_LOG_TICK("lastLogTick");
std::string const BARRIER_ID("barrierId");
std::string const FOLLOWER_ID("followerId");

using namespace std::chrono;

SynchronizeShard::SynchronizeShard(
  MaintenanceFeature& feature, ActionDescription const& desc) :
  ActionBase(feature, desc) {
  TRI_ASSERT(desc.has(COLLECTION));
  TRI_ASSERT(desc.has(DATABASE));
  TRI_ASSERT(desc.has(COLLECTION));
  TRI_ASSERT(desc.has(LEADER));
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
  
  return arangodb::Result();
  
}


arangodb::Result count(
  std::shared_ptr<arangodb::LogicalCollection> const col, uint64_t& c) {
  
  std::string collectionName(col->name());
  auto ctx = std::make_shared<transaction::StandaloneContext>(col->vocbase());
  SingleCollectionTransaction trx(
    ctx, collectionName, AccessMode::Type::READ);

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
  std::string const& shard, uint64_t lockJobId,
  std::string const& clientId, double timeout = 120.0 ) {

  LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
    << "addShardFollower: tell the leader to put us into the follower list...";
  
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
    body.add(FOLLOWER_ID, VPackValue(arangodb::ServerState::instance()->getId()));
    body.add(SHARD, VPackValue(shard));
    body.add("checksum", VPackValue(std::to_string(c)));
    if (lockJobId != 0) {
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
    if (lockJobId != 0) {
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

  LOG_TOPIC(WARN, Logger::MAINTENANCE) <<
    "removeShardFollower: tell the leader to take us off the follower list...";
  
  auto cc = arangodb::ClusterComm::instance();
  if (cc == nullptr) { // nullptr only happens during controlled shutdown
    return arangodb::Result(
      TRI_ERROR_SHUTTING_DOWN, "startReadLockOnLeader: Shutting down");
  }

  VPackBuilder body;
  { VPackObjectBuilder b(&body);
    body.add(SHARD, VPackValue(shard));
    body.add(FOLLOWER_ID,
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

  LOG_TOPIC(WARN, Logger::MAINTENANCE) << "removeShardFollower: success" ;
  return arangodb::Result();

}

arangodb::Result cancelReadLockOnLeader (
  std::string const& endpoint, std::string const& database,
  uint64_t lockJobId, std::string const& clientId,
  double timeout = 120.0) {

  auto cc = arangodb::ClusterComm::instance();
  if (cc == nullptr) { // nullptr only happens during controlled shutdown
    return arangodb::Result(
      TRI_ERROR_SHUTTING_DOWN, "startReadLockOnLeader: Shutting down");
  }

  VPackBuilder body;
  { VPackObjectBuilder b(&body);
    body.add(ID, VPackValue(lockJobId)); }

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
  
  LOG_TOPIC(WARN, Logger::MAINTENANCE) << "cancelReadLockOnLeader: success";
  return arangodb::Result();
  
}


arangodb::Result cancelBarrier(
  std::string const& endpoint, std::string const& database,
  std::string const& barrierId, std::string const& clientId,
  double timeout = 120.0) {
  
  if (std::stol(barrierId) <= 0) {
    return Result();;
  }

  auto cc = arangodb::ClusterComm::instance();
  if (cc == nullptr) { // nullptr only happens during controlled shutdown
    return arangodb::Result(
      TRI_ERROR_SHUTTING_DOWN, "startReadLockOnLeader: Shutting down");
  }

  auto comres = cc->syncRequest(
    clientId, 1, endpoint, rest::RequestType::DELETE_REQ,
    DB + database + REPL_BARRIER_API + barrierId, std::string(),
    std::unordered_map<std::string, std::string>(), timeout);

  if (comres->status == CL_COMM_SENT) { 
    auto result = comres->result;
    if (result->getHttpReturnCode() != 200 &&
        result->getHttpReturnCode() != 204) {
      auto errorMessage = comres->stringifyErrorMessage();
      LOG_TOPIC(ERR, Logger::MAINTENANCE)
        << "CancelBarrier: error" << errorMessage;
      return arangodb::Result(TRI_ERROR_INTERNAL, errorMessage);
    }
  } else {
    std::string error ("CancelBarrier: failed to send message to leader : status ");
    error += comres->status; 
    LOG_TOPIC(ERR, Logger::MAINTENANCE) << error;
    return arangodb::Result(TRI_ERROR_INTERNAL, error);
  }
  
  LOG_TOPIC(WARN, Logger::MAINTENANCE) << "cancelBarrier: success";
  return arangodb::Result();
  
}


arangodb::Result SynchronizeShard::getReadLock(
  std::string const& endpoint, std::string const& database,
  std::string const& collection, std::string const& clientId,
  uint64_t rlid, double timeout) {

  auto start = steady_clock::now();

  auto cc = arangodb::ClusterComm::instance();
  if (cc == nullptr) { // nullptr only happens during controlled shutdown
    return arangodb::Result(
      TRI_ERROR_SHUTTING_DOWN, "startReadLockOnLeader: Shutting down");
  }

  VPackBuilder body;
  { VPackObjectBuilder o(&body);
    body.add(ID, VPackValue(rlid));
    body.add(COLLECTION, VPackValue(collection));
    body.add(TTL, VPackValue(timeout)); }

  auto url = DB + database + REPL_HOLD_READ_LOCK;
  
  cc->asyncRequest(
    clientId, 2, endpoint, rest::RequestType::POST, url,
    std::make_shared<std::string>(body.toJson()),
    std::unordered_map<std::string, std::string>(),
    std::make_shared<SynchronizeShardCallback>(this), timeout, true, timeout);
  
  // Intentionally do not look at the outcome, even in case of an error
  // we must make sure that the read lock on the leader is not active!
  // This is done automatically below.
  
  size_t count = 0;
  while (++count < 20) { // wait for some time until read lock established:
    // Now check that we hold the read lock:
    auto putres = cc->syncRequest(
      clientId, 1, endpoint, rest::RequestType::PUT, url, body.toJson(),
      std::unordered_map<std::string, std::string>(), timeout);
    
    auto result = putres->result; 
    if (result->getHttpReturnCode() == 200) {
      auto const vp = putres->result->getBodyVelocyPack();
      auto const& slice = vp->slice();
      TRI_ASSERT(slice.isObject());
      if (slice.hasKey("lockHeld") && slice.get("lockHeld").isBoolean() &&
          slice.get("lockHeld").getBool()) {
        return arangodb::Result();
      }
      LOG_TOPIC(WARN, Logger::MAINTENANCE)
        << "startReadLockOnLeader: Lock not yet acquired...";
    } else {
      LOG_TOPIC(WARN, Logger::MAINTENANCE)
        << "startReadLockOnLeader: Do not see read lock yet...";
    }

    // std::hash<ActionDescription>(_description)
    std::this_thread::sleep_for(duration<double>(.5));
    if ((steady_clock::now() - start).count() > timeout) {
      LOG_TOPIC(ERR, Logger::MAINTENANCE) << READ_LOCK_TIMEOUT;
      return arangodb::Result(TRI_ERROR_CLUSTER_TIMEOUT, READ_LOCK_TIMEOUT);
    }  
  }

  LOG_TOPIC(ERR, Logger::MAINTENANCE) << "startReadLockOnLeader: giving up";

  try {
    auto r = cc->syncRequest(
      clientId, 1, endpoint, rest::RequestType::DELETE_REQ, url, body.toJson(),
      std::unordered_map<std::string, std::string>(), timeout);
    if (r->result->getHttpReturnCode() != 200) {
      LOG_TOPIC(ERR, Logger::MAINTENANCE)
        << "startReadLockOnLeader: cancelation error for shard - " << collection
        << " " << r->getErrorCode() << ": " << r->stringifyErrorMessage();
    }
  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, Logger::MAINTENANCE)
      << "startReadLockOnLeader: expection in cancel: " << e.what();
  }
  
  return arangodb::Result(TRI_ERROR_CLUSTER_TIMEOUT, READ_LOCK_TIMEOUT);
  
}

bool isStopping() {
  return application_features::ApplicationServer::isStopping();
}

arangodb::Result SynchronizeShard::startReadLockOnLeader(
  std::string const& endpoint, std::string const& database,
  std::string const& collection, std::string const& clientId,
  uint64_t& rlid, double timeout) {

  // Read lock id
  rlid = 0;
  arangodb::Result result =
    getReadLockId(endpoint, database, clientId, timeout, rlid);
  if (!result.ok()) {
    LOG_TOPIC(ERR, Logger::MAINTENANCE) << result.errorMessage();
    return result;
  }

  result = getReadLock(endpoint, database, collection, clientId, rlid, timeout);
  
  return result;
  
}


arangodb::Result terminateAndStartOther() {
  return arangodb::Result();
}


enum ApplierType {
  APPLIER_DATABASE,
  APPLIER_GLOBAL
};

arangodb::Result replicationSynchronize(
  VPackSlice const& config, ApplierType applierType, std::shared_ptr<VPackBuilder> sy) {

  auto database = config.get(DATABASE).copyString();
  auto shard = config.get(SHARD).copyString();
  bool keepBarrier = config.get(KEEP_BARRIER).getBool();
  std::string leaderId;
  if (config.hasKey(LEADER_ID)) {
    leaderId = config.get(LEADER_ID).copyString();
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

  ReplicationApplierConfiguration configuration =
    ReplicationApplierConfiguration::fromVelocyPack(config, database);
  configuration.validate();

  std::unique_ptr<InitialSyncer> syncer;
 
  if (applierType == APPLIER_DATABASE) { 
    // database-specific synchronization
    syncer.reset(new DatabaseInitialSyncer(*vocbase, configuration));

    if (!leaderId.empty()) {
      syncer->setLeaderId(leaderId);
    }
  } else if (applierType == APPLIER_GLOBAL) {
    configuration._skipCreateDrop = false;
    syncer.reset(new GlobalInitialSyncer(configuration));
  } else {
    TRI_ASSERT(false);
  }

  try {
    Result r = syncer->run(configuration._incremental);
    
    if (r.fail()) {
      LOG_TOPIC(ERR, Logger::REPLICATION)
        << "initial sync failed for database '" << vocbase->name() << "': "
        << r.errorMessage();
      THROW_ARANGO_EXCEPTION_MESSAGE(
        r.errorNumber(), "cannot sync from remote endpoint: " +
        r.errorMessage() + ". last progress message was '" + syncer->progress()
        + "'");
    }
    
    { VPackObjectBuilder o(sy.get());
      if (keepBarrier) {
        sy->add(BARRIER_ID, VPackValue(syncer->stealBarrier()));
      }
      sy->add(LAST_LOG_TICK, VPackValue(syncer->getLastLogTick()));
      sy->add(VPackValue(COLLECTIONS));
      { VPackArrayBuilder a(sy.get());
        for (auto const& i : syncer->getProcessedCollections()) {
          VPackObjectBuilder e(sy.get());
          sy->add(ID, VPackValue(i.first));
          sy->add(NAME, VPackValue(i.second));
        }}}
    
  } catch (arangodb::basics::Exception const& ex) {
    std::string s("cannot sync from remote endpoint: ");
    s += ex.what() + std::string(". last progress message was '") + syncer->progress() + "'";
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    std::string s("cannot sync from remote endpoint: ");
    s += ex.what() + std::string(". last progress message was '") + syncer->progress() + "'";
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    std::string s(
      "cannot sync from remote endpoint: unknown exception. last progress message was '");
      s+= syncer->progress() + "'";
    return Result(TRI_ERROR_INTERNAL);
  }
 
  return arangodb::Result();
}

arangodb::Result syncCollection(
  std::shared_ptr<arangodb::LogicalCollection> const col,
  VPackSlice const& config, std::shared_ptr<VPackBuilder> sy) {

  VPackBuilder builder;
  { VPackObjectBuilder o(&builder);
    for (auto const& i : VPackObjectIterator(config)) {
      builder.add(i.key.copyString(), i.value);
    }
    builder.add("restrictType", VPackValue("include"));
    builder.add(VPackValue("restrictCollections"));
    { VPackArrayBuilder a(&builder);
      builder.add(VPackValue(col->name())); }
    builder.add("includeSystem", VPackValue(true));
    if (!config.hasKey("verbose")) {
      builder.add("verbose", VPackValue(false));
    }}

  return replicationSynchronize(builder.slice(), APPLIER_DATABASE, sy);

}


arangodb::Result replicationSynchronizeFinalize(VPackSlice const& conf) {

  auto const database = conf.get(DATABASE).copyString();
  auto const collection = conf.get(COLLECTION).copyString();
  auto const leaderId = conf.get(LEADER_ID).copyString();
  auto const fromTick = conf.get("from").getNumber<uint64_t>();
  
  ReplicationApplierConfiguration configuration =
    ReplicationApplierConfiguration::fromVelocyPack(conf, database);
  // will throw if invalid
  configuration.validate();
  
  DatabaseGuard guard(database);
  DatabaseTailingSyncer syncer(guard.database(), configuration, fromTick, true, 0);

  if (!leaderId.empty()) {
    syncer.setLeaderId(leaderId);
  }

  Result r;
  try {
    r = syncer.syncCollectionFinalize(collection);
  } catch (arangodb::basics::Exception const& ex) {
    r = Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    r = Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    r = Result(TRI_ERROR_INTERNAL, "unknown exception");
  }

  if (r.fail()) {
    LOG_TOPIC(ERR, Logger::REPLICATION)
      << "syncCollectionFinalize failed: " << r.errorMessage();
  }

  return r;
}


arangodb::Result SynchronizeShard::synchronise() {

  std::string database = _description.get(DATABASE);
  std::string planId = _description.get(COLLECTION);
  std::string shard = _description.get(SHARD);
  std::string leader = _description.get(LEADER);
  
  auto* clusterInfo = ClusterInfo::instance();
  auto const ourselves = arangodb::ServerState::instance()->getId();
  auto startTime = system_clock::now();
  auto const startTimeStr = timepointToString(startTime);
  auto const clientId(database + planId + shard + leader);
  
  // First wait until the leader has created the shard (visible in
  // Current in the Agency) or we or the shard have vanished from
  // the plan:
  while(true) {

    if (isStopping()) {
      _state = FAILED;
      return Result(TRI_ERROR_INTERNAL, "shutting down");
    }

    std::vector<std::string> planned;
    auto result = clusterInfo->getShardServers(shard, planned);
    
    if (!result.ok() ||
        std::find(planned.begin(), planned.end(), ourselves) == planned.end() ||
        planned.front() != leader) {
      // Things have changed again, simply terminate:
      auto const endTime = system_clock::now();
      LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
        << "synchronizeOneShard: cancelled, " << database << "/" << shard
        << ", " << database << "/" << planId << ", started "
        << startTimeStr << ", ended " << timepointToString(endTime);
      _state = FAILED;
      return arangodb::Result(TRI_ERROR_FAILED, "synchronizeOneShard: cancelled");
    }

    std::shared_ptr<LogicalCollection> ci =
      clusterInfo->getCollection(database, planId);
    TRI_ASSERT(ci != nullptr);

    std::string const cid = std::to_string(ci->id());
    std::shared_ptr<CollectionInfoCurrent> cic =
      ClusterInfo::instance()->getCollectionCurrent(database, cid);
    std::vector<std::string> current = cic->servers(shard);

    if (current.empty()) {
      Result(TRI_ERROR_FAILED,
             "synchronizeOneShard: cancelled, no servers in 'Current'");
    }
    if (current.front() == leader) {
      if (std::find(current.begin(), current.end(), ourselves) == current.end()) {
        break; // start synchronization work
      }
      // We are already there, this is rather strange, but never mind:
      auto const endTime = system_clock::now();
      LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
        << "synchronizeOneShard: already done, " << database << "/" << shard
        << ", " << database << "/" << planId << ", started "
        << startTimeStr << ", ended " << timepointToString(endTime);
      _state = FAILED;
      return arangodb::Result(TRI_ERROR_FAILED, "synchronizeOneShard: cancelled");
    }

    LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
      << "synchronizeOneShard: waiting for leader, " << database
      << "/" << shard << ", " << database << "/" << planId;
    
    std::this_thread::sleep_for(duration<double>(0.2));
    
  }
  
  // Once we get here, we know that the leader is ready for sync, so we give it a try:
  auto vocbase = Databases::lookup(database);
  if (vocbase == nullptr) {
    std::string errorMsg(
      "SynchronizeShard::SynchronizeOneShard: Failed to lookup database ");
    errorMsg += database;
    LOG_TOPIC(ERR, Logger::MAINTENANCE) << errorMsg;
    _state = FAILED;
    return arangodb::Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, errorMsg);
  }
  
  auto collection = vocbase->lookupCollection(shard);
  if (collection == nullptr) {
    std::string errorMsg(
      "SynchronizeShard::synchronizeOneShard: Failed to lookup local shard ");
    errorMsg += shard;
    LOG_TOPIC(ERR, Logger::MAINTENANCE) << errorMsg;
    _state=FAILED;
    return arangodb::Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, errorMsg);    
  }
  
  auto ep = clusterInfo->getServerEndpoint(leader);
  size_t c;
  if (!count(collection, c).ok()) {
    std::string errorMsg(
      "SynchronizeShard::synchronizeOneShard: Failed to get a count on leader ");
    errorMsg += shard;
    LOG_TOPIC(ERR, Logger::MAINTENANCE) << errorMsg;
    _state=FAILED;
    return arangodb::Result(TRI_ERROR_INTERNAL, errorMsg);    
  }
  
  if (c == 0) {
    // We have a short cut:
    LOG_TOPIC(DEBUG, Logger::MAINTENANCE) <<
      "synchronizeOneShard: trying short cut to synchronize local shard '" <<
      database << "/" << shard << "' for central '" << database << "/" <<
      planId << "'";
    try {

      auto asResult = addShardFollower(ep, database, shard, 0, clientId, 60.0);

      if (asResult.ok()) {

        auto const endTime = system_clock::now();
        LOG_TOPIC(DEBUG, Logger::MAINTENANCE) <<
          "synchronizeOneShard: shortcut worked, done, " << database << "/" <<
          shard << ", " << database << "/" << planId <<", started: " << startTimeStr
          << " ended: " << timepointToString(endTime);
        collection->followers()->setTheLeader(leader);
        _state=COMPLETE;
        return Result();
      }
    } catch (...) { }
  }

  LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
    << "synchronizeOneShard: trying to synchronize local shard '" << database
    << "/" << shard << "' for central '" << database << "/" << planId << "'";

  try {
    // First once without a read transaction:
    
    if (isStopping()) {
      _state=FAILED;
      return Result(TRI_ERROR_INTERNAL, "server is shutting down");
    }

    collection->followers()->setTheLeader(leader);

    startTime = system_clock::now();

    VPackBuilder config;
    { VPackObjectBuilder c(&config);
      config.add(ENDPOINT, VPackValue(ep));
      config.add(INCREMENTAL, VPackValue(true));
      config.add(KEEP_BARRIER, VPackValue(true));
      config.add(LEADER_ID, VPackValue(leader));
      config.add(SKIP_CREATE_DROP, VPackValue(true));
    }

    auto details = std::make_shared<VPackBuilder>();
    Result syncRes = syncCollection(collection, config.slice(), details);
    auto sy = details->slice();
    auto const endTime = system_clock::now();
    bool longSync = false;

    // Long shard sync initialisation
    if (endTime-startTime > seconds(5)) {
      LOG_TOPIC(WARN, Logger::MAINTENANCE) <<
        "synchronizeOneShard: long call to syncCollection for shard"
          << shard << " " << syncRes.errorMessage() <<  " start time: "
          << timepointToString(startTime) <<  "end time: " 
          << timepointToString(system_clock::now());
      longSync = true;
    }
    
    // 
    if (!syncRes.ok()) {
      LOG_TOPIC(ERR, Logger::MAINTENANCE)
        << "synchronizeOneShard: could not initially synchronize shard " << shard
        << syncRes.errorMessage();
      _state=FAILED;
      return syncRes;
      
    } else {

      VPackSlice collections = sy.get(COLLECTIONS);

      if (collections.length() == 0 ||
          collections[0].get("name").copyString() != shard) {

        if (longSync) {
          LOG_TOPIC(ERR, Logger::MAINTENANCE)
            << "synchronizeOneShard: long sync, before cancelBarrier" 
            << timepointToString(system_clock::now());
        }
        cancelBarrier(ep, database, sy.get(BARRIER_ID).copyString(), clientId);
        if (longSync) {
          LOG_TOPIC(ERR, Logger::MAINTENANCE)
            << "synchronizeOneShard: long sync, after cancelBarrier" 
            << timepointToString(system_clock::now());
        }
        std::string errorMessage("synchronizeOneShard: Shard ");
        errorMessage += shard + " seems to be gone from leader!";
        LOG_TOPIC(ERR,  Logger::MAINTENANCE) << errorMessage;
        _state=FAILED;
        return Result(TRI_ERROR_INTERNAL, errorMessage);

      } else {

        // Now start a read transaction to stop writes:
        uint64_t lockJobId = 0;
        LOG_TOPIC(ERR, Logger::MAINTENANCE)
          << "synchronizeOneShard: startReadLockOnLeader: " << ep << ":"
          << database << ":" << collection->name();
        Result result = startReadLockOnLeader(
          ep, database, collection->name(), clientId, lockJobId);
        if (result.ok()) {
          LOG_TOPIC(DEBUG, Logger::MAINTENANCE) << "lockJobId: " <<  lockJobId;
        } else {
          LOG_TOPIC(ERR, Logger::MAINTENANCE)
            << "synchronizeOneShard: error in startReadLockOnLeader:"
            << result.errorMessage();
        }

        cancelBarrier(ep, database, sy.get("barrierId").copyString(), clientId); 

        if (lockJobId != 0) {

          VPackBuilder builder;
          { VPackObjectBuilder o(&builder);
            builder.add(ENDPOINT, VPackValue(ep));
            builder.add(DATABASE, VPackValue(database));
            builder.add(COLLECTION, VPackValue(shard));
            builder.add(LEADER_ID, VPackValue(leader));
            builder.add("from", sy.get(LAST_LOG_TICK));
            builder.add("requestTimeout", VPackValue(60.0));
            builder.add("connectTimeout", VPackValue(60.0));
          }

          Result fres = replicationSynchronizeFinalize (builder.slice());

          if (fres.ok()) {
            result = addShardFollower(ep, database, shard, lockJobId, clientId, 60.0);

            if (!result.ok()) {
              //db._drop(shard, {isSystem: true });
              LOG_TOPIC(ERR, Logger::MAINTENANCE)
                << "synchronizeOneShard: failed to add follower"
                << result.errorMessage();
            }
          } else {
            std::string errorMessage(
              "synchronizeOneshard: error in syncCollectionFinalize: ") ;
            errorMessage += fres.errorMessage();
            result = Result(TRI_ERROR_INTERNAL, errorMessage);
          }

          result = cancelReadLockOnLeader(ep, database, lockJobId, clientId, 60.0);
          if (!result.ok()) {
            LOG_TOPIC(ERR, Logger::MAINTENANCE)
              << "synchronizeOneShard: read lock has timed out for shard " << shard;
          }
        } else {
          LOG_TOPIC(ERR, Logger::MAINTENANCE)
            << "synchronizeOneShard: lockJobId was false for shard" << shard;
        }
          
        if (result.ok()) {
          LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
            << "synchronizeOneShard: synchronization worked for shard " << shard;
        } else {
          LOG_TOPIC(ERR, Logger::MAINTENANCE)
            << "synchronizeOneShard: synchronization failed for shard " << shard;
        }
      }
    }
  } catch (std::exception const& e) {
    auto const endTime = system_clock::now();
    LOG_TOPIC(ERR,Logger::MAINTENANCE)
      << "synchronization of local shard '" << database << "/" << shard
      << "' for central '" << database << "/" << planId << "' failed: "
      << e.what() << timepointToString(endTime);
    _state=FAILED;
    return Result (TRI_ERROR_INTERNAL, e.what());
  }
  
  // Tell others that we are done:
  terminateAndStartOther();
  auto const endTime = system_clock::now();
  LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
    << "synchronizeOneShard: done, " << database << "/" << shard << ", "
    << database << "/" << planId << ", started: "
    << timepointToString(startTime) << ", ended: " << timepointToString(endTime);
  
  _state=COMPLETE;
  return Result();
  
}

SynchronizeShard::~SynchronizeShard() {};

bool SynchronizeShard::first() {
  arangodb::Result res = synchronise();
  return false;
}

arangodb::Result SynchronizeShard::run(
  duration<double> const&, bool& finished) {
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


