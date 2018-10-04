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
/// @author Kaveh Vahedipour
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#include "SynchronizeShard.h"

#include "Agency/TimeString.h"
#include "Agency/AgencyStrings.h"
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
using namespace arangodb::consensus;


std::string const ENDPOINT("endpoint");
std::string const INCLUDE("include");
std::string const INCLUDE_SYSTEM("includeSystem");
std::string const INCREMENTAL("incremental");
std::string const KEEP_BARRIER("keepBarrier");
std::string const LEADER_ID("leaderId");
std::string const BARRIER_ID("barrierId");
std::string const LAST_LOG_TICK("lastLogTick");
std::string const API_REPLICATION("/_api/replication/");
std::string const REPL_ADD_FOLLOWER(API_REPLICATION + "addFollower");
std::string const REPL_BARRIER_API(API_REPLICATION + "barrier/");
std::string const REPL_HOLD_READ_LOCK(API_REPLICATION + "holdReadLockCollection");
std::string const REPL_REM_FOLLOWER(API_REPLICATION + "removeFollower");
std::string const RESTRICT_TYPE("restrictType");
std::string const RESTRICT_COLLECTIONS("restrictCollections");
std::string const SKIP_CREATE_DROP("skipCreateDrop");
std::string const TTL("ttl");
using namespace std::chrono;

SynchronizeShard::SynchronizeShard(
  MaintenanceFeature& feature, ActionDescription const& desc) :
  ActionBase(feature, desc) {

  std::stringstream error;

  if (!desc.has(COLLECTION)) {
    error << "collection must be specified";
  }
  TRI_ASSERT(desc.has(COLLECTION));

  if (!desc.has(DATABASE)) {
    error << "database must be specified";
  }
  TRI_ASSERT(desc.has(DATABASE));

  if (!desc.has(SHARD)) {
    error << "SHARD must be specified";
  }
  TRI_ASSERT(desc.has(SHARD));

  if (!desc.has(THE_LEADER)) {
    error << "leader must be stecified";
  }
  TRI_ASSERT(desc.has(THE_LEADER));

  if (!desc.has(SHARD_VERSION)) {
    error << "local shard version must be specified. ";
  }
  TRI_ASSERT(desc.has(SHARD_VERSION));

  if (!error.str().empty()) {
    LOG_TOPIC(ERR, Logger::MAINTENANCE) << "SynchronizeShard: " << error.str();
    _result.reset(TRI_ERROR_INTERNAL, error.str());
    setState(FAILED);
  }

}

class SynchronizeShardCallback  : public arangodb::ClusterCommCallback {
public:
  explicit SynchronizeShardCallback(SynchronizeShard* callie) {};
  virtual bool operator()(arangodb::ClusterCommResult*) override final {
    return true;
  }
};

SynchronizeShard::~SynchronizeShard() {}


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
    TRI_NewTickServer(), endpoint, rest::RequestType::GET,
    DB + database + REPL_HOLD_READ_LOCK, std::string(),
    std::unordered_map<std::string, std::string>(), timeout);

  auto result = comres->result;

  if (result != nullptr && result->getHttpReturnCode() == 200) {
    auto const idv = comres->result->getBodyVelocyPack();
    auto const& idSlice = idv->slice();
    TRI_ASSERT(idSlice.isObject());
    TRI_ASSERT(idSlice.hasKey(ID));
    try {
      id = std::stoll(idSlice.get(ID).copyString());
    } catch (std::exception const&) {
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


arangodb::Result collectionCount(
  std::shared_ptr<arangodb::LogicalCollection> const& col, uint64_t& c) {

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

  OperationResult opResult = trx.count(collectionName,
      arangodb::transaction::CountType::Normal);
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

  try {
    DatabaseGuard guard(database);
    auto vocbase = &guard.database();

    auto collection = vocbase->lookupCollection(shard);
    if (collection == nullptr) {
      std::string errorMsg(
        "SynchronizeShard::addShardFollower: Failed to lookup collection ");
      errorMsg += shard;
      LOG_TOPIC(ERR, Logger::MAINTENANCE) << errorMsg;
      return arangodb::Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, errorMsg);
    }

    uint64_t docCount;
    Result res = collectionCount(collection, docCount);
    if (res.fail()) {
      return res;
    }
    VPackBuilder body;
    { VPackObjectBuilder b(&body);
      body.add(FOLLOWER_ID, VPackValue(arangodb::ServerState::instance()->getId()));
      body.add(SHARD, VPackValue(shard));
      body.add("checksum", VPackValue(std::to_string(docCount)));
      if (lockJobId != 0) {
        body.add("readLockId", VPackValue(std::to_string(lockJobId)));
      } else {
        TRI_ASSERT(docCount == 0);
      }
    }

    auto comres = cc->syncRequest(
      TRI_NewTickServer(), endpoint, rest::RequestType::PUT,
      DB + database + REPL_ADD_FOLLOWER, body.toJson(),
      std::unordered_map<std::string, std::string>(), timeout);

    auto result = comres->result;
    std::string errorMessage (
      "addShardFollower: could not add us to the leader's follower list. ");
    if (result == nullptr || result->getHttpReturnCode() != 200) {
      if (lockJobId != 0) {
        errorMessage += comres->stringifyErrorMessage();
        LOG_TOPIC(ERR, Logger::MAINTENANCE) << errorMessage;
      } else {
        errorMessage += "with shortcut.";
        LOG_TOPIC(ERR, Logger::MAINTENANCE) << errorMessage;
      }
      return arangodb::Result(TRI_ERROR_INTERNAL, errorMessage);
    }

    LOG_TOPIC(DEBUG, Logger::MAINTENANCE) << "cancelReadLockOnLeader: success";
    return arangodb::Result();
  } catch (std::exception const& e) {
    std::string errorMsg(
      "SynchronizeShard::addShardFollower: Failed to lookup database ");
    errorMsg += database;
    errorMsg += " exception: ";
    errorMsg += e.what();
    LOG_TOPIC(ERR, Logger::MAINTENANCE) << errorMsg;
    return arangodb::Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, errorMsg);
  }
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
    TRI_NewTickServer(), endpoint, rest::RequestType::PUT,
    DB + database + REPL_REM_FOLLOWER, body.toJson(),
    std::unordered_map<std::string, std::string>(), timeout);

  auto result = comres->result;
  if (result == nullptr || result->getHttpReturnCode() != 200) {
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
  double timeout = 10.0) {

  auto cc = arangodb::ClusterComm::instance();
  if (cc == nullptr) { // nullptr only happens during controlled shutdown
    return arangodb::Result(
      TRI_ERROR_SHUTTING_DOWN, "startReadLockOnLeader: Shutting down");
  }

  VPackBuilder body;
  { VPackObjectBuilder b(&body);
    body.add(ID, VPackValue(std::to_string(lockJobId))); }

  // Note that we always use the _system database here because the actual
  // database might be gone already on the leader and we need to cancel
  // the read lock under all circumstances.
  auto comres = cc->syncRequest(
    TRI_NewTickServer(), endpoint, rest::RequestType::DELETE_REQ,
    DB + StaticStrings::SystemDatabase + REPL_HOLD_READ_LOCK, body.toJson(),
    std::unordered_map<std::string, std::string>(), timeout);

  auto result = comres->result;

  if (result == nullptr || result->getHttpReturnCode() != 200) {
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
  int64_t barrierId, std::string const& clientId,
  double timeout = 120.0) {

  if (barrierId <= 0) {
    return Result();;
  }

  auto cc = arangodb::ClusterComm::instance();
  if (cc == nullptr) { // nullptr only happens during controlled shutdown
    return arangodb::Result(
      TRI_ERROR_SHUTTING_DOWN, "startReadLockOnLeader: Shutting down");
  }

  auto comres = cc->syncRequest(
    TRI_NewTickServer(), endpoint, rest::RequestType::DELETE_REQ,
    DB + database + REPL_BARRIER_API + std::to_string(barrierId), std::string(),
    std::unordered_map<std::string, std::string>(), timeout);

  if (comres->status == CL_COMM_SENT) {
    auto result = comres->result;
    if (result != nullptr && result->getHttpReturnCode() != 200 &&
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

  LOG_TOPIC(DEBUG, Logger::MAINTENANCE) << "cancelBarrier: success";
  return arangodb::Result();

}


arangodb::Result SynchronizeShard::getReadLock(
  std::string const& endpoint, std::string const& database,
  std::string const& collection, std::string const& clientId,
  uint64_t rlid, double timeout) {

  auto cc = arangodb::ClusterComm::instance();
  if (cc == nullptr) { // nullptr only happens during controlled shutdown
    return arangodb::Result(
      TRI_ERROR_SHUTTING_DOWN, "startReadLockOnLeader: Shutting down");
  }

  VPackBuilder body;
  { VPackObjectBuilder o(&body);
    body.add(ID, VPackValue(std::to_string(rlid)));
    body.add(COLLECTION, VPackValue(collection));
    body.add(TTL, VPackValue(timeout)); }

  auto url = DB + database + REPL_HOLD_READ_LOCK;

  cc->asyncRequest(
    TRI_NewTickServer(), endpoint, rest::RequestType::POST, url,
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
      TRI_NewTickServer(), endpoint, rest::RequestType::PUT, url, body.toJson(),
      std::unordered_map<std::string, std::string>(), timeout);

    auto result = putres->result;
    if (result != nullptr && result->getHttpReturnCode() == 200) {
      auto const vp = putres->result->getBodyVelocyPack();
      auto const& slice = vp->slice();
      TRI_ASSERT(slice.isObject());
      if (slice.hasKey("lockHeld") && slice.get("lockHeld").isBoolean() &&
          slice.get("lockHeld").getBool()) {
        return arangodb::Result();
      }
      LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
        << "startReadLockOnLeader: Lock not yet acquired...";
    } else {
      LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
        << "startReadLockOnLeader: Do not see read lock yet:"
        << putres->stringifyErrorMessage();
    }

    std::this_thread::sleep_for(duration<double>(.5));
  }

  LOG_TOPIC(ERR, Logger::MAINTENANCE) << "startReadLockOnLeader: giving up";

  try {
    auto r = cc->syncRequest(
      TRI_NewTickServer(), endpoint, rest::RequestType::DELETE_REQ, url, body.toJson(),
      std::unordered_map<std::string, std::string>(), timeout);
    if (r->result == nullptr && r->result->getHttpReturnCode() != 200) {
      LOG_TOPIC(ERR, Logger::MAINTENANCE)
        << "startReadLockOnLeader: cancelation error for shard - " << collection
        << " " << r->getErrorCode() << ": " << r->stringifyErrorMessage();
    }
  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, Logger::MAINTENANCE)
      << "startReadLockOnLeader: expection in cancel: " << e.what();
  }

  return arangodb::Result(TRI_ERROR_CLUSTER_TIMEOUT, "startReadLockOnLeader: giving up");

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
  } else {
    LOG_TOPIC(DEBUG, Logger::MAINTENANCE) << "Got read lock id: " << rlid;
  }

  result = getReadLock(endpoint, database, collection, clientId, rlid, timeout);

  return result;

}

enum ApplierType {
  APPLIER_DATABASE,
  APPLIER_GLOBAL
};

arangodb::Result replicationSynchronize(
  std::shared_ptr<arangodb::LogicalCollection> const &col, VPackSlice const& config,
  ApplierType applierType, std::shared_ptr<VPackBuilder> sy) {

  auto& vocbase = col->vocbase();

  auto database = vocbase.name();

  auto shard = col->name();

  bool keepBarrier = config.get(KEEP_BARRIER).getBool();
  std::string leaderId;
  if (config.hasKey(LEADER_ID)) {
    leaderId = config.get(LEADER_ID).copyString();
  }

  ReplicationApplierConfiguration configuration =
    ReplicationApplierConfiguration::fromVelocyPack(config, database);
  configuration.validate();

  std::shared_ptr<InitialSyncer> syncer;

  config.toJson();

  if (applierType == APPLIER_DATABASE) {
    // database-specific synchronization
    syncer.reset(new DatabaseInitialSyncer(vocbase, configuration));

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
        << "initial sync failed for database '" << database << "': "
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
    return Result(ex.code(), s);
  } catch (std::exception const& ex) {
    std::string s("cannot sync from remote endpoint: ");
    s += ex.what() + std::string(". last progress message was '") + syncer->progress() + "'";
    return Result(TRI_ERROR_INTERNAL, s);
  } catch (...) {
    std::string s(
      "cannot sync from remote endpoint: unknown exception. last progress message was '");
      s+= syncer->progress() + "'";
    return Result(TRI_ERROR_INTERNAL, s);
  }

  return arangodb::Result();
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


bool SynchronizeShard::first() {

  std::string database = _description.get(DATABASE);
  std::string planId = _description.get(COLLECTION);
  std::string shard = _description.get(SHARD);
  std::string leader = _description.get(THE_LEADER);

  LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
    << "SynchronizeShard: synchronizing shard '" << database << "/" << shard
    << "' for central '" << database << "/" << planId << "'";

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
      _result.reset(TRI_ERROR_INTERNAL, "shutting down");
      return false;
    }

    std::vector<std::string> planned;
    auto result = clusterInfo->getShardServers(shard, planned);

    if (!result.ok() ||
        std::find(planned.begin(), planned.end(), ourselves) == planned.end() ||
        planned.front() != leader) {
      // Things have changed again, simply terminate:
      auto const endTime = system_clock::now();
      std::stringstream error;
      error  << "cancelled, " << database << "/" << shard << ", " << database
             << "/" << planId << ", started " << startTimeStr << ", ended "
             << timepointToString(endTime);
      LOG_TOPIC(DEBUG, Logger::MAINTENANCE) << "SynchronizeOneShard: " << error.str();
      _result.reset(TRI_ERROR_FAILED, error.str());
      return false;
    }

    std::shared_ptr<LogicalCollection> ci;
    try {   // ci->getCollection can throw
      ci = clusterInfo->getCollection(database, planId);
    } catch(...) {
      auto const endTime = system_clock::now();
      std::stringstream msg;
      msg << "exception in getCollection, " << database << "/"
          << shard << ", " << database
          << "/" << planId << ", started " << startTimeStr << ", ended "
          << timepointToString(endTime);
      LOG_TOPIC(DEBUG, Logger::MAINTENANCE) << "SynchronizeOneShard: "
          << msg.str();
      _result.reset(TRI_ERROR_FAILED, msg.str());
      return false;
    }
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
      std::stringstream error;
      error
        << "already done, " << database << "/" << shard
        << ", " << database << "/" << planId << ", started "
        << startTimeStr << ", ended " << timepointToString(endTime);
      LOG_TOPIC(DEBUG, Logger::MAINTENANCE) << "SynchronizeOneShard: " << error.str();
      _result.reset(TRI_ERROR_FAILED, error.str());
      return false;
    }

    LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
      << "synchronizeOneShard: waiting for leader, " << database
      << "/" << shard << ", " << database << "/" << planId;

    std::this_thread::sleep_for(duration<double>(0.2));

  }

  // Once we get here, we know that the leader is ready for sync, so we give it a try:

  try {

    DatabaseGuard guard(database);
    auto vocbase = &guard.database();

    auto collection = vocbase->lookupCollection(shard);
    if (collection == nullptr) {
      std::stringstream error;
      error << "failed to lookup local shard " << shard;
      LOG_TOPIC(ERR, Logger::MAINTENANCE) << "SynchronizeOneShard: " << error.str();
      _result.reset(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, error.str());
      return false;
    }

    auto ep = clusterInfo->getServerEndpoint(leader);
    uint64_t docCount;
    if (!collectionCount(collection, docCount).ok()) {
      std::stringstream error;
      error << "failed to get a count on leader " << shard;
      LOG_TOPIC(ERR, Logger::MAINTENANCE) << "SynchronizeShard " << error.str();
      _result.reset(TRI_ERROR_INTERNAL, error.str());
      return false;
    }

    if (docCount == 0) {
      // We have a short cut:
      LOG_TOPIC(DEBUG, Logger::MAINTENANCE) <<
        "synchronizeOneShard: trying short cut to synchronize local shard '" <<
        database << "/" << shard << "' for central '" << database << "/" <<
        planId << "'";
      try {

        auto asResult = addShardFollower(ep, database, shard, 0, clientId, 60.0);

        if (asResult.ok()) {

          auto const endTime = system_clock::now();
          LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
            << "synchronizeOneShard: shortcut worked, done, " << database << "/"
            << shard << ", " << database << "/" << planId <<", started: "
            << startTimeStr << " ended: " << timepointToString(endTime);
          collection->followers()->setTheLeader(leader);
          notify();
          return false;
        }
      } catch (...) {}
    }

    LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
      << "synchronizeOneShard: trying to synchronize local shard '" << database
      << "/" << shard << "' for central '" << database << "/" << planId << "'";

    try {
      // First once without a read transaction:

      if (isStopping()) {
        _result.reset(TRI_ERROR_INTERNAL, "server is shutting down");
      }

      collection->followers()->setTheLeader(leader);

      if (leader.empty()) {
        collection->followers()->clear();
      }

      // do not reset followers when we resign at this time...we are
      // still the only source of truth to trust, in particular, in the
      // planned leader resignation, we will shortly after the call to
      // this function here report the controlled resignation to the
      // agency. This report must still contain the correct follower list
      // or else the supervision is super angry with us.

      startTime = system_clock::now();

      VPackBuilder config;
      { VPackObjectBuilder o(&config);
        config.add(ENDPOINT, VPackValue(ep));
        config.add(INCREMENTAL, VPackValue(true));
        config.add(KEEP_BARRIER, VPackValue(true));
        config.add(LEADER_ID, VPackValue(leader));
        config.add(SKIP_CREATE_DROP, VPackValue(true));
        config.add(RESTRICT_TYPE, VPackValue(INCLUDE));
        config.add(VPackValue(RESTRICT_COLLECTIONS));
        { VPackArrayBuilder a(&config);
          config.add(VPackValue(shard)); }
        config.add(INCLUDE_SYSTEM, VPackValue(true));
        config.add("verbose", VPackValue(false)); }

      auto details = std::make_shared<VPackBuilder>();

      Result syncRes = replicationSynchronize(
        collection, config.slice(), APPLIER_DATABASE, details);

      auto sy = details->slice();
      auto const endTime = system_clock::now();
      bool longSync = false;

      // Long shard sync initialisation
      if (endTime - startTime > seconds(5)) {
        LOG_TOPIC(WARN, Logger::MAINTENANCE)
          << "synchronizeOneShard: long call to syncCollection for shard"
          << shard << " " << syncRes.errorMessage() <<  " start time: "
          << timepointToString(startTime) <<  "end time: "
          << timepointToString(system_clock::now());
        longSync = true;
      }

      //
      if (!syncRes.ok()) {

        std::stringstream error;
        error << "could not initially synchronize shard " << shard
              << syncRes.errorMessage();
        LOG_TOPIC(ERR, Logger::MAINTENANCE) << "SynchronizeOneShard: " << error.str();
        _result.reset(TRI_ERROR_INTERNAL, error.str());
        return false;

      } else {

        VPackSlice collections = sy.get(COLLECTIONS);

        if (collections.length() == 0 ||
            collections[0].get("name").copyString() != shard) {

          if (longSync) {
            LOG_TOPIC(ERR, Logger::MAINTENANCE)
              << "synchronizeOneShard: long sync, before cancelBarrier"
              << timepointToString(system_clock::now());
          }
          cancelBarrier(ep, database, sy.get(BARRIER_ID).getNumber<int64_t>(), clientId);
          if (longSync) {
            LOG_TOPIC(ERR, Logger::MAINTENANCE)
              << "synchronizeOneShard: long sync, after cancelBarrier"
              << timepointToString(system_clock::now());
          }

          std::stringstream error;
          error << "shard " << shard << " seems to be gone from leader!";
          LOG_TOPIC(ERR,  Logger::MAINTENANCE) << "SynchronizeOneShard: " << error.str();
          _result.reset(TRI_ERROR_INTERNAL, error.str());
          return false;

        } else {

          // Now start a read transaction to stop writes:
          uint64_t lockJobId = 0;
          LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
            << "synchronizeOneShard: startReadLockOnLeader: " << ep << ":"
            << database << ":" << collection->name();
          Result result = startReadLockOnLeader(
            ep, database, collection->name(), clientId, lockJobId);
          if (result.ok()) {
            LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
                << "lockJobId: " <<  lockJobId;
          } else {
            LOG_TOPIC(ERR, Logger::MAINTENANCE)
              << "synchronizeOneShard: error in startReadLockOnLeader:"
              << result.errorMessage();
          }

          cancelBarrier(ep, database, sy.get("barrierId").getNumber<int64_t>(), clientId);

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

            // This result is unused, only in logs
            Result lockResult = cancelReadLockOnLeader(ep, database, lockJobId, clientId, 60.0);
            if (!lockResult.ok()) {
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
            _result.reset(TRI_ERROR_NO_ERROR);
          } else {
            LOG_TOPIC(ERR, Logger::MAINTENANCE)
              << "synchronizeOneShard: synchronization failed for shard " << shard;
            std::string errorMessage(
              "synchronizeOneShard: synchronization failed for shard "
              + shard + ":" + result.errorMessage());
            _result = Result(TRI_ERROR_INTERNAL, errorMessage);;
          }
        }
      }
    } catch (std::exception const& e) {
      auto const endTime = system_clock::now();
      std::stringstream error;
      error << "synchronization of local shard '" << database << "/" << shard
            << "' for central '" << database << "/" << planId << "' failed: "
            << e.what() << timepointToString(endTime);
      LOG_TOPIC(ERR, Logger::MAINTENANCE) << error.str();
      _result.reset(TRI_ERROR_INTERNAL, e.what());
      return false;
    }
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, Logger::MAINTENANCE)
      << "action " << _description << " failed with exception " << e.what();
    _result.reset(TRI_ERROR_INTERNAL, e.what());
    return false;
  }

  // Tell others that we are done:
  auto const endTime = system_clock::now();
  LOG_TOPIC(INFO, Logger::MAINTENANCE)
    << "synchronizeOneShard: done, " << database << "/" << shard << ", "
    << database << "/" << planId << ", started: "
    << timepointToString(startTime) << ", ended: " << timepointToString(endTime);

  notify();
  return false;

}


void SynchronizeShard::setState(ActionState state) {

  if ((COMPLETE==state || FAILED==state) && _state != state) {
    TRI_ASSERT(_description.has("shard"));
    _feature.incShardVersion(_description.get("shard"));
  }

  ActionBase::setState(state);

}
