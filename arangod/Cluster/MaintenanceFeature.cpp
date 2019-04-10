////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

#include "MaintenanceFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ConditionLocker.h"
#include "Basics/MutexLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Cluster/Action.h"
#include "Cluster/ActionDescription.h"
#include "Cluster/CreateDatabase.h"
#include "Cluster/MaintenanceWorker.h"
#include "Cluster/ServerState.h"
#include "Random/RandomGenerator.h"
#include "Agency/AgencyComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterFeature.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::options;
using namespace arangodb::maintenance;

const uint32_t MaintenanceFeature::minThreadLimit = 2;
const uint32_t MaintenanceFeature::maxThreadLimit = 64;

MaintenanceFeature::MaintenanceFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "Maintenance"),
      _forceActivation(false),
      _maintenanceThreadsMax(2) {
  // the number of threads will be adjusted later. it's just that we want to initialize all members properly

  // this feature has to know the role of this server in its `start`. The role
  // is determined by `ClusterFeature::validateOptions`, hence the following
  // line of code is not required. For philosophical reasons we added it to the
  // ClusterPhase and let it start after `Cluster`.
  startsAfter("Cluster");

  init();
}  // MaintenanceFeature::MaintenanceFeature

void MaintenanceFeature::init() {
  _isShuttingDown = false;
  _nextActionId = 1;

  setOptional(true);
  requiresElevatedPrivileges(false);  // ??? this mean admin priv?

  // these parameters might be updated by config and/or command line options

  _maintenanceThreadsMax =
      (std::max)(static_cast<uint32_t>(minThreadLimit),
                 static_cast<uint32_t>(TRI_numberProcessors() / 4 + 1));
  _secondsActionsBlock = 2;
  _secondsActionsLinger = 3600;
}  // MaintenanceFeature::init

void MaintenanceFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("server", "Server features");

  options->addOption(
      "--server.maintenance-threads",
      "maximum number of threads available for maintenance actions",
      new UInt32Parameter(&_maintenanceThreadsMax),
      arangodb::options::makeFlags(arangodb::options::Flags::Hidden,
                                   arangodb::options::Flags::Dynamic));

  options->addOption(
      "--server.maintenance-actions-block",
      "minimum number of seconds finished Actions block duplicates",
      new Int32Parameter(&_secondsActionsBlock),
      arangodb::options::makeFlags(arangodb::options::Flags::Hidden));

  options->addOption(
      "--server.maintenance-actions-linger",
      "minimum number of seconds finished Actions remain in deque",
      new Int32Parameter(&_secondsActionsLinger),
      arangodb::options::makeFlags(arangodb::options::Flags::Hidden));

  options->addOption("--cluster.resign-leadership-on-shutdown",
                    "create resign leader ship job for this dbsever on shutdown",
                    new BooleanParameter(&_resignLeadershipOnShutdown),
                    arangodb::options::makeFlags(arangodb::options::Flags::Hidden));

}  // MaintenanceFeature::collectOptions

void MaintenanceFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  if (_maintenanceThreadsMax < minThreadLimit) {
    LOG_TOPIC(WARN, Logger::MAINTENANCE)
        << "Need at least" << minThreadLimit << "maintenance-threads";
    _maintenanceThreadsMax = minThreadLimit;
  } else if (_maintenanceThreadsMax >= maxThreadLimit) {
    LOG_TOPIC(WARN, Logger::MAINTENANCE) << "maintenance-threads limited to " << maxThreadLimit;
    _maintenanceThreadsMax = maxThreadLimit;
  }
}

/// do not start threads in prepare
void MaintenanceFeature::prepare() {}  // MaintenanceFeature::prepare

void MaintenanceFeature::start() {
  auto serverState = ServerState::instance();

  // _forceActivation is set by the catch tests
  if (!_forceActivation && (serverState->isAgent() || serverState->isSingleServer())) {
    LOG_TOPIC(TRACE, Logger::MAINTENANCE) << "Disable maintenance-threads"
                                          << " for single-server or agents.";
    return;
  }

  // start threads
  for (uint32_t loop = 0; loop < _maintenanceThreadsMax; ++loop) {
    // First worker will be available only to fast track
    std::unordered_set<std::string> labels{};
    if (loop == 0) {
      labels.emplace(ActionBase::FAST_TRACK);
    }

    auto newWorker = std::make_unique<maintenance::MaintenanceWorker>(*this, labels);

    if (!newWorker->start(&_workerCompletion)) {
      LOG_TOPIC(ERR, Logger::MAINTENANCE)
          << "MaintenanceFeature::start:  newWorker start failed";
    } else {
      _activeWorkers.push_back(std::move(newWorker));
    }
  }  // for
}  // MaintenanceFeature::start

void MaintenanceFeature::beginShutdown() {

  if (_resignLeadershipOnShutdown) {

    struct callback_data {
      uint64_t _jobId;              // initialised before callback
      bool _completed;              // populated by the callback
      std::mutex _mutex;            // mutex used by callback and loop to sync access to callback_data
      std::condition_variable _cv;  // signaled if callback has found something

      callback_data(uint64_t jobId) : _jobId(jobId), _completed(false) {}
    };

    // create common shared memory with jobid
    auto shared = std::make_shared<callback_data>(ClusterInfo::instance()->uniqid());

    AgencyComm am;

    // copy jobId and serverId by value
    /*auto jobCallback = [shared](VPackSlice const& result) -> bool {
      std::unique_lock<std::mutex> lock(shared->_mutex);
      shared->_completed = true;
      shared->_cv.notify_one();
      return false;
    };



    auto acr = ClusterFeature::instance()->agencyCallbackRegistry();

    // register agency callback
    //  false, false -> we don't care about the value
    auto callbackOnFailed = std::make_shared<AgencyCallback>(am, "Target/Failed/" + std::to_string(shared->_jobId), jobCallback, false, false);
    auto callbackOnFinished = std::make_shared<AgencyCallback>(am, "Target/Finished/" + std::to_string(shared->_jobId), jobCallback, false, false);

    acr->registerCallback(callbackOnFailed);
    acr->registerCallback(callbackOnFinished);
    auto cbGuard = scopeGuard([&] {
      acr->unregisterCallback(callbackOnFailed);
      acr->unregisterCallback(callbackOnFinished);
    });*/

    std::string serverId = ServerState::instance()->getId();
    VPackBuilder jobDesc;
    {
      VPackObjectBuilder jobObj(&jobDesc);
      jobDesc.add("type", VPackValue("resignLeadership"));
      jobDesc.add("server", VPackValue(serverId));
      jobDesc.add("jobId", VPackValue(std::to_string(shared->_jobId)));
      jobDesc.add("timeCreated", VPackValue("2019-04-10T11:34:19Z"));
      jobDesc.add("creator", VPackValue(serverId));
    }

    LOG_TOPIC(INFO, arangodb::Logger::CLUSTER) <<
      "Starting resigning leadership of shards";
    am.setValue("Target/ToDo/" + std::to_string(shared->_jobId), jobDesc.slice(), 0.0);

    using clock = std::chrono::steady_clock;

    auto startTime = clock::now();
    auto timeout = std::chrono::seconds(120);

    auto endtime = startTime + timeout;

    auto checkAgencyPathExists = [&am](std::string const& path, uint64_t jobId) -> bool {
      try {
        AgencyCommResult result = am.getValues("Target/" + path + "/" + std::to_string(jobId));
        if (result.successful()) {
          LOG_DEVEL << result.slice().toJson();
          VPackSlice value = result.slice()[0].get(std::vector<std::string>{AgencyCommManager::path(), "Target", path, std::to_string(jobId)});
          if (value.isObject() && value.hasKey("jobId") && value.get("jobId").isEqualString(std::to_string(jobId))) {
            return true;
          }
        } else {
          LOG_DEVEL << "Failed to read agency";
        }
      } catch(...) {
        LOG_TOPIC(ERR, arangodb::Logger::CLUSTER) <<
          "Exception when checking for job completion";
      }

      return false;
    };

    // we can not test for application_features::ApplicationServer::isRetryOK() because it is never okay in shutdown
    while (clock::now() < endtime) {

      bool completed = checkAgencyPathExists ("Failed", shared->_jobId)
        || checkAgencyPathExists ("Finished", shared->_jobId);

      if (completed) {
        break;
      }

      std::unique_lock<std::mutex> lock(shared->_mutex);
      shared->_cv.wait_for(lock, std::chrono::seconds(1));

      if (shared->_completed) {
        break ;
      }
    }

    LOG_TOPIC(INFO, arangodb::Logger::CLUSTER) <<
      "Resigning leadership completed (finished, failed or timed out)";
  }

  _isShuttingDown = true;
  CONDITION_LOCKER(cLock, _actionRegistryCond);
  _actionRegistryCond.broadcast();
}  // MaintenanceFeature

void MaintenanceFeature::stop() {
  for (auto const& itWorker : _activeWorkers) {
    CONDITION_LOCKER(cLock, _workerCompletion);

    // loop on each worker, retesting at 10ms just in case
    while (itWorker->isRunning()) {
      _workerCompletion.wait(std::chrono::microseconds(10000));
    }  // if
  }    // for

}  // MaintenanceFeature::stop

/// @brief Move an incomplete action to failed state
Result MaintenanceFeature::deleteAction(uint64_t action_id) {
  Result result;

  // pointer to action, or nullptr
  auto action = findActionId(action_id);

  if (action) {
    if (maintenance::COMPLETE != action->getState()) {
      action->setState(maintenance::FAILED);
    } else {
      result.reset(TRI_ERROR_BAD_PARAMETER,
                   "deleteAction called after action complete.");
    }  // else
  } else {
    result.reset(TRI_ERROR_BAD_PARAMETER,
                 "deleteAction could not find action to delete.");
  }  // else

  return result;

}  // MaintenanceFeature::deleteAction

/// @brief This is the  API for creating an Action and executing it.
///  Execution can be immediate by calling thread, or asynchronous via thread
///  pool. not yet:  ActionDescription parameter will be MOVED to new object.
Result MaintenanceFeature::addAction(std::shared_ptr<maintenance::Action> newAction,
                                     bool executeNow) {
  Result result;

  // the underlying routines are believed to be safe and throw free,
  //  but just in case
  try {
    size_t action_hash = newAction->hash();
    WRITE_LOCKER(wLock, _actionRegistryLock);

    std::shared_ptr<Action> curAction = findActionHashNoLock(action_hash);

    // similar action not in the queue (or at least no longer viable)
    if (curAction == nullptr || curAction->done()) {
      if (newAction && newAction->ok()) {
        // Register action only if construction was ok
        registerAction(newAction, executeNow);
      } else {
        /// something failed in action creation ... go check logs
        result.reset(TRI_ERROR_BAD_PARAMETER,
                     "createAction rejected parameters.");
      }  // if
    } else {
      // action already exist, need write lock to prevent race
      result.reset(TRI_ERROR_BAD_PARAMETER,
                   "addAction called while similar action already processing.");
    }  // else

    // executeNow process on this thread, right now!
    if (result.ok() && executeNow) {
      maintenance::MaintenanceWorker worker(*this, newAction);
      worker.run();
      result = worker.result();
    }  // if
  } catch (...) {
    result.reset(TRI_ERROR_INTERNAL,
                 "addAction experienced an unexpected throw.");
  }  // catch

  return result;

}  // MaintenanceFeature::addAction

/// @brief This is the  API for creating an Action and executing it.
///  Execution can be immediate by calling thread, or asynchronous via thread
///  pool. not yet:  ActionDescription parameter will be MOVED to new object.
Result MaintenanceFeature::addAction(std::shared_ptr<maintenance::ActionDescription> const& description,
                                     bool executeNow) {
  Result result;

  // the underlying routines are believed to be safe and throw free,
  //  but just in case
  try {
    std::shared_ptr<Action> newAction;

    // is there a known name field
    auto find_it = description->get("name");

    size_t action_hash = description->hash();
    WRITE_LOCKER(wLock, _actionRegistryLock);

    std::shared_ptr<Action> curAction = findActionHashNoLock(action_hash);

    // similar action not in the queue (or at least no longer viable)
    if (!curAction || curAction->done()) {
      newAction = createAndRegisterAction(description, executeNow);

      if (!newAction || !newAction->ok()) {
        /// something failed in action creation ... go check logs
        result.reset(TRI_ERROR_BAD_PARAMETER,
                     "createAction rejected parameters.");
      }  // if
    } else {
      // action already exist, need write lock to prevent race
      result.reset(TRI_ERROR_BAD_PARAMETER,
                   "addAction called while similar action already processing.");
    }  // else

    // executeNow process on this thread, right now!
    if (result.ok() && executeNow) {
      maintenance::MaintenanceWorker worker(*this, newAction);
      worker.run();
      result = worker.result();
    }  // if
  } catch (...) {
    result.reset(TRI_ERROR_INTERNAL,
                 "addAction experienced an unexpected throw.");
  }  // catch

  return result;

}  // MaintenanceFeature::addAction

std::shared_ptr<Action> MaintenanceFeature::preAction(std::shared_ptr<ActionDescription> const& description) {
  return createAndRegisterAction(description, true);

}  // MaintenanceFeature::preAction

std::shared_ptr<Action> MaintenanceFeature::postAction(std::shared_ptr<ActionDescription> const& description) {
  auto action = createAction(description);

  if (action->ok()) {
    action->setState(WAITINGPOST);
    registerAction(action, false);
  }

  return action;
}  // MaintenanceFeature::postAction

void MaintenanceFeature::registerAction(std::shared_ptr<Action> action, bool executeNow) {
  // Assumes write lock on _actionRegistryLock

  // mark as executing so no other workers accidentally grab it
  if (executeNow) {
    action->setState(maintenance::EXECUTING);
  } else if (action->getState() == maintenance::READY) {
    _prioQueue.push(action);
  }

  // WARNING: holding write lock to _actionRegistry and about to
  //   lock condition variable
  {
    _actionRegistry.push_back(action);

    if (!executeNow) {
      CONDITION_LOCKER(cLock, _actionRegistryCond);
      _actionRegistryCond.broadcast();
      // Note that we do a broadcast here for the following reason: if we did
      // signal here, we cannot control which of the sleepers is woken up.
      // If the new action is not fast track, then we could wake up the
      // fast track worker, which would leave the action as it is. This would
      // cause a delay of up to 0.1 seconds. With the broadcast, the worst
      // case is that we wake up sleeping workers unnecessarily.
    }  // if
  }    // lock
}

std::shared_ptr<Action> MaintenanceFeature::createAction(std::shared_ptr<ActionDescription> const& description) {
  // write lock via _actionRegistryLock is assumed held
  std::shared_ptr<Action> newAction;

  // name should already be verified as existing ... but trust no one
  std::string name = description->get(NAME);

  // call factory
  newAction = std::make_shared<Action>(*this, *description);

  // if a new action constructed successfully
  if (!newAction->ok()) {
    LOG_TOPIC(ERR, Logger::MAINTENANCE)
        << "createAction:  unknown action name given, \"" << name.c_str()
        << "\", or other construction failure.";
  }

  return newAction;

}  // if

std::shared_ptr<Action> MaintenanceFeature::createAndRegisterAction(
    std::shared_ptr<ActionDescription> const& description, bool executeNow) {
  std::shared_ptr<Action> newAction = createAction(description);

  if (newAction->ok()) {
    registerAction(newAction, executeNow);
  }

  return newAction;
}

std::shared_ptr<Action> MaintenanceFeature::findAction(std::shared_ptr<ActionDescription> const description) {
  return findActionHash(description->hash());
}

std::shared_ptr<Action> MaintenanceFeature::findActionHash(size_t hash) {
  READ_LOCKER(rLock, _actionRegistryLock);

  return findActionHashNoLock(hash);
}  // MaintenanceFeature::findActionHash

std::shared_ptr<Action> MaintenanceFeature::findActionHashNoLock(size_t hash) {
  // assert to test lock held?

  std::shared_ptr<Action> ret_ptr;

  for (auto action_it = _actionRegistry.begin();
       _actionRegistry.end() != action_it && !ret_ptr; ++action_it) {
    if ((*action_it)->hash() == hash) {
      ret_ptr = *action_it;
    }  // if
  }    // for

  return ret_ptr;

}  // MaintenanceFeature::findActionHashNoLock

std::shared_ptr<Action> MaintenanceFeature::findActionId(uint64_t id) {
  READ_LOCKER(rLock, _actionRegistryLock);

  return findActionIdNoLock(id);
}  // MaintenanceFeature::findActionId

std::shared_ptr<Action> MaintenanceFeature::findActionIdNoLock(uint64_t id) {
  // assert to test lock held?

  std::shared_ptr<Action> ret_ptr;

  for (auto action_it = _actionRegistry.begin();
       _actionRegistry.end() != action_it && !ret_ptr; ++action_it) {
    if ((*action_it)->id() == id) {
      // should we return the first match here or the last match?
      // if first, we could simply add a break
      ret_ptr = *action_it;
    }  // if
  }    // for

  return ret_ptr;

}  // MaintenanceFeature::findActionIdNoLock

std::shared_ptr<Action> MaintenanceFeature::findReadyAction(std::unordered_set<std::string> const& labels) {
  std::shared_ptr<Action> ret_ptr;

  while (!_isShuttingDown) {
    // use priority queue for ready action (and purge any that are done waiting)
    {
      WRITE_LOCKER(wLock, _actionRegistryLock);

      while (!_prioQueue.empty()) {
        // If _prioQueue is empty, we have no ready job and simply loop in the
        // outer loop.
        auto const& top = _prioQueue.top();
        if (top->getState() != maintenance::READY) {  // in case it is deleted
          _prioQueue.pop();
          continue;
        }
        if (top->matches(labels)) {
          ret_ptr = top;
          _prioQueue.pop();
          return ret_ptr;
        }
        // We are not interested, this can only mean that we are fast track
        // and the top action is not. Therefore, the whole queue does not
        // contain any fast track, so we can idle.
        break;
      }

      // When we get here, there is currently nothing to do, so we might
      // as well clean up those jobs in the _actionRegistry, which are
      // in state DONE:
      if (RandomGenerator::interval(uint32_t(10)) == 0) {
        for (auto loop = _actionRegistry.begin(); _actionRegistry.end() != loop;) {
          if ((*loop)->done()) {
            loop = _actionRegistry.erase(loop);
          } else {
            ++loop;
          }  // else
        }    // for
      }
    }      // WRITE

    // no pointer ... wait 0.1 seconds unless woken up
    if (!_isShuttingDown) {
      CONDITION_LOCKER(cLock, _actionRegistryCond);
      _actionRegistryCond.wait(std::chrono::milliseconds(100));
    }  // if

  }  // while

  return ret_ptr;

}  // MaintenanceFeature::findReadyAction

VPackBuilder MaintenanceFeature::toVelocyPack() const {
  VPackBuilder vb;
  toVelocyPack(vb);
  return vb;
}

void MaintenanceFeature::toVelocyPack(VPackBuilder& vb) const {
  READ_LOCKER(rLock, _actionRegistryLock);

  {
    VPackArrayBuilder ab(&vb);
    for (auto const& action : _actionRegistry) {
      action->toVelocyPack(vb);
    }  // for
  }

}  // MaintenanceFeature::toVelocyPack
#if 0
std::string MaintenanceFeature::toJson(VPackBuilder & builder) {
} // MaintenanceFeature::toJson
#endif

std::string const SLASH("/");

arangodb::Result MaintenanceFeature::storeDBError(std::string const& database,
                                                  Result const& failure) {
  VPackBuilder eb;
  {
    VPackObjectBuilder b(&eb);
    eb.add(NAME, VPackValue(database));
    eb.add("error", VPackValue(true));
    eb.add("errorNum", VPackValue(failure.errorNumber()));
    eb.add("errorMessage", VPackValue(failure.errorMessage()));
  }

  return storeDBError(database, eb.steal());
}

arangodb::Result MaintenanceFeature::storeDBError(std::string const& database,
                                                  std::shared_ptr<VPackBuffer<uint8_t>> error) {
  MUTEX_LOCKER(guard, _dbeLock);
  auto const it = _dbErrors.find(database);
  if (it != _dbErrors.end()) {
    std::stringstream error;
    error << "database " << database << " already has pending error";
    LOG_TOPIC(DEBUG, Logger::MAINTENANCE) << error.str();
    return Result(TRI_ERROR_FAILED, error.str());
  }

  try {
    _dbErrors.emplace(database, error);
  } catch (std::exception const& e) {
    return Result(TRI_ERROR_FAILED, e.what());
  }

  return Result();
}

arangodb::Result MaintenanceFeature::dbError(std::string const& database,
                                             std::shared_ptr<VPackBuffer<uint8_t>>& error) const {
  MUTEX_LOCKER(guard, _dbeLock);
  auto const it = _dbErrors.find(database);
  error = (it != _dbErrors.end()) ? it->second : nullptr;
  return Result();
}

arangodb::Result MaintenanceFeature::removeDBError(std::string const& database) {
  try {
    MUTEX_LOCKER(guard, _seLock);
    _shardErrors.erase(database);
  } catch (std::exception const&) {
    std::stringstream error;
    error << "erasing database error for " << database << " failed";
    LOG_TOPIC(DEBUG, Logger::MAINTENANCE) << error.str();
    return Result(TRI_ERROR_FAILED, error.str());
  }

  return Result();
}

arangodb::Result MaintenanceFeature::storeShardError(std::string const& database,
                                                     std::string const& collection,
                                                     std::string const& shard,
                                                     std::string const& serverId,
                                                     arangodb::Result const& failure) {
  VPackBuilder eb;
  {
    VPackObjectBuilder o(&eb);
    eb.add("error", VPackValue(true));
    eb.add("errorMessage", VPackValue(failure.errorMessage()));
    eb.add("errorNum", VPackValue(failure.errorNumber()));
    eb.add(VPackValue("indexes"));
    { VPackArrayBuilder a(&eb); }  // []
    eb.add(VPackValue("servers"));
    {
      VPackArrayBuilder a(&eb);  // [serverId]
      eb.add(VPackValue(serverId));
    }
  }

  return storeShardError(database, collection, shard, eb.steal());
}

arangodb::Result MaintenanceFeature::storeShardError(
    std::string const& database, std::string const& collection,
    std::string const& shard, std::shared_ptr<VPackBuffer<uint8_t>> error) {
  std::string key = database + SLASH + collection + SLASH + shard;

  MUTEX_LOCKER(guard, _seLock);
  auto const it = _shardErrors.find(key);
  if (it != _shardErrors.end()) {
    std::stringstream error;
    error << "shard " << key << " already has pending error";
    LOG_TOPIC(DEBUG, Logger::MAINTENANCE) << error.str();
    return Result(TRI_ERROR_FAILED, error.str());
  }

  try {
    _shardErrors.emplace(key, error);
  } catch (std::exception const& e) {
    return Result(TRI_ERROR_FAILED, e.what());
  }

  return Result();
}

arangodb::Result MaintenanceFeature::shardError(
    std::string const& database, std::string const& collection,
    std::string const& shard, std::shared_ptr<VPackBuffer<uint8_t>>& error) const {
  std::string key = database + SLASH + collection + SLASH + shard;

  MUTEX_LOCKER(guard, _seLock);
  auto const it = _shardErrors.find(key);
  error = (it != _shardErrors.end()) ? it->second : nullptr;
  return Result();
}

arangodb::Result MaintenanceFeature::removeShardError(std::string const& key) {
  try {
    MUTEX_LOCKER(guard, _seLock);
    _shardErrors.erase(key);
  } catch (std::exception const&) {
    std::stringstream error;
    error << "erasing shard error for " << key << " failed";
    LOG_TOPIC(DEBUG, Logger::MAINTENANCE) << error.str();
    return Result(TRI_ERROR_FAILED, error.str());
  }

  return Result();
}

arangodb::Result MaintenanceFeature::removeShardError(std::string const& database,
                                                      std::string const& collection,
                                                      std::string const& shard) {
  return removeShardError(database + SLASH + collection + SLASH + shard);
}

arangodb::Result MaintenanceFeature::storeIndexError(
    std::string const& database, std::string const& collection, std::string const& shard,
    std::string const& indexId, std::shared_ptr<VPackBuffer<uint8_t>> error) {
  using buffer_t = std::shared_ptr<VPackBuffer<uint8_t>>;
  std::string key = database + SLASH + collection + SLASH + shard;

  MUTEX_LOCKER(guard, _ieLock);

  auto errorsIt = _indexErrors.find(key);
  if (errorsIt == _indexErrors.end()) {
    try {
      _indexErrors.emplace(key, std::map<std::string, buffer_t>());
    } catch (std::exception const& e) {
      return Result(TRI_ERROR_FAILED, e.what());
    }
  }
  auto& errors = _indexErrors.find(key)->second;
  auto const it = errors.find(indexId);

  if (it != errors.end()) {
    std::stringstream error;
    error << "index " << indexId << " for shard " << key << " already has pending error";
    LOG_TOPIC(DEBUG, Logger::MAINTENANCE) << error.str();
    return Result(TRI_ERROR_FAILED, error.str());
  }

  try {
    errors.emplace(indexId, error);
  } catch (std::exception const& e) {
    return Result(TRI_ERROR_FAILED, e.what());
  }

  return Result();
}

arangodb::Result MaintenanceFeature::indexErrors(
    std::string const& database, std::string const& collection, std::string const& shard,
    std::map<std::string, std::shared_ptr<VPackBuffer<uint8_t>>>& error) const {
  std::string key = database + SLASH + collection + SLASH + shard;

  MUTEX_LOCKER(guard, _ieLock);
  auto const& it = _indexErrors.find(key);
  if (it != _indexErrors.end()) {
    error = it->second;
  }

  return Result();
}

template <typename T>
std::ostream& operator<<(std::ostream& os, std::set<T> const& st) {
  size_t j = 0;
  os << "[";
  for (auto const& i : st) {
    os << i;
    if (++j < st.size()) {
      os << ", ";
    }
  }
  os << "]";
  return os;
}

arangodb::Result MaintenanceFeature::removeIndexErrors(
    std::string const& key, std::unordered_set<std::string> const& indexIds) {
  MUTEX_LOCKER(guard, _ieLock);

  // If no entry for this shard exists bail out
  auto kit = _indexErrors.find(key);
  if (kit == _indexErrors.end()) {
    std::stringstream error;
    error << "erasing index " << indexIds << " error for shard " << key
          << " failed as no such key is found in index error bucket";
    LOG_TOPIC(DEBUG, Logger::MAINTENANCE) << error.str();
    return Result(TRI_ERROR_FAILED, error.str());
  }

  auto& errors = kit->second;

  try {
    for (auto const& indexId : indexIds) {
      errors.erase(indexId);
    }
  } catch (std::exception const&) {
    std::stringstream error;
    error << "erasing index errors " << indexIds << " for " << key << " failed";
    LOG_TOPIC(DEBUG, Logger::MAINTENANCE) << error.str();
    return Result(TRI_ERROR_FAILED, error.str());
  }

  return Result();
}

arangodb::Result MaintenanceFeature::removeIndexErrors(
    std::string const& database, std::string const& collection,
    std::string const& shard, std::unordered_set<std::string> const& indexIds) {
  return removeIndexErrors(database + SLASH + collection + SLASH + shard, indexIds);
}

arangodb::Result MaintenanceFeature::copyAllErrors(errors_t& errors) const {
  {
    MUTEX_LOCKER(guard, _seLock);
    errors.shards = _shardErrors;
  }
  {
    MUTEX_LOCKER(guard, _ieLock);
    errors.indexes = _indexErrors;
  }
  {
    MUTEX_LOCKER(guard, _dbeLock);
    errors.databases = _dbErrors;
  }
  return Result();
}

uint64_t MaintenanceFeature::shardVersion(std::string const& shname) const {
  MUTEX_LOCKER(guard, _versionLock);
  auto const it = _shardVersion.find(shname);
  LOG_TOPIC(TRACE, Logger::MAINTENANCE)
      << "getting shard version for '" << shname << "' from " << _shardVersion;
  return (it != _shardVersion.end()) ? it->second : 0;
}

uint64_t MaintenanceFeature::incShardVersion(std::string const& shname) {
  MUTEX_LOCKER(guard, _versionLock);
  auto ret = ++_shardVersion[shname];
  LOG_TOPIC(TRACE, Logger::MAINTENANCE)
      << "incremented shard version for " << shname << " to " << ret;
  return ret;
}

void MaintenanceFeature::delShardVersion(std::string const& shname) {
  MUTEX_LOCKER(guard, _versionLock);
  auto const it = _shardVersion.find(shname);
  if (it != _shardVersion.end()) {
    _shardVersion.erase(it);
  }
}
