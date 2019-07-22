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

#include <set>

#include "MaintenanceFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ConditionLocker.h"
#include "Basics/MutexLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Basics/system-functions.h"
#include "Cluster/Action.h"
#include "Cluster/ActionDescription.h"
#include "Cluster/CreateDatabase.h"
#include "Cluster/MaintenanceWorker.h"
#include "Cluster/ServerState.h"
#include "Random/RandomGenerator.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::options;
using namespace arangodb::maintenance;

const uint32_t MaintenanceFeature::minThreadLimit = 2;
const uint32_t MaintenanceFeature::maxThreadLimit = 64;


namespace {

bool findNotDoneActions(std::shared_ptr<maintenance::Action> const& action) {
  return !action->done();
}

} // namespace

MaintenanceFeature::MaintenanceFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "Maintenance"),
      _forceActivation(false),
      _maintenanceThreadsMax(2),
      _currentCounter(0) {
  // the number of threads will be adjusted later. it's just that we want to
  // initialize all members properly

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

}  // MaintenanceFeature::collectOptions

void MaintenanceFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  if (_maintenanceThreadsMax < minThreadLimit) {
    LOG_TOPIC("37726", WARN, Logger::MAINTENANCE)
        << "Need at least" << minThreadLimit << "maintenance-threads";
    _maintenanceThreadsMax = minThreadLimit;
  } else if (_maintenanceThreadsMax >= maxThreadLimit) {
    LOG_TOPIC("8fb0e", WARN, Logger::MAINTENANCE)
        << "maintenance-threads limited to " << maxThreadLimit;
    _maintenanceThreadsMax = maxThreadLimit;
  }
}

/// do not start threads in prepare
void MaintenanceFeature::prepare() {}  // MaintenanceFeature::prepare

void MaintenanceFeature::start() {
  auto serverState = ServerState::instance();

  // _forceActivation is set by the catch tests
  if (!_forceActivation && (serverState->isAgent() || serverState->isSingleServer())) {
    LOG_TOPIC("deb1a", TRACE, Logger::MAINTENANCE)
        << "Disable maintenance-threads"
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
      LOG_TOPIC("4d8b8", ERR, Logger::MAINTENANCE)
          << "MaintenanceFeature::start:  newWorker start failed";
    } else {
      _activeWorkers.push_back(std::move(newWorker));
    }
  }  // for
}  // MaintenanceFeature::start

void MaintenanceFeature::beginShutdown() {
  _isShuttingDown = true;
  CONDITION_LOCKER(cLock, _actionRegistryCond);
  _actionRegistryCond.broadcast();
}  // MaintenanceFeature

void MaintenanceFeature::stop() {
  for (auto const& itWorker : _activeWorkers) {
    CONDITION_LOCKER(cLock, _workerCompletion);

    // loop on each worker, retesting at 10ms just in case
    while (itWorker->isRunning()) {
      _workerCompletion.wait(std::chrono::milliseconds(10));
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

    std::shared_ptr<Action> curAction = findFirstActionHashNoLock(action_hash, ::findNotDoneActions);

    // similar action not in the queue (or at least no longer viable)
    if (!curAction) {
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

    size_t action_hash = description->hash();
    WRITE_LOCKER(wLock, _actionRegistryLock);

    std::shared_ptr<Action> curAction = findFirstActionHashNoLock(action_hash, ::findNotDoneActions);

    // similar action not in the queue (or at least no longer viable)
    if (!curAction) {
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
    LOG_TOPIC("ef5cb", ERR, Logger::MAINTENANCE)
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

std::shared_ptr<Action> MaintenanceFeature::findFirstNotDoneAction(std::shared_ptr<ActionDescription> const& description) {
  return findFirstActionHash(description->hash(), ::findNotDoneActions);
}

std::shared_ptr<Action> MaintenanceFeature::findFirstActionHash(size_t hash,
    std::function<bool(std::shared_ptr<maintenance::Action> const&)> const& predicate) {
  READ_LOCKER(rLock, _actionRegistryLock);

  return findFirstActionHashNoLock(hash, predicate);
}  

std::shared_ptr<Action> MaintenanceFeature::findFirstActionHashNoLock(size_t hash,
    std::function<bool(std::shared_ptr<maintenance::Action> const&)> const& predicate) {
  // assert to test lock held?

  for (auto const& action : _actionRegistry) {
    if (action->hash() == hash && predicate(action)) {
      return action;
    }
  }
  return std::shared_ptr<Action>();
}

std::shared_ptr<Action> MaintenanceFeature::findActionId(uint64_t id) {
  READ_LOCKER(rLock, _actionRegistryLock);

  return findActionIdNoLock(id);
} 

std::shared_ptr<Action> MaintenanceFeature::findActionIdNoLock(uint64_t id) {
  // assert to test lock held?

  for (auto const& action : _actionRegistry) {
    if (action->id() == id) {
      return action;
    }
  }
  return std::shared_ptr<Action>();
}

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
    }  // WRITE

    // no pointer ... wait 0.1 seconds unless woken up
    if (!_isShuttingDown) {
      CONDITION_LOCKER(cLock, _actionRegistryCond);
      _actionRegistryCond.wait(std::chrono::milliseconds(100));
    }  // if

  }  // while

  return ret_ptr;

} 

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
    LOG_TOPIC("0d580", DEBUG, Logger::MAINTENANCE) << error.str();
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
    LOG_TOPIC("4ab17", DEBUG, Logger::MAINTENANCE) << error.str();
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
    LOG_TOPIC("378fa", DEBUG, Logger::MAINTENANCE) << error.str();
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
    LOG_TOPIC("b05d6", DEBUG, Logger::MAINTENANCE) << error.str();
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
    LOG_TOPIC("d3c92", DEBUG, Logger::MAINTENANCE) << error.str();
    return Result(TRI_ERROR_FAILED, error.str());
  }

  try {
    errors.emplace(indexId, error);
  } catch (std::exception const& e) {
    return Result(TRI_ERROR_FAILED, e.what());
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
    LOG_TOPIC("678a2", DEBUG, Logger::MAINTENANCE) << error.str();
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
    LOG_TOPIC("e75c8", DEBUG, Logger::MAINTENANCE) << error.str();
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
  LOG_TOPIC("23fbc", TRACE, Logger::MAINTENANCE)
      << "getting shard version for '" << shname << "' from " << _shardVersion;
  return (it != _shardVersion.end()) ? it->second : 0;
}

uint64_t MaintenanceFeature::incShardVersion(std::string const& shname) {
  MUTEX_LOCKER(guard, _versionLock);
  auto ret = ++_shardVersion[shname];
  LOG_TOPIC("cc492", TRACE, Logger::MAINTENANCE)
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

uint64_t MaintenanceFeature::getCurrentCounter() const {
  // It is guaranteed that getCurrentCounter is not executed
  // concurrent to increase / wait.
  // This guarantee is created by the following:
  // 1) There is one inifinite loop that will call
  //    PhaseOne and PhaseTwo in exactly this ordering.
  //    It is guaranteed that only one thread at a time is
  //    in this loop.
  //    Between PhaseOne and PhaseTwo the increaseCurrentCounter is called
  //    Within PhaseOne this getCurrentCounter is called, but never after.
  //    so getCurrentCounter and increaseCurrentCounter are strictily serialized.
  // 2) waitForLargerCurrentCounter can be called in concurrent threads at any time.
  //    It is read-only, so it is save to have it concurrent to getCurrentCounter
  //    without any locking.
  //    However we need locking for increase and waitFor in order to guarantee
  //    it's functionallity.
  // For now we actually do not need this guard, but as this is NOT performance
  // critical we can simply get it, just to be save for later use.
  std::unique_lock<std::mutex> guard(_currentCounterLock);
  return _currentCounter;
}

void MaintenanceFeature::increaseCurrentCounter() {
  std::unique_lock<std::mutex> guard(_currentCounterLock);
  _currentCounter++;
  _currentCounterCondition.notify_all();
}

void MaintenanceFeature::waitForLargerCurrentCounter(uint64_t old) {
  std::unique_lock<std::mutex> guard(_currentCounterLock);
  if (_currentCounter > old) {
    return;
  }
  _currentCounterCondition.wait(guard);
  TRI_ASSERT(_currentCounter > old);
  return;
}