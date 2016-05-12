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

#ifndef ARANGOD_CONSENSUS_SUPERVISION_H
#define ARANGOD_CONSENSUS_SUPERVISION_H 1

#include "AgencyCommon.h"
#include "Node.h"

#include "Basics/Thread.h"
#include "Basics/ConditionVariable.h"

namespace arangodb {
namespace consensus {

class Agent;
class Store;

struct JobResult {
  JobResult() {}
};

struct JobCallback {
  JobCallback() {}
  virtual ~JobCallback() {};
  virtual bool operator()(JobResult*) = 0;
};

enum JOB_CASE {FAILED_DBSERVER};
template<enum arangodb::consensus::JOB_CASE> struct Job {};

struct check_t {
  bool good;
  std::string name;
  check_t (std::string const& n, bool g) : good(g), name(n) {}
};

class Supervision : public arangodb::Thread {
  
public:

  typedef std::chrono::system_clock::time_point TimePoint;
  typedef std::string ServerID;
  typedef std::string ServerStatus;
  typedef std::string ServerTimestamp;

  enum TASKS {LEADER_FAILURE_MIGRATION, FOLLOWER_FAILURE_MIGRATION,
              LEADER_INTENDED_MIGRATION, FOLLOWER_INTENDED_MIGRATION};

  template<TASKS T>
  class Task {
    explicit Task (const VPackSlice& config) {}
    ServerID _serverID;
    std::string _endpoint;
  };

  struct VitalSign {

    VitalSign(ServerStatus s, ServerTimestamp t) :
      myTimestamp(std::chrono::system_clock::now()),
      serverStatus(s), serverTimestamp(t), jobId(0) {} 

    void update (ServerStatus s, ServerTimestamp t) {
      myTimestamp = std::chrono::system_clock::now();
      serverStatus = s;
      serverTimestamp = t;
      jobId = 0;
    }

    void maintenance (uint64_t jid) {
      jobId = jid;
    }

    uint64_t maintenance () {
      return jobId;
    }
      
    TimePoint myTimestamp;
    ServerStatus serverStatus;
    ServerTimestamp serverTimestamp;
    uint64_t jobId;
    
  };
  
  /// @brief Construct sanity checking
  Supervision ();
  
  /// @brief Default dtor
  ~Supervision ();
  
  /// @brief Start thread
  bool start ();

  /// @brief Start thread with access to agent
  bool start (Agent*);

  /// @brief Run woker
  void run() override final;
  
  /// @brief Begin thread shutdown
  void beginShutdown() override final;

  /// @brief Wake up to task
  void wakeUp ();

private:

  /// @brief Move shard from one db server to other db server
  bool moveShard (std::string const& from, std::string const& to);

  /// @brief Move shard from one db server to other db server
  bool replicateShard (std::string const& to);

  /// @brief Move shard from one db server to other db server
  bool removeShard (std::string const& from);

  /// @Brief Check machines under path in agency
  std::vector<check_t> checkDBServers ();
  std::vector<check_t> checkShards ();

  /// @brief Read db
  Store const& store () const;

  /// @brief Perform sanity checking
  bool doChecks(bool);
  
  Agent* _agent; /**< @brief My agent */
  Node   _snapshot;

  arangodb::basics::ConditionVariable _cv; /**< @brief Control if thread
                                              should run */

  ///@brief last vital signs as reported through heartbeats to agency
  ///
  std::map<ServerID,
           std::shared_ptr<VitalSign>> _vitalSigns;

  long _frequency;
  long _gracePeriod;
  
};

}}

#endif
