////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Thread.h"

#include "Agency/AgencyComm.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Mutex.h"
#include "Basics/Thread.h"
#include "Cluster/AgencyCallback.h"
#include "Cluster/DBServerAgencySync.h"
#include "Metrics/Fwd.h"

#include <velocypack/Slice.h>
#include <chrono>

namespace arangodb {
namespace application_features {
class ApplicationServer;
}

struct AgencyVersions {
  uint64_t plan;
  uint64_t current;

  AgencyVersions(uint64_t _plan, uint64_t _current)
      : plan(_plan), current(_plan) {}

  explicit AgencyVersions(const DBServerAgencySyncResult& result)
      : plan(result.planIndex), current(result.currentIndex) {}
};

class AgencyCallbackRegistry;
class HeartbeatBackgroundJobThread;

class HeartbeatThread : public ServerThread<ArangodServer>,
                        public std::enable_shared_from_this<HeartbeatThread> {
 public:
  HeartbeatThread(Server&, AgencyCallbackRegistry*, std::chrono::microseconds,
                  uint64_t maxFailsBeforeWarning);
  ~HeartbeatThread();

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief initializes the heartbeat
  //////////////////////////////////////////////////////////////////////////////

  bool init();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not the thread is ready
  //////////////////////////////////////////////////////////////////////////////

  bool isReady() const { return _ready.load(); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief set the thread status to ready
  //////////////////////////////////////////////////////////////////////////////

  void setReady() { _ready.store(true); }

  // void runBackgroundJob();

  void dispatchedJobResult(DBServerAgencySyncResult);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not the thread has run at least once.
  /// this is used on the coordinator only
  //////////////////////////////////////////////////////////////////////////////

  static bool hasRunOnce() {
    return HasRunOnce.load(std::memory_order_acquire);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief break runDBserver out of wait on condition after setting state in
  /// base class
  //////////////////////////////////////////////////////////////////////////////
  void beginShutdown() override;

  /// @brief Reference to agency sync job
  DBServerAgencySync& agencySync();

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief heartbeat main loop
  //////////////////////////////////////////////////////////////////////////////

  void run() override;

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief heartbeat main loop, coordinator version
  //////////////////////////////////////////////////////////////////////////////

  void runCoordinator();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief heartbeat main loop, dbserver version
  //////////////////////////////////////////////////////////////////////////////

  void runDBServer();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief heartbeat main loop, single server version
  //////////////////////////////////////////////////////////////////////////////

  void runSingleServer();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief heartbeat main loop for agents
  //////////////////////////////////////////////////////////////////////////////

  void runAgent();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief handles a plan change, coordinator case
  //////////////////////////////////////////////////////////////////////////////

  bool handlePlanChangeCoordinator(uint64_t);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief handles a plan change, DBServer case
  //////////////////////////////////////////////////////////////////////////////

  bool handlePlanChangeDBServer(uint64_t);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sends the current server's state to the agency
  //////////////////////////////////////////////////////////////////////////////

  bool sendServerState();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get some regular news from the agency, a closure which calls this
  /// method is regularly posted to the scheduler. This is for the
  /// DBServer.
  //////////////////////////////////////////////////////////////////////////////

  void getNewsFromAgencyForDBServer();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get some regular news from the agency, a closure which calls this
  /// method is regularly posted to the scheduler. This is for the
  /// Coordinator.
  //////////////////////////////////////////////////////////////////////////////

  void getNewsFromAgencyForCoordinator();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief force update of db servers
  //////////////////////////////////////////////////////////////////////////////

  void updateDBServers();

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief bring the db server in sync with the desired state
  //////////////////////////////////////////////////////////////////////////////

  void notify();

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief update the local agent pool from the slice
  //////////////////////////////////////////////////////////////////////////////

  void updateAgentPool(arangodb::velocypack::Slice const& agentPool);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief update the server mode from the slice
  //////////////////////////////////////////////////////////////////////////////

  void updateServerMode(arangodb::velocypack::Slice const& readOnlySlice);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief AgencyCallbackRegistry
  //////////////////////////////////////////////////////////////////////////////

  AgencyCallbackRegistry* _agencyCallbackRegistry;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief status lock
  //////////////////////////////////////////////////////////////////////////////

  std::shared_ptr<arangodb::Mutex> _statusLock;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief AgencyComm instance
  //////////////////////////////////////////////////////////////////////////////

  AgencyComm _agency;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief condition variable for heartbeat
  //////////////////////////////////////////////////////////////////////////////

  arangodb::basics::ConditionVariable _condition;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief this server's id
  //////////////////////////////////////////////////////////////////////////////

  std::string const _myId;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief heartbeat interval
  //////////////////////////////////////////////////////////////////////////////

  std::chrono::microseconds _interval;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief number of fails in a row before a warning is issued
  //////////////////////////////////////////////////////////////////////////////

  uint64_t _maxFailsBeforeWarning;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief current number of fails in a row
  //////////////////////////////////////////////////////////////////////////////

  uint64_t _numFails;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief last successfully dispatched version
  //////////////////////////////////////////////////////////////////////////////

  uint64_t _lastSuccessfulVersion;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief current plan version
  //////////////////////////////////////////////////////////////////////////////

  uint64_t _currentPlanVersion;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not the thread is ready
  //////////////////////////////////////////////////////////////////////////////

  std::atomic<bool> _ready;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not the heartbeat thread has run at least once
  /// this is used on the coordinator only
  //////////////////////////////////////////////////////////////////////////////

  static std::atomic<bool> HasRunOnce;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief keeps track of the currently installed versions
  //////////////////////////////////////////////////////////////////////////////
  AgencyVersions _currentVersions;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief keeps track of the currently desired versions
  //////////////////////////////////////////////////////////////////////////////
  std::shared_ptr<AgencyVersions> _desiredVersions;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief number of background jobs that have been posted to the scheduler
  //////////////////////////////////////////////////////////////////////////////

  std::atomic<uint64_t> _backgroundJobsPosted;

  // when was the sync routine last run?
  double _lastSyncTime;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief handle of the dedicated thread to execute the phase 1 and phase 2
  /// code. Only created on dbservers.
  //////////////////////////////////////////////////////////////////////////////
  std::unique_ptr<HeartbeatBackgroundJobThread> _maintenanceThread;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief number of subsequent failed version updates
  //////////////////////////////////////////////////////////////////////////////
  uint64_t _failedVersionUpdates;

  // The following are only used in the coordinator case. This
  // is the coordinator's way to learn of new Plan and Current
  // Versions. The heartbeat thread schedules a closure which calls
  // getNewsFromAgencyForCoordinator but makes sure that it only ever
  // has one running at a time. Therefore, it is safe to have these atomics
  // as members.

  // invalidate coordinators every 2nd call
  std::atomic<bool> _invalidateCoordinators;

  // last value of plan which we have noticed:
  std::atomic<uint64_t> _lastPlanVersionNoticed;
  // last value of current which we have noticed:
  std::atomic<uint64_t> _lastCurrentVersionNoticed;
  // For periodic update of the current DBServer list:
  std::atomic<int> _updateCounter;
  // For event-based update of the current DBServer list:
  std::atomic<bool> _updateDBServers;

  /// @brief point in time the server last reported itself as unhealthy (used
  /// to prevent log spamming on every occurrence of unhealthiness)
  std::chrono::steady_clock::time_point _lastUnhealthyTimestamp;

  /// @brief Sync job
  DBServerAgencySync _agencySync;

  metrics::Histogram<metrics::LogScale<uint64_t>>& _heartbeat_send_time_ms;
  metrics::Counter& _heartbeat_failure_counter;
};
}  // namespace arangodb
