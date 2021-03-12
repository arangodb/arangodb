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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "HeartbeatThread.h"

#include <map>

#include <Agency/AsyncAgencyComm.h>
#include <Basics/application-exit.h>
#include <date/date.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ConditionLocker.h"
#include "Basics/MutexLocker.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/tri-strings.h"
#include "Cluster/AgencyCache.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/DBServerAgencySync.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AsyncJobManager.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "Logger/Logger.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/Recovery.h"
#include "Replication/GlobalReplicationApplier.h"
#include "Replication/ReplicationFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/TtlFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/ClusterUtils.h"
#include "Utils/Events.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::rest;

std::atomic<bool> HeartbeatThread::HasRunOnce(false);

namespace arangodb {

class HeartbeatBackgroundJobThread : public Thread {
 public:
  explicit HeartbeatBackgroundJobThread(application_features::ApplicationServer& server,
                                        HeartbeatThread* heartbeatThread)
      : Thread(server, "Maintenance"),
        _heartbeatThread(heartbeatThread),
        _stop(false),
        _sleeping(false),
        _anotherRun(true),
        _backgroundJobsLaunched(0) {}

  ~HeartbeatBackgroundJobThread() { shutdown(); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief asks the thread to stop, but does not wait.
  //////////////////////////////////////////////////////////////////////////////
  void stop() {
    {
      std::unique_lock<std::mutex> guard(_mutex);
      _stop = true;
    }
    _condition.notify_one();
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief notifies the background thread: when the thread is sleeping, wakes
  /// it up. Otherwise sets a flag to start another round.
  //////////////////////////////////////////////////////////////////////////////
  void notify() {
    std::unique_lock<std::mutex> guard(_mutex);
    _anotherRun.store(true, std::memory_order_release);
    if (_sleeping.load(std::memory_order_acquire)) {
      guard.unlock();
      _condition.notify_one();
    }
  }

 protected:
  void run() override {

    using namespace std::chrono_literals;

    while (!_stop) {
      {
        std::unique_lock<std::mutex> guard(_mutex);

        if (!_anotherRun.load(std::memory_order_acquire)) {
          _sleeping.store(true, std::memory_order_release);

          while (true) {

            if(_condition.wait_for(guard, std::chrono::seconds(5)) == std::cv_status::timeout) {
              _anotherRun = true;
            }

            if (_stop) {
              return;
            } else if (_anotherRun) {
              break;
            }  // otherwise spurious wakeup
          }

          _sleeping.store(false, std::memory_order_release);
        }

        _anotherRun.store(false, std::memory_order_release);
      }

      // execute schmutz here
      uint64_t jobNr = ++_backgroundJobsLaunched;
      LOG_TOPIC("9ec42", DEBUG, Logger::HEARTBEAT) << "sync callback started " << jobNr;
      try {
        auto& job = _heartbeatThread->agencySync();
        job.work();
      } catch (std::exception const& ex) {
        LOG_TOPIC("1c9a4", ERR, Logger::HEARTBEAT) << "caught exception during agencySync: " << ex.what();
      } catch (...) {
        LOG_TOPIC("659a8", ERR, Logger::HEARTBEAT) << "caught unknown exception during agencySync";
      }
      LOG_TOPIC("71f07", DEBUG, Logger::HEARTBEAT) << "sync callback ended " << jobNr;
    }
  }

 private:
  HeartbeatThread* _heartbeatThread;

  std::mutex _mutex;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief used to wake up the background thread
  /// guarded via _mutex.
  //////////////////////////////////////////////////////////////////////////////
  std::condition_variable _condition;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Set by the HeartbeatThread when the BackgroundThread should stop
  /// guarded via _mutex.
  //////////////////////////////////////////////////////////////////////////////
  std::atomic<bool> _stop;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief wether the background thread sleeps or not
  /// guarded via _mutex.
  //////////////////////////////////////////////////////////////////////////////
  std::atomic<bool> _sleeping;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief when awake, the background thread will execute another round of
  /// phase 1 and phase 2, after resetting this to false
  /// guarded via _mutex.
  //////////////////////////////////////////////////////////////////////////////
  std::atomic<bool> _anotherRun;

  uint64_t _backgroundJobsLaunched;
};
}  // namespace arangodb

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a heartbeat thread
////////////////////////////////////////////////////////////////////////////////

struct HeartbeatScale {
  static log_scale_t<uint64_t> scale() { return {2, 4, 8000, 10}; }
};
DECLARE_COUNTER(arangodb_heartbeat_failures_total, "Counting failed heartbeat transmissions");
DECLARE_HISTOGRAM(arangodb_heartbeat_send_time_msec, HeartbeatScale, "Time required to send heartbeat [ms]");

HeartbeatThread::HeartbeatThread(application_features::ApplicationServer& server,
                                 AgencyCallbackRegistry* agencyCallbackRegistry,
                                 std::chrono::microseconds interval, uint64_t maxFailsBeforeWarning)
    : Thread(server, "Heartbeat"),
      _agencyCallbackRegistry(agencyCallbackRegistry),
      _statusLock(std::make_shared<Mutex>()),
      _agency(server),
      _condition(),
      _myId(ServerState::instance()->getId()),
      _interval(interval),
      _maxFailsBeforeWarning(maxFailsBeforeWarning),
      _numFails(0),
      _lastSuccessfulVersion(0),
      _currentPlanVersion(0),
      _ready(false),
      _currentVersions(0, 0),
      _desiredVersions(std::make_shared<AgencyVersions>(0, 0)),
      _backgroundJobsPosted(0),
      _lastSyncTime(0),
      _maintenanceThread(nullptr),
      _failedVersionUpdates(0),
      _invalidateCoordinators(true),
      _lastPlanVersionNoticed(0),
      _lastCurrentVersionNoticed(0),
      _updateCounter(0),
      _lastUnhealthyTimestamp(std::chrono::steady_clock::time_point()),
      _agencySync(_server, this),
      _heartbeat_send_time_ms(
        server.getFeature<arangodb::MetricsFeature>().add(arangodb_heartbeat_send_time_msec{})),
      _heartbeat_failure_counter(
        server.getFeature<arangodb::MetricsFeature>().add(arangodb_heartbeat_failures_total{})) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a heartbeat thread
////////////////////////////////////////////////////////////////////////////////
HeartbeatThread::~HeartbeatThread() {
  shutdown();
  if (_maintenanceThread) {
    _maintenanceThread->stop();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief heartbeat main loop
/// the heartbeat thread constantly reports the current server status to the
/// agency. it does so by sending the current state string to the key
/// "Sync/ServerStates/" + my-id.
/// then the request it aborted and the heartbeat thread will go on with
/// reporting its state to the agency again. If it notices a change when
/// watching the command key, it will wake up and apply the change locally.
////////////////////////////////////////////////////////////////////////////////

void HeartbeatThread::run() {
  ServerState::RoleEnum role = ServerState::instance()->getRole();

  // mop: the heartbeat thread itself is now ready
  setReady();
  // mop: however we need to wait for the rest server here to come up
  // otherwise we would already create collections and the coordinator would
  // think
  // ohhh the dbserver is online...pump some documents into it
  // which fails when it is still in maintenance mode
  auto server = ServerState::instance();
  if (!server->isCoordinator(role)) {
    while (server->isMaintenance()) {
      if (isStopping()) {
        // startup aborted
        return;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  LOG_TOPIC("9788a", TRACE, Logger::HEARTBEAT)
      << "starting heartbeat thread (" << role << ")";

  if (ServerState::instance()->isCoordinator(role)) {
    runCoordinator();
  } else if (ServerState::instance()->isDBServer(role)) {
    runDBServer();
  } else if (ServerState::instance()->isSingleServer(role)) {
    if (_server.getFeature<ReplicationFeature>().isActiveFailoverEnabled()) {
      runSingleServer();
    }
  } else if (ServerState::instance()->isAgent(role)) {
    runAgent();
  } else {
    LOG_TOPIC("8291e", ERR, Logger::HEARTBEAT)
        << "invalid role setup found when starting HeartbeatThread";
    TRI_ASSERT(false);
  }

  LOG_TOPIC("eab40", TRACE, Logger::HEARTBEAT)
      << "stopped heartbeat thread (" << role << ")";
}

void HeartbeatThread::getNewsFromAgencyForDBServer() {
  // ATTENTION: This method will usually be run in a scheduler thread and
  // not in the HeartbeatThread itself. Therefore, we must protect ourselves
  // against concurrent accesses.

  LOG_TOPIC("26372", DEBUG, Logger::HEARTBEAT) << "getting news from agency...";
  auto start = std::chrono::steady_clock::now();

  auto& cache = server().getFeature<ClusterFeature>().agencyCache();
  auto [acb,idx] = cache.read(
  std::vector<std::string>{
    AgencyCommHelper::path("Shutdown"), AgencyCommHelper::path("Current/Version"),
    AgencyCommHelper::path("Target/FailedServers"), "/.agency"});
  auto timeDiff = std::chrono::steady_clock::now() - start;
  if (timeDiff > std::chrono::seconds(10)) {
    LOG_TOPIC("77644", WARN, Logger::HEARTBEAT)
        << "ATTENTION: Getting news from agency took longer than 10 seconds, "
           "this might be causing trouble. Please "
           "contact ArangoDB and ask for help.";
  }
  auto result = acb->slice();

  LOG_TOPIC("26373", DEBUG, Logger::HEARTBEAT)
    << "got news from agency: " << result.toJson();
  if (!result.isArray()) {
    if (!_server.isStopping()) {
      LOG_TOPIC("17c99", WARN, Logger::HEARTBEAT)
        << "Heartbeat: Could not read from agency!";
    }
  } else {
    VPackSlice agentPool = result[0].get(".agency");
    updateAgentPool(agentPool);

    VPackSlice shutdownSlice = result[0].get(
        std::vector<std::string>({AgencyCommHelper::path(), "Shutdown"}));

    if (shutdownSlice.isBool() && shutdownSlice.getBool()) {
      _server.beginShutdown();
    }

    VPackSlice failedServersSlice = result[0].get(std::vector<std::string>(
        {AgencyCommHelper::path(), "Target", "FailedServers"}));
    if (failedServersSlice.isObject()) {
      std::vector<ServerID> failedServers = {};
      for (auto const& server : VPackObjectIterator(failedServersSlice)) {
        failedServers.push_back(server.key.copyString());
      }
      LOG_TOPIC("52626", DEBUG, Logger::HEARTBEAT)
          << "Updating failed servers list.";
      auto& ci = _server.getFeature<ClusterFeature>().clusterInfo();
      ci.setFailedServers(failedServers);
      transaction::cluster::abortTransactionsWithFailedServers(ci);
    } else {
      LOG_TOPIC("80491", WARN, Logger::HEARTBEAT)
          << "FailedServers is not an object. ignoring for now";
    }

    VPackSlice s = result[0].get(std::vector<std::string>(
        {AgencyCommHelper::path(), std::string("Current"), std::string("Version")}));
    if (!s.isInteger()) {
      LOG_TOPIC("40527", ERR, Logger::HEARTBEAT)
          << "Current/Version in agency is not an integer.";
    } else {
      uint64_t currentVersion = 0;
      try {
        currentVersion = s.getUInt();
      } catch (...) {
      }
      if (currentVersion == 0) {
        LOG_TOPIC("12a02", ERR, Logger::HEARTBEAT)
            << "Current/Version in agency is 0.";
      } else {
        {
          MUTEX_LOCKER(mutexLocker, *_statusLock);
          if (currentVersion > _desiredVersions->current) {
            _desiredVersions->current = currentVersion;
            LOG_TOPIC("33559", DEBUG, Logger::HEARTBEAT)
                << "Found greater Current/Version in agency.";
          }
        }
      }
    }
  }

  // Periodically update the list of DBServers and prune agency comm
  // connection pool:
  if (++_updateCounter >= 60) {
    auto& clusterFeature = server().getFeature<ClusterFeature>();
    auto& ci = clusterFeature.clusterInfo();
    ci.loadCurrentDBServers();
    _updateCounter = 0;
    clusterFeature.pruneAsyncAgencyConnectionPool();
  }
}

DBServerAgencySync& HeartbeatThread::agencySync() { return _agencySync; }

////////////////////////////////////////////////////////////////////////////////
/// @brief heartbeat main loop, dbserver version
////////////////////////////////////////////////////////////////////////////////

void HeartbeatThread::runDBServer() {

  using namespace std::chrono_literals;

  _maintenanceThread = std::make_unique<HeartbeatBackgroundJobThread>(_server, this);

  while (!isStopping() && !server().getFeature<DatabaseFeature>().started()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    LOG_TOPIC("eec21", DEBUG, Logger::HEARTBEAT)
        << "Waiting for database feature to finish start up";
  }

  if (!_maintenanceThread->start()) {
    // WHAT TO DO NOW?
    LOG_TOPIC("12cee", ERR, Logger::HEARTBEAT)
        << "Failed to start dedicated thread for maintenance";
  }

  // The following helps to synchronize between a background read
  // operation run in a scheduler thread and a the heartbeat
  // thread. If it is zero, the heartbeat schedules another
  // run, which at its end, sets it back to 0:
  auto getNewsRunning = std::make_shared<std::atomic<int>>(0);

  // Loop priorities / goals
  // 0. send state to agency server
  // 1. schedule handlePlanChange immediately when agency callback occurs
  // 2. poll for plan change, schedule handlePlanChange immediately if change
  // detected
  // 3. force handlePlanChange every 7.4 seconds just in case
  //     (7.4 seconds is just less than half the 15 seconds agency uses to
  //     declare dead server)
  // 4. if handlePlanChange runs long (greater than 7.4 seconds), have another
  // start immediately after

  while (!isStopping()) {
    try {
      auto const start = std::chrono::steady_clock::now();
      // send our state to the agency.
      sendServerState();

      if (isStopping()) {
        break;
      }

      if (getNewsRunning->load(std::memory_order_seq_cst) == 0) {
        // Schedule a getNewsFromAgency call in the Scheduler:
        auto self = shared_from_this();
        Scheduler* scheduler = SchedulerFeature::SCHEDULER;
        *getNewsRunning = 1;
        bool queued = scheduler->queue(RequestLane::CLUSTER_INTERNAL, [self, getNewsRunning] {
          self->getNewsFromAgencyForDBServer();
          *getNewsRunning = 0;  // indicate completion to trigger a new schedule
        });
        if (!queued && !isStopping()) {
          LOG_TOPIC("aacce", WARN, Logger::HEARTBEAT)
              << "Could not schedule getNewsFromAgency job in scheduler. Don't "
                 "worry, this will be tried again later.";
          *getNewsRunning = 0;
        } else {
          LOG_TOPIC("aaccf", DEBUG, Logger::HEARTBEAT)
              << "Have scheduled getNewsFromAgency job.";
        }
      }

      if (isStopping()) {
        break;
      }

      CONDITION_LOCKER(locker, _condition);
      auto remain = _interval - (std::chrono::steady_clock::now() - start);
      if (remain.count() > 0 && !isStopping()) {
        locker.wait(std::chrono::duration_cast<std::chrono::microseconds>(remain));
      }

    } catch (std::exception const& e) {
      LOG_TOPIC("49198", ERR, Logger::HEARTBEAT)
          << "Got an exception in DBServer heartbeat: " << e.what();
    } catch (...) {
      LOG_TOPIC("9946d", ERR, Logger::HEARTBEAT)
          << "Got an unknown exception in DBServer heartbeat";
    }
    LOG_TOPIC("f5628", DEBUG, Logger::HEARTBEAT) << "Heart beating.";
  }

}

void HeartbeatThread::getNewsFromAgencyForCoordinator() {
  // ATTENTION: This method will usually be run in a scheduler thread and
  // not in the HeartbeatThread itself. Therefore, we must protect ourselves
  // against concurrent accesses.

  AuthenticationFeature& af = _server.getFeature<AuthenticationFeature>();
  auto& ci = _server.getFeature<ClusterFeature>().clusterInfo();

  LOG_TOPIC("33452", DEBUG, Logger::HEARTBEAT) << "getting news from agency...";

  auto start = std::chrono::steady_clock::now();
  auto& cache = server().getFeature<ClusterFeature>().agencyCache();
  auto [acb, idx] = cache.read(std::vector<std::string>{
      AgencyCommHelper::path("Current/Version"), AgencyCommHelper::path("Current/Foxxmaster"),
        AgencyCommHelper::path("Current/FoxxmasterQueueupdate"),
        AgencyCommHelper::path("Plan/Version"), AgencyCommHelper::path("Readonly"),
        AgencyCommHelper::path("Shutdown"), AgencyCommHelper::path("Sync/UserVersion"),
        AgencyCommHelper::path("Target/FailedServers"), "/.agency"});
  auto result = acb->slice();
  LOG_TOPIC("53262", DEBUG, Logger::HEARTBEAT)
    << "got news from agency: " << acb->slice().toJson();
  auto timeDiff = std::chrono::steady_clock::now() - start;
  if (timeDiff > std::chrono::seconds(10)) {
    LOG_TOPIC("77622", WARN, Logger::HEARTBEAT)
      << "ATTENTION: Getting news from agency took longer than 10 seconds, "
      "this might be causing trouble. Please "
      "contact ArangoDB Support.";
  }

  if (!result.isArray()) {
    if (!_server.isStopping()) {
      LOG_TOPIC("539fc", WARN, Logger::HEARTBEAT)
        << "Heartbeat: Could not read from agency! status code: " << result.toJson();
    }
  } else {
    VPackSlice agentPool = result[0].get(".agency");
    updateAgentPool(agentPool);

    VPackSlice shutdownSlice = result[0].get(
        std::vector<std::string>({AgencyCommHelper::path(), "Shutdown"}));

    if (shutdownSlice.isBool() && shutdownSlice.getBool()) {
      _server.beginShutdown();
    }

    // mop: order is actually important here...FoxxmasterQueueupdate will
    // be set only when somebody registers some new queue stuff (for example
    // on a different coordinator than this one)... However when we are just
    // about to become the new foxxmaster we must immediately refresh our
    // queues this is done in ServerState...if queueupdate is set after
    // foxxmaster the change will be reset again
    VPackSlice foxxmasterQueueupdateSlice = result[0].get(std::vector<std::string>(
        {AgencyCommHelper::path(), "Current", "FoxxmasterQueueupdate"}));

    if (foxxmasterQueueupdateSlice.isBool() && foxxmasterQueueupdateSlice.getBool()) {
      ServerState::instance()->setFoxxmasterQueueupdate(true);
    }

    VPackSlice foxxmasterSlice = result[0].get(std::vector<std::string>(
        {AgencyCommHelper::path(), "Current", "Foxxmaster"}));

    if (foxxmasterSlice.isString() && foxxmasterSlice.getStringLength() != 0) {
      ServerState::instance()->setFoxxmaster(foxxmasterSlice.copyString());
    } else {
      auto state = ServerState::instance();
      VPackBuilder myIdBuilder;
      myIdBuilder.add(VPackValue(state->getId()));

      AgencyComm agency(_server);

      auto updateLeader = agency.casValue("/Current/Foxxmaster", foxxmasterSlice,
                                          myIdBuilder.slice(), 0, 10.0);
      if (updateLeader.successful()) {
        // We won the race we are the master
        ServerState::instance()->setFoxxmaster(state->getId());
      }
      agency.increment("Current/Version");
    }

    VPackSlice versionSlice = result[0].get(std::vector<std::string>(
        {AgencyCommHelper::path(), "Plan", "Version"}));

    if (versionSlice.isInteger()) {
      // there is a plan version

      uint64_t planVersion = 0;
      try {
        planVersion = versionSlice.getUInt();
      } catch (...) {
      }

      if (planVersion > _lastPlanVersionNoticed) {
        LOG_TOPIC("7c80a", TRACE, Logger::HEARTBEAT)
            << "Found planVersion " << planVersion << " which is newer than "
            << _lastPlanVersionNoticed;
        if (handlePlanChangeCoordinator(planVersion)) {
          _lastPlanVersionNoticed = planVersion;
        } else {
          LOG_TOPIC("1bfb0", WARN, Logger::HEARTBEAT)
              << "handlePlanChangeCoordinator was unsuccessful";
        }
      }
    }

    VPackSlice slice = result[0].get(std::vector<std::string>(
        {AgencyCommHelper::path(), "Sync", "UserVersion"}));

    if (slice.isInteger()) {
      // there is a UserVersion
      uint64_t userVersion = 0;
      try {
        userVersion = slice.getUInt();
      } catch (...) {
      }

      if (userVersion > 0) {
        if (af.isActive() && af.userManager() != nullptr) {
          af.userManager()->setGlobalVersion(userVersion);
        }
      }
    }

    versionSlice = result[0].get(std::vector<std::string>(
        {AgencyCommHelper::path(), "Current", "Version"}));
    if (versionSlice.isInteger()) {
      uint64_t currentVersion = 0;
      try {
        currentVersion = versionSlice.getUInt();
      } catch (...) {
      }
      if (currentVersion > _lastCurrentVersionNoticed) {
        LOG_TOPIC("c3bcb", TRACE, Logger::HEARTBEAT)
            << "Found currentVersion " << currentVersion
            << " which is newer than " << _lastCurrentVersionNoticed;
        _lastCurrentVersionNoticed = currentVersion;
        _invalidateCoordinators = false;
      }
    }

    VPackSlice failedServersSlice = result[0].get(std::vector<std::string>(
        {AgencyCommHelper::path(), "Target", "FailedServers"}));

    if (failedServersSlice.isObject()) {
      std::vector<ServerID> failedServers = {};
      for (auto const& server : VPackObjectIterator(failedServersSlice)) {
        failedServers.push_back(server.key.copyString());
      }
      LOG_TOPIC("43332", DEBUG, Logger::HEARTBEAT)
          << "Updating failed servers list.";
      ci.setFailedServers(failedServers);
      transaction::cluster::abortTransactionsWithFailedServers(ci);

      std::shared_ptr<pregel::PregelFeature> prgl = pregel::PregelFeature::instance();
      if (prgl) {
        pregel::RecoveryManager* mngr = prgl->recoveryManager();
        if (mngr != nullptr) {
          mngr->updatedFailedServers(failedServers);
        }
      }

    } else {
      LOG_TOPIC("cd95f", WARN, Logger::HEARTBEAT)
          << "FailedServers is not an object. ignoring for now";
    }

    auto readOnlySlice = result[0].get(
        std::vector<std::string>({AgencyCommHelper::path(), "Readonly"}));
    updateServerMode(readOnlySlice);
  }

  // the Foxx stuff needs an updated list of coordinators
  // and this is only updated when current version has changed
  if (_invalidateCoordinators) {
    ci.invalidateCurrentCoordinators();
  }
  _invalidateCoordinators = !_invalidateCoordinators;

  // Periodically update the list of DBServers:
  if (++_updateCounter >= 60) {
    ci.loadCurrentDBServers();
    _updateCounter = 0;
    auto& clusterFeature = server().getFeature<ClusterFeature>();
    clusterFeature.pruneAsyncAgencyConnectionPool();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief heartbeat main loop, single server version
////////////////////////////////////////////////////////////////////////////////

void HeartbeatThread::runSingleServer() {
  AuthenticationFeature* af = AuthenticationFeature::instance();
  TRI_ASSERT(af != nullptr);
  ReplicationFeature& replication = _server.getFeature<ReplicationFeature>();

  GlobalReplicationApplier* applier = replication.globalReplicationApplier();
  TRI_ASSERT(applier != nullptr && _server.hasFeature<ClusterFeature>());
  ClusterInfo& ci = _server.getFeature<ClusterFeature>().clusterInfo();

  TtlFeature& ttlFeature = _server.getFeature<TtlFeature>();

  std::string const leaderPath = "Plan/AsyncReplication/Leader";
  std::string const transientPath = "AsyncReplication/" + _myId;

  VPackBuilder myIdBuilder;
  myIdBuilder.add(VPackValue(_myId));

  uint64_t lastSentVersion = 0;
  auto start = std::chrono::steady_clock::now();
  while (!isStopping()) {
    {
      CONDITION_LOCKER(locker, _condition);
      auto remain = _interval - (std::chrono::steady_clock::now() - start);
      if (remain.count() > 0 && !isStopping()) {
        locker.wait(std::chrono::duration_cast<std::chrono::microseconds>(remain));
      }
    }
    start = std::chrono::steady_clock::now();

    if (isStopping()) {
      break;
    }

    try {
      // send our state to the agency.
      // we don't care if this fails
      sendServerState();
      double const timeout = 1.0;

      // check current local version of database objects version, and bump
      // the global version number in the agency in case it changed. this
      // informs other listeners about our local DDL changes
      uint64_t currentVersion =
          _server.getFeature<DatabaseFeature>().versionTracker()->current();
      if (currentVersion != lastSentVersion) {
        AgencyOperation incrementVersion("Plan/Version", AgencySimpleOperationType::INCREMENT_OP);
        AgencyWriteTransaction trx(incrementVersion);
        AgencyCommResult res = _agency.sendTransactionWithFailover(trx, timeout);

        if (res.successful()) {
          LOG_TOPIC("927b1", TRACE, Logger::HEARTBEAT)
              << "successfully increased plan version in agency";
          lastSentVersion = currentVersion;
          _failedVersionUpdates = 0;
        } else {
          if (++_failedVersionUpdates % _maxFailsBeforeWarning == 0) {
            LOG_TOPIC("700c7", WARN, Logger::HEARTBEAT)
                << "could not increase version number in agency: "
                << res.errorMessage();
          }
        }
      }

      auto& cache = server().getFeature<ClusterFeature>().agencyCache();
      auto [acb, idx] = cache.read(
        std::vector<std::string>{
          AgencyCommHelper::path("Shutdown"), AgencyCommHelper::path("Readonly"),
          AgencyCommHelper::path("Plan/AsyncReplication"), "/.agency"});
      auto slc = acb->slice();

      if (!slc.isArray()) {
        if (!_server.isStopping()) {
          LOG_TOPIC("229fd", WARN, Logger::HEARTBEAT)
            << "Heartbeat: Could not read from agency! "
            << "incriminating body: " << slc.toJson();
        }

        if (!applier->isActive()) {  // assume agency and leader are gone
          ServerState::instance()->setFoxxmaster(_myId);
          ServerState::instance()->setServerMode(ServerState::Mode::DEFAULT);
        }
        continue;
      }

      VPackSlice response = slc[0];
      VPackSlice agentPool = response.get(".agency");
      updateAgentPool(agentPool);

      VPackSlice shutdownSlice =
          response.get<std::string>({AgencyCommHelper::path(), "Shutdown"});
      if (shutdownSlice.isBool() && shutdownSlice.getBool()) {
        _server.beginShutdown();
        break;
      }

      // performing failover checks
      VPackSlice async = response.get<std::string>(
          {AgencyCommHelper::path(), "Plan", "AsyncReplication"});
      if (!async.isObject()) {
        LOG_TOPIC("04d3b", WARN, Logger::HEARTBEAT)
            << "Heartbeat: Could not read async-replication metadata from "
               "agency!";
        continue;
      }

      VPackSlice leader = async.get("Leader");
      if (!leader.isString() || leader.getStringLength() == 0) {
        // Case 1: No leader in agency. Race for leadership
        // ONLY happens on the first startup. Supervision performs failovers
        LOG_TOPIC("759aa", WARN, Logger::HEARTBEAT)
            << "Leadership vacuum detected, "
            << "attempting a takeover";

        // if we stay a slave, the redirect will be turned on again
        ServerState::instance()->setServerMode(ServerState::Mode::TRYAGAIN);
        AgencyCommResult result;
        if (leader.isNone()) {
          result = _agency.casValue(leaderPath, myIdBuilder.slice(),
                                    /*prevExist*/ false,
                                    /*ttl*/ 0, /*timeout*/ 5.0);
        } else {
          result = _agency.casValue(leaderPath, /*old*/ leader,
                                    /*new*/ myIdBuilder.slice(),
                                    /*ttl*/ 0, /*timeout*/ 5.0);
        }

        if (result.successful()) {  // successful leadership takeover
          leader = myIdBuilder.slice();
          // intentionally falls through to case 2
        } else if (result.httpCode() == rest::ResponseCode::PRECONDITION_FAILED) {
          // we did not become leader, someone else is, response contains
          // current value in agency
          LOG_TOPIC("7cf85", INFO, Logger::HEARTBEAT)
              << "Did not become leader";
          continue;
        } else {
          LOG_TOPIC("8514b", WARN, Logger::HEARTBEAT)
              << "got an unexpected agency error "
              << "code: " << result.httpCode() << " msg: " << result.errorMessage();
          continue;  // try again next time
        }
      }

      TRI_voc_tick_t lastTick = 0;  // we always want to set lastTick
      auto sendTransient = [&]() {
        VPackBuilder builder;
        builder.openObject();
        builder.add("leader", leader);
        builder.add("lastTick", VPackValue(lastTick));
        builder.close();
        double ttl =
            std::chrono::duration_cast<std::chrono::seconds>(_interval).count() * 5.0;

        double timeout = 20.0;
        if (isStopping()) {
          timeout = 5.0;
        }
        _agency.setTransient(transientPath, builder.slice(), static_cast<uint64_t>(ttl), timeout);
      };
      TRI_DEFER(sendTransient());

      // Case 2: Current server is leader
      if (leader.compareString(_myId) == 0) {
        if (applier->isActive()) {
          applier->stopAndJoin();
        }
        // we are leader now. make sure the applier drops its previous state
        applier->forget();
        lastTick = _server.getFeature<EngineSelectorFeature>().engine().currentTick();

        // put the leader in optional read-only mode
        auto readOnlySlice = response.get(
            std::vector<std::string>({AgencyCommHelper::path(), "Readonly"}));
        updateServerMode(readOnlySlice);

        // ensure everyone has server access
        ServerState::instance()->setFoxxmaster(_myId);
        auto prv = ServerState::setServerMode(ServerState::Mode::DEFAULT);
        if (prv == ServerState::Mode::REDIRECT) {
          LOG_TOPIC("98325", INFO, Logger::HEARTBEAT)
              << "Successful leadership takeover: "
              << "All your base are belong to us";
        }

        // server is now responsible for expiring outdated documents
        ttlFeature.allowRunning(true);
        continue;  // nothing more to do
      }

      // Case 3: Current server is follower, should not get here otherwise
      std::string const leaderStr = leader.copyString();
      TRI_ASSERT(!leaderStr.empty());
      LOG_TOPIC("aeb38", TRACE, Logger::HEARTBEAT) << "Following: " << leaderStr;

      // server is not responsible anymore for expiring outdated documents
      ttlFeature.allowRunning(false);

      ServerState::instance()->setFoxxmaster(leaderStr);  // leader is foxxmater
      ServerState::instance()->setReadOnly(true);  // Disable writes with dirty-read header

      std::string endpoint = ci.getServerEndpoint(leaderStr);
      if (endpoint.empty()) {
        LOG_TOPIC("05196", ERR, Logger::HEARTBEAT)
            << "Failed to resolve leader endpoint";
        continue;  // try again next time
      }

      // enable redirection to leader
      auto prv = ServerState::instance()->setServerMode(ServerState::Mode::REDIRECT);
      if (prv == ServerState::Mode::DEFAULT) {
        // we were leader previously, now we need to ensure no ongoing
        // operations on this server may prevent us from being a proper
        // follower. We wait for all ongoing ops to stop, and make sure nothing
        // is committed
        LOG_TOPIC("d09d2", INFO, Logger::HEARTBEAT)
            << "Detected leader to follower switch, now following " << leaderStr;
        TRI_ASSERT(!applier->isActive());
        applier->forget();  // make sure applier is doing a resync

        auto& gs = _server.getFeature<GeneralServerFeature>();
        Result res = gs.jobManager().clearAllJobs();
        if (res.fail()) {
          LOG_TOPIC("e0817", WARN, Logger::HEARTBEAT)
              << "could not cancel all async jobs " << res.errorMessage();
        }
        // wait for everything to calm down for good measure
        std::this_thread::sleep_for(std::chrono::seconds(10));
      }

      if (applier->isActive() && applier->endpoint() == endpoint) {
        lastTick = applier->lastTick();
      } else if (applier->endpoint() != endpoint) {  // configure applier for new endpoint
        // this means there is a new leader in the agency
        if (applier->isActive()) {
          applier->stopAndJoin();
        }
        while (applier->isShuttingDown() && !isStopping()) {
          std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        LOG_TOPIC("04e4e", INFO, Logger::HEARTBEAT)
            << "Starting replication from " << endpoint;
        ReplicationApplierConfiguration config = applier->configuration();
        config._jwt = af->tokenCache().jwtToken();
        config._endpoint = endpoint;
        config._autoResync = true;
        config._autoResyncRetries = 2;
        LOG_TOPIC("ab4a2", INFO, Logger::HEARTBEAT)
            << "start initial sync from leader";
        config._requireFromPresent = true;
        config._incremental = true;
        config._idleMinWaitTime = 250 * 1000;       // 250ms
        config._idleMaxWaitTime = 3 * 1000 * 1000;  // 3s
        TRI_ASSERT(!config._skipCreateDrop);
        config._includeFoxxQueues = true;  // sync _queues and _jobs


        if (_server.hasFeature<ReplicationFeature>()) {
          auto& feature = _server.getFeature<ReplicationFeature>();
          config._connectTimeout = feature.checkConnectTimeout(config._connectTimeout);
          config._requestTimeout = feature.checkRequestTimeout(config._requestTimeout);
        }

        applier->forget();  // forget about any existing configuration
        applier->reconfigure(config);
        applier->startReplication();

      } else if (!applier->isActive() && !applier->isShuttingDown()) {
        // try to restart the applier unless the user actively stopped it
        if (applier->hasState()) {
          Result error = applier->lastError();
          if (error.is(TRI_ERROR_REPLICATION_APPLIER_STOPPED)) {
            LOG_TOPIC("94dbc", WARN, Logger::HEARTBEAT)
                << "user stopped applier, please restart";
            continue;
          } else if (error.isNot(TRI_ERROR_REPLICATION_LEADER_INCOMPATIBLE) &&
                     error.isNot(TRI_ERROR_REPLICATION_LEADER_CHANGE) &&
                     error.isNot(TRI_ERROR_REPLICATION_LOOP) &&
                     error.isNot(TRI_ERROR_REPLICATION_NO_START_TICK) &&
                     error.isNot(TRI_ERROR_REPLICATION_START_TICK_NOT_PRESENT)) {
            LOG_TOPIC("5dee4", WARN, Logger::HEARTBEAT)
                << "restarting stopped applier... ";
            VPackBuilder debug;
            debug.openObject();
            applier->toVelocyPack(debug);
            debug.close();
            LOG_TOPIC("3ffb1", DEBUG, Logger::HEARTBEAT)
                << "previous applier state was: " << debug.toJson();

            auto config = applier->configuration();
            config._jwt = af->tokenCache().jwtToken();
            applier->reconfigure(config);

            // reads ticks from configuration, check again next time
            applier->startTailing(/*initialTick*/0, /*useTick*/false);
            continue;
          }
        }
        // complete resync next round
        LOG_TOPIC("66d82", WARN, Logger::HEARTBEAT)
            << "forgetting previous applier state. Will trigger a full resync "
               "now";
        applier->forget();
      }

    } catch (std::exception const& e) {
      LOG_TOPIC("1dc85", ERR, Logger::HEARTBEAT)
          << "got an exception in single server heartbeat: " << e.what();
    } catch (...) {
      LOG_TOPIC("9a79c", ERR, Logger::HEARTBEAT)
          << "got an unknown exception in single server heartbeat";
    }
    // Periodically prune the connection pool
    if (++_updateCounter >= 60) {
      _updateCounter = 0;
      auto& clusterFeature = server().getFeature<ClusterFeature>();
      clusterFeature.pruneAsyncAgencyConnectionPool();
    }
  }
}

void HeartbeatThread::updateServerMode(VPackSlice const& readOnlySlice) {
  bool readOnly = false;
  if (readOnlySlice.isBoolean()) {
    readOnly = readOnlySlice.getBool();
  }

  ServerState::instance()->setReadOnly(readOnly);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief heartbeat main loop, coordinator version
////////////////////////////////////////////////////////////////////////////////

void HeartbeatThread::runCoordinator() {
  // The following helps to synchronize between a background read
  // operation run in a scheduler thread and a the heartbeat
  // thread. If it is zero, the heartbeat schedules another
  // run, which at its end, sets it back to 0:
  std::shared_ptr<std::atomic<int>> getNewsRunning(new std::atomic<int>(0));

  while (!isStopping()) {
    try {
      auto const start = std::chrono::steady_clock::now();
      // send our state to the agency.
      sendServerState();

      if (isStopping()) {
        break;
      }

      if (getNewsRunning->load(std::memory_order_seq_cst) == 0) {
        // Schedule a getNewsFromAgency call in the Scheduler:
        auto self = shared_from_this();
        Scheduler* scheduler = SchedulerFeature::SCHEDULER;
        *getNewsRunning = 1;
        bool queued = scheduler->queue(RequestLane::CLUSTER_INTERNAL, [self, getNewsRunning] {
          self->getNewsFromAgencyForCoordinator();
          *getNewsRunning = 0;  // indicate completion to trigger a new schedule
        });
        if (!queued) {
          LOG_TOPIC("aacc2", WARN, Logger::HEARTBEAT)
              << "Could not schedule getNewsFromAgency job in scheduler. Don't "
                 "worry, this will be tried again later.";
          *getNewsRunning = 0;
        } else {
          LOG_TOPIC("aacc3", DEBUG, Logger::HEARTBEAT)
              << "Have scheduled getNewsFromAgency job.";
        }
      }

      CONDITION_LOCKER(locker, _condition);
      auto remain = _interval - (std::chrono::steady_clock::now() - start);
      if (remain.count() > 0 && !isStopping()) {
        locker.wait(std::chrono::duration_cast<std::chrono::microseconds>(remain));
      }

    } catch (std::exception const& e) {
      LOG_TOPIC("27a96", ERR, Logger::HEARTBEAT)
          << "Got an exception in coordinator heartbeat: " << e.what();
    } catch (...) {
      LOG_TOPIC("f4de9", ERR, Logger::HEARTBEAT)
          << "Got an unknown exception in coordinator heartbeat";
    }
    LOG_TOPIC("f5627", DEBUG, Logger::HEARTBEAT) << "Heart beating.";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief heartbeat main loop, agent version
////////////////////////////////////////////////////////////////////////////////

void HeartbeatThread::runAgent() {
  // simple loop to post dead threads every hour, no other tasks today
  while (!isStopping()) {
    try {
      CONDITION_LOCKER(locker, _condition);
      if (!isStopping()) {
        locker.wait(std::chrono::hours(1));
      }
    } catch (std::exception const& ex) {
      LOG_TOPIC("e6761", ERR, Logger::HEARTBEAT)
          << "Got an exception in agent heartbeat: " << ex.what();
    } catch (...) {
      LOG_TOPIC("e1dbc", ERR, Logger::HEARTBEAT)
          << "Got an unknown exception in agent heartbeat";
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes the heartbeat
////////////////////////////////////////////////////////////////////////////////

bool HeartbeatThread::init() {
  // send the server state a first time and use this as an indicator about
  // the agency's health
  if (ServerState::instance()->isClusterRole() && !sendServerState()) {
    return false;
  }

  return true;
}

void HeartbeatThread::beginShutdown() {
  // First shut down the maintenace thread:
  if (_maintenanceThread != nullptr) {
    _maintenanceThread->stop();

    // Wait until maintenance thread has terminated:
    size_t counter = 0;
    auto start = std::chrono::steady_clock::now();
    while (_maintenanceThread->isRunning()) {
      if (std::chrono::steady_clock::now() - start > std::chrono::seconds(65)) {
        LOG_TOPIC("d8a5c", FATAL, Logger::CLUSTER)
        << "exiting prematurely as we failed terminating the maintenance thread";
        FATAL_ERROR_EXIT();
      }
      if (++counter % 50 == 0) {
        LOG_TOPIC("acaaa", WARN, arangodb::Logger::CLUSTER)
        << "waiting for maintenance thread to finish";
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  // And now the heartbeat thread:
  CONDITION_LOCKER(guard, _condition);

  // set the shutdown state in parent class
  Thread::beginShutdown();

  // break _condition.wait() in runDBserver
  _condition.signal();

}

////////////////////////////////////////////////////////////////////////////////
/// @brief finished plan change
////////////////////////////////////////////////////////////////////////////////

void HeartbeatThread::dispatchedJobResult(DBServerAgencySyncResult result) {
  LOG_TOPIC("f3358", DEBUG, Logger::HEARTBEAT) << "Dispatched job returned!";
  MUTEX_LOCKER(mutexLocker, *_statusLock);
  if (result.success) {
    LOG_TOPIC("ce0db", DEBUG, Logger::HEARTBEAT)
      << "Sync request successful. Now have Plan index " << result.planIndex
      << ", Current index " << result.currentIndex;
    _currentVersions = AgencyVersions(result);
  } else {
    LOG_TOPIC("b72a6", ERR, Logger::HEARTBEAT)
      << "Sync request failed: " << result.errorMessage;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a plan version change, coordinator case
/// this is triggered if the heartbeat thread finds a new plan version number
////////////////////////////////////////////////////////////////////////////////

static std::string const prefixPlanChangeCoordinator = "Plan/Databases";
bool HeartbeatThread::handlePlanChangeCoordinator(uint64_t currentPlanVersion) {
  DatabaseFeature& databaseFeature = _server.getFeature<DatabaseFeature>();

  LOG_TOPIC("eda7d", TRACE, Logger::HEARTBEAT) << "found a plan update";
  auto& cache = server().getFeature<ClusterFeature>().agencyCache();
  auto [acb, idx] = cache.read(
    std::vector<std::string>{AgencyCommHelper::path(prefixPlanChangeCoordinator)});
  auto result = acb->slice();

  if (result.isArray()) {
    std::vector<TRI_voc_tick_t> ids;
    velocypack::Slice databases = result[0].get(std::vector<std::string>(
        {AgencyCommHelper::path(), "Plan", "Databases"}));

    if (!databases.isObject()) {
      return false;
    }

    // loop over all database names we got and create a local database
    // instance if not yet present:

    for (VPackObjectIterator::ObjectPair options : VPackObjectIterator(databases)) {
      if (!options.value.isObject()) {
        continue;
      }

      arangodb::CreateDatabaseInfo info(_server, ExecContext::current());
      TRI_ASSERT(options.value.get("name").isString());
      // when loading we allow system database names
      auto infoResult = info.load(options.value, VPackSlice::emptyArraySlice());
      if (infoResult.fail()) {
        LOG_TOPIC("3fa12", ERR, Logger::HEARTBEAT)
            << "In agency database plan" << infoResult.errorMessage();
        TRI_ASSERT(false);
      }

      // known plan IDs
      ids.push_back(info.getId());

      auto dbName = info.getName();
      TRI_vocbase_t* vocbase = databaseFeature.useDatabase(dbName);
      if (vocbase == nullptr) {
        // database does not yet exist, create it now

        // create a local database object...
        Result res = databaseFeature.createDatabase(std::move(info), vocbase);
        events::CreateDatabase(dbName, res, ExecContext::current());

        if (res.fail()) {
          LOG_TOPIC("ca877", ERR, arangodb::Logger::HEARTBEAT)
              << "creating local database '" << dbName
              << "' failed: " << res.errorMessage();
        } else {
          HasRunOnce.store(true, std::memory_order_release);
        }
      } else {
        if (vocbase->isSystem()) {
          // workaround: _system collection already exists now on every
          // coordinator setting HasRunOnce lets coordinator startup continue
          TRI_ASSERT(vocbase->id() == 1);
          HasRunOnce.store(true, std::memory_order_release);
        }
        vocbase->release();
      }
    }

    // get the list of databases that we know about locally
    std::vector<TRI_voc_tick_t> localIds = databaseFeature.getDatabaseIds(false);

    for (auto id : localIds) {
      auto r = std::find(ids.begin(), ids.end(), id);

      if (r == ids.end()) {
        // local database not found in the plan...
        TRI_vocbase_t* db = databaseFeature.useDatabase(id);
        TRI_ASSERT(db);
        std::string dbName = db ? db->name() : "n/a";
        if (db) {
          db->release();
        }
        Result res = databaseFeature.dropDatabase(id, true);
        events::DropDatabase(dbName, res, ExecContext::current());
      }
    }

  } else {
    return false;
  }

  // invalidate our local cache
  auto& ci = _server.getFeature<ClusterFeature>().clusterInfo();
  ci.flush();

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a plan version change, DBServer case
/// this is triggered if the heartbeat thread finds a new plan version number,
/// and every few heartbeats if the Current/Version has changed. Note that the
/// latter happens not in the heartbeat thread itself but in a scheduler thread.
/// Therefore we need to do proper locking.
////////////////////////////////////////////////////////////////////////////////

void HeartbeatThread::notify() {
  if (_maintenanceThread != nullptr) {
    _maintenanceThread->notify();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sends the current server's state to the agency
////////////////////////////////////////////////////////////////////////////////

bool HeartbeatThread::sendServerState() {
  LOG_TOPIC("3369a", TRACE, Logger::HEARTBEAT) << "sending heartbeat to agency";

  auto const start = std::chrono::steady_clock::now();
  TRI_DEFER({
    auto timeDiff = std::chrono::steady_clock::now() - start;
    _heartbeat_send_time_ms.count(
        std::chrono::duration_cast<std::chrono::milliseconds>(timeDiff).count());

    if (timeDiff > std::chrono::seconds(2) && !isStopping()) {
      LOG_TOPIC("77655", WARN, Logger::HEARTBEAT)
          << "ATTENTION: Sending a heartbeat took longer than 2 seconds, "
             "this might be causing trouble with health checks. Please "
             "contact ArangoDB Support.";
    }
  });

  double timeout = 20.0;
  if (isStopping()) {
    timeout = 5.0;
  }

  AgencyCommResult const result = _agency.sendServerState(timeout);

  if (result.successful()) {
    _numFails = 0;
    return true;
  }

  if (!isStopping()) {
    if (++_numFails % _maxFailsBeforeWarning == 0) {
      _heartbeat_failure_counter.count();
      std::string const endpoints = AsyncAgencyCommManager::INSTANCE->endpointsString();

      LOG_TOPIC("3e2f5", WARN, Logger::HEARTBEAT)
          << "heartbeat could not be sent to agency endpoints (" << endpoints
          << "): http code: " << result.httpCode() << ", body: " << result.body();
      _numFails = 0;
    }
  }

  return false;
}

void HeartbeatThread::updateAgentPool(VPackSlice const& agentPool) {
  if (agentPool.isObject() && agentPool.get("pool").isObject() && agentPool.hasKey("size") &&
      agentPool.get("size").getUInt() > 0 && agentPool.get("id").isString()) {
    try {
      std::vector<std::string> values;
      // we have to make sure that the leader is on the front
      auto leaderId = agentPool.get("id").stringRef();
      auto pool = agentPool.get("pool");
      values.reserve(pool.length());
      values.emplace_back(pool.get(leaderId).copyString());
      // now add all non leaders
      for (auto pair : VPackObjectIterator(pool)) {
        if (!pair.key.isEqualString(leaderId)) {
          values.emplace_back(pair.value.copyString());
        }
      }
      AsyncAgencyCommManager::INSTANCE->updateEndpoints(values);
    } catch (basics::Exception const& e) {
      LOG_TOPIC("1cec6", WARN, Logger::HEARTBEAT)
          << "Error updating agency pool: " << e.message();
    } catch (std::exception const& e) {
      LOG_TOPIC("889d4", WARN, Logger::HEARTBEAT)
          << "Error updating agency pool: " << e.what();
    } catch (...) {
    }
  } else {
    LOG_TOPIC("92522", ERR, Logger::AGENCYCOMM)
        << "Cannot find an agency persisted in RAFT 8|";
  }
}
