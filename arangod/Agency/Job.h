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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CONSENSUS_JOB_H
#define ARANGOD_CONSENSUS_JOB_H 1

#include <stddef.h>
#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "Agency/AgencyCommon.h"
#include "Agency/AgentInterface.h"
#include "Basics/Result.h"
#include "Basics/debugging.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

namespace arangodb {
namespace consensus {
class Node;

enum JOB_STATUS { TODO, PENDING, FINISHED, FAILED, NOTFOUND };
const std::vector<std::string> pos({"/Target/ToDo/", "/Target/Pending/",
                                    "/Target/Finished/", "/Target/Failed/"});
extern std::string const mapUniqueToShortID;
extern std::string const pendingPrefix;
extern std::string const failedPrefix;
extern std::string const finishedPrefix;
extern std::string const toDoPrefix;
extern std::string const cleanedPrefix;
extern std::string const toBeCleanedPrefix;
extern std::string const failedServersPrefix;
extern std::string const planColPrefix;
extern std::string const curColPrefix;
extern std::string const planDBPrefix;
extern std::string const curServersKnown;
extern std::string const blockedServersPrefix;
extern std::string const blockedShardsPrefix;
extern std::string const planVersion;
extern std::string const currentVersion;
extern std::string const plannedServers;
extern std::string const healthPrefix;
extern std::string const asyncReplLeader;
extern std::string const asyncReplTransientPrefix;
extern std::string const planAnalyzersPrefix;

struct Job {
  struct shard_t {
    std::string collection;
    std::string shard;
    shard_t(std::string const& c, std::string const& s)
        : collection(c), shard(s) {}
  };

  Job(JOB_STATUS status, Node const& snapshot, AgentInterface* agent,
      std::string const& jobId, std::string const& creator = std::string());

  virtual ~Job();

  virtual void run(bool& aborts) = 0;

  void runHelper(std::string const& server, std::string const& shard, bool& aborts) {
    if (_status == FAILED) {  // happens when the constructor did not work
      return;
    }
    try {
      status();  // This runs everything to to with state PENDING if needed!
    } catch (std::exception const& e) {
      LOG_TOPIC("e2d06", WARN, Logger::AGENCY)
          << "Exception caught in status() method: " << e.what();
      finish(server, shard, false, e.what());
    }
    try {
      if (_status == TODO) {
        start(aborts);
      } else if (_status == NOTFOUND) {
        if (create(nullptr)) {
          start(aborts);
        }
      }
    } catch (std::exception const& e) {
      LOG_TOPIC("5ac04", WARN, Logger::AGENCY) << "Exception caught in create() or "
                                         "start() method: "
                                      << e.what();
      finish("", "", false, e.what());
    }
  }

  virtual Result abort(std::string const& reason) = 0;

  virtual bool finish(std::string const& server, std::string const& shard,
                      bool success = true, std::string const& reason = std::string(),
                      query_t const payload = nullptr);

  virtual JOB_STATUS status() = 0;

  virtual bool create(std::shared_ptr<velocypack::Builder> b) = 0;

  // Returns if job was actually started (i.e. false if directly failed!)
  virtual bool start(bool& aborts) = 0;

  static bool abortable(Node const& snapshot, std::string const& jobId);

  std::string id(std::string const& idOrShortName);
  std::string uuidLookup(std::string const& shortID);

  /// @brief Get a random server, which is not blocked, in good condition and
  ///        excluding "exclude" vector
  static std::string randomIdleAvailableServer(Node const& snap,
                                                   std::vector<std::string> const& exclude);
  static std::string randomIdleAvailableServer(Node const& snap,
                                               velocypack::Slice const& exclude);
  static size_t countGoodOrBadServersInList(Node const& snap,
                                            velocypack::Slice const& serverList);
  static size_t countGoodOrBadServersInList(Node const& snap, std::vector<std::string> const& serverList);
  static bool isInServerList(Node const& snap, std::string const& prefix, std::string const& server, bool isArray);

  /// @brief Get servers from plan, which are not failed or cleaned out
  static std::vector<std::string> availableServers(const arangodb::consensus::Node&);

  /// @brief Get servers from Supervision with health status GOOD
  static std::vector<std::string> healthyServers(arangodb::consensus::Node const&);

  static std::vector<shard_t> clones(Node const& snap, std::string const& db,
                                     std::string const& col, std::string const& shrd);

  static std::string findNonblockedCommonHealthyInSyncFollower(Node const& snap,
                                                               std::string const& db,
                                                               std::string const& col,
                                                               std::string const& shrd,
                                                               std::string const& serverToAvoid);

  /// @brief The shard must be one of a collection without
  /// `distributeShardsLike`. This returns all servers which 
  /// are in `failoverCandidates` for this shard or for any of its clones.
  static std::unordered_set<std::string> findAllFailoverCandidates(
      Node const& snap,
      std::string const& db,
      std::vector<Job::shard_t> const& shardsLikeMe);

  JOB_STATUS _status;
  Node const& _snapshot;
  AgentInterface* _agent;
  std::string _jobId;
  std::string _creator;

  static std::string agencyPrefix;  // will be initialized in AgencyFeature

  std::shared_ptr<velocypack::Builder> _jb;

  static void doForAllShards(
      Node const& snapshot, std::string& database, std::vector<shard_t>& shards,
      std::function<void(velocypack::Slice plan, velocypack::Slice current, std::string& planPath, std::string& curPath)> worker);

  // The following methods adds an operation to a transaction object or
  // a condition to a precondition object. In all cases, the builder trx
  // or pre must be in the state that an object has been opened, this
  // method adds some attribute/value pairs and leaves the object open:
  static void addIncreasePlanVersion(velocypack::Builder& trx);
  static void addIncreaseCurrentVersion(velocypack::Builder& trx);
  static void addIncreaseRebootId(velocypack::Builder& trx, std::string const& server);
  static void addRemoveJobFromSomewhere(velocypack::Builder& trx, std::string const& where,
                                        std::string const& jobId);
  static void addPutJobIntoSomewhere(velocypack::Builder& trx,
                                     std::string const& where, velocypack::Slice job,
                                     std::string const& reason = "");
  static void addPreconditionCollectionStillThere(velocypack::Builder& pre,
                                                  std::string const& database,
                                                  std::string const& collection);
  static void addBlockServer(velocypack::Builder& trx, std::string const& server,
                             std::string const& jobId);
  static void addBlockShard(velocypack::Builder& trx, std::string const& shard,
                            std::string const& jobId);
  static void addReadLockServer(velocypack::Builder& trx, std::string const& server,
                               std::string const& jobId);
  static void addWriteLockServer(velocypack::Builder& trx, std::string const& server,
                                std::string const& jobId);
  static void addReadUnlockServer(velocypack::Builder& trx, std::string const& server,
                                 std::string const& jobId);
  static void addWriteUnlockServer(velocypack::Builder& trx, std::string const& server,
                                  std::string const& jobId);
  static void addReleaseServer(velocypack::Builder& trx, std::string const& server);
  static void addReleaseShard(velocypack::Builder& trx, std::string const& shard);
  static void addPreconditionServerNotBlocked(velocypack::Builder& pre,
                                              std::string const& server);
  static void addPreconditionCurrentReplicaShardGroup(VPackBuilder& pre,
                                                      std::string const& database,
                                                      std::vector<shard_t> const&,
                                                      std::string const& server);
  static void addPreconditionServerHealth(velocypack::Builder& pre, std::string const& server,
                                          std::string const& health);
  static void addPreconditionShardNotBlocked(velocypack::Builder& pre,
                                             std::string const& shard);
  static void addPreconditionServerReadLockable(velocypack::Builder& pre,
                                               std::string const& server,
                                               std::string const& jobId);
  static void addPreconditionServerReadLocked(velocypack::Builder& pre,
                                             std::string const& server,
                                             std::string const& jobId);
  static void addPreconditionServerWriteLockable(velocypack::Builder& pre,
                                                std::string const& server,
                                                std::string const& jobId);
  static void addPreconditionServerWriteLocked(velocypack::Builder& pre,
                                              std::string const& server,
                                              std::string const& jobId);
  static void addPreconditionUnchanged(velocypack::Builder& pre, std::string const& key,
                                       velocypack::Slice value);
  static void addPreconditionJobStillInPending(velocypack::Builder& pre,
                                               std::string const& jobId);
  static std::string checkServerHealth(Node const& snapshot, std::string const& server);
};

inline arangodb::consensus::write_ret_t singleWriteTransaction(AgentInterface* _agent,
                                                               velocypack::Builder const& transaction,
                                                               bool waitForCommit = true) {
  query_t envelope = std::make_shared<velocypack::Builder>();

  velocypack::Slice trx = transaction.slice();
  try {
    {
      VPackArrayBuilder listOfTrxs(envelope.get());
      VPackArrayBuilder onePair(envelope.get());
      {
        VPackObjectBuilder mutationPart(envelope.get());
        for (auto const& pair : VPackObjectIterator(trx[0])) {
          envelope->add("/" + Job::agencyPrefix + pair.key.copyString(), pair.value);
        }
      }
      if (trx.length() > 1) {
        VPackObjectBuilder preconditionPart(envelope.get());
        for (auto const& pair : VPackObjectIterator(trx[1])) {
          envelope->add("/" + Job::agencyPrefix + pair.key.copyString(), pair.value);
        }
      }
    }
  } catch (std::exception const& e) {
    LOG_TOPIC("5be90", ERR, Logger::SUPERVISION)
        << "Supervision failed to build single-write transaction: " << e.what();
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

inline arangodb::consensus::trans_ret_t generalTransaction(AgentInterface* _agent,
                                                           velocypack::Builder const& transaction) {
  query_t envelope = std::make_shared<velocypack::Builder>();
  velocypack::Slice trx = transaction.slice();

  try {
    {
      VPackArrayBuilder listOfTrxs(envelope.get());
      for (auto const& singleTrans : VPackArrayIterator(trx)) {
        TRI_ASSERT(singleTrans.isArray() && singleTrans.length() > 0);
        if (singleTrans[0].isObject()) {
          VPackArrayBuilder onePair(envelope.get());
          {
            VPackObjectBuilder mutationPart(envelope.get());
            for (auto const& pair : VPackObjectIterator(singleTrans[0])) {
              envelope->add("/" + Job::agencyPrefix + pair.key.copyString(), pair.value);
            }
          }
          if (singleTrans.length() > 1) {
            VPackObjectBuilder preconditionPart(envelope.get());
            for (auto const& pair : VPackObjectIterator(singleTrans[1])) {
              envelope->add("/" + Job::agencyPrefix + pair.key.copyString(), pair.value);
            }
          }
        } else if (singleTrans[0].isString()) {
          VPackArrayBuilder reads(envelope.get());
          for (auto const& path : VPackArrayIterator(singleTrans)) {
            envelope->add(VPackValue("/" + Job::agencyPrefix + path.copyString()));
          }
        }
      }
    }
  } catch (std::exception const& e) {
    LOG_TOPIC("aae99", ERR, Logger::SUPERVISION)
        << "Supervision failed to build transaction: " << e.what();
  }

  auto ret = _agent->transact(envelope);

  // This is for now disabled to speed up things. We wait after a full
  // Supervision run, which is good enough.
  //if (ret.maxind > 0) {
  // _agent->waitFor(ret.maxind);
  //

  return ret;
}

inline arangodb::consensus::trans_ret_t transient(AgentInterface* _agent,
                                                  velocypack::Builder const& transaction) {
  query_t envelope = std::make_shared<velocypack::Builder>();

  velocypack::Slice trx = transaction.slice();
  try {
    {
      VPackArrayBuilder listOfTrxs(envelope.get());
      VPackArrayBuilder onePair(envelope.get());
      {
        VPackObjectBuilder mutationPart(envelope.get());
        for (auto const& pair : VPackObjectIterator(trx[0])) {
          envelope->add("/" + Job::agencyPrefix + pair.key.copyString(), pair.value);
        }
      }
      if (trx.length() > 1) {
        VPackObjectBuilder preconditionPart(envelope.get());
        for (auto const& pair : VPackObjectIterator(trx[1])) {
          envelope->add("/" + Job::agencyPrefix + pair.key.copyString(), pair.value);
        }
      }
    }
  } catch (std::exception const& e) {
    LOG_TOPIC("d03d5", ERR, Logger::SUPERVISION)
        << "Supervision failed to build transaction for transient: " << e.what();
  }

  return _agent->transient(envelope);
}

}  // namespace consensus
}  // namespace arangodb

#endif
