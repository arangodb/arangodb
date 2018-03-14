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
#include "Cluster/ClusterFeature.h"
#include "Cluster/FollowerInfo.h"
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

std::string const DB = "/_db/";

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
  std::string const& endpoint, std::string const& database, uint64_t& id) {

  std::string error("startReadLockOnLeader: Failed to get read lock - ");
  
  auto comm = ClusterComm::instance();
  if (cc == nullptr) { // nullptr only happens during controlled shutdown
    return arangodb::Result(
      ERROR_SHUTTING_DOWN, "startReadLockOnLeader: Shutting down");
  }
  
  auto comres = cc->syncRequest(
    std::hash(_description), 1, endpoint, rest::RequestType::GET,
    DB + database + REPL_HOLD_READ_LOCK, std::string(),
    std::unordered_map<std::string, std::string>(), timeout);
  
  if (comres->status == CL_COMM_SENT) {
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
  } else {
    return arangodb::Result(
      ERROR_SHUTTING_DOWN,
      "startReadLockOnLeader: Failed to get read lock - GET request never sent ");
  }
  
  return arangodb::Result();
  
}


arangodb::Result getReadLock(std::string const& endpoint, std::string const& ) {

  auto comm = ClusterComm::instance();
  if (cc == nullptr) { // nullptr only happens during controlled shutdown
    return arangodb::Result(
      ERROR_SHUTTING_DOWN, "startReadLockOnLeader: Shutting down");
  }

  VPackBuilder b;
  { VPackObjectBuilder(&b);
    b.add(ID, VPackValue(rlid));
    b.add(COLLECTION, collection);
    b.add(TTL, elapsed);
  }

  auto hf =
    std::make_unique<std::unordered_map<std::string, std::string>>();
  
  auto postres = cc->asyncRequest(
    std::hash(_description), 2, endpoint, rest::RequestType::POST,
    REPL_HOLD_READ_LOCK, b.toJson(), hf,
    std::make_shared<SynchronizeShardCallback>(this), 1.0, true, 0.5);

  // Intentionally do not look at the outcome, even in case of an error
  // we must make sure that the read lock on the leader is not active!
  // This is done automatically below.

  size_t count = 0;
  while (++count < 20) { // wait for some time until read lock established:
    // Now check that we hold the read lock:
    r = request({ url: url + '/_api/replication/holdReadLockCollection',
                  body: JSON.stringify(body), method: 'PUT' });

    comre
    
    if (r.status === 200) {
      let ansBody = {};
      try {
        ansBody = JSON.parse(r.body);
      } catch (err) {
      }
      if (ansBody.lockHeld) {
        return id;
      } else {
        console.topic('heartbeat=debug', 'startReadLockOnLeader: Lock not yet acquired...');
      }
    } else {
      console.topic('heartbeat=debug', 'startReadLockOnLeader: Do not see read lock yet...');
    }
    wait(0.5);
  }
  console.topic('heartbeat=error', 'startReadLockOnLeader: giving up');
  try {
    r = request({ url: url + '/_api/replication/holdReadLockCollection',
                  body: JSON.stringify({'id': id}), method: 'DELETE' });
  } catch (err2) {
    console.topic('heartbeat=error', 'startReadLockOnLeader: expection in cancel:',
                  JSON.stringify(err2));
  }
  if (r.status !== 200) {
    console.topic('heartbeat=error', 'startReadLockOnLeader: cancelation error for shard',
                  collName, r);
  }
  return false;

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


