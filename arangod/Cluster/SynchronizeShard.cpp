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
/// @author Kaveh Vahedipour
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#include "SynchronizeShard.h"

#include "Agency/AgencyStrings.h"
#include "Agency/TimeString.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ActionDescription.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/FollowerInfo.h"
#include "Cluster/MaintenanceFeature.h"
#include "Cluster/ServerState.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"
#include "Replication/DatabaseInitialSyncer.h"
#include "Replication/DatabaseReplicationApplier.h"
#include "Replication/DatabaseTailingSyncer.h"
#include "Replication/GlobalInitialSyncer.h"
#include "Replication/GlobalReplicationApplier.h"
#include "Replication/ReplicationApplierConfiguration.h"
#include "Replication/ReplicationFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/ServerIdFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionNameResolver.h"
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
using namespace arangodb::basics;
using namespace arangodb::consensus;

std::string const ENDPOINT("endpoint");
std::string const INCLUDE("include");
std::string const INCLUDE_SYSTEM("includeSystem");
std::string const INCREMENTAL("incremental");
std::string const LEADER_ID("leaderId");
std::string const LAST_LOG_TICK("lastLogTick");
std::string const API_REPLICATION("/_api/replication/");
std::string const REPL_ADD_FOLLOWER(API_REPLICATION + "addFollower");
std::string const REPL_HOLD_READ_LOCK(API_REPLICATION +
                                      "holdReadLockCollection");
std::string const REPL_REM_FOLLOWER(API_REPLICATION + "removeFollower");
std::string const RESTRICT_TYPE("restrictType");
std::string const RESTRICT_COLLECTIONS("restrictCollections");
std::string const SKIP_CREATE_DROP("skipCreateDrop");
std::string const TTL("ttl");

using namespace std::chrono;

SynchronizeShard::SynchronizeShard(MaintenanceFeature& feature, ActionDescription const& desc)
  : ActionBase(feature, desc),
    _leaderInfo(arangodb::replutils::LeaderInfo::createEmpty()) {
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

  if (!desc.has(THE_LEADER) || desc.get(THE_LEADER).empty()) {
    error << "leader must be specified and must be non-empty";
  }
  TRI_ASSERT(desc.has(THE_LEADER) && !desc.get(THE_LEADER).empty());

  if (!desc.has(SHARD_VERSION)) {
    error << "local shard version must be specified. ";
  }
  TRI_ASSERT(desc.has(SHARD_VERSION));

  if (!error.str().empty()) {
    LOG_TOPIC("03780", ERR, Logger::MAINTENANCE) << "SynchronizeShard: " << error.str();
    _result.reset(TRI_ERROR_INTERNAL, error.str());
    setState(FAILED);
  }
}
  
std::string const& SynchronizeShard::clientInfoString() const { 
  return _clientInfoString;
}

arangodb::replutils::LeaderInfo const& SynchronizeShard::leaderInfo() const {
  return _leaderInfo;
}

void SynchronizeShard::setLeaderInfo(arangodb::replutils::LeaderInfo const& leaderInfo) {
  _leaderInfo = leaderInfo;
}

SynchronizeShard::~SynchronizeShard() = default;

static std::stringstream& AppendShardInformationToMessage(
  std::string const& database, std::string const& shard, std::string const& planId,
  std::chrono::system_clock::time_point const& startTime, std::stringstream& msg) {
  auto const endTime = system_clock::now();
  msg << "local shard: '" << database << "/" << shard << "', "
      << "for central: '" << database << "/" << planId << "', "
      << "started: " << timepointToString(startTime) << ", "
      << "ended: " << timepointToString(endTime);
  return msg;
}

static arangodb::Result getReadLockId(network::ConnectionPool* pool,
                                      std::string const& endpoint,
                                      std::string const& database, std::string const& clientId,
                                      double timeout, uint64_t& id) {
  TRI_ASSERT(timeout > 0);

  if (pool == nullptr) {  // nullptr only happens during controlled shutdown
    return arangodb::Result(TRI_ERROR_SHUTTING_DOWN,
                            "startReadLockOnLeader: Shutting down");
  }
  
  std::string error("startReadLockOnLeader: Failed to get read lock");

  network::RequestOptions options;
  options.database = database;
  options.timeout = network::Timeout(timeout);
  options.skipScheduler = true; // hack to speed up future.get()
  
  auto response = network::sendRequest(pool, endpoint, fuerte::RestVerb::Get,
                                  REPL_HOLD_READ_LOCK,
                                  VPackBuffer<uint8_t>(), options)
                 .get();
  auto res = response.combinedResult();

  if (res.ok()) {
    auto const idSlice = response.slice();
    TRI_ASSERT(idSlice.isObject());
    TRI_ASSERT(idSlice.hasKey(ID));
    try {
      id = std::stoull(idSlice.get(ID).copyString());
    } catch (std::exception const&) {
      error += " - expecting id to be uint64_t ";
      error += idSlice.toJson();
      res.reset(TRI_ERROR_INTERNAL, error);
    } catch (...) {
      TRI_ASSERT(false);
      res.reset(TRI_ERROR_INTERNAL, error);
    }
  } 

  return res;
}

static arangodb::Result collectionCount(arangodb::LogicalCollection const& collection,
                                        uint64_t& c) {
  std::string collectionName(collection.name());
  auto ctx = std::make_shared<transaction::StandaloneContext>(collection.vocbase());
  SingleCollectionTransaction trx(ctx, collectionName, AccessMode::Type::READ);

  Result res = trx.begin();
  if (res.fail()) {
    LOG_TOPIC("5be16", ERR, Logger::MAINTENANCE) << "Failed to start count transaction: " << res;
    return res;
  }

  OperationOptions options(ExecContext::current());
  OperationResult opResult =
      trx.count(collectionName, arangodb::transaction::CountType::Normal, options);
  res = trx.finish(opResult.result);

  if (res.fail()) {
    LOG_TOPIC("26ed2", ERR, Logger::MAINTENANCE)
        << "Failed to finish count transaction: " << res;
    return res;
  }

  VPackSlice s = opResult.slice();
  TRI_ASSERT(s.isNumber());
  c = s.getNumber<uint64_t>();

  return opResult.result;
}

arangodb::Result collectionReCount(LogicalCollection& collection,
                                   uint64_t& c) {
  Result res;
  try {
    c = collection.getPhysical()->recalculateCounts();
  } catch (basics::Exception const& e) {
    res.reset(e.code(), e.message());
  }
  return res;
}

static arangodb::Result addShardFollower(
    network::ConnectionPool* pool, std::string const& endpoint,
    std::string const& database, std::string const& shard, uint64_t lockJobId,
    std::string const& clientId, SyncerId const syncerId,
    std::string const& clientInfoString, double timeout = 120.0) {

  if (pool == nullptr) {  // nullptr only happens during controlled shutdown
    return arangodb::Result(TRI_ERROR_SHUTTING_DOWN,
                            "startReadLockOnLeader: Shutting down");
  }
  
  LOG_TOPIC("b982e", DEBUG, Logger::MAINTENANCE)
      << "addShardFollower: tell the leader to put us into the follower "
         "list for " << database << "/" << shard << "...";

  try {
    auto& df = pool->config().clusterInfo->server().getFeature<DatabaseFeature>();
    DatabaseGuard guard(df, database);
    auto vocbase = &guard.database();

    auto collection = vocbase->lookupCollection(shard);
    if (collection == nullptr) {
      std::string errorMsg(
        "SynchronizeShard::addShardFollower: Failed to lookup collection ");
      errorMsg += database + "/" + shard;
      LOG_TOPIC("4a8db", ERR, Logger::MAINTENANCE) << errorMsg;
      return arangodb::Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, errorMsg);
    }

    uint64_t docCount;
    Result res = collectionCount(*collection, docCount);
    if (res.fail()) {
       return res;
    }
    
    VPackBuilder body;
    {
      VPackObjectBuilder b(&body);
      body.add(FOLLOWER_ID, VPackValue(arangodb::ServerState::instance()->getId()));
      body.add(SHARD, VPackValue(shard));
      body.add("checksum", VPackValue(std::to_string(docCount)));
      body.add("serverId",
               VPackValue(basics::StringUtils::itoa(ServerIdFeature::getId().id())));
      if (syncerId.value != 0) {
        body.add("syncerId", VPackValue(syncerId.toString()));
      }
      if (!clientInfoString.empty()) {
        body.add("clientInfo", VPackValue(clientInfoString));
      }
      if (lockJobId != 0) {
        body.add("readLockId", VPackValue(std::to_string(lockJobId)));
#if 0
        // shortcut code disabled
      } else {  // short cut case
        if (docCount != 0) {
          // This can happen if we once were an in-sync follower and a
          // synchronization request has timed out, but still runs on our
          // side here. In this case, we can simply continue with the slow
          // path and run the full sync protocol. Therefore we error out
          // here. Note that we are in the lockJobId == 0 case, which is
          // the shortcut.
          std::string msg =
              "Short cut synchronization for " + database + "/" + shard +
              " did not work, since we got a document in the meantime.";
          LOG_TOPIC("ef299", INFO, Logger::MAINTENANCE) << msg;
          return arangodb::Result(TRI_ERROR_REPLICATION_SHARD_NONEMPTY, msg);
        }
#endif
      }
    }

    network::RequestOptions options;
    options.database = database;
    options.timeout = network::Timeout(timeout);
    options.skipScheduler = true; // hack to speed up future.get()
    
    auto response = network::sendRequest(pool, endpoint, fuerte::RestVerb::Put,
                                    REPL_ADD_FOLLOWER,
                                    std::move(*body.steal()), options)
                   .get();
    auto result = response.combinedResult();

    if (result.fail()) {
      auto const errorMessage =
          "addShardFollower: could not add us to the leader's follower list for " +
          database + "/" + shard;

      if (lockJobId != 0) {
        LOG_TOPIC("22e0a", WARN, Logger::MAINTENANCE)
            << errorMessage << ", " << result.errorMessage();
      } else {
        LOG_TOPIC("abf2e", INFO, Logger::MAINTENANCE)
            << errorMessage << " with shortcut (can happen, no problem).";
        if (result.errorNumber() == TRI_ERROR_REPLICATION_SHARD_NONEMPTY) {
          return result;   // hand on leader protest
        }
      }
      return arangodb::Result(result.errorNumber(),
                              StringUtils::concatT(errorMessage, ", ", result.errorMessage()));
    }

    LOG_TOPIC("79935", DEBUG, Logger::MAINTENANCE) << "addShardFollower: success";
    return arangodb::Result();
  } catch (std::exception const& e) {
    std::string errorMsg(
      "SynchronizeShard::addShardFollower: Failed to lookup database ");
    errorMsg += database;
    errorMsg += " exception: ";
    errorMsg += e.what();
    LOG_TOPIC("6b7e8", ERR, Logger::MAINTENANCE) << errorMsg;
    return arangodb::Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, errorMsg);
  }
}

static arangodb::Result cancelReadLockOnLeader(network::ConnectionPool* pool,
                                               std::string const& endpoint,
                                               std::string const& database, uint64_t lockJobId,
                                               std::string const& clientId,
                                               double timeout) {
  TRI_ASSERT(timeout > 0.0);

  if (pool == nullptr) {  // nullptr only happens during controlled shutdown
    return arangodb::Result(TRI_ERROR_SHUTTING_DOWN,
                            "cancelReadLockOnLeader: Shutting down");
  }

  VPackBuilder body;
  {
    VPackObjectBuilder b(&body);
    body.add(ID, VPackValue(std::to_string(lockJobId)));
  }

  network::RequestOptions options;
  options.database = database;
  options.timeout = network::Timeout(timeout);
  options.skipScheduler = true; // hack to speed up future.get()

  auto response = network::sendRequest(pool, endpoint, fuerte::RestVerb::Delete,
                                  REPL_HOLD_READ_LOCK,
                                  std::move(*body.steal()), options)
                 .get();

  auto res = response.combinedResult();
  if (res.is(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND)) {
    // database is gone. that means our lock is also gone
    return arangodb::Result();
  }

  if (res.fail()) {
    // rebuild body since we stole it earlier
    VPackBuilder body;
    {
      VPackObjectBuilder b(&body);
      body.add(ID, VPackValue(std::to_string(lockJobId)));
    }
    LOG_TOPIC("52924", WARN, Logger::MAINTENANCE)
        << "cancelReadLockOnLeader: exception caught for " << body.toJson()
        << ": " << res.errorMessage();
    return arangodb::Result(TRI_ERROR_INTERNAL, res.errorMessage());
  }

  LOG_TOPIC("4355c", DEBUG, Logger::MAINTENANCE) << "cancelReadLockOnLeader: success";
  return arangodb::Result();
}

arangodb::Result SynchronizeShard::getReadLock(
  network::ConnectionPool* pool,
  std::string const& endpoint, std::string const& database,
  std::string const& collection, std::string const& clientId,
  uint64_t rlid, bool soft, double timeout) {

  TRI_ASSERT(timeout > 0.0);

  // This function can be implemented in a more robust manner for server
  // versions > 3.4. Starting with 3.4 the POST requests to the read lock API
  // terminates the server side thread as soon as the lock request comes in.
  // The POST request thus is answered immediately back to the caller.
  // The servers (<=3.3) with lower versions hold the POST request for as long
  // as the corresponding DELETE_REQ has not been successfully submitted.
  
  // nullptr only happens during controlled shutdown
  if (pool == nullptr) {
    return arangodb::Result(TRI_ERROR_SHUTTING_DOWN,
                            "cancelReadLockOnLeader: Shutting down");
  }

  VPackBuilder body;
  { 
    VPackObjectBuilder o(&body);
    body.add(ID, VPackValue(std::to_string(rlid)));
    body.add(COLLECTION, VPackValue(collection));
    body.add(TTL, VPackValue(timeout));
    body.add("serverId", VPackValue(arangodb::ServerState::instance()->getId()));
    body.add(StaticStrings::RebootId, VPackValue(ServerState::instance()->getRebootId().value()));
    body.add(StaticStrings::ReplicationSoftLockOnly, VPackValue(soft)); 
  }
  auto buf = body.steal();

  // Try to POST the lock body. If POST fails, we should just exit and retry
  // SynchroShard anew. 
  network::RequestOptions options;
  options.timeout = network::Timeout(timeout);
  options.database = database;

  auto response = network::sendRequest(
    pool, endpoint, fuerte::RestVerb::Post,
    REPL_HOLD_READ_LOCK, *buf, options).get();

  auto res = response.combinedResult();

  if (res.ok()) {
    // Habemus clausum, we have a lock
    return arangodb::Result();
  }

  LOG_TOPIC("cba32", DEBUG, Logger::MAINTENANCE)
    << "startReadLockOnLeader: couldn't POST lock body, "
    << network::fuerteToArangoErrorMessage(response) << ", giving up.";

  // We MUSTN'T exit without trying to clean up a lock that was maybe acquired
  if (response.error == fuerte::Error::CouldNotConnect) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      "startReadLockOnLeader: couldn't POST lock body, giving up.");
  }

  // Ambiguous POST, we'll try to DELETE a potentially acquired lock
  try {
    auto response = network::sendRequest(pool, endpoint, fuerte::RestVerb::Delete, REPL_HOLD_READ_LOCK,
                                  *buf, options)
                 .get();
    auto res = response.combinedResult();
    if (res.fail()) {
      LOG_TOPIC("4f34d", WARN, Logger::MAINTENANCE)
          << "startReadLockOnLeader: cancelation error for shard "
          << database << "/" << collection << ": " << res.errorMessage();
    }
  } catch (std::exception const& e) {
    LOG_TOPIC("7fcc9", WARN, Logger::MAINTENANCE)
        << "startReadLockOnLeader: exception in cancel: " << e.what();
  }

  return arangodb::Result(TRI_ERROR_CLUSTER_TIMEOUT,
                          "startReadLockOnLeader: giving up");
}

arangodb::Result SynchronizeShard::startReadLockOnLeader(
  std::string const& endpoint, std::string const& database, std::string const& collection,
  std::string const& clientId, uint64_t& rlid, bool soft, double timeout) {

  TRI_ASSERT(timeout > 0);
  // Read lock id
  rlid = 0;
  NetworkFeature& nf = _feature.server().getFeature<NetworkFeature>();
  network::ConnectionPool* pool = nf.pool();
  arangodb::Result result =
      getReadLockId(pool, endpoint, database, clientId, timeout, rlid);
  if (!result.ok()) {
    LOG_TOPIC("2e5ae", WARN, Logger::MAINTENANCE) << result.errorMessage();
  } else {
    LOG_TOPIC("c8d18", DEBUG, Logger::MAINTENANCE) << "Got read lock id: " << rlid;

    result.reset(getReadLock(pool, endpoint, database, collection, clientId, rlid, soft, timeout));
  }

  return result;
}

static arangodb::ResultT<SyncerId> replicationSynchronize(
    SynchronizeShard& job,
    std::shared_ptr<arangodb::LogicalCollection> const& col,
    VPackSlice const& config,
    std::shared_ptr<VPackBuilder> sy) {

  auto& vocbase = col->vocbase();
  auto database = vocbase.name();

  std::string leaderId;
  if (config.hasKey(LEADER_ID)) {
    leaderId = config.get(LEADER_ID).copyString();
  }

  ReplicationApplierConfiguration configuration =
      ReplicationApplierConfiguration::fromVelocyPack(vocbase.server(), config, database);
  configuration.setClientInfo(job.clientInfoString());
  configuration.validate();

  // database-specific synchronization
  auto syncer = DatabaseInitialSyncer::create(vocbase, configuration);

  if (!leaderId.empty()) {
    syncer->setLeaderId(leaderId);
  }

  SyncerId syncerId{syncer->syncerId()};

  try {
    std::string const context = "syncing shard " + database + "/" + col->name();
    Result r = syncer->run(configuration._incremental, context.c_str());
  
    if (r.fail()) {
      LOG_TOPIC("3efff", DEBUG, Logger::REPLICATION)
          << "initial sync failed for " << database << "/" << col->name()
          << ": " << r.errorMessage();
      THROW_ARANGO_EXCEPTION(r);
    }

    // store leader info for later, so that the next phases don't need to acquire it again
    job.setLeaderInfo(syncer->leaderInfo());

    {
      VPackObjectBuilder o(sy.get());
      sy->add(LAST_LOG_TICK, VPackValue(syncer->getLastLogTick()));
      sy->add(VPackValue(COLLECTIONS));
      {
        VPackArrayBuilder a(sy.get());
        for (auto const& i : syncer->getProcessedCollections()) {
          VPackObjectBuilder e(sy.get());
          sy->add(ID, VPackValue(i.first.id()));
          sy->add(NAME, VPackValue(i.second));
        }
      }
    }

  } catch (arangodb::basics::Exception const& ex) {
    std::string s("cannot sync from remote endpoint: ");
    s += ex.what() + std::string(". last progress message was '") +
      syncer->progress() + "'";
    return Result(ex.code(), s);
  } catch (std::exception const& ex) {
    std::string s("cannot sync from remote endpoint: ");
    s += ex.what() + std::string(". last progress message was '") +
      syncer->progress() + "'";
    return Result(TRI_ERROR_INTERNAL, s);
  } catch (...) {
    std::string s(
      "cannot sync from remote endpoint: unknown exception. last progress "
      "message was '");
    s += syncer->progress() + "'";
    return Result(TRI_ERROR_INTERNAL, s);
  }

  return ResultT<SyncerId>::success(syncerId);
}

static arangodb::Result replicationSynchronizeCatchup(
    SynchronizeShard const& job,
    application_features::ApplicationServer& server, VPackSlice const& conf,
    double timeout, TRI_voc_tick_t& tickReached, bool& didTimeout) {
  didTimeout = false;

  auto const database = conf.get(DATABASE).copyString();
  auto const collection = conf.get(COLLECTION).copyString();
  auto const leaderId = conf.get(LEADER_ID).copyString();
  auto const fromTick = conf.get("from").getNumber<uint64_t>();

  ReplicationApplierConfiguration configuration =
      ReplicationApplierConfiguration::fromVelocyPack(server, conf, database);
  // will throw if invalid
  configuration.validate();

  auto& df = server.getFeature<DatabaseFeature>();
  DatabaseGuard guard(df, database);
  auto syncer = DatabaseTailingSyncer::create(guard.database(), configuration, fromTick, /*useTick*/true);

  if (!leaderId.empty()) {
    syncer->setLeaderId(leaderId);
  }

  Result r;
  try {
    std::string const context = "catching up delta changes for shard " + database + "/" + collection;
    r = syncer->syncCollectionCatchup(job.leaderInfo(), collection, timeout, tickReached, didTimeout, context.c_str());
  } catch (arangodb::basics::Exception const& ex) {
    r.reset(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    r.reset(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    r.reset(TRI_ERROR_INTERNAL, "unknown exception");
  }

  if (r.fail()) {
    LOG_TOPIC("fa2ab", WARN, Logger::REPLICATION)
        << "syncCollectionCatchup failed: " << r.errorMessage();
  }

  return r;
}

static arangodb::Result replicationSynchronizeFinalize(SynchronizeShard const& job,
                                                       application_features::ApplicationServer& server,
                                                       VPackSlice const& conf) {
  auto const database = conf.get(DATABASE).copyString();
  auto const collection = conf.get(COLLECTION).copyString();
  auto const leaderId = conf.get(LEADER_ID).copyString();
  auto const fromTick = conf.get("from").getNumber<uint64_t>();
    
  ReplicationApplierConfiguration configuration =
      ReplicationApplierConfiguration::fromVelocyPack(server, conf, database);
  // will throw if invalid
  configuration.validate();

  auto& df = server.getFeature<DatabaseFeature>();
  DatabaseGuard guard(df, database);
  auto syncer = DatabaseTailingSyncer::create(guard.database(), configuration, fromTick, /*useTick*/ true);

  if (!leaderId.empty()) {
    syncer->setLeaderId(leaderId);
  }

  Result r;
  try {
    std::string const context = "finalizing shard " + database + "/" + collection;
    r = syncer->syncCollectionFinalize(job.leaderInfo(), collection, context.c_str());
  } catch (arangodb::basics::Exception const& ex) {
    r.reset(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    r.reset(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    r.reset(TRI_ERROR_INTERNAL, "unknown exception");
  }

  if (r.fail()) {
    LOG_TOPIC("e8056", WARN, Logger::REPLICATION)
        << "syncCollectionFinalize failed: " << r.errorMessage();
  }

  return r;
}

bool SynchronizeShard::first() {
  std::string database = _description.get(DATABASE);
  std::string planId = _description.get(COLLECTION);
  std::string shard = _description.get(SHARD);
  std::string leader = _description.get(THE_LEADER);

  LOG_TOPIC("fa651", DEBUG, Logger::MAINTENANCE)
      << "SynchronizeShard: synchronizing shard '" << database << "/" << shard
      << "' for central '" << database << "/" << planId << "'";

  auto& clusterInfo = feature().server().getFeature<ClusterFeature>().clusterInfo();
  auto const ourselves = arangodb::ServerState::instance()->getId();
  auto startTime = system_clock::now();
  auto const startTimeStr = timepointToString(startTime);
  std::string const clientId(database + planId + shard + leader);

  // First wait until the leader has created the shard (visible in
  // Current in the Agency) or we or the shard have vanished from
  // the plan:
  while (true) {
    if (feature().server().isStopping()) {
      _result.reset(TRI_ERROR_SHUTTING_DOWN);
      return false;
    }

    std::vector<std::string> planned;
    auto result = clusterInfo.getShardServers(shard, planned);

    if (!result.ok() ||
        std::find(planned.begin(), planned.end(), ourselves) == planned.end() ||
        planned.front() != leader) {
      // Things have changed again, simply terminate:
      std::stringstream error;
      error << "cancelled, ";
      AppendShardInformationToMessage(database, shard, planId, startTime, error);
      LOG_TOPIC("a1dc7", DEBUG, Logger::MAINTENANCE) << "SynchronizeOneShard: " << error.str();
      _result.reset(TRI_ERROR_FAILED, error.str());
      return false;
    }

    auto ci = clusterInfo.getCollectionNT(database, planId);
    if (ci == nullptr) {
      std::stringstream msg;
      msg << "exception in getCollection, ";
      AppendShardInformationToMessage(database, shard, planId, startTime, msg);
      LOG_TOPIC("89972", DEBUG, Logger::MAINTENANCE) << "SynchronizeOneShard: " << msg.str();
      _result.reset(TRI_ERROR_FAILED, msg.str());
      return false;
    }

    std::string const cid = std::to_string(ci->id().id());
    std::shared_ptr<CollectionInfoCurrent> cic =
        clusterInfo.getCollectionCurrent(database, cid);
    std::vector<std::string> current = cic->servers(shard);

    if (current.empty()) {
      LOG_TOPIC("b0ccf", DEBUG, Logger::MAINTENANCE)
          << "synchronizeOneShard: cancelled, no servers in 'Current'";
    } else if (current.front() == leader) {
      if (std::find(current.begin(), current.end(), ourselves) == current.end()) {
        break;  // start synchronization work
      }
      // We are already there, this is rather strange, but never mind:
      std::stringstream error;
      error << "already done, ";
      AppendShardInformationToMessage(database, shard, planId, startTime, error);
      LOG_TOPIC("4abcb", DEBUG, Logger::MAINTENANCE) << "SynchronizeOneShard: " << error.str();
      _result.reset(TRI_ERROR_FAILED, error.str());
      return false;
    }

    LOG_TOPIC("28600", DEBUG, Logger::MAINTENANCE)
        << "synchronizeOneShard: waiting for leader, " << database << "/"
        << shard << ", " << database << "/" << planId;

    std::this_thread::sleep_for(duration<double>(0.2));
  }

  // Once we get here, we know that the leader is ready for sync, so we give it
  // a try:

  try {
    auto& df = _feature.server().getFeature<DatabaseFeature>();
    DatabaseGuard guard(df, database);
    auto vocbase = &guard.database();

    auto collection = vocbase->lookupCollection(shard);
    if (collection == nullptr) {
      std::stringstream error;
      error << "failed to lookup local shard " << database << "/" << shard;
      LOG_TOPIC("06489", ERR, Logger::MAINTENANCE) << "SynchronizeOneShard: " << error.str();
      _result.reset(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, error.str());
      return false;
    }

    auto ep = clusterInfo.getServerEndpoint(leader);
    uint64_t docCount = 0;
    if (!collectionCount(*collection, docCount).ok()) {
      std::stringstream error;
      error << "failed to get a count on leader " << database << "/" << shard;
      LOG_TOPIC("da225", ERR, Logger::MAINTENANCE) << "SynchronizeShard " << error.str();
      _result.reset(TRI_ERROR_INTERNAL, error.str());
      return false;
    }

    { // Initialize _clientInfoString
      CollectionNameResolver resolver(collection->vocbase());
      _clientInfoString =
          std::string{"follower "} + ServerState::instance()->getId() +
          " of shard " + database + "/" + collection->name() + " of collection " + database +
          "/" + resolver.getCollectionName(collection->id());
    }

    // old "shortcut" code for getting in sync. This code takes a shortcut if the
    // shard in question is supposed to be empty. in this case it will simply try
    // to add itself as an in-sync follower, without running the full replication
    // protocol. this shortcut relies on the collection counts being correct, and
    // as these can be at least temporarily off, we need to disable it.
#if 0
    if (docCount == 0) {
      // We have a short cut:
      LOG_TOPIC("0932a", DEBUG, Logger::MAINTENANCE)
          << "synchronizeOneShard: trying short cut to synchronize local shard "
             "'"
          << database << "/" << shard << "' for central '" << database << "/"
          << planId << "'";

      // now do a final sync-to-disk call. note that this can fail
      auto& engine = vocbase->server().getFeature<EngineSelectorFeature>().engine();
      Result res = engine.flushWal(/*waitForSync*/ true, /*waitForCollector*/ false);
      if (res.fail()) {
        LOG_TOPIC("a49d1", INFO, Logger::MAINTENANCE) << res.errorMessage();
        _result.reset(res);
        return false;
      }

      try {
        NetworkFeature& nf = _feature.server().getFeature<NetworkFeature>();
        network::ConnectionPool* pool = nf.pool();
        auto asResult = addShardFollower(pool, ep, database, shard, 0, clientId,
                                         SyncerId{}, _clientInfoString, 60.0);

        if (asResult.ok()) {
          if (Logger::isEnabled(LogLevel::DEBUG, Logger::MAINTENANCE)) {
            std::stringstream msg;
            msg << "SynchronizeShard: shortcut worked, done, ";
            AppendShardInformationToMessage(database, shard, planId, startTime, msg);
            LOG_TOPIC("f4a5b", DEBUG, Logger::MAINTENANCE) << msg.str();
          }
          collection->followers()->setTheLeader(leader);
          return false;
        }
        if (asResult.errorNumber() != TRI_ERROR_REPLICATION_SHARD_NONEMPTY) {
          // Stop action in this case
          LOG_TOPIC("daaaa", INFO, Logger::MAINTENANCE) << "SynchronizeShard, error in addFollower (short cut): " << asResult.errorMessage();
          _result.reset(asResult);
          return false;
        }
        // Otherwise move on.
      } catch (...) {
      }
    }
#endif

    LOG_TOPIC("53337", DEBUG, Logger::MAINTENANCE)
        << "synchronizeOneShard: trying to synchronize local shard '" << database
        << "/" << shard << "' for central '" << database << "/" << planId << "'";

    try {
      // From here on we perform a number of steps, each of which can
      // fail. If it fails with an exception, it is caught, but this
      // should usually not happen. If it fails without an exception,
      // we log and use return.

      // First once without a read transaction:

      if (feature().server().isStopping()) {
        std::string errorMessage(
          "SynchronizeShard: synchronization failed for shard ");
        errorMessage += shard + ": shutdown in progress, giving up";
        LOG_TOPIC("a0f9a", INFO, Logger::MAINTENANCE) << errorMessage;
        _result.reset(TRI_ERROR_SHUTTING_DOWN, errorMessage);
        return false;
      }

      // This is necessary to accept replications from the leader which can
      // happen as soon as we are in sync.
      collection->followers()->setTheLeader(leader);

      startTime = system_clock::now();

      VPackBuilder config;
      {
        VPackObjectBuilder o(&config);
        config.add(ENDPOINT, VPackValue(ep));
        config.add(INCREMENTAL,
                   VPackValue(docCount > 0));  // use dump if possible
        config.add(LEADER_ID, VPackValue(leader));
        config.add(SKIP_CREATE_DROP, VPackValue(true));
        config.add(RESTRICT_TYPE, VPackValue(INCLUDE));
        config.add(VPackValue(RESTRICT_COLLECTIONS));
        {
          VPackArrayBuilder a(&config);
          config.add(VPackValue(shard));
        }
        config.add(INCLUDE_SYSTEM, VPackValue(true));
        config.add("verbose", VPackValue(false));
      }

      auto details = std::make_shared<VPackBuilder>();

      ResultT<SyncerId> syncRes =
          replicationSynchronize(*this, collection, config.slice(), details);

      auto sy = details->slice();
      auto const endTime = system_clock::now();

      // Long shard sync initialization
      if (endTime - startTime > seconds(5)) {
        LOG_TOPIC("ca7e3", INFO, Logger::MAINTENANCE)
            << "synchronizeOneShard: long call to syncCollection for shard"
            << database << "/" << shard << " " << syncRes.errorMessage()
            << " start time: " << timepointToString(startTime)
            << ", end time: " << timepointToString(system_clock::now());
      }

      // If this did not work, then we cannot go on:
      if (!syncRes.ok()) {
        std::stringstream error;
        error << "could not initially synchronize shard " << database << "/" << shard << ": "
              << syncRes.errorMessage();
        LOG_TOPIC("c1b31", DEBUG, Logger::MAINTENANCE) << "SynchronizeOneShard: " << error.str();
        _result.reset(TRI_ERROR_INTERNAL, error.str());
        return false;
      }

      SyncerId syncerId = syncRes.get();

      VPackSlice collections = sy.get(COLLECTIONS);
      if (collections.length() == 0 || collections[0].get("name").copyString() != shard) {
        std::stringstream error;
        error << "shard " << database << "/" << shard << " seems to be gone from leader, this "
               "can happen if a collection was dropped during synchronization!";
        LOG_TOPIC("664ae", WARN, Logger::MAINTENANCE) << "SynchronizeOneShard: " << error.str();
        _result.reset(TRI_ERROR_INTERNAL, error.str());
        return false;
      }

      auto lastTick =
        arangodb::basics::VelocyPackHelper::getNumericValue<TRI_voc_tick_t>(
          sy, LAST_LOG_TICK, 0);
      VPackBuilder builder;

      ResultT<TRI_voc_tick_t> tickResult =
        catchupWithReadLock(ep, database, *collection, clientId, shard,
                            leader, lastTick, builder);
      if (!tickResult.ok()) {
        LOG_TOPIC("0a4d4", INFO, Logger::MAINTENANCE) << tickResult.errorMessage();
        _result.reset(std::move(tickResult).result());
        return false;
      }
      lastTick = tickResult.get();

      // Now start an exclusive transaction to stop writes:
      Result res = catchupWithExclusiveLock(ep, database, *collection, clientId, shard,
                                            leader, syncerId, lastTick, builder);
      if (!res.ok()) {
        LOG_TOPIC("be85f", INFO, Logger::MAINTENANCE) << res.errorMessage();
        _result.reset(res);
        return false;
      }
    } catch (basics::Exception const& e) {
      // don't log errors for already dropped databases/collections
      if (e.code() != TRI_ERROR_ARANGO_DATABASE_NOT_FOUND &&
          e.code() != TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND) {
        std::stringstream error;
        error << "synchronization of ";
        AppendShardInformationToMessage(database, shard, planId, startTime, error);
        error << " failed: " << e.what();
        LOG_TOPIC("65d6f", ERR, Logger::MAINTENANCE) << error.str();
      }
      _result.reset(e.code(), e.what());
      return false;
    } catch (std::exception const& e) {
      std::stringstream error;
      error << "synchronization of ";
      AppendShardInformationToMessage(database, shard, planId, startTime, error);
      error << " failed: " << e.what();
      LOG_TOPIC("1e576", ERR, Logger::MAINTENANCE) << error.str();
      _result.reset(TRI_ERROR_INTERNAL, e.what());
      return false;
    }
    // Validate that HARDLOCK only works!
  } catch (std::exception const& e) {
    // This catches the case that we could not even find the collection
    // locally, because the DatabaseGuard constructor threw.
    LOG_TOPIC("9f2c0", WARN, Logger::MAINTENANCE)
        << "action " << _description << " failed with exception " << e.what();
    _result.reset(TRI_ERROR_INTERNAL, e.what());
    return false;
  }

  // Tell others that we are done:
  if (Logger::isEnabled(LogLevel::INFO, Logger::MAINTENANCE)) {
    // This wrap is just to not write the stream if not needed.
    std::stringstream msg;
    AppendShardInformationToMessage(database, shard, planId, startTime, msg);
    LOG_TOPIC("e6780", INFO, Logger::MAINTENANCE) << "synchronizeOneShard: done, " << msg.str();
  }
  return false;
}

ResultT<TRI_voc_tick_t> SynchronizeShard::catchupWithReadLock(
  std::string const& ep, std::string const& database, LogicalCollection const& collection,
  std::string const& clientId, std::string const& shard,
  std::string const& leader, TRI_voc_tick_t lastLogTick, VPackBuilder& builder) {
  bool didTimeout = true;
  int tries = 0;
  double timeout = 300.0;
  TRI_voc_tick_t tickReached = 0;
  while (didTimeout && tries++ < 18) {  // This will try to sync for at most 1 hour. (200 * 18 == 3600)

    if (feature().server().isStopping()) {
      std::string errorMessage =
        "SynchronizeShard: startReadLockOnLeader (soft): shutting down";
      return ResultT<TRI_voc_tick_t>::error(TRI_ERROR_SHUTTING_DOWN, errorMessage);
    }

    didTimeout = false;
    // Now ask for a "soft stop" on the leader, in case of mmfiles, this
    // will be a hard stop, but for rocksdb, this is a no-op:
    uint64_t lockJobId = 0;
    LOG_TOPIC("b4f2b", DEBUG, Logger::MAINTENANCE)
        << "synchronizeOneShard: startReadLockOnLeader (soft): " << ep << ":"
        << database << ":" << collection.name();
    Result res = startReadLockOnLeader(ep, database, collection.name(),
                                       clientId, lockJobId, true, timeout);
    if (!res.ok()) {
      auto errorMessage = StringUtils::concatT(
          "SynchronizeShard: error in startReadLockOnLeader (soft):", res.errorMessage());
      return ResultT<TRI_voc_tick_t>::error(TRI_ERROR_INTERNAL, errorMessage);
    }

    auto readLockGuard = arangodb::scopeGuard([&, this]() {
      // Always cancel the read lock.
      // Reported seperately
      NetworkFeature& nf = _feature.server().getFeature<NetworkFeature>();
      network::ConnectionPool* pool = nf.pool();
      auto res = cancelReadLockOnLeader(pool, ep, database, lockJobId, clientId, 60.0);
      if (!res.ok()) {
        LOG_TOPIC("b15ee", INFO, Logger::MAINTENANCE)
            << "Could not cancel soft read lock on leader: " << res.errorMessage();
        }
      });

    LOG_TOPIC("5eb37", DEBUG, Logger::MAINTENANCE) << "lockJobId: " << lockJobId;

    // From now on, we need to cancel the read lock on the leader regardless
    // if things go wrong or right!

    // Do a first try of a catch up with the WAL. In case of RocksDB,
    // this has not yet stopped the writes, so we have to be content
    // with nearly reaching the end of the WAL, which is a "soft" catchup.
    builder.clear();
    {
      VPackObjectBuilder o(&builder);
      builder.add(ENDPOINT, VPackValue(ep));
      builder.add(DATABASE, VPackValue(database));
      builder.add(COLLECTION, VPackValue(shard));
      builder.add(LEADER_ID, VPackValue(leader));
      builder.add("from", VPackValue(lastLogTick));
      builder.add("requestTimeout", VPackValue(600.0));
      builder.add("connectTimeout", VPackValue(60.0));
    }

    // We only allow to hold this lock for 60% of the timeout time, so to avoid
    // any issues with Locks timeouting on the Leader and the Client not
    // recognizing it.
    res = replicationSynchronizeCatchup(*this, feature().server(), builder.slice(),
                                        timeout * 0.6, tickReached, didTimeout);

    if (!res.ok()) {
      std::string errorMessage(
        "synchronizeOneshard: error in syncCollectionCatchup: ");
      errorMessage += res.errorMessage();
      return ResultT<TRI_voc_tick_t>::error(TRI_ERROR_INTERNAL, errorMessage);
    }

    // Stop the read lock again:
    NetworkFeature& nf = _feature.server().getFeature<NetworkFeature>();
    network::ConnectionPool* pool = nf.pool();
    res = cancelReadLockOnLeader(pool, ep, database, lockJobId, clientId, 60.0);
    // We removed the readlock
    readLockGuard.cancel();
    if (!res.ok()) {
      auto errorMessage = StringUtils::concatT(
          "synchronizeOneShard: error when cancelling soft read lock: ", res.errorMessage());
      LOG_TOPIC("c37d1", INFO, Logger::MAINTENANCE) << errorMessage;
      _result.reset(TRI_ERROR_INTERNAL, errorMessage);
      return ResultT<TRI_voc_tick_t>::error(TRI_ERROR_INTERNAL, errorMessage);
    }
    lastLogTick = tickReached;
    if (didTimeout) {
      LOG_TOPIC("e516e", INFO, Logger::MAINTENANCE)
        << "Renewing softLock for " << shard << " on leader: " << leader;
    }
  }
  if (didTimeout) {
    LOG_TOPIC("f1a61", WARN, Logger::MAINTENANCE)
        << "Could not catchup under softLock for " << shard << " on leader: " << leader
        << " now activating hardLock. This is expected under high load.";
  }
  return ResultT<TRI_voc_tick_t>::success(tickReached);
}

Result SynchronizeShard::catchupWithExclusiveLock(
    std::string const& ep, std::string const& database, LogicalCollection& collection,
    std::string const& clientId, std::string const& shard, std::string const& leader,
    SyncerId const syncerId, TRI_voc_tick_t lastLogTick, VPackBuilder& builder) {
  uint64_t lockJobId = 0;
  LOG_TOPIC("da129", DEBUG, Logger::MAINTENANCE)
      << "synchronizeOneShard: startReadLockOnLeader: " << ep << ":" << database
      << ":" << collection.name();
  Result res = startReadLockOnLeader(ep, database, collection.name(), clientId,
                                     lockJobId, false);
  if (!res.ok()) {
    auto errorMessage = StringUtils::concatT(
        "SynchronizeShard: error in startReadLockOnLeader (hard):", res.errorMessage());
    return {TRI_ERROR_INTERNAL, std::move(errorMessage)};
  }
  auto readLockGuard = arangodb::scopeGuard([&, this]() {
    // Always cancel the read lock.
    // Reported seperately
    NetworkFeature& nf = _feature.server().getFeature<NetworkFeature>();
    network::ConnectionPool* pool = nf.pool();
    auto res = cancelReadLockOnLeader(pool, ep, database, lockJobId, clientId, 60.0);
    if (!res.ok()) {
      LOG_TOPIC("067a8", INFO, Logger::MAINTENANCE)
          << "Could not cancel hard read lock on leader: " << res.errorMessage();
      }
    });

  LOG_TOPIC("d76cb", DEBUG, Logger::MAINTENANCE) << "lockJobId: " << lockJobId;

  builder.clear();
  {
    VPackObjectBuilder o(&builder);
    builder.add(ENDPOINT, VPackValue(ep));
    builder.add(DATABASE, VPackValue(database));
    builder.add(COLLECTION, VPackValue(shard));
    builder.add(LEADER_ID, VPackValue(leader));
    builder.add("from", VPackValue(lastLogTick));
    builder.add("requestTimeout", VPackValue(600.0));
    builder.add("connectTimeout", VPackValue(60.0));
  }

  res = replicationSynchronizeFinalize(*this, feature().server(), builder.slice());

  if (!res.ok()) {
    std::string errorMessage(
      "synchronizeOneshard: error in syncCollectionFinalize: ");
    errorMessage += res.errorMessage();
    return {TRI_ERROR_INTERNAL, errorMessage};
  }

  NetworkFeature& nf = _feature.server().getFeature<NetworkFeature>();
  network::ConnectionPool* pool = nf.pool();
  res = addShardFollower(pool, ep, database, shard, lockJobId, clientId,
                         syncerId, _clientInfoString, 60.0);

  // if we get a checksum mismatch, it means that we got different counts of
  // documents on the leader and the follower, which can happen if collection
  // counts are off for whatever reason. 
  // under many cicrumstances the counts will have been auto-healed by the initial
  // or the incremental replication before, so in many cases we will not even get
  // into this if case
  if (res.is(TRI_ERROR_REPLICATION_WRONG_CHECKSUM)) {
    // give up the lock on the leader, so writes aren't stopped unncessarily
    // on the leader while we are recalculating the counts
    readLockGuard.fire();
    
    collection.vocbase().server().getFeature<ClusterFeature>().followersWrongChecksumCounter()++;

    // recalculate collection count on follower
    LOG_TOPIC("29384", INFO, Logger::MAINTENANCE) 
       << "recalculating collection count on follower for "
       << database << "/" << shard;

    uint64_t docCount = 0;
    Result countRes = collectionCount(collection, docCount);
    if (countRes.fail()) {
      return countRes;
    }
    // store current count value
    uint64_t oldCount = docCount;

    // recalculate on follower. this can take a long time
    countRes = collectionReCount(collection, docCount);
    if (countRes.fail()) {
      return countRes;
    }

    LOG_TOPIC("d2689", INFO, Logger::MAINTENANCE) 
       << "recalculated collection count on follower for "
       << database << "/" << shard << ", old: " << oldCount << ", new: " << docCount;

    // check if our recalculation has made a difference
    if (oldCount == docCount) {
      // no change happened due to recalculation. now try recounting on leader too.
      // this is last resort and should not happen often!
      LOG_TOPIC("3dc64", INFO, Logger::MAINTENANCE) 
         << "recalculating collection count on leader for "
         << database << "/" << shard;

      VPackBuffer<uint8_t> buffer;
      VPackBuilder tmp(buffer);
      tmp.add(VPackSlice::emptyObjectSlice());

      network::RequestOptions options;
      options.database = database;
      options.timeout = network::Timeout(600.0);  // this can be slow!!!
      options.skipScheduler = true;  // hack to speed up future.get()

      std::string const url = "/_api/collection/" + collection.name() + "/recalculateCount";

      auto response = network::sendRequest(pool, ep, fuerte::RestVerb::Put,
                                           url, std::move(buffer), options)
                          .get();
      auto result = response.combinedResult();

      if (result.fail()) {
        auto const errorMessage = StringUtils::concatT(
            "addShardFollower: could not add us to the leader's follower list "
            "for ",
            database, "/", shard,
            ", error while recalculating count on leader: ", result.errorMessage());
        LOG_TOPIC("22e0b", WARN, Logger::MAINTENANCE) << errorMessage;
        return arangodb::Result(result.errorNumber(), std::move(errorMessage));
      } else {
        auto const resultSlice = response.slice();
        if (VPackSlice c = resultSlice.get("count"); c.isNumber()) {
          LOG_TOPIC("bc26d", DEBUG, Logger::MAINTENANCE) << "leader's shard count response is " << c.getNumber<uint64_t>();
        }
      }
    }

    // still let the operation fail here, because we gave up the lock 
    // already and cannot be sure the data on the leader hasn't changed in
    // the meantime. we will sort this issue out during the next maintenance
    // run
    TRI_ASSERT(res.fail());
    TRI_ASSERT(res.is(TRI_ERROR_REPLICATION_WRONG_CHECKSUM));
    return res;
  }

  // no more retrying...
  if (!res.ok()) {
    std::string errorMessage(
      "synchronizeOneshard: error in addShardFollower: ");
    errorMessage += res.errorMessage();
    return {TRI_ERROR_INTERNAL, errorMessage};
  }

  // Report success:
  LOG_TOPIC("3423d", DEBUG, Logger::MAINTENANCE)
      << "synchronizeOneShard: synchronization worked for shard " << shard;
  _result.reset(TRI_ERROR_NO_ERROR);
  return {TRI_ERROR_NO_ERROR};
}

void SynchronizeShard::setState(ActionState state) {
  if ((COMPLETE == state || FAILED == state) && _state != state) {
    auto const& shard = _description.get(SHARD);
    std::string database = _description.get(DATABASE);
    if (COMPLETE == state) {
      LOG_TOPIC("50827", INFO, Logger::MAINTENANCE)
        << "SynchronizeShard: synchronization completed for shard " << database << "/" << shard;
    }

    // Acquire current version from agency and wait for it to have been dealt
    // with in local current cache. Any future current version will do, as
    // the version is incremented by the leader ahead of getting here on the
    // follower.
    uint64_t v = 0;
    using namespace std::chrono;
    using clock = steady_clock;
    auto timeout = duration<double>(600.0);
    auto stoppage = clock::now() + timeout;
    auto snooze = milliseconds(100);
    while (!_feature.server().isStopping() && clock::now() < stoppage ) {
      cluster::fetchCurrentVersion(0.1 * timeout)
        .thenValue(
          [&v] (auto&& res) { v = res.get(); })
        .thenError<std::exception>(
          [&shard, database] (std::exception const& e) {
            LOG_TOPIC("3ae99", ERR, Logger::CLUSTER)
              << "Failed to acquire current version from agency while increasing shard version"
              << " for shard "  << database << "/" << shard << e.what();
          })
        .wait();
      if (v > 0) {
        break;
      }
      std::this_thread::sleep_for(snooze);
      if (snooze < seconds(2)) {
        snooze += milliseconds(100);
      }
    }

    // We're here, cause we either ran out of time or have an actual version number.
    // In the former case, we tried our best and will safely continue some 10 min later.
    // If however v is an actual positive integer, we'll wait for it to sync in out
    // ClusterInfo cache through loadCurrent.
    if ( v > 0) {
      _feature.server().getFeature<ClusterFeature>().clusterInfo().waitForCurrentVersion(v).wait();
    }
    _feature.incShardVersion(shard);
    _feature.unlockShard(shard);
  }
  ActionBase::setState(state);
}
