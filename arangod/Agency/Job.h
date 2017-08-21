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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CONSENSUS_JOB_H
#define ARANGOD_CONSENSUS_JOB_H 1

#include "AgentInterface.h"
#include "Node.h"
#include "Supervision.h"

#include "Basics/Result.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <string>

namespace arangodb {
namespace consensus {

enum JOB_STATUS { TODO, PENDING, FINISHED, FAILED, NOTFOUND };
const std::vector<std::string> pos({"/Target/ToDo/", "/Target/Pending/",
                                    "/Target/Finished/", "/Target/Failed/"});
static std::string const mapUniqueToShortID = "/Target/MapUniqueToShortID/";
static std::string const pendingPrefix = "/Target/Pending/";
static std::string const failedPrefix = "/Target/Failed/";
static std::string const finishedPrefix = "/Target/Finished/";
static std::string const toDoPrefix = "/Target/ToDo/";
static std::string const cleanedPrefix = "/Target/CleanedServers";
static std::string const failedServersPrefix = "/Target/FailedServers";
static std::string const planColPrefix = "/Plan/Collections/";
static std::string const curColPrefix = "/Current/Collections/";
static std::string const blockedServersPrefix = "/Supervision/DBServers/";
static std::string const blockedShardsPrefix = "/Supervision/Shards/";
static std::string const serverStatePrefix = "/Sync/ServerStates/";
static std::string const planVersion = "/Plan/Version";
static std::string const plannedServers = "/Plan/DBServers";
static std::string const healthPrefix = "/Supervision/Health/";

struct JobResult {
  JobResult() {}
};

struct JobCallback {
  JobCallback() {}
  virtual ~JobCallback(){};
  virtual bool operator()(JobResult*) = 0;
};

struct Job {

  struct shard_t {
    std::string collection;
    std::string shard;
    shard_t (std::string const& c, std::string const& s) :
      collection(c), shard(s) {}
  };
  
  Job(JOB_STATUS status, Node const& snapshot, AgentInterface* agent,
      std::string const& jobId, std::string const& creator = std::string());

  virtual ~Job();

  virtual void run() = 0;

  void runHelper(std::string const& server, std::string const& shard) {
    if (_status == FAILED) {  // happens when the constructor did not work
      return;
    }
    try {
      status();   // This runs everything to to with state PENDING if needed!
    } catch (std::exception const& e) {
      LOG_TOPIC(WARN, Logger::AGENCY) << "Exception caught in status() method: "
        << e.what() << ": " << __FILE__
        << ":" << __LINE__;
      finish(server, shard, false, e.what());
    }
    try {
      if (_status == TODO) {
        start();
      } else if (_status == NOTFOUND) {
        if (create(nullptr)) {
          start();
        }
      }
    } catch (std::exception const& e) {
      LOG_TOPIC(WARN, Logger::AGENCY) << "Exception caught in create() or "
        "start() method: " << e.what() << ": " << __FILE__ << ":" << __LINE__;
      finish("", "", false, e.what());
    }
  }

  virtual Result abort() = 0;

  virtual bool finish(
    std::string const& server, std::string const& shard, bool success = true,
    std::string const& reason = std::string(), query_t const payload = nullptr);
  
  virtual JOB_STATUS status() = 0;

  virtual bool create(std::shared_ptr<VPackBuilder> b) = 0;

  // Returns if job was actually started (i.e. false if directly failed!)
  virtual bool start() = 0;

  static bool abortable(Node const& snapshot, std::string const& jobId);

  std::string id(std::string const& idOrShortName);
  std::string uuidLookup(std::string const& shortID);

  /// @brief Get a random server, which is not blocked, in good condition and
  ///        excluding "exclude" vector
  static std::string randomIdleGoodAvailableServer(
    Node const& snap, std::vector<std::string> const& exclude);
  static std::string randomIdleGoodAvailableServer(
    Node const& snap, VPackSlice const& exclude);
  
  static std::vector<std::string> availableServers(
    const arangodb::consensus::Node&);

  static std::vector<shard_t> clones(
    Node const& snap, std::string const& db, std::string const& col,
    std::string const& shrd);

  static std::string findNonblockedCommonHealthyInSyncFollower(
    Node const& snap, std::string const& db, std::string const& col,
    std::string const& shrd);

  JOB_STATUS _status;
  Node const _snapshot;
  AgentInterface* _agent;
  std::string _jobId;
  std::string _creator;
  static std::string agencyPrefix;  // will be initialized in AgencyFeature

  std::shared_ptr<Builder> _jb;
  
  static void doForAllShards(Node const& snapshot,
    std::string& database,
    std::vector<shard_t>& shards,
    std::function<void(Slice plan, Slice current, std::string& planPath)> worker);

  // The following methods adds an operation to a transaction object or
  // a condition to a precondition object. In all cases, the builder trx
  // or pre must be in the state that an object has been opened, this
  // method adds some attribute/value pairs and leaves the object open:
  static void addIncreasePlanVersion(Builder& trx);
  static void addRemoveJobFromSomewhere(Builder& trx, std::string const& where,
                                        std::string const& jobId);
  static void addPutJobIntoSomewhere(Builder& trx, std::string const& where,
    Slice job, std::string const& reason = "");
  static void addPreconditionCollectionStillThere(Builder& pre,
    std::string const& database, std::string const& collection);
  static void addBlockServer(Builder& trx, std::string const& server,
                             std::string const& jobId);
  static void addBlockShard(Builder& trx, std::string const& shard, std::string const& jobId);
  static void addReleaseServer(Builder& trx, std::string const& server);
  static void addReleaseShard(Builder& trx, std::string const& shard);
  static void addPreconditionServerNotBlocked(Builder& pre, std::string const& server);
  static void addPreconditionServerGood(Builder& pre, std::string const& server);
  static void addPreconditionShardNotBlocked(Builder& pre, std::string const& shard);
  static void addPreconditionUnchanged(Builder& pre,
    std::string const& key, Slice value);
  static std::string checkServerGood(Node const& snapshot,
                                     std::string const& server);

};

inline arangodb::consensus::write_ret_t singleWriteTransaction(
  AgentInterface* _agent,
  Builder const& transaction,
  bool waitForCommit = true) {
  
  query_t envelope = std::make_shared<Builder>();
  
  Slice trx = transaction.slice();
  try {
    { VPackArrayBuilder listOfTrxs(envelope.get());
      VPackArrayBuilder onePair(envelope.get());
      { VPackObjectBuilder mutationPart(envelope.get());
        for (auto const& pair : VPackObjectIterator(trx[0])) {
          envelope->add(Job::agencyPrefix + pair.key.copyString(), pair.value);
        }
      }
      if (trx.length() > 1) {
        VPackObjectBuilder preconditionPart(envelope.get());
        for (auto const& pair : VPackObjectIterator(trx[1])) {
          envelope->add(Job::agencyPrefix + pair.key.copyString(), pair.value);
        }
      }
    }
  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, Logger::SUPERVISION)
      << "Supervision failed to build transaction.";
    LOG_TOPIC(ERR, Logger::SUPERVISION) << e.what() << " " << __FILE__ << __LINE__;
  }
  
  auto ret = _agent->write(envelope);
  if (waitForCommit && !ret.indices.empty()) {
    auto maximum = *std::max_element(ret.indices.begin(), ret.indices.end());
    if (maximum > 0) {  // some baby has worked
      _agent->waitFor(maximum);
    }
  }
  return ret;
}

inline arangodb::consensus::trans_ret_t generalTransaction(
  AgentInterface* _agent, Builder const& transaction) {
  
  query_t envelope = std::make_shared<Builder>();
  Slice trx = transaction.slice();
  
  try {
    { VPackArrayBuilder listOfTrxs(envelope.get());
      for (auto const& singleTrans : VPackArrayIterator(trx)) {
        
        TRI_ASSERT(singleTrans.isArray() && singleTrans.length() > 0);
        if (singleTrans[0].isObject()) {
          VPackArrayBuilder onePair(envelope.get());
          { VPackObjectBuilder mutationPart(envelope.get());
            for (auto const& pair : VPackObjectIterator(singleTrans[0])) {
              envelope->add(Job::agencyPrefix + pair.key.copyString(), pair.value);
            }
          }
          if (singleTrans.length() > 1) {
            VPackObjectBuilder preconditionPart(envelope.get());
            for (auto const& pair : VPackObjectIterator(singleTrans[1])) {
              envelope->add(Job::agencyPrefix + pair.key.copyString(), pair.value);
            }
          }
        } else if (singleTrans[0].isString()) {
          VPackArrayBuilder reads(envelope.get());
          for (auto const& path : VPackArrayIterator(singleTrans)) {
            envelope->add(VPackValue(Job::agencyPrefix + path.copyString()));
          }
        }
      }
    }
  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, Logger::SUPERVISION)
      << "Supervision failed to build transaction.";
    LOG_TOPIC(ERR, Logger::SUPERVISION) << e.what() << " " << __FILE__ << __LINE__;
  }

  auto ret = _agent->transact(envelope);
  
  if (ret.maxind > 0) {
    _agent->waitFor(ret.maxind);
  }
  
  return ret;

}

inline arangodb::consensus::trans_ret_t transient(AgentInterface* _agent,
                                                  Builder const& transaction,
                                                  bool waitForCommit = true) {
  query_t envelope = std::make_shared<Builder>();

  Slice trx = transaction.slice();
  try {
    { VPackArrayBuilder listOfTrxs(envelope.get());
      VPackArrayBuilder onePair(envelope.get());
      { VPackObjectBuilder mutationPart(envelope.get());
        for (auto const& pair : VPackObjectIterator(trx[0])) {
          envelope->add(Job::agencyPrefix + pair.key.copyString(), pair.value);
        }
      }
      if (trx.length() > 1) {
        VPackObjectBuilder preconditionPart(envelope.get());
        for (auto const& pair : VPackObjectIterator(trx[1])) {
          envelope->add(Job::agencyPrefix + pair.key.copyString(), pair.value);
        }
      }
    }
  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, Logger::SUPERVISION)
        << "Supervision failed to build transaction.";
    LOG_TOPIC(ERR, Logger::SUPERVISION) << e.what() << " " << __FILE__ << __LINE__;
  }

  
  return _agent->transient(envelope);
}

}  // namespace consensus
}  // namespace arangodb

#endif
