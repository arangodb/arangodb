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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "HeartbeatThread.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ConditionLocker.h"
#include "Basics/MutexLocker.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/tri-strings.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/DBServerAgencySync.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/RestHandlerFactory.h"
#include "Logger/Logger.h"
#include "RestServer/DatabaseFeature.h"
#include "Scheduler/JobGuard.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "V8/v8-globals.h"
#include "VocBase/AuthInfo.h"
#include "VocBase/vocbase.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/Recovery.h"

using namespace arangodb;
using namespace arangodb::application_features;

std::atomic<bool> HeartbeatThread::HasRunOnce(false);

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a heartbeat thread
////////////////////////////////////////////////////////////////////////////////

HeartbeatThread::HeartbeatThread(AgencyCallbackRegistry* agencyCallbackRegistry,
                                 uint64_t interval,
                                 uint64_t maxFailsBeforeWarning) 
    : Thread("Heartbeat"),
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
      _backgroundJobsLaunched(0),
      _backgroundJobScheduledOrRunning(false),
      _launchAnotherBackgroundJob(false),
      _lastSyncTime(0) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a heartbeat thread
////////////////////////////////////////////////////////////////////////////////

HeartbeatThread::~HeartbeatThread() { shutdown(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief running of heartbeat background jobs (in JavaScript), we run
/// these by instantiating an object in class HeartbeatBackgroundJob,
/// which is a std::function<void()> and holds a shared_ptr to the
/// HeartbeatThread singleton itself. This instance is then posted to
/// the io_service for execution in the thread pool. Should the heartbeat
/// thread itself terminate during shutdown, then the HeartbeatThread
/// singleton itself is still kept alive by the shared_ptr in the instance
/// of HeartbeatBackgroundJob. The operator() method simply calls the
/// runBackgroundJob() method of the heartbeat thread. Should this have
/// to schedule another background job, then it can simply create a new
/// HeartbeatBackgroundJob instance, again using shared_from_this() to
/// create a new shared_ptr keeping the HeartbeatThread object alive.
////////////////////////////////////////////////////////////////////////////////

class HeartbeatBackgroundJob {
  std::shared_ptr<HeartbeatThread> _heartbeatThread;
  double _startTime;
  std::string _schedulerInfo;
 public:
  explicit HeartbeatBackgroundJob(std::shared_ptr<HeartbeatThread> hbt,
                                  double startTime)
    : _heartbeatThread(hbt), _startTime(startTime),_schedulerInfo(SchedulerFeature::SCHEDULER->infoStatus()) {
  }

  void operator()() {
    // first tell the scheduler that this thread is working:
    JobGuard guard(SchedulerFeature::SCHEDULER);
    guard.work();

    double now = TRI_microtime();
    if (now > _startTime + 5.0) {
      LOG_TOPIC(ERR, Logger::HEARTBEAT) << "ALARM: Scheduling background job "
        "took " << now - _startTime
        << " seconds, scheduler info at schedule time: " << _schedulerInfo
        << ", scheduler info now: "
        << SchedulerFeature::SCHEDULER->infoStatus();
    }
    _heartbeatThread->runBackgroundJob();
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief method runBackgroundJob()
////////////////////////////////////////////////////////////////////////////////

void HeartbeatThread::runBackgroundJob() {
  uint64_t jobNr = ++_backgroundJobsLaunched;
  LOG_TOPIC(DEBUG, Logger::HEARTBEAT) << "sync callback started " << jobNr;
  {
    DBServerAgencySync job(this);
    job.work();
  }
  LOG_TOPIC(DEBUG, Logger::HEARTBEAT) << "sync callback ended " << jobNr;

  {
    MUTEX_LOCKER(mutexLocker, *_statusLock);
    TRI_ASSERT(_backgroundJobScheduledOrRunning);
    if (_launchAnotherBackgroundJob) {
      jobNr = ++_backgroundJobsPosted;
      LOG_TOPIC(DEBUG, Logger::HEARTBEAT) << "dispatching sync tail " << jobNr;
      _launchAnotherBackgroundJob = false;

      // the JobGuard is in the operator() of HeartbeatBackgroundJob
      SchedulerFeature::SCHEDULER->post(HeartbeatBackgroundJob(shared_from_this(), TRI_microtime()));
    } else {
      _backgroundJobScheduledOrRunning = false;
      _launchAnotherBackgroundJob = false;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief heartbeat main loop
/// the heartbeat thread constantly reports the current server status to the
/// agency. it does so by sending the current state string to the key
/// "Sync/ServerStates/" + my-id.
/// after transferring the current state to the agency, the heartbeat thread
/// will wait for changes on the "Sync/Commands/" + my-id key. If no changes
/// occur,
/// then the request it aborted and the heartbeat thread will go on with
/// reporting its state to the agency again. If it notices a change when
/// watching the command key, it will wake up and apply the change locally.
////////////////////////////////////////////////////////////////////////////////

void HeartbeatThread::run() {
  if (ServerState::instance()->isCoordinator()) {
    runCoordinator();
  } else {
    runDBServer();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief heartbeat main loop, dbserver version
////////////////////////////////////////////////////////////////////////////////

void HeartbeatThread::runDBServer() {
  LOG_TOPIC(TRACE, Logger::HEARTBEAT)
      << "starting heartbeat thread (DBServer version)";

  // mop: the heartbeat thread itself is now ready
  setReady();
  // mop: however we need to wait for the rest server here to come up
  // otherwise we would already create collections and the coordinator would
  // think
  // ohhh the dbserver is online...pump some documents into it
  // which fails when it is still in maintenance mode
  while (arangodb::rest::RestHandlerFactory::isMaintenance()) {
    usleep(100000);
  }

  // convert timeout to seconds
  double const interval = (double)_interval / 1000.0 / 1000.0;

  

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
          << "Desired Current Version is now " << _desiredVersions->plan;
        doSync = true;
      }
    }
    
    if (doSync) {
      syncDBServerStatusQuo();
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
      sleep(1);
    }
  }

  // we check Current/Version every few heartbeats:
  int const currentCountStart = 1;  // set to 1 by Max to speed up discovery
  int currentCount = currentCountStart;


  while (!isStopping()) {

    try {
      LOG_TOPIC(TRACE, Logger::HEARTBEAT) << "sending heartbeat to agency";

      double const start = TRI_microtime();
      // send our state to the agency.
      // we don't care if this fails
      sendState();

      if (isStopping()) {
        break;
      }

      if (--currentCount == 0) {
        currentCount = currentCountStart;

        // send an initial GET request to Sync/Commands/my-id
        LOG_TOPIC(TRACE, Logger::HEARTBEAT)
            << "Looking at Sync/Commands/" + _myId;

        AgencyReadTransaction trx(
          std::vector<std::string>({
              AgencyCommManager::path("Shutdown"),
                AgencyCommManager::path("Current/Version"),
                AgencyCommManager::path("Sync/Commands", _myId),
                "/.agency"}));
        
        AgencyCommResult result = _agency.sendTransactionWithFailover(trx, 1.0);
        if (!result.successful()) {
          LOG_TOPIC(WARN, Logger::HEARTBEAT)
              << "Heartbeat: Could not read from agency!";
        } else {

          VPackSlice agentPool =
            result.slice()[0].get(
              std::vector<std::string>({".agency","pool"}));

          if (agentPool.isObject() && agentPool.hasKey("size") &&
              agentPool.get("size").getUInt() > 1) {
            _agency.updateEndpoints(agentPool);
          } else {
            LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "Cannot find an agency persisted in RAFT 8|";
          }
          
          VPackSlice shutdownSlice =
              result.slice()[0].get(std::vector<std::string>(
                  {AgencyCommManager::path(), "Shutdown"}));

          if (shutdownSlice.isBool() && shutdownSlice.getBool()) {
            ApplicationServer::server->beginShutdown();
            break;
          }
          LOG_TOPIC(TRACE, Logger::HEARTBEAT)
              << "Looking at Sync/Commands/" + _myId;
          handleStateChange(result);

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

      double remain = interval - (TRI_microtime() - start);
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
          if (!wasNotified) {
            if (remain > 0.0) {
              locker.wait(static_cast<uint64_t>(remain * 1000000.0));
              wasNotified = _wasNotified;
            }
          }
          _wasNotified = false;
        }

        if (!wasNotified) {
          LOG_TOPIC(DEBUG, Logger::HEARTBEAT) << "Lock reached timeout";
          planAgencyCallback->refetchAndUpdate(true);
        } else {
          // mop: a plan change returned successfully...
          // recheck and redispatch in case our desired versions increased
          // in the meantime
          LOG_TOPIC(TRACE, Logger::HEARTBEAT) << "wasNotified==true";
          syncDBServerStatusQuo();
        }
        remain = interval - (TRI_microtime() - start);
      } while (remain > 0);
    } catch (std::exception const& e) {
      LOG_TOPIC(ERR, Logger::HEARTBEAT)
          << "Got an exception in DBServer heartbeat: " << e.what();
    } catch (...) {
      LOG_TOPIC(ERR, Logger::HEARTBEAT)
          << "Got an unknown exception in DBServer heartbeat";
    }
  }

  _agencyCallbackRegistry->unregisterCallback(planAgencyCallback);
  LOG_TOPIC(TRACE, Logger::HEARTBEAT)
      << "stopped heartbeat thread (DBServer version)";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief heartbeat main loop, coordinator version
////////////////////////////////////////////////////////////////////////////////

void HeartbeatThread::runCoordinator() {
  AuthenticationFeature* authentication =
      application_features::ApplicationServer::getFeature<
          AuthenticationFeature>("Authentication");
  TRI_ASSERT(authentication != nullptr);
  LOG_TOPIC(TRACE, Logger::HEARTBEAT)
      << "starting heartbeat thread (coordinator version)";

  uint64_t oldUserVersion = 0;

  // convert timeout to seconds
  double const interval = (double)_interval / 1000.0 / 1000.0;
  // invalidate coordinators every 2nd call
  bool invalidateCoordinators = true;

  // last value of plan which we have noticed:
  uint64_t lastPlanVersionNoticed = 0;
  // last value of current which we have noticed:
  uint64_t lastCurrentVersionNoticed = 0;

  setReady();

  while (!isStopping()) {
    try {
      LOG_TOPIC(TRACE, Logger::HEARTBEAT) << "sending heartbeat to agency";

      double const start = TRI_microtime();
      // send our state to the agency.
      // we don't care if this fails
      sendState();

      if (isStopping()) {
        break;
      }

      AgencyReadTransaction trx
        (std::vector<std::string>(
          {AgencyCommManager::path("Current/Version"),
           AgencyCommManager::path("Current/Foxxmaster"),
           AgencyCommManager::path("Current/FoxxmasterQueueupdate"),
           AgencyCommManager::path("Plan/Version"),
           AgencyCommManager::path("Shutdown"),
           AgencyCommManager::path("Sync/Commands", _myId),
           AgencyCommManager::path("Sync/UserVersion"),
           AgencyCommManager::path("Target/FailedServers"), "/.agency"}));
      AgencyCommResult result = _agency.sendTransactionWithFailover(trx, 1.0);

      if (!result.successful()) {
        LOG_TOPIC(WARN, Logger::HEARTBEAT)
            << "Heartbeat: Could not read from agency!";
      } else {

          VPackSlice agentPool =
            result.slice()[0].get(
              std::vector<std::string>({".agency","pool"}));
          if (agentPool.isObject() && agentPool.hasKey("size") &&
              agentPool.get("size").getUInt() > 1) {
            _agency.updateEndpoints(agentPool);
          } else {
            LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "Cannot find an agency persisted in RAFT 8|";
          }
        
        VPackSlice shutdownSlice = result.slice()[0].get(
            std::vector<std::string>({AgencyCommManager::path(), "Shutdown"}));

        if (shutdownSlice.isBool() && shutdownSlice.getBool()) {
          ApplicationServer::server->beginShutdown();
          break;
        }

        LOG_TOPIC(TRACE, Logger::HEARTBEAT)
            << "Looking at Sync/Commands/" + _myId;

        handleStateChange(result);

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

        if (foxxmasterSlice.isString()) {
          ServerState::instance()->setFoxxmaster(foxxmasterSlice.copyString());
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

          if (userVersion > 0 && userVersion != oldUserVersion) {
            oldUserVersion = userVersion;
            if (authentication->isActive()) {
              authentication->authInfo()->outdate();
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
          std::vector<std::string> failedServers = {};
          for (auto const& server : VPackObjectIterator(failedServersSlice)) {
            if (server.value.isArray() && server.value.length() == 0) {
              failedServers.push_back(server.key.copyString());
            }
          }
          // calling pregel code
          ClusterInfo::instance()->setFailedServers(failedServers);
          
          pregel::PregelFeature *prgl = pregel::PregelFeature::instance();
          if (prgl != nullptr && failedServers.size() > 0) {
            pregel::RecoveryManager* mngr = prgl->recoveryManager();
            if (mngr != nullptr) {
              try {
                mngr->updatedFailedServers();
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
      }

      // the foxx stuff needs an updated list of coordinators
      // and this is only updated when current version has changed
      if (invalidateCoordinators) {
        ClusterInfo::instance()->invalidateCurrentCoordinators();
      }
      invalidateCoordinators = !invalidateCoordinators;

      double remain = interval - (TRI_microtime() - start);

      // sleep for a while if appropriate, on some systems usleep does not
      // like arguments greater than 1000000
      while (remain > 0.0) {
        if (remain >= 0.5) {
          usleep(500000);
          remain -= 0.5;
        } else {
          usleep((TRI_usleep_t)(remain * 1000.0 * 1000.0));
          remain = 0.0;
        }
      }
    } catch (std::exception const& e) {
      LOG_TOPIC(ERR, Logger::HEARTBEAT)
          << "Got an exception in coordinator heartbeat: " << e.what();
    } catch (...) {
      LOG_TOPIC(ERR, Logger::HEARTBEAT)
          << "Got an unknown exception in coordinator heartbeat";
    }
  }

  LOG_TOPIC(TRACE, Logger::HEARTBEAT) << "stopped heartbeat thread";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes the heartbeat
////////////////////////////////////////////////////////////////////////////////

bool HeartbeatThread::init() {
  // send the server state a first time and use this as an indicator about
  // the agency's health
  if (!sendState()) {
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finished plan change
////////////////////////////////////////////////////////////////////////////////

void HeartbeatThread::dispatchedJobResult(DBServerAgencySyncResult result) {
  LOG_TOPIC(DEBUG, Logger::HEARTBEAT) << "Dispatched job returned!";
  bool doSleep = false;
  {
    MUTEX_LOCKER(mutexLocker, *_statusLock);
    if (result.success) {
      LOG_TOPIC(DEBUG, Logger::HEARTBEAT)
          << "Sync request successful. Now have Plan " << result.planVersion
          << ", Current " << result.currentVersion;
      _currentVersions = AgencyVersions(result);
    } else {
      LOG_TOPIC(DEBUG, Logger::HEARTBEAT) << "Sync request failed!";
      // mop: we will retry immediately so wait at least a LITTLE bit
      doSleep = true;
    }
  }
  if (doSleep) {
    // Sleep a little longer, since this might be due to some synchronisation
    // of shards going on in the background
    usleep(500000);
    usleep(500000);
  }
  CONDITION_LOCKER(guard, _condition);
  _wasNotified = true;
  _condition.signal();
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

    for (auto const& options : VPackObjectIterator(databases)) {
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
      TRI_voc_tick_t id = 0;

      if (options.value.hasKey("id")) {
        VPackSlice const v = options.value.get("id");
        if (v.isString()) {
          try {
            id = std::stoul(v.copyString());
          } catch (std::exception const& e) {
            LOG_TOPIC(ERR, Logger::HEARTBEAT)
                << "Failed to convert id string to number";
            LOG_TOPIC(ERR, Logger::HEARTBEAT) << e.what();
          }
        }
      }

      if (id > 0) {
        ids.push_back(id);
      }

      TRI_vocbase_t* vocbase = databaseFeature->useDatabaseCoordinator(name);

      if (vocbase == nullptr) {
        // database does not yet exist, create it now

        if (id == 0) {
          // verify that we have an id
          id = ClusterInfo::instance()->uniqid();
        }

        // create a local database object...
        int res = databaseFeature->createDatabaseCoordinator(id, name, vocbase);

        if (res != TRI_ERROR_NO_ERROR) {
          LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "creating local database '" << name
                   << "' failed: " << TRI_errno_string(res);
        } else {
          HasRunOnce = true;
        }
      } else {
        vocbase->release();
      }
    }

    // get the list of databases that we know about locally
    std::vector<TRI_voc_tick_t> localIds =
        databaseFeature->getDatabaseIdsCoordinator(false);

    for (auto id : localIds) {
      auto r = std::find(ids.begin(), ids.end(), id);

      if (r == ids.end()) {
        // local database not found in the plan...
        databaseFeature->dropDatabaseCoordinator(id, false);
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

void HeartbeatThread::syncDBServerStatusQuo() {
  bool shouldUpdate = false;
  bool becauseOfPlan = false;
  bool becauseOfCurrent = false;

  MUTEX_LOCKER(mutexLocker, *_statusLock);

  if (_desiredVersions->plan > _currentVersions.plan) {
    LOG_TOPIC(DEBUG, Logger::HEARTBEAT)
        << "Plan version " << _currentVersions.plan
        << " is lower than desired version " << _desiredVersions->plan;
    shouldUpdate = true;
    becauseOfPlan = true;
  }
  if (_desiredVersions->current > _currentVersions.current) {
    LOG_TOPIC(DEBUG, Logger::HEARTBEAT)
        << "Current version " << _currentVersions.current
        << " is lower than desired version " << _desiredVersions->current;
    shouldUpdate = true;
    becauseOfCurrent = true;
  }

  double now = TRI_microtime();
  if (now > _lastSyncTime + 7.4) {
    shouldUpdate = true;
  }

  if (!shouldUpdate) {
    return;
  }

  // First invalidate the caches in ClusterInfo:
  auto ci = ClusterInfo::instance();
  if (becauseOfPlan) {
    ci->invalidatePlan();
  }
  if (becauseOfCurrent) {
    ci->invalidateCurrent();
  }

  if (_backgroundJobScheduledOrRunning) {
    _launchAnotherBackgroundJob = true;
    return;
  }

  // schedule a job for the change:
  uint64_t jobNr = ++_backgroundJobsPosted;
  LOG_TOPIC(DEBUG, Logger::HEARTBEAT) << "dispatching sync " << jobNr;
  _backgroundJobScheduledOrRunning = true;

  // the JobGuard is in the operator() of HeartbeatBackgroundJob
  _lastSyncTime = TRI_microtime();
  SchedulerFeature::SCHEDULER->post(HeartbeatBackgroundJob(shared_from_this(), _lastSyncTime));

}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a state change
/// this is triggered if the watch command reports a change
/// when this is called, it will update the index value of the last command
/// (we'll pass the updated index value to the next watches so we don't get
/// notified about this particular change again).
////////////////////////////////////////////////////////////////////////////////

bool HeartbeatThread::handleStateChange(AgencyCommResult& result) {
  VPackSlice const slice = result.slice()[0].get(std::vector<std::string>(
      {AgencyCommManager::path(), "Sync", "Commands", _myId}));

  if (slice.isString()) {
    std::string command = slice.copyString();
    ServerState::StateEnum newState = ServerState::stringToState(command);

    if (newState != ServerState::STATE_UNDEFINED) {
      // state change.
      ServerState::instance()->setState(newState);
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sends the current server's state to the agency
////////////////////////////////////////////////////////////////////////////////

bool HeartbeatThread::sendState() {
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
