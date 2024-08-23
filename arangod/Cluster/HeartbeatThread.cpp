////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "Agency/AsyncAgencyComm.h"
#include "Auth/UserManager.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/application-exit.h"
#include "Basics/tri-strings.h"
#include "Cluster/AgencyCache.h"
#include "Cluster/AgencyCallbackRegistry.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/DBServerAgencySync.h"
#include "Cluster/ServerState.h"
#include "Containers/FlatHashSet.h"
#include "GeneralServer/AsyncJobManager.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/HistogramBuilder.h"
#include "Metrics/LogScale.h"
#include "Metrics/MetricsFeature.h"
#include "Replication/GlobalReplicationApplier.h"
#include "Replication/ReplicationFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/TtlFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/HealthData.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/ClusterUtils.h"
#include "Utils/Events.h"
#ifdef USE_V8
#include "V8Server/FoxxFeature.h"
#include "V8Server/V8DealerFeature.h"
#endif
#include "VocBase/vocbase.h"

#include <date/date.h>
#include <velocypack/Iterator.h>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::rest;

namespace arangodb {

class HeartbeatBackgroundJobThread : public Thread {
 public:
  explicit HeartbeatBackgroundJobThread(
      application_features::ApplicationServer& server,
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
            if (_condition.wait_for(guard, std::chrono::seconds(5)) ==
                std::cv_status::timeout) {
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
      LOG_TOPIC("9ec42", DEBUG, Logger::HEARTBEAT)
          << "sync callback started " << jobNr;
      try {
        auto& job = _heartbeatThread->agencySync();
        job.work();
      } catch (std::exception const& ex) {
        LOG_TOPIC("1c9a4", ERR, Logger::HEARTBEAT)
            << "caught exception during agencySync: " << ex.what();
      } catch (...) {
        LOG_TOPIC("659a8", ERR, Logger::HEARTBEAT)
            << "caught unknown exception during agencySync";
      }
      LOG_TOPIC("71f07", DEBUG, Logger::HEARTBEAT)
          << "sync callback ended " << jobNr;
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
  static metrics::LogScale<uint64_t> scale() { return {2, 4, 8000, 10}; }
};
DECLARE_COUNTER(arangodb_heartbeat_failures_total,
                "Counting failed heartbeat transmissions");
DECLARE_HISTOGRAM(arangodb_heartbeat_send_time_msec, HeartbeatScale,
                  "Time required to send heartbeat [ms]");

HeartbeatThread::HeartbeatThread(Server& server,
                                 AgencyCallbackRegistry* agencyCallbackRegistry,
                                 std::chrono::microseconds interval,
                                 uint64_t maxFailsBeforeWarning)
    : arangodb::ServerThread<Server>(server, "Heartbeat"),
      _agencyCallbackRegistry(agencyCallbackRegistry),
      _statusLock(std::make_shared<std::mutex>()),
      _agency(server),
      _condition(),
      _myId(ServerState::instance()->getId()),
      _interval(interval),
      _maxFailsBeforeWarning(maxFailsBeforeWarning),
      _numFails(0),
      _lastSuccessfulVersion(0),
      _currentPlanVersion(0),
      _ready(false),
      _hasRunOnce(false),
      _currentVersions(0, 0),
      _desiredVersions(std::make_shared<AgencyVersions>(0, 0)),
      _backgroundJobsPosted(0),
      _lastSyncTime(0),
      _failedVersionUpdates(0),
      _invalidateCoordinators(true),
      _lastPlanVersionNoticed(0),
      _lastCurrentVersionNoticed(0),
      _updateCounter(0),
      _updateDBServers(false),
      _lastUnhealthyTimestamp(std::chrono::steady_clock::time_point()),
      _agencySync(server, this),
      _clusterFeature(server.getFeature<ClusterFeature>()),
      _heartbeat_send_time_ms(server.getFeature<metrics::MetricsFeature>().add(
          arangodb_heartbeat_send_time_msec{})),
      _heartbeat_failure_counter(
          server.getFeature<metrics::MetricsFeature>().add(
              arangodb_heartbeat_failures_total{})) {
  TRI_ASSERT(_maxFailsBeforeWarning > 0);
}

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

  std::unordered_map<std::string, std::shared_ptr<AgencyCallback>>
      serverCallbacks{};
  if (ServerState::instance()->isCoordinator(role) ||
      ServerState::instance()->isDBServer(role)) {
    std::function<bool(VPackSlice const& result)> updbs =
        [self = shared_from_this()](VPackSlice const& result) {
          LOG_TOPIC("fe092", DEBUG, Logger::HEARTBEAT)
              << "Updating cluster's cache of current db servers";
          self->updateDBServers();
          return true;
        };
    std::vector<std::string> const dbServerAgencyPaths{
        "Current/DBServers", "Target/FailedServers", "Target/CleanedServers",
        "Target/ToBeCleanedServers"};
    for (auto const& path : dbServerAgencyPaths) {
      serverCallbacks.try_emplace(
          path,
          std::make_shared<AgencyCallback>(server(), path, updbs, true, false));
    }
    std::function<bool(VPackSlice const& result)> upsrv =
        [self = shared_from_this()](VPackSlice const& result) {
          LOG_TOPIC("2e09f", DEBUG, Logger::HEARTBEAT)
              << "Updating cluster's cache of current servers and rebootIds";
          self->_clusterFeature.clusterInfo().loadServers();
          return true;
        };
    std::vector<std::string> const serverAgencyPaths{
        "Current/ServersRegistered", "Target/MapUniqueToShortID",
        "Current/ServersKnown", "Supervision/Health",
        "Current/ServersKnown/" + ServerState::instance()->getId()};
    for (auto const& path : serverAgencyPaths) {
      serverCallbacks.try_emplace(
          path,
          std::make_shared<AgencyCallback>(server(), path, upsrv, true, false));
    }
    std::function<bool(VPackSlice const& result)> upcrd =
        [self = shared_from_this()](VPackSlice const& result) {
          LOG_TOPIC("2f09e", DEBUG, Logger::HEARTBEAT)
              << "Updating cluster's cache of current coordinators";
          self->_clusterFeature.clusterInfo().loadCurrentCoordinators();
          self->_clusterFeature.clusterInfo().loadCurrentDBServers();
          return true;
        };
    std::string const path = "Current/Coordinators";
    serverCallbacks.try_emplace(path, std::make_shared<AgencyCallback>(
                                          server(), path, upcrd, true, false));

    for (auto const& cb : serverCallbacks) {
      auto res = _agencyCallbackRegistry->registerCallback(cb.second);
      if (!res.ok()) {
        LOG_TOPIC("97aa8", WARN, Logger::HEARTBEAT)
            << "Failed to register agency cache callback to " << cb.first
            << " degrading performance";
      }
    }
  }

  // mop: the heartbeat thread itself is now ready
  setReady();
  // mop: however we need to wait for the rest server here to come up
  // otherwise we would already create collections and the coordinator would
  // think
  // ohhh the dbserver is online...pump some documents into it
  // which fails when it is still in maintenance mode
  auto server = ServerState::instance();
  if (!server->isCoordinator(role)) {
    while (server->isStartupOrMaintenance()) {
      if (isStopping()) {
        // startup aborted
        return;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  LOG_TOPIC("978a8", TRACE, Logger::HEARTBEAT)
      << "starting heartbeat thread (" << role << ")";

  if (ServerState::instance()->isCoordinator(role)) {
    runCoordinator();
  } else if (ServerState::instance()->isDBServer(role)) {
    runDBServer();
  } else if (ServerState::instance()->isAgent(role) ||
             ServerState::instance()->isSingleServer(role)) {
    // do nothing
  } else {
    LOG_TOPIC("8291e", ERR, Logger::HEARTBEAT)
        << "invalid role setup found when starting HeartbeatThread";
    TRI_ASSERT(false);
  }

  for (auto const& acb : serverCallbacks) {
    _agencyCallbackRegistry->unregisterCallback(acb.second);
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

  auto& cache = _clusterFeature.agencyCache();
  auto [acb, idx] = cache.read(std::vector<std::string>{
      AgencyCommHelper::path("Shutdown"),
      AgencyCommHelper::path("Current/Version"),
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
      containers::FlatHashSet<ServerID> failedServers;
      for (auto const& server : VPackObjectIterator(failedServersSlice)) {
        failedServers.emplace(server.key.stringView());
      }
      LOG_TOPIC("52626", DEBUG, Logger::HEARTBEAT)
          << "Updating failed servers list.";
      auto& ci = _clusterFeature.clusterInfo();
      ci.setFailedServers(std::move(failedServers));
      transaction::cluster::abortTransactionsWithFailedServers(ci);
    } else {
      LOG_TOPIC("80491", WARN, Logger::HEARTBEAT)
          << "FailedServers is not an object. ignoring for now";
    }

    VPackSlice s = result[0].get(std::vector<std::string>(
        {AgencyCommHelper::path(), std::string("Current"),
         std::string("Version")}));
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
          std::lock_guard mutexLocker{*_statusLock};
          if (currentVersion > _desiredVersions->current) {
            _desiredVersions->current = currentVersion;
            LOG_TOPIC("33559", DEBUG, Logger::HEARTBEAT)
                << "Found greater Current/Version in agency.";
          }
        }
      }
    }
  }

  // Periodical or event-based update of DBServers and pruning of
  // agency comm connection pool:
  auto updateDBServers = _updateDBServers.exchange(false);
  if (updateDBServers || ++_updateCounter >= 60) {
    auto& ci = _clusterFeature.clusterInfo();
    ci.loadCurrentDBServers();
    _updateCounter = 0;
    _clusterFeature.pruneAsyncAgencyConnectionPool();
  }
}

void HeartbeatThread::updateDBServers() { _updateDBServers.store(true); }

DBServerAgencySync& HeartbeatThread::agencySync() { return _agencySync; }

////////////////////////////////////////////////////////////////////////////////
/// @brief heartbeat main loop, dbserver version
////////////////////////////////////////////////////////////////////////////////

void HeartbeatThread::runDBServer() {
  using namespace std::chrono_literals;

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
      sendServerStateAsync();

      if (isStopping()) {
        break;
      }

      if (getNewsRunning->load(std::memory_order_seq_cst) == 0) {
        // Schedule a getNewsFromAgency call in the Scheduler:
        auto self = shared_from_this();
        Scheduler* scheduler = SchedulerFeature::SCHEDULER;
        *getNewsRunning = 1;
        scheduler->queue(RequestLane::CLUSTER_INTERNAL, [self, getNewsRunning] {
          self->getNewsFromAgencyForDBServer();
          *getNewsRunning = 0;  // indicate completion to trigger a new schedule
        });
        LOG_TOPIC("aaccf", DEBUG, Logger::HEARTBEAT)
            << "Have scheduled getNewsFromAgency job.";
      }

      if (isStopping()) {
        break;
      }

      std::unique_lock locker{_condition.mutex};
      auto remain = _interval - (std::chrono::steady_clock::now() - start);
      if (remain.count() > 0 && !isStopping()) {
        _condition.cv.wait_for(
            locker,
            std::chrono::duration_cast<std::chrono::microseconds>(remain));
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
  auto& ci = _clusterFeature.clusterInfo();

  LOG_TOPIC("33452", DEBUG, Logger::HEARTBEAT) << "getting news from agency...";

  auto start = std::chrono::steady_clock::now();
  auto& cache = _clusterFeature.agencyCache();
  auto [acb, idx] = cache.read(std::vector<std::string>{
      AgencyCommHelper::path("Current/Version"),
      AgencyCommHelper::path("Current/Foxxmaster"),
      AgencyCommHelper::path("Current/FoxxmasterQueueupdate"),
      AgencyCommHelper::path("Plan/Version"),
      AgencyCommHelper::path("Readonly"), AgencyCommHelper::path("Shutdown"),
      AgencyCommHelper::path("Sync/UserVersion"),
      AgencyCommHelper::path("Sync/FoxxQueueVersion"),
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
          << "Heartbeat: Could not read from agency! status code: "
          << result.toJson();
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
    VPackSlice foxxmasterQueueupdateSlice =
        result[0].get(std::vector<std::string>(
            {AgencyCommHelper::path(), "Current", "FoxxmasterQueueupdate"}));

    if (foxxmasterQueueupdateSlice.isBool() &&
        foxxmasterQueueupdateSlice.getBool()) {
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

      AgencyComm agency(server());

      auto updateLeader = agency.casValue(
          "/Current/Foxxmaster", foxxmasterSlice, myIdBuilder.slice(), 0, 10.0);
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

    // handle global changes to Sync/UserVersion
    handleUserVersionChange(result);

#ifdef USE_V8
    // handle global changes to Sync/FoxxQueueVersion
    handleFoxxQueueVersionChange(result);
#endif

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
      containers::FlatHashSet<ServerID> failedServers;
      for (auto const& server : VPackObjectIterator(failedServersSlice)) {
        failedServers.emplace(server.key.stringView());
      }
      LOG_TOPIC("43332", DEBUG, Logger::HEARTBEAT)
          << "Updating failed servers list.";
      ci.setFailedServers(failedServers);
      transaction::cluster::abortTransactionsWithFailedServers(ci);

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
    _clusterFeature.pruneAsyncAgencyConnectionPool();
  }
}

void HeartbeatThread::handleUserVersionChange(VPackSlice userVersion) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());

  AuthenticationFeature& af = server().getFeature<AuthenticationFeature>();

  VPackSlice slice = userVersion[0].get(std::vector<std::string>(
      {AgencyCommHelper::path(), "Sync", "UserVersion"}));

  if (slice.isInteger()) {
    // there is a UserVersion
    uint64_t version = 0;
    try {
      version = slice.getUInt();
    } catch (...) {
    }

    if (version > 0 && af.isActive() && af.userManager() != nullptr) {
      af.userManager()->setGlobalVersion(version);
    }
  }
}

#ifdef USE_V8
void HeartbeatThread::handleFoxxQueueVersionChange(
    VPackSlice foxxQueueVersion) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());

  VPackSlice slice = foxxQueueVersion[0].get(std::vector<std::string>(
      {AgencyCommHelper::path(), "Sync", "FoxxQueueVersion"}));

  if (slice.isInteger()) {
    // there is a UserVersion
    uint64_t version = 0;
    try {
      version = slice.getNumber<uint64_t>();
    } catch (...) {
    }

    if (version > 0) {
      // track the global foxx queues version from the agency. any
      // coordinator can update this any time. the setQueueVersion
      // method makes sure we are not going below a value that
      // we have already seen.
      server().getFeature<FoxxFeature>().setQueueVersion(version);
    }
  }
}
#endif

void HeartbeatThread::updateServerMode(VPackSlice const& readOnlySlice) {
  bool readOnly = false;
  if (readOnlySlice.isBoolean()) {
    readOnly = readOnlySlice.getBool();
  }

  ServerState::instance()->setReadOnly(readOnly ? ServerState::API_TRUE
                                                : ServerState::API_FALSE);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief heartbeat main loop, coordinator version
////////////////////////////////////////////////////////////////////////////////

void HeartbeatThread::runCoordinator() {
  // The following helps to synchronize between a background read
  // operation run in a scheduler thread and a the heartbeat
  // thread. If it is zero, the heartbeat schedules another
  // run, which at its end, sets it back to 0:
  auto getNewsRunning = std::make_shared<std::atomic<int>>(0);

  while (!isStopping()) {
    try {
      auto const start = std::chrono::steady_clock::now();
      // send our state to the agency.
      sendServerStateAsync();

      if (isStopping()) {
        break;
      }

      if (getNewsRunning->load(std::memory_order_seq_cst) == 0) {
        // Schedule a getNewsFromAgency call in the Scheduler:
        auto self = shared_from_this();
        Scheduler* scheduler = SchedulerFeature::SCHEDULER;
        *getNewsRunning = 1;
        scheduler->queue(RequestLane::CLUSTER_INTERNAL, [self, getNewsRunning] {
          self->getNewsFromAgencyForCoordinator();
          *getNewsRunning = 0;  // indicate completion to trigger a new schedule
        });
        LOG_TOPIC("aacc3", DEBUG, Logger::HEARTBEAT)
            << "Have scheduled getNewsFromAgency job.";
      }

      std::unique_lock locker{_condition.mutex};
      auto remain = _interval - (std::chrono::steady_clock::now() - start);
      if (remain.count() > 0 && !isStopping()) {
        _condition.cv.wait_for(
            locker,
            std::chrono::duration_cast<std::chrono::microseconds>(remain));
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
/// @brief initializes the heartbeat
////////////////////////////////////////////////////////////////////////////////

bool HeartbeatThread::init() {
  // send the server state a first time and use this as an indicator about
  // the agency's health
  if (ServerState::instance()->isClusterRole() && !sendServerState()) {
    return false;
  }
  if (ServerState::instance()->isDBServer()) {
    // the maintenanceThread is only required by DB server instances, but we
    // deliberately initialize it here instead of in runDBServer because the
    // ClusterInfo::SyncerThread uses HeartbeatThread::notify to notify this
    // thread, but that SyncerThread is started before runDBServer is called. So
    // in order to prevent a data race we should initialize _maintenanceThread
    // before that SyncerThread is started.
    _maintenanceThread =
        std::make_unique<HeartbeatBackgroundJobThread>(_server, this);
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
      double timeout = agencySync().requestTimeout() + 5.0;
      if (std::chrono::steady_clock::now() - start >
          std::chrono::duration<double>(timeout)) {
        LOG_TOPIC("d8a5c", FATAL, Logger::CLUSTER)
            << "exiting prematurely as we failed terminating the maintenance "
               "thread";
        FATAL_ERROR_EXIT();
      }
      if (++counter % 50 == 0) {
        LOG_TOPIC("acaaa", WARN, arangodb::Logger::CLUSTER)
            << "waiting for maintenance thread to finish";
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    // otherwise FATAL_ERROR_EXIT
    TRI_ASSERT(!_maintenanceThread->isRunning());
    _maintenanceThread->shutdown();
  }

  // And now the heartbeat thread:
  std::lock_guard guard{_condition.mutex};

  // set the shutdown state in parent class
  Thread::beginShutdown();

  // break _condition.wait() in runDBserver
  _condition.cv.notify_one();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finished plan change
////////////////////////////////////////////////////////////////////////////////

void HeartbeatThread::dispatchedJobResult(DBServerAgencySyncResult result) {
  LOG_TOPIC("f3358", DEBUG, Logger::HEARTBEAT) << "Dispatched job returned!";
  std::lock_guard mutexLocker{*_statusLock};
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
  DatabaseFeature& databaseFeature = server().getFeature<DatabaseFeature>();

  LOG_TOPIC("eda7d", TRACE, Logger::HEARTBEAT) << "found a plan update";
  auto& cache = _clusterFeature.agencyCache();
  auto [acb, idx] = cache.read(std::vector<std::string>{
      AgencyCommHelper::path(prefixPlanChangeCoordinator)});
  auto result = acb->slice();

  if (!result.isArray()) {
    return false;
  }
  velocypack::Slice databases = result[0].get(std::vector<std::string>(
      {AgencyCommHelper::path(), "Plan", "Databases"}));
  if (!databases.isObject()) {
    return false;
  }

  containers::FlatHashSet<TRI_voc_tick_t> ids;
  for (auto options : VPackObjectIterator(databases)) {
    try {
      ids.emplace(std::stoull(options.value.get("id").copyString()));
    } catch (std::exception const& e) {
      LOG_TOPIC("a9235", ERR, Logger::CLUSTER)
          << "Failed to read planned databases " << options.key.stringView()
          << " from agency: " << e.what();
      // we cannot continue from here, because otherwise we may be
      // deleting databases we are not clear about
      return false;
    }
  }

  // get the list of databases that we know about locally
  for (auto id : databaseFeature.getDatabaseIds(false)) {
    if (ids.contains(id)) {
      continue;
    }

    // local database not found in the plan...
    std::string dbName;
    if (auto db = databaseFeature.useDatabase(id); db) {
      dbName = db->name();
    } else {
      TRI_ASSERT(false);
      dbName = "n/a";
    }
    Result res = databaseFeature.dropDatabase(id);
    events::DropDatabase(dbName, res, ExecContext::current());
  }

  // loop over all database names we got and create a local database
  // instance if not yet present.
  // redundant functionality also exists in ClusterInfo::loadPlan().
  // however, keeping the same code around here can lead to the
  // databases being picked up more quickly, in case they were created
  // by a different coordinator.
  // if this code is removed, some tests will fail that create a
  // database on one coordinator and then access the newly created
  // database via another coordinator.
  for (auto options : VPackObjectIterator(databases)) {
    if (!options.value.isObject()) {
      continue;
    }

    TRI_ASSERT(options.value.get("name").isString());
    CreateDatabaseInfo info(server(), ExecContext::current());
    // set strict validation for database options to false.
    // we don't want the heartbeat thread to fail here in case some
    // invalid settings are present
    info.strictValidation(false);
    // do not validate database names for existing databases.
    // the rationale is that if a database was already created with
    // an extended name, we should not declare it invalid and abort
    // the startup once the extended names option is turned off.
    info.validateNames(false);
    // when loading we allow system database names
    auto infoResult = info.load(options.value, VPackSlice::emptyArraySlice());
    if (infoResult.fail()) {
      LOG_TOPIC("3fa12", ERR, Logger::HEARTBEAT)
          << "in agency database plan for database " << options.value.toJson()
          << ": " << infoResult.errorMessage();
      TRI_ASSERT(false);
    }

    auto const dbName = info.getName();

    if (auto vocbase = databaseFeature.useDatabase(dbName);
        vocbase == nullptr) {
      // database does not yet exist, create it now

      // create a local database object...
      [[maybe_unused]] TRI_vocbase_t* unused{};
      Result res = databaseFeature.createDatabase(std::move(info), unused);
      events::CreateDatabase(dbName, res, ExecContext::current());

      // note: it is possible that we race with the PlanSyncer thread here,
      // which can also create local databases upon changes in the agency
      // Plan. if so, we don't log an error message.
      if (res.fail() && res.isNot(TRI_ERROR_ARANGO_DUPLICATE_NAME)) {
        LOG_TOPIC("ca877", ERR, arangodb::Logger::HEARTBEAT)
            << "creating local database '" << dbName
            << "' failed: " << res.errorMessage();
      }
    }
  }

  _hasRunOnce.store(true, std::memory_order_release);

  // invalidate our local cache
  _clusterFeature.clusterInfo().flush();
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

  TRI_IF_FAILURE("HeartbeatThread::sendServerState") { return false; }

  auto const start = std::chrono::steady_clock::now();
  ScopeGuard sg([&]() noexcept {
    auto timeDiff = std::chrono::steady_clock::now() - start;
    _heartbeat_send_time_ms.count(
        std::chrono::duration_cast<std::chrono::milliseconds>(timeDiff)
            .count());

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
    TRI_ASSERT(_maxFailsBeforeWarning > 0);
    if (++_numFails % _maxFailsBeforeWarning == 0) {
      _heartbeat_failure_counter.count();
      std::string const endpoints =
          AsyncAgencyCommManager::INSTANCE->endpointsString();

      LOG_TOPIC("3e2f5", WARN, Logger::HEARTBEAT)
          << "heartbeat could not be sent to agency endpoints (" << endpoints
          << "): http code: " << result.httpCode()
          << ", body: " << result.body();
      _numFails = 0;
    }
  }

  return false;
}

void HeartbeatThread::sendServerStateAsync() {
  auto const start = std::chrono::steady_clock::now();

  // construct JSON value { "status": "...", "time": "...", "healthy": ... }
  VPackBuilder builder;

  try {
    builder.openObject();
    std::string const status =
        ServerState::stateToString(ServerState::instance()->getState());
    builder.add("status", VPackValue(status));
    std::string const stamp = AgencyCommHelper::generateStamp();
    builder.add("time", VPackValue(stamp));

    if (ServerState::instance()->isDBServer()) {
      // use storage engine health self-assessment and send it to agency too
      arangodb::HealthData hd =
          server().getFeature<EngineSelectorFeature>().engine().healthCheck();
      // intentionally dont transmit details so we can save a bit of traffic
      hd.toVelocyPack(builder, /*withDetails*/ false);
    }

    builder.close();
  } catch (std::exception const& e) {
    LOG_TOPIC("b109f", WARN, Logger::HEARTBEAT)
        << "failed to construct heartbeat agency transaction: " << e.what();
    return;
  } catch (...) {
    LOG_TOPIC("06781", WARN, Logger::HEARTBEAT)
        << "failed to construct heartbeat agency transaction";
    return;
  }

  network::Timeout timeout = std::chrono::seconds{20};
  if (isStopping()) {
    timeout = std::chrono::seconds{5};
  }

  AsyncAgencyComm ac;
  auto f = ac.setTransientValue(
      "arango/Sync/ServerStates/" + ServerState::instance()->getId(),
      builder.slice(), {.skipScheduler = true, .timeout = timeout});

  std::move(f).thenFinal(
      [self = shared_from_this(),
       start](futures::Try<AsyncAgencyCommResult> const& tryResult) noexcept {
        auto timeDiff = std::chrono::steady_clock::now() - start;
        self->_heartbeat_send_time_ms.count(
            std::chrono::duration_cast<std::chrono::milliseconds>(timeDiff)
                .count());

        if (timeDiff > std::chrono::seconds(2) && !self->isStopping()) {
          LOG_TOPIC("776a5", WARN, Logger::HEARTBEAT)
              << "ATTENTION: Sending a heartbeat took longer than 2 seconds, "
                 "this might be causing trouble with health checks. Please "
                 "contact ArangoDB Support.";
        }

        try {
          auto const& result = tryResult.get().asResult();
          if (result.fail()) {
            TRI_ASSERT(self->_maxFailsBeforeWarning > 0);
            if (++self->_numFails % self->_maxFailsBeforeWarning == 0) {
              self->_heartbeat_failure_counter.count();
              std::string const endpoints =
                  AsyncAgencyCommManager::INSTANCE->endpointsString();
              LOG_TOPIC("4e2f5", WARN, Logger::HEARTBEAT)
                  << "heartbeat could not be sent to agency endpoints ("
                  << endpoints << "): error code: " << result.errorMessage();
              self->_numFails = 0;
            }
          } else {
            self->_numFails = 0;
          }
        } catch (std::exception const& e) {
          LOG_TOPIC("cfd83", WARN, Logger::HEARTBEAT)
              << "failed to send heartbeat - exception occurred: " << e.what();
        } catch (...) {
          LOG_TOPIC("cfe83", WARN, Logger::HEARTBEAT)
              << "failed to send heartbeat - unknown exception occurred.";
        }
      });
}

void HeartbeatThread::updateAgentPool(VPackSlice const& agentPool) {
  if (agentPool.isObject() && agentPool.get("pool").isObject() &&
      agentPool.hasKey("size") && agentPool.get("size").getUInt() > 0 &&
      agentPool.get("id").isString()) {
    try {
      std::vector<std::string> values;
      // we have to make sure that the leader is on the front
      auto leaderId = agentPool.get("id").stringView();
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
