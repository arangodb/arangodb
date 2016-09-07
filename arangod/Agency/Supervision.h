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

#include "Agency/Node.h"
#include "AgencyCommon.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"

namespace arangodb {
namespace consensus {

class Agent;
class Store;

struct check_t {
  bool good;
  std::string name;
  check_t(std::string const& n, bool g) : good(g), name(n) {}
};

class Supervision : public arangodb::Thread {
 public:
  typedef std::chrono::system_clock::time_point TimePoint;
  typedef std::string ServerID;
  typedef std::string ServerStatus;
  typedef std::string ServerTimestamp;

  enum TASKS {
    LEADER_FAILURE_MIGRATION,
    FOLLOWER_FAILURE_MIGRATION,
    LEADER_INTENDED_MIGRATION,
    FOLLOWER_INTENDED_MIGRATION
  };

  template <TASKS T>
  class Task {
    explicit Task(const VPackSlice& config) {}
    ServerID _serverID;
    std::string _endpoint;
  };

  struct VitalSign {
    VitalSign(ServerStatus const& s, ServerTimestamp const& t)
        : myTimestamp(std::chrono::system_clock::now()),
          serverStatus(s),
          serverTimestamp(t),
          jobId("0") {}

    void update(ServerStatus const& s, ServerTimestamp const& t) {
      myTimestamp = std::chrono::system_clock::now();
      serverStatus = s;
      serverTimestamp = t;
      jobId = "0";
    }

    void maintenance(std::string const& jid) { jobId = jid; }

    std::string const& maintenance() const { return jobId; }

    TimePoint myTimestamp;
    ServerStatus serverStatus;
    ServerTimestamp serverTimestamp;
    std::string jobId;
  };

  /// @brief Construct sanity checking
  Supervision();

  /// @brief Default dtor
  ~Supervision();

  /// @brief Start thread
  bool start();

  /// @brief Start thread with access to agent
  bool start(Agent*);

  /// @brief Run woker
  void run() override final;

  /// @brief Begin thread shutdown
  void beginShutdown() override final;

  /// @brief Wake up to task
  void wakeUp();

 private:
  static constexpr const char* HEALTH_STATUS_GOOD = "GOOD";
  static constexpr const char* HEALTH_STATUS_BAD = "BAD";
  static constexpr const char* HEALTH_STATUS_FAILED = "FAILED";

  /// @brief Update agency prefix from agency itself
  bool updateAgencyPrefix(size_t nTries = 10, int intervalSec = 1);

  /// @brief Move shard from one db server to other db server
  bool moveShard(std::string const& from, std::string const& to);

  /// @brief Move shard from one db server to other db server
  bool replicateShard(std::string const& to);

  /// @brief Move shard from one db server to other db server
  bool removeShard(std::string const& from);

  /// @brief Check machines under path in agency
  std::vector<check_t> checkDBServers();
  std::vector<check_t> checkCoordinators();
  std::vector<check_t> checkShards();

  void workJobs();

  /// @brief Get unique ids from agency
  void getUniqueIds();

  /// @brief Update local cache from agency
  void updateFromAgency();

  /// @brief Read db
  Store const& store() const;

  /// @brief Perform sanity checking
  bool doChecks();

  /// @brief update my local agency snapshot
  bool updateSnapshot();

  void shrinkCluster();

  bool isShuttingDown();

  bool handleJobs();
  void handleShutdown();

  Agent* _agent; /**< @brief My agent */
  Node _snapshot;

  arangodb::basics::ConditionVariable _cv; /**< @brief Control if thread
                                              should run */

  ///@brief last vital signs as reported through heartbeats to agency
  ///
  //  std::map<ServerID, std::shared_ptr<VitalSign>> _vitalSigns;

  long _frequency;
  long _gracePeriod;
  uint64_t _jobId;
  uint64_t _jobIdMax;

  // mop: this feels very hacky...we have a hen and egg problem here
  // we are using /Shutdown in the agency to determine that the cluster should
  // shutdown. When every member is down we should of course not persist this
  // flag so we don't immediately initiate shutdown after restart. we use this
  // flag to temporarily store that shutdown was initiated...when the /Shutdown
  // stuff has been removed we shutdown ourselves. The assumption (heheh...) is
  // that while the cluster is shutting down every agent hit the shutdown stuff
  // at least once so this flag got set at some point
  bool _selfShutdown;

  std::string const serverHealth(const std::string&);

  static std::string _agencyPrefix;
};

inline std::string timepointToString(Supervision::TimePoint const& t) {
  time_t tt = std::chrono::system_clock::to_time_t(t);
  struct tm tb;
  size_t const len(21);
  char buffer[len];
  TRI_localtime(tt, &tb);
  ::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tb);
  return std::string(buffer, len - 1);
}

inline Supervision::TimePoint stringToTimepoint(std::string const& s) {
  std::tm tt;
  try {
    tt.tm_year = std::stoi(s.substr(0, 4)) - 1900;
    tt.tm_mon = std::stoi(s.substr(5, 2)) - 1;
    tt.tm_mday = std::stoi(s.substr(8, 2));
    tt.tm_hour = std::stoi(s.substr(11, 2));
    tt.tm_min = std::stoi(s.substr(14, 2));
    tt.tm_sec = std::stoi(s.substr(17, 2));
    tt.tm_isdst = -1;
    auto time_c = ::mktime(&tt);
    return std::chrono::system_clock::from_time_t(time_c);
  } catch (...) {
    return std::chrono::system_clock::now();
  }
}
}
}  // Name spaces

#endif
