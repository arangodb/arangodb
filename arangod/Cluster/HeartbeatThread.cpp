////////////////////////////////////////////////////////////////////////////////
/// disclaimer
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "HeartbeatThread.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>
#include <date/date.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ConditionLocker.h"
#include "Basics/MutexLocker.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/tri-strings.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/DBServerAgencySync.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/AsyncJobManager.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "Logger/Logger.h"
#include "Replication/GlobalInitialSyncer.h"
#include "Replication/GlobalReplicationApplier.h"
#include "Replication/ReplicationFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Scheduler/JobGuard.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "V8/v8-globals.h"
#include "VocBase/vocbase.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/Recovery.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::rest;

std::atomic<bool> HeartbeatThread::HasRunOnce(false);

////////////////////////////////////////////////////////////////////////////////
/// @brief static object to record thread crashes.  it is static so that
///        it can contain information about threads that crash before HeartbeatThread
///        starts (i.e. HeartbeatThread starts late).  this list is intentionally
///        NEVER PURGED so that it can be reposted to logs regularly
////////////////////////////////////////////////////////////////////////////////

static std::multimap<std::chrono::system_clock::time_point /* when */, const std::string /* threadName */> deadThreads;

static std::chrono::system_clock::time_point deadThreadsPosted;  // defaults to epoch

static arangodb::Mutex deadThreadsMutex;

namespace arangodb {


class HeartbeatBackgroundJobThread : public Thread {

public:
  explicit HeartbeatBackgroundJobThread(HeartbeatThread* heartbeatThread) 
      : Thread("Maintenance"),
        _heartbeatThread(heartbeatThread),
        _stop(false),
        _sleeping(false),
        _backgroundJobsLaunched(0) {}

  ~HeartbeatBackgroundJobThread() { shutdown(); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief asks the thread to stop, but does not wait.
  //////////////////////////////////////////////////////////////////////////////
  void stop() {
    std::unique_lock<std::mutex> guard(_mutex);
    _stop = true;
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
      _condition.notify_one();
    }
  }

protected:
  void run() override {

    while (!_stop) {

      {
        std::unique_lock<std::mutex> guard(_mutex);

        if (!_anotherRun.load(std::memory_order_acquire)) {
          _sleeping.store(true, std::memory_order_release);

          while (true) {
            _condition.wait(guard);

            if (_stop) {
              return ;
            } else if (_anotherRun) {
              break ;
            } // otherwise spurious wakeup
          }

          _sleeping.store(false, std::memory_order_release);
        }

        _anotherRun.store(false, std::memory_order_release);
      }

      // execute schmutz here
      uint64_t jobNr = ++_backgroundJobsLaunched;
      LOG_TOPIC(DEBUG, Logger::HEARTBEAT) << "sync callback started " << jobNr;
      {
        DBServerAgencySync job(_heartbeatThread);
        job.work();
      }
      LOG_TOPIC(DEBUG, Logger::HEARTBEAT) << "sync callback ended " << jobNr;

    }
  }

private:
  HeartbeatThread *_heartbeatThread;

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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a heartbeat thread
////////////////////////////////////////////////////////////////////////////////

HeartbeatThread::HeartbeatThread(AgencyCallbackRegistry* agencyCallbackRegistry,
                                 std::chrono::microseconds interval,
                                 uint64_t maxFailsBeforeWarning)
    : CriticalThread("Heartbeat"),
      _agencyCallbackRegistry(agencyCallbackRegistry),
      _statusLock(std::make_shared<Mutex>()),
      _agency(),
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
      _wasNotified(false),
      _backgroundJobsPosted(0),
      _lastSyncTime(0),
      _maintenanceThread(nullptr) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a heartbeat thread
////////////////////////////////////////////////////////////////////////////////

HeartbeatThread::~HeartbeatThread() {
  if (_maintenanceThread) {
    _maintenanceThread->stop();
  }
  shutdown();
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
      std::this_thread::sleep_for(std::chrono::microseconds(100000));
    }
  }

  LOG_TOPIC(TRACE, Logger::HEARTBEAT)
      << "starting heartbeat thread (" << role << ")";

  if (ServerState::instance()->isCoordinator(role)) {
    runCoordinator();
  } else if (ServerState::instance()->isDBServer(role)) {
    runDBServer();
  } else if (ServerState::instance()->isSingleServer(role)) {
    if (ReplicationFeature::INSTANCE->isActiveFailoverEnabled()) {
      runSingleServer();
    } else {
      // runSimpleServer();  // for later when CriticalThreads identified
    } // else
  } else if (ServerState::instance()->isAgent(role)) {
    runSimpleServer();
  } else {
    LOG_TOPIC(ERR, Logger::HEARTBEAT)
        << "invalid role setup found when starting HeartbeatThread";
    TRI_ASSERT(false);
  }

  LOG_TOPIC(TRACE, Logger::HEARTBEAT)
      << "stopped heartbeat thread (" << role << ")";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief heartbeat main loop, dbserver version
////////////////////////////////////////////////////////////////////////////////

void HeartbeatThread::runDBServer() {

  _maintenanceThread = std::make_unique<HeartbeatBackgroundJobThread>(this);
  if (!_maintenanceThread->start()) {
    // WHAT TO DO NOW?
    LOG_TOPIC(ERR, Logger::HEARTBEAT) << "Failed to start dedicated thread for maintenance";
  }

  std::function<bool(VPackSlice const& result)> updatePlan =
    [=](VPackSlice const& result) {

    if (!result.isNumber()) {
      LOG_TOPIC(ERR, Logger::HEARTBEAT)
      << "Plan Version is not a number! " << result.toJson();
      return false;
    }

    uint64_t version = result.getNumber<uint64_t>();
    bool doSync = false;

    {
      MUTEX_LOCKER(mutexLocker, *_statusLock);
      if (version > _desiredVersions->plan) {
        _desiredVersions->plan = version;
        LOG_TOPIC(DEBUG, Logger::HEARTBEAT)
          << "Desired Plan Version is now " << _desiredVersions->plan;
        doSync = true;
      }
    }

    if (doSync) {
      syncDBServerStatusQuo(true);
    }

    return true;
  };

  auto planAgencyCallback = std::make_shared<AgencyCallback>(
      _agency, "Plan/Version", updatePlan, true);

  bool registered = false;
  while (!registered) {
    registered =
      _agencyCallbackRegistry->registerCallback(planAgencyCallback);
    if (!registered) {
      LOG_TOPIC(ERR, Logger::HEARTBEAT)
          << "Couldn't register plan change in agency!";
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }

  std::function<bool(VPackSlice const& result)> updateCurrent =
    [=](VPackSlice const& result) {

    if (!result.isNumber()) {
      LOG_TOPIC(ERR, Logger::HEARTBEAT)
      << "Plan Version is not a number! " << result.toJson();
      return false;
    }

    uint64_t version = result.getNumber<uint64_t>();
    bool doSync = false;

    {
      MUTEX_LOCKER(mutexLocker, *_statusLock);
      if (version > _desiredVersions->current) {
        _desiredVersions->current = version;
        LOG_TOPIC(DEBUG, Logger::HEARTBEAT)
          << "Desired Current Version is now " << _desiredVersions->plan;
        doSync = true;
      }
    }

    if (doSync) {
      syncDBServerStatusQuo(true);
    }

    return true;
  };

  auto currentAgencyCallback = std::make_shared<AgencyCallback>(
      _agency, "Current/Version", updateCurrent, true);

  registered = false;
  while (!registered) {
    registered =
      _agencyCallbackRegistry->registerCallback(currentAgencyCallback);
    if (!registered) {
      LOG_TOPIC(ERR, Logger::HEARTBEAT)
          << "Couldn't register current change in agency!";
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }

  // we check Current/Version every few heartbeats:
  int const currentCountStart = 1;  // set to 1 by Max to speed up discovery
  int currentCount = currentCountStart;

  // Loop priorities / goals
  // 0. send state to agency server
  // 1. schedule handlePlanChange immediately when agency callback occurs
  // 2. poll for plan change, schedule handlePlanChange immediately if change detected
  // 3. force handlePlanChange every 7.4 seconds just in case
  //     (7.4 seconds is just less than half the 15 seconds agency uses to declare dead server)
  // 4. if handlePlanChange runs long (greater than 7.4 seconds), have another start immediately after

  while (!isStopping()) {
    logThreadDeaths();

    try {
      auto const start = std::chrono::steady_clock::now();
      // send our state to the agency.
      // we don't care if this fails
      sendServerState();

      if (isStopping()) {
        break;
      }

      if (--currentCount == 0) {
        currentCount = currentCountStart;

        // DBServers disregard the ReadOnly flag, otherwise (without authentication and JWT)
        // we are not able to identify valid requests from other cluster servers
        AgencyReadTransaction trx(
          std::vector<std::string>({
              AgencyCommManager::path("Shutdown"),
              AgencyCommManager::path("Current/Version"),
              "/.agency"}));

        AgencyCommResult result = _agency.sendTransactionWithFailover(trx, 1.0);
        if (!result.successful()) {
          if (!application_features::ApplicationServer::isStopping()) {
            LOG_TOPIC(WARN, Logger::HEARTBEAT)
                << "Heartbeat: Could not read from agency!";
          }
        } else {

          VPackSlice agentPool = result.slice()[0].get(".agency");
          updateAgentPool(agentPool);

          VPackSlice shutdownSlice =
              result.slice()[0].get(std::vector<std::string>(
                  {AgencyCommManager::path(), "Shutdown"}));

          if (shutdownSlice.isBool() && shutdownSlice.getBool()) {
            ApplicationServer::server->beginShutdown();
            break;
          }

          VPackSlice s = result.slice()[0].get(std::vector<std::string>(
              {AgencyCommManager::path(), std::string("Current"),
               std::string("Version")}));
          if (!s.isInteger()) {
            LOG_TOPIC(ERR, Logger::HEARTBEAT)
                << "Current/Version in agency is not an integer.";
          } else {
            uint64_t currentVersion = 0;
            try {
              currentVersion = s.getUInt();
            } catch (...) {
            }
            if (currentVersion == 0) {
              LOG_TOPIC(ERR, Logger::HEARTBEAT)
                  << "Current/Version in agency is 0.";
            } else {
              {
                MUTEX_LOCKER(mutexLocker, *_statusLock);
                if (currentVersion > _desiredVersions->current) {
                  _desiredVersions->current = currentVersion;
                  LOG_TOPIC(DEBUG, Logger::HEARTBEAT)
                      << "Found greater Current/Version in agency.";
                }
              }
              syncDBServerStatusQuo();
            }
          }
        }
      }

      if (isStopping()) {
        break;
      }

      auto remain = _interval - (std::chrono::steady_clock::now() - start);
      // mop: execute at least once
      do {

        if (isStopping()) {
          break;
        }

        LOG_TOPIC(TRACE, Logger::HEARTBEAT) << "Entering update loop";

        bool wasNotified;
        {
          CONDITION_LOCKER(locker, _condition);
          wasNotified = _wasNotified;
          if (!wasNotified && !isStopping()) {
            if (remain.count() > 0) {
              locker.wait(std::chrono::duration_cast<std::chrono::microseconds>(remain));
              wasNotified = _wasNotified;
            }
          }
          _wasNotified = false;
        }

        if (isStopping()) {
          break;
        }

        if (!wasNotified) {
          LOG_TOPIC(DEBUG, Logger::HEARTBEAT) << "Heart beating...";
          planAgencyCallback->refetchAndUpdate(true, false);
          currentAgencyCallback->refetchAndUpdate(true, false);
        } else {
          // mop: a plan change returned successfully...
          // recheck and redispatch in case our desired versions increased
          // in the meantime
          LOG_TOPIC(TRACE, Logger::HEARTBEAT) << "wasNotified==true";
          syncDBServerStatusQuo();
        }
        remain = _interval - (std::chrono::steady_clock::now() - start);
      } while (remain.count() > 0);
    } catch (std::exception const& e) {
      LOG_TOPIC(ERR, Logger::HEARTBEAT)
          << "Got an exception in DBServer heartbeat: " << e.what();
    } catch (...) {
      LOG_TOPIC(ERR, Logger::HEARTBEAT)
          << "Got an unknown exception in DBServer heartbeat";
    }
  }

  _agencyCallbackRegistry->unregisterCallback(currentAgencyCallback);
  _agencyCallbackRegistry->unregisterCallback(planAgencyCallback);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief heartbeat main loop, single server version
////////////////////////////////////////////////////////////////////////////////

void HeartbeatThread::runSingleServer() {
  AuthenticationFeature* af = AuthenticationFeature::instance();
  TRI_ASSERT(af != nullptr);
  ReplicationFeature* replication = ReplicationFeature::INSTANCE;
  TRI_ASSERT(replication != nullptr);

  GlobalReplicationApplier* applier = replication->globalReplicationApplier();
  ClusterInfo* ci = ClusterInfo::instance();
  TRI_ASSERT(applier != nullptr && ci != nullptr);

  std::string const leaderPath = "Plan/AsyncReplication/Leader";
  std::string const transientPath = "AsyncReplication/" + _myId;

  VPackBuilder myIdBuilder;
  myIdBuilder.add(VPackValue(_myId));

  uint64_t lastSentVersion = 0;
  auto start = std::chrono::steady_clock::now();
  while (!isStopping()) {
    logThreadDeaths();

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
      uint64_t currentVersion = DatabaseFeature::DATABASE->versionTracker()->current();
      if (currentVersion != lastSentVersion) {
        AgencyOperation incrementVersion("Plan/Version", AgencySimpleOperationType::INCREMENT_OP);
        AgencyWriteTransaction trx(incrementVersion);
        AgencyCommResult res = _agency.sendTransactionWithFailover(trx, timeout);

        if (res.successful()) {
          LOG_TOPIC(TRACE, Logger::HEARTBEAT) << "successfully increased plan version in agency";
        } else {
          LOG_TOPIC(WARN, Logger::HEARTBEAT) << "could not increase version number in agency";
        }
        lastSentVersion = currentVersion;
      }

      AgencyReadTransaction trx(
        std::vector<std::string>({
            AgencyCommManager::path("Shutdown"),
            AgencyCommManager::path("Readonly"),
            AgencyCommManager::path("Plan/AsyncReplication"),
            "/.agency"}));
      AgencyCommResult result = _agency.sendTransactionWithFailover(trx, timeout);
      if (!result.successful()) {
        if (!application_features::ApplicationServer::isStopping()) {
          LOG_TOPIC(WARN, Logger::HEARTBEAT)
              << "Heartbeat: Could not read from agency! status code: "
              << result._statusCode << ", incriminating body: "
              << result.bodyRef() << ", timeout: " << timeout;
        }

        if (!applier->isActive()) { // assume agency and leader are gone
          ServerState::instance()->setFoxxmaster(_myId);
          ServerState::instance()->setServerMode(ServerState::Mode::DEFAULT);
        }
        continue;
      }

      VPackSlice response = result.slice()[0];
      VPackSlice agentPool = response.get(".agency");
      updateAgentPool(agentPool);

      VPackSlice shutdownSlice = response.get<std::string>({AgencyCommManager::path(), "Shutdown"});
      if (shutdownSlice.isBool() && shutdownSlice.getBool()) {
        ApplicationServer::server->beginShutdown();
        break;
      }

      // performing failover checks
      VPackSlice async = response.get<std::string>({AgencyCommManager::path(), "Plan", "AsyncReplication"});
      if (!async.isObject()) {
        LOG_TOPIC(WARN, Logger::HEARTBEAT)
          << "Heartbeat: Could not read async-replication metadata from agency!";
        continue;
      }

      VPackSlice leader = async.get("Leader");
      if (!leader.isString() || leader.getStringLength() == 0) {
        // Case 1: No leader in agency. Race for leadership
        // ONLY happens on the first startup. Supervision performs failovers
        LOG_TOPIC(WARN, Logger::HEARTBEAT) << "Leadership vaccuum detected, "
        << "attempting a takeover";

        // if we stay a slave, the redirect will be turned on again
        ServerState::instance()->setServerMode(ServerState::Mode::TRYAGAIN);
        if (leader.isNone()) {
          result = _agency.casValue(leaderPath, myIdBuilder.slice(), /*prevExist*/ false,
                                    /*ttl*/ 0, /*timeout*/ 5.0);
        } else {
          result = _agency.casValue(leaderPath, /*old*/leader, /*new*/myIdBuilder.slice(),
                                    /*ttl*/ 0, /*timeout*/ 5.0);
        }

        if (result.successful()) { // successful leadership takeover
          leader = myIdBuilder.slice();
          // intentionally falls through to case 2
        } else if (result.httpCode() == TRI_ERROR_HTTP_PRECONDITION_FAILED) {
          // we did not become leader, someone else is, response contains
          // current value in agency
          LOG_TOPIC(INFO, Logger::HEARTBEAT) << "Did not become leader";
          continue;
        } else {
          LOG_TOPIC(WARN, Logger::HEARTBEAT) << "got an unexpected agency error "
          << "code: " << result.httpCode() << " msg: " << result.errorMessage();
          continue; // try again next time
        }
      }

      TRI_voc_tick_t lastTick = 0; // we always want to set lastTick
      auto sendTransient = [&]() {
        VPackBuilder builder;
        builder.openObject();
        builder.add("leader", leader);
        builder.add("lastTick", VPackValue(lastTick));
        builder.close();
        double ttl = std::chrono::duration_cast<std::chrono::seconds>(_interval).count() * 5.0;
        _agency.setTransient(transientPath, builder.slice(), ttl);
      };
      TRI_DEFER(sendTransient());

      // Case 2: Current server is leader
      if (leader.compareString(_myId) == 0) {
        if (applier->isActive()) {
          applier->stopAndJoin();
        }
        lastTick = EngineSelectorFeature::ENGINE->currentTick();

        // put the leader in optional read-only mode
        auto readOnlySlice = response.get(std::vector<std::string>(
                                          {AgencyCommManager::path(), "Readonly"}));
        updateServerMode(readOnlySlice);

        // ensure everyone has server access
        ServerState::instance()->setFoxxmaster(_myId);
        auto prv = ServerState::setServerMode(ServerState::Mode::DEFAULT);
        if (prv == ServerState::Mode::REDIRECT) {
          LOG_TOPIC(INFO, Logger::HEARTBEAT) << "Successful leadership takeover: "
                                             << "All your base are belong to us";
        }
        continue; // nothing more to do
      }

      // Case 3: Current server is follower, should not get here otherwise
      std::string const leaderStr = leader.copyString();
      TRI_ASSERT(!leaderStr.empty());
      LOG_TOPIC(TRACE, Logger::HEARTBEAT) << "Following: " << leader;

      ServerState::instance()->setFoxxmaster(leaderStr); // leader is foxxmater
      ServerState::instance()->setReadOnly(true); // Disable writes with dirty-read header

      std::string endpoint = ci->getServerEndpoint(leaderStr);
      if (endpoint.empty()) {
        LOG_TOPIC(ERR, Logger::HEARTBEAT) << "Failed to resolve leader endpoint";
        continue; // try again next time
      }

      // enable redirection to leader
      auto prv = ServerState::instance()->setServerMode(ServerState::Mode::REDIRECT);
      if (prv == ServerState::Mode::DEFAULT) {
        // we were leader previously, now we need to ensure no ongoing operations
        // on this server may prevent us from being a proper follower. We wait for
        // all ongoing ops to stop, and make sure nothing is committed
        LOG_TOPIC(INFO, Logger::HEARTBEAT) << "Detected leader to follower switch";
        TRI_ASSERT(!applier->isActive());
        applier->forget(); // make sure applier is doing a resync

        Result res = GeneralServerFeature::JOB_MANAGER->clearAllJobs();
        if (res.fail()) {
          LOG_TOPIC(WARN, Logger::HEARTBEAT) << "could not cancel all async jobs "
            << res.errorMessage();
        }
        // wait for everything to calm down for good measure
        std::this_thread::sleep_for(std::chrono::seconds(10));
      }

      if (applier->isActive() && applier->endpoint() == endpoint) {
        lastTick = applier->lastTick();
      } else if (applier->endpoint() != endpoint) { // configure applier for new endpoint
        // this means there is a new leader in the agency
        if (applier->isActive()) {
          applier->stopAndJoin();
        }
        while (applier->isShuttingDown() && !isStopping()) {
          std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        LOG_TOPIC(INFO, Logger::HEARTBEAT) << "Starting replication from " << endpoint;
        ReplicationApplierConfiguration config = applier->configuration();
        if (config._jwt.empty()) {
          config._jwt = af->tokenCache().jwtToken();
        }
        config._endpoint = endpoint;
        config._autoResync = true;
        config._autoResyncRetries = 2;
        LOG_TOPIC(INFO, Logger::HEARTBEAT) << "start initial sync from leader";
        config._requireFromPresent = true;
        config._incremental = true;
        TRI_ASSERT(!config._skipCreateDrop);

        applier->forget(); // forget about any existing configuration
        applier->reconfigure(config);
        applier->startReplication();

      } else if (!applier->isActive() && !applier->isShuttingDown()) {
        // try to restart the applier unless the user actively stopped it
        if (applier->hasState()) {
          Result error = applier->lastError();
          if (error.is(TRI_ERROR_REPLICATION_APPLIER_STOPPED)) {
            LOG_TOPIC(WARN, Logger::HEARTBEAT) << "user stopped applier, please restart";
            continue;
          } else if (error.isNot(TRI_ERROR_REPLICATION_MASTER_INCOMPATIBLE) &&
                     error.isNot(TRI_ERROR_REPLICATION_MASTER_CHANGE) &&
                     error.isNot(TRI_ERROR_REPLICATION_LOOP) &&
                     error.isNot(TRI_ERROR_REPLICATION_NO_START_TICK) &&
                     error.isNot(TRI_ERROR_REPLICATION_START_TICK_NOT_PRESENT)) {
            LOG_TOPIC(WARN, Logger::HEARTBEAT) << "restarting stopped applier... ";
            VPackBuilder debug;
            debug.openObject();
            applier->toVelocyPack(debug);
            debug.close();
            LOG_TOPIC(DEBUG, Logger::HEARTBEAT) << "previous applier state was: " << debug.toJson();
            applier->startTailing(0, false, 0); // reads ticks from configuration
            continue; // check again next time
          }
        }
        // complete resync next round
        LOG_TOPIC(WARN, Logger::HEARTBEAT) << "forgetting previous applier state. Will trigger a full resync now";
        applier->forget();
      }

    } catch (std::exception const& e) {
      LOG_TOPIC(ERR, Logger::HEARTBEAT)
          << "got an exception in single server heartbeat: " << e.what();
    } catch (...) {
      LOG_TOPIC(ERR, Logger::HEARTBEAT)
          << "got an unknown exception in single server heartbeat";
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
  AuthenticationFeature* af = application_features::ApplicationServer::getFeature<
            AuthenticationFeature>("Authentication");
  TRI_ASSERT(af != nullptr);

  // invalidate coordinators every 2nd call
  bool invalidateCoordinators = true;

  // last value of plan which we have noticed:
  uint64_t lastPlanVersionNoticed = 0;
  // last value of current which we have noticed:
  uint64_t lastCurrentVersionNoticed = 0;
  // For periodic update of the current DBServer list:
  int DBServerUpdateCounter = 0;

  while (!isStopping()) {
    try {
      logThreadDeaths();
      auto const start = std::chrono::steady_clock::now();
      // send our state to the agency.
      // we don't care if this fails
      sendServerState();

      if (isStopping()) {
        break;
      }

      double const timeout = 1.0;

      AgencyReadTransaction trx
        (std::vector<std::string>(
          {AgencyCommManager::path("Current/Version"),
           AgencyCommManager::path("Current/Foxxmaster"),
           AgencyCommManager::path("Current/FoxxmasterQueueupdate"),
           AgencyCommManager::path("Plan/Version"),
           AgencyCommManager::path("Readonly"),
           AgencyCommManager::path("Shutdown"),
           AgencyCommManager::path("Sync/UserVersion"),
           AgencyCommManager::path("Target/FailedServers"), "/.agency"}));
      AgencyCommResult result = _agency.sendTransactionWithFailover(trx, timeout);

      if (!result.successful()) {
        if (!application_features::ApplicationServer::isStopping()) {
          LOG_TOPIC(WARN, Logger::HEARTBEAT)
              << "Heartbeat: Could not read from agency! status code: "
              << result._statusCode << ", incriminating body: "
              << result.bodyRef() << ", timeout: " << timeout;
        }
      } else {

        VPackSlice agentPool = result.slice()[0].get(".agency");
        updateAgentPool(agentPool);

        VPackSlice shutdownSlice = result.slice()[0].get(
            std::vector<std::string>({AgencyCommManager::path(), "Shutdown"}));

        if (shutdownSlice.isBool() && shutdownSlice.getBool()) {
          ApplicationServer::server->beginShutdown();
          break;
        }

        // mop: order is actually important here...FoxxmasterQueueupdate will
        // be set only when somebody registers some new queue stuff (for example
        // on a different coordinator than this one)... However when we are just
        // about to become the new foxxmaster we must immediately refresh our
        // queues this is done in ServerState...if queueupdate is set after
        // foxxmaster the change will be reset again
        VPackSlice foxxmasterQueueupdateSlice = result.slice()[0].get(
            std::vector<std::string>({AgencyCommManager::path(), "Current",
                                      "FoxxmasterQueueupdate"}));

        if (foxxmasterQueueupdateSlice.isBool()) {
          ServerState::instance()->setFoxxmasterQueueupdate(
              foxxmasterQueueupdateSlice.getBool());
        }

        VPackSlice foxxmasterSlice =
            result.slice()[0].get(std::vector<std::string>(
                {AgencyCommManager::path(), "Current", "Foxxmaster"}));

        if (foxxmasterSlice.isString() && foxxmasterSlice.getStringLength() != 0) {
          ServerState::instance()->setFoxxmaster(foxxmasterSlice.copyString());
        } else {
          auto state = ServerState::instance();
          VPackBuilder myIdBuilder;
          myIdBuilder.add(VPackValue(state->getId()));

          auto updateMaster =
              _agency.casValue("/Current/Foxxmaster", foxxmasterSlice,
                               myIdBuilder.slice(), 0, 1.0);
          if (updateMaster.successful()) {
            // We won the race we are the master
            ServerState::instance()->setFoxxmaster(state->getId());
          }
          _agency.increment("Current/Version");
        }

        VPackSlice versionSlice =
            result.slice()[0].get(std::vector<std::string>(
                {AgencyCommManager::path(), "Plan", "Version"}));

        if (versionSlice.isInteger()) {
          // there is a plan version

          uint64_t planVersion = 0;
          try {
            planVersion = versionSlice.getUInt();
          } catch (...) {
          }

          if (planVersion > lastPlanVersionNoticed) {
            LOG_TOPIC(TRACE, Logger::HEARTBEAT)
                << "Found planVersion " << planVersion
                << " which is newer than " << lastPlanVersionNoticed;
            if (handlePlanChangeCoordinator(planVersion)) {
              lastPlanVersionNoticed = planVersion;
            } else {
              LOG_TOPIC(WARN, Logger::HEARTBEAT)
                  << "handlePlanChangeCoordinator was unsuccessful";
            }
          }
        }

        VPackSlice slice = result.slice()[0].get(std::vector<std::string>(
            {AgencyCommManager::path(), "Sync", "UserVersion"}));

        if (slice.isInteger()) {
          // there is a UserVersion
          uint64_t userVersion = 0;
          try {
            userVersion = slice.getUInt();
          } catch (...) {
          }

          if (userVersion > 0) {
            if (af->isActive() && af->userManager() != nullptr) {
              af->userManager()->setGlobalVersion(userVersion);
            }
          }
        }

        versionSlice = result.slice()[0].get(std::vector<std::string>(
            {AgencyCommManager::path(), "Current", "Version"}));
        if (versionSlice.isInteger()) {
          uint64_t currentVersion = 0;
          try {
            currentVersion = versionSlice.getUInt();
          } catch (...) {
          }
          if (currentVersion > lastCurrentVersionNoticed) {
            LOG_TOPIC(TRACE, Logger::HEARTBEAT)
                << "Found currentVersion " << currentVersion
                << " which is newer than " << lastCurrentVersionNoticed;
            lastCurrentVersionNoticed = currentVersion;

            ClusterInfo::instance()->invalidateCurrent();
            invalidateCoordinators = false;
          }
        }

        VPackSlice failedServersSlice =
            result.slice()[0].get(std::vector<std::string>(
                {AgencyCommManager::path(), "Target", "FailedServers"}));

        if (failedServersSlice.isObject()) {
          std::vector<ServerID> failedServers = {};
          for (auto const& server : VPackObjectIterator(failedServersSlice)) {
            failedServers.push_back(server.key.copyString());
          }
          ClusterInfo::instance()->setFailedServers(failedServers);

          pregel::PregelFeature *prgl = pregel::PregelFeature::instance();
          if (prgl != nullptr && failedServers.size() > 0) {
            pregel::RecoveryManager* mngr = prgl->recoveryManager();
            if (mngr != nullptr) {
              try {
                mngr->updatedFailedServers(failedServers);
              } catch (std::exception const& e) {
                LOG_TOPIC(ERR, Logger::HEARTBEAT)
                << "Got an exception in coordinator heartbeat: " << e.what();
              }
            }
          }
        } else {
          LOG_TOPIC(WARN, Logger::HEARTBEAT)
              << "FailedServers is not an object. ignoring for now";
        }

        auto readOnlySlice = result.slice()[0].get(std::vector<std::string>(
          {AgencyCommManager::path(), "Readonly"}));
        updateServerMode(readOnlySlice);
      }

      // the Foxx stuff needs an updated list of coordinators
      // and this is only updated when current version has changed
      if (invalidateCoordinators) {
        ClusterInfo::instance()->invalidateCurrentCoordinators();
      }
      invalidateCoordinators = !invalidateCoordinators;

      // Periodically update the list of DBServers:
      if (++DBServerUpdateCounter >= 60) {
        ClusterInfo::instance()->loadCurrentDBServers();
        DBServerUpdateCounter = 0;
      }

      CONDITION_LOCKER(locker, _condition);
      auto remain = _interval - (std::chrono::steady_clock::now() - start);
      if (remain.count() > 0 && !isStopping()) {
        locker.wait(std::chrono::duration_cast<std::chrono::microseconds>(remain));
      }

    } catch (std::exception const& e) {
      LOG_TOPIC(ERR, Logger::HEARTBEAT)
          << "Got an exception in coordinator heartbeat: " << e.what();
    } catch (...) {
      LOG_TOPIC(ERR, Logger::HEARTBEAT)
          << "Got an unknown exception in coordinator heartbeat";
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief heartbeat main loop, agent and sole db version
////////////////////////////////////////////////////////////////////////////////

void HeartbeatThread::runSimpleServer() {

  // simple loop to post dead threads every hour, no other tasks today
  while (!isStopping()) {
    logThreadDeaths();

    {
      CONDITION_LOCKER(locker, _condition);
      if (!isStopping()) {
        locker.wait(std::chrono::hours(1));
      }
    }
  } // while

  logThreadDeaths(true);

} // HeartbeatThread::runSimpleServer

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
  LOG_TOPIC(DEBUG, Logger::HEARTBEAT) << "Dispatched job returned!";
  MUTEX_LOCKER(mutexLocker, *_statusLock);
  if (result.success) {
    LOG_TOPIC(DEBUG, Logger::HEARTBEAT)
        << "Sync request successful. Now have Plan " << result.planVersion
        << ", Current " << result.currentVersion;
    _currentVersions = AgencyVersions(result);
  } else {
    LOG_TOPIC(ERR, Logger::HEARTBEAT) << "Sync request failed: "
      << result.errorMessage;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a plan version change, coordinator case
/// this is triggered if the heartbeat thread finds a new plan version number
////////////////////////////////////////////////////////////////////////////////

static std::string const prefixPlanChangeCoordinator = "Plan/Databases";
bool HeartbeatThread::handlePlanChangeCoordinator(uint64_t currentPlanVersion) {
  DatabaseFeature* databaseFeature =
      application_features::ApplicationServer::getFeature<DatabaseFeature>(
          "Database");

  LOG_TOPIC(TRACE, Logger::HEARTBEAT) << "found a plan update";
  AgencyCommResult result = _agency.getValues(prefixPlanChangeCoordinator);

  if (result.successful()) {
    std::vector<TRI_voc_tick_t> ids;
    velocypack::Slice databases =
        result.slice()[0].get(std::vector<std::string>(
            {AgencyCommManager::path(), "Plan", "Databases"}));

    if (!databases.isObject()) {
      return false;
    }

    // loop over all database names we got and create a local database
    // instance if not yet present:

    for (VPackObjectIterator::ObjectPair options : VPackObjectIterator(databases)) {
      if (!options.value.isObject()) {
        continue;
      }
      auto nameSlice = options.value.get("name");
      if (nameSlice.isNone()) {
        LOG_TOPIC(ERR, Logger::HEARTBEAT)
            << "Missing name in agency database plan";
        continue;
      }
      std::string const name = options.value.get("name").copyString();
      TRI_ASSERT(!name.empty());

      VPackSlice const idSlice = options.value.get("id");
      if (!idSlice.isString()) {
        LOG_TOPIC(ERR, Logger::HEARTBEAT) << "Missing id in agency database plan";
        TRI_ASSERT(false);
        continue;
      }
      TRI_voc_tick_t id = basics::StringUtils::uint64(idSlice.copyString());
      TRI_ASSERT(id != 0);
      if (id == 0) {
        LOG_TOPIC(ERR, Logger::HEARTBEAT)
        << "Failed to convert database id string to number";
        TRI_ASSERT(false);
        continue;
      }

      // known plan IDs
      ids.push_back(id);

      TRI_vocbase_t* vocbase = databaseFeature->useDatabase(name);
      if (vocbase == nullptr) {
        // database does not yet exist, create it now

        // create a local database object...
        int res = databaseFeature->createDatabase(id, name, vocbase);

        if (res != TRI_ERROR_NO_ERROR) {
          LOG_TOPIC(ERR, arangodb::Logger::HEARTBEAT)
              << "creating local database '" << name
              << "' failed: " << TRI_errno_string(res);
        } else {
          HasRunOnce.store(true, std::memory_order_release);
        }
      } else {
        if (vocbase->isSystem()) {
          // workaround: _system collection already exists now on every coordinator
          // setting HasRunOnce lets coordinator startup continue
          TRI_ASSERT(vocbase->id() == 1);
          HasRunOnce.store(true, std::memory_order_release);
        }
        vocbase->release();
      }
    }

    // get the list of databases that we know about locally
    std::vector<TRI_voc_tick_t> localIds = databaseFeature->getDatabaseIds(false);

    for (auto id : localIds) {
      auto r = std::find(ids.begin(), ids.end(), id);

      if (r == ids.end()) {
        // local database not found in the plan...
        databaseFeature->dropDatabase(id, false, true);
      }
    }

  } else {
    return false;
  }

  // invalidate our local cache
  ClusterInfo::instance()->flush();

  // turn on error logging now
  auto cc = ClusterComm::instance();
  if (cc != nullptr && cc->enableConnectionErrorLogging(true)) {
    LOG_TOPIC(DEBUG, Logger::HEARTBEAT)
        << "created coordinator databases for the first time";
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a plan version change, DBServer case
/// this is triggered if the heartbeat thread finds a new plan version number,
/// and every few heartbeats if the Current/Version has changed.
////////////////////////////////////////////////////////////////////////////////

void HeartbeatThread::syncDBServerStatusQuo(bool asyncPush) {

  MUTEX_LOCKER(mutexLocker, *_statusLock);
  bool shouldUpdate = false;

  if (_desiredVersions->plan > _currentVersions.plan) {
    LOG_TOPIC(DEBUG, Logger::HEARTBEAT)
      << "Plan version " << _currentVersions.plan
      << " is lower than desired version " << _desiredVersions->plan;
    shouldUpdate = true;
  }
  if (_desiredVersions->current > _currentVersions.current) {
    LOG_TOPIC(DEBUG, Logger::HEARTBEAT)
      << "Current version " << _currentVersions.current
      << " is lower than desired version " << _desiredVersions->current;
    shouldUpdate = true;
  }

  // 7.4 seconds is just less than half the 15 seconds agency uses to declare dead server,
  //  perform a safety execution of job in case other plan changes somehow incomplete or undetected
  double now = TRI_microtime();
  if (now > _lastSyncTime + 7.4 || asyncPush) {
    shouldUpdate = true;
  }

  if (!shouldUpdate) {
    return;
  }

  // First invalidate the caches in ClusterInfo:
  auto ci = ClusterInfo::instance();
  if (_desiredVersions->plan > ci->getPlanVersion()) {
    ci->invalidatePlan();
  }
  if (_desiredVersions->current > ci->getCurrentVersion()) {
    ci->invalidateCurrent();
  }

  // schedule a job for the change:
  uint64_t jobNr = ++_backgroundJobsPosted;
  LOG_TOPIC(DEBUG, Logger::HEARTBEAT) << "dispatching sync " << jobNr;

  _lastSyncTime = TRI_microtime();
  TRI_ASSERT(_maintenanceThread != nullptr);
  _maintenanceThread->notify();

}

////////////////////////////////////////////////////////////////////////////////
/// @brief sends the current server's state to the agency
////////////////////////////////////////////////////////////////////////////////

bool HeartbeatThread::sendServerState() {
  LOG_TOPIC(TRACE, Logger::HEARTBEAT) << "sending heartbeat to agency";

  const AgencyCommResult result = _agency.sendServerState(0.0);
  //      8.0 * static_cast<double>(_interval) / 1000.0 / 1000.0);

  if (result.successful()) {
    _numFails = 0;
    return true;
  }

  if (++_numFails % _maxFailsBeforeWarning == 0) {
    std::string const endpoints = AgencyCommManager::MANAGER->endpointsString();

    LOG_TOPIC(WARN, Logger::HEARTBEAT)
        << "heartbeat could not be sent to agency endpoints (" << endpoints
        << "): http code: " << result.httpCode() << ", body: " << result.body();
    _numFails = 0;
  }

  return false;
}

void HeartbeatThread::updateAgentPool(VPackSlice const& agentPool) {
  if (agentPool.isObject() && agentPool.get("pool").isObject() &&
      agentPool.hasKey("size") && agentPool.get("size").getUInt() > 0) {
    try {
      std::vector<std::string> values;
      for (auto pair : VPackObjectIterator(agentPool.get("pool"))) {
        values.emplace_back(pair.value.copyString());
      }
      AgencyCommManager::MANAGER->updateEndpoints(values);
    } catch(basics::Exception const& e) {
      LOG_TOPIC(WARN, Logger::HEARTBEAT) << "Error updating agency pool: " << e.message();
    } catch(std::exception const& e) {
      LOG_TOPIC(WARN, Logger::HEARTBEAT) << "Error updating agency pool: " << e.what();
    } catch(...) {}
  } else {
    LOG_TOPIC(ERR, Logger::AGENCYCOMM) << "Cannot find an agency persisted in RAFT 8|";
  }
}

  //////////////////////////////////////////////////////////////////////////////
  /// @brief record the death of a thread, adding std::chrono::system_clock::now().
  ///        This is a static function because HeartbeatThread might not have
  ///        started yet
  //////////////////////////////////////////////////////////////////////////////

void HeartbeatThread::recordThreadDeath(const std::string & threadName) {
  MUTEX_LOCKER(mutexLocker, deadThreadsMutex);

  deadThreads.insert(std::pair<std::chrono::system_clock::time_point, const std::string>
                     (std::chrono::system_clock::now(), threadName));
} // HeartbeatThread::recordThreadDeath

  //////////////////////////////////////////////////////////////////////////////
  /// @brief post list of deadThreads to current log.  Called regularly, but only
  ///        posts to log roughly every 60 minutes
  //////////////////////////////////////////////////////////////////////////////

void HeartbeatThread::logThreadDeaths(bool force) {

  bool doLogging(force);

  MUTEX_LOCKER(mutexLocker, deadThreadsMutex);

  if (std::chrono::hours(1) < (std::chrono::system_clock::now() - deadThreadsPosted)) {
    doLogging = true;
  } // if

  if (doLogging) {
    deadThreadsPosted = std::chrono::system_clock::now();

    LOG_TOPIC(DEBUG, Logger::HEARTBEAT) << "HeartbeatThread ok.";
    std::string buffer;
    buffer.reserve(40);

    for (auto const& it : deadThreads) {
      buffer = date::format("%FT%TZ", date::floor<std::chrono::milliseconds>(it.first));

      LOG_TOPIC(ERR, Logger::HEARTBEAT) << "Prior crash of thread " << it.second
                                        << " occurred at " << buffer;
    } // for
  } // if

} // HeartbeatThread::logThreadDeaths
