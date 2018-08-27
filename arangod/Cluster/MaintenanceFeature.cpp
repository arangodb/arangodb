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
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Basics/MutexLocker.h"
#include "Cluster/ActionDescription.h"
#include "Cluster/CreateDatabase.h"
#include "Cluster/Action.h"
#include "Cluster/MaintenanceWorker.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::options;
using namespace arangodb::maintenance;

MaintenanceFeature::MaintenanceFeature(application_features::ApplicationServer& server)
  : ApplicationFeature(server, "Maintenance") {

//  startsAfter("EngineSelector");    // ??? what should this be
//  startsBefore("StorageEngine");

  init();
} // MaintenanceFeature::MaintenanceFeature

void MaintenanceFeature::init() {
  _isShuttingDown = false;
  _nextActionId = 1;

  setOptional(true);
  requiresElevatedPrivileges(false); // ??? this mean admin priv?

  // these parameters might be updated by config and/or command line options
  _maintenanceThreadsMax = static_cast<int32_t>(TRI_numberProcessors() / 4 + 1);
  _secondsActionsBlock = 2;
  _secondsActionsLinger = 3600;
} // MaintenanceFeature::init


void MaintenanceFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("server", "Server features");

  options->addHiddenOption(
    "--server.maintenance-threads",
    "maximum number of threads available for maintenance actions",
    new Int32Parameter(&_maintenanceThreadsMax));

  options->addHiddenOption(
    "--server.maintenance-actions-block",
    "minimum number of seconds finished Actions block duplicates",
    new Int32Parameter(&_secondsActionsBlock));

  options->addHiddenOption(
    "--server.maintenance-actions-linger",
    "minimum number of seconds finished Actions remain in deque",
    new Int32Parameter(&_secondsActionsLinger));

} // MaintenanceFeature::collectOptions


/// do not start threads in prepare
void MaintenanceFeature::prepare() {
} // MaintenanceFeature::prepare


void MaintenanceFeature::start() {
  // start threads
  for (int32_t loop = 0; loop < _maintenanceThreadsMax; ++loop) {
    auto newWorker = std::make_unique<maintenance::MaintenanceWorker>(*this);
    if (!newWorker->start(&_workerCompletion)) {
      LOG_TOPIC(ERR, Logger::MAINTENANCE)
        << "MaintenanceFeature::start:  newWorker start failed";
    } else {
      _activeWorkers.push_back(std::move(newWorker));
    }
  } // for
} // MaintenanceFeature::start


void MaintenanceFeature::beginShutdown() {
  _isShuttingDown = true;
  CONDITION_LOCKER(cLock, _actionRegistryCond);
  _actionRegistryCond.broadcast();
} // MaintenanceFeature


void MaintenanceFeature::stop() {
  for (auto const& itWorker : _activeWorkers ) {
    CONDITION_LOCKER(cLock, _workerCompletion);

    // loop on each worker, retesting at 10ms just in case
    if (itWorker->isRunning()) {
      _workerCompletion.wait(10000);
    } // if
  } // for

} // MaintenanceFeature::stop


/// @brief Move an incomplete action to failed state
Result MaintenanceFeature::deleteAction(uint64_t action_id) {
  Result result;

  // pointer to action, or nullptr
  auto action = findActionId(action_id);

  if (action) {
    if (maintenance::COMPLETE != action->getState()) {
      action->setState(maintenance::FAILED);
    } else {
      result.reset(TRI_ERROR_BAD_PARAMETER,"deleteAction called after action complete.");
    } // else
  } else {
    result.reset(TRI_ERROR_BAD_PARAMETER,"deleteAction could not find action to delete.");
  } // else

  return result;

} // MaintenanceFeature::deleteAction


// FIXMEMAINTENANCE: None of the addAction() and createAction() routines
// explicitly check to see if construction of action set FAILED.
// Therefore it is possible for an "executeNow" action to start running
// with known invalid parameters.

/// @brief This is the  API for creating an Action and executing it.
///  Execution can be immediate by calling thread, or asynchronous via thread pool.
///  not yet:  ActionDescription parameter will be MOVED to new object.
Result MaintenanceFeature::addAction(
  std::shared_ptr<maintenance::Action> newAction, bool executeNow) {

  Result result;

  // the underlying routines are believed to be safe and throw free,
  //  but just in case
  try {

    size_t action_hash = newAction->hash();
    WRITE_LOCKER(wLock, _actionRegistryLock);

    std::shared_ptr<Action> curAction = findActionHashNoLock(action_hash);

    // similar action not in the queue (or at least no longer viable)
    if (curAction == nullptr || curAction->done()) {

      createAction(newAction, executeNow);

      if (!newAction || !newAction->ok()) {
        /// something failed in action creation ... go check logs
        result.reset(TRI_ERROR_BAD_PARAMETER, "createAction rejected parameters.");
      } // if
    } else {
      // action already exist, need write lock to prevent race
      result.reset(TRI_ERROR_BAD_PARAMETER, "addAction called while similar action already processing.");
    } //else

    // executeNow process on this thread, right now!
    if (result.ok() && executeNow) {
      maintenance::MaintenanceWorker worker(*this, newAction);
      worker.run();
      result = worker.result();
    } // if
  } catch (...) {
    result.reset(TRI_ERROR_INTERNAL, "addAction experienced an unexpected throw.");
  } // catch

  return result;

} // MaintenanceFeature::addAction



/// @brief This is the  API for creating an Action and executing it.
///  Execution can be immediate by calling thread, or asynchronous via thread pool.
///  not yet:  ActionDescription parameter will be MOVED to new object.
Result MaintenanceFeature::addAction(
  std::shared_ptr<maintenance::ActionDescription> const & description,
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
      newAction = createAction(description, executeNow);

      if (!newAction || !newAction->ok()) {
        /// something failed in action creation ... go check logs
        result.reset(TRI_ERROR_BAD_PARAMETER, "createAction rejected parameters.");
      } // if
    } else {
      // action already exist, need write lock to prevent race
      result.reset(TRI_ERROR_BAD_PARAMETER, "addAction called while similar action already processing.");
    } //else

    // executeNow process on this thread, right now!
    if (result.ok() && executeNow) {
      maintenance::MaintenanceWorker worker(*this, newAction);
      worker.run();
      result = worker.result();
    } // if
  } catch (...) {
    result.reset(TRI_ERROR_INTERNAL, "addAction experienced an unexpected throw.");
  } // catch

  return result;

} // MaintenanceFeature::addAction


std::shared_ptr<Action> MaintenanceFeature::preAction(
  std::shared_ptr<ActionDescription> const & description) {

  return createAction(description, true);

} // MaintenanceFeature::preAction


std::shared_ptr<Action> MaintenanceFeature::postAction(
  std::shared_ptr<ActionDescription> const & description) {

  return createAction(description, false);

} // MaintenanceFeature::postAction


void MaintenanceFeature::createAction(
  std::shared_ptr<Action> action, bool executeNow) {

  // mark as executing so no other workers accidentally grab it
  if (executeNow) {
    action->setState(maintenance::EXECUTING);
  } // if

  // WARNING: holding write lock to _actionRegistry and about to
  //   lock condition variable
  {
    _actionRegistry.push_back(action);

    if (!executeNow) {
      CONDITION_LOCKER(cLock, _actionRegistryCond);
      _actionRegistryCond.signal();
    } // if
  } // lock

}


std::shared_ptr<Action> MaintenanceFeature::createAction(
  std::shared_ptr<ActionDescription> const & description,
  bool executeNow) {

  // write lock via _actionRegistryLock is assumed held
  std::shared_ptr<Action> newAction;

  // name should already be verified as existing ... but trust no one
  std::string name = description->get(NAME);

  // call factory
  newAction = std::make_shared<Action>(*this, *description);

  // if a new action constructed successfully
  if (newAction->ok()) {

    createAction(newAction, executeNow);

  } else {
    LOG_TOPIC(ERR, Logger::MAINTENANCE)
      << "createAction:  unknown action name given, \"" << name.c_str() << "\", or other construction failure.";
  } // else

  return newAction;

} // if


std::shared_ptr<Action> MaintenanceFeature::findAction(
  std::shared_ptr<ActionDescription> const description) {
  return findActionHash(description->hash());
}


std::shared_ptr<Action> MaintenanceFeature::findActionHash(size_t hash) {
  READ_LOCKER(rLock, _actionRegistryLock);

  return findActionHashNoLock(hash);
} // MaintenanceFeature::findActionHash


std::shared_ptr<Action> MaintenanceFeature::findActionHashNoLock(size_t hash) {
  // assert to test lock held?

  std::shared_ptr<Action> ret_ptr;

  for (auto action_it=_actionRegistry.begin();
       _actionRegistry.end() != action_it && !ret_ptr; ++action_it) {
    if ((*action_it)->hash() == hash) {
      ret_ptr=*action_it;
    } // if
  } // for

  return ret_ptr;

} // MaintenanceFeature::findActionHashNoLock


std::shared_ptr<Action> MaintenanceFeature::findActionId(uint64_t id) {
  READ_LOCKER(rLock, _actionRegistryLock);

  return findActionIdNoLock(id);
} // MaintenanceFeature::findActionId


std::shared_ptr<Action> MaintenanceFeature::findActionIdNoLock(uint64_t id) {
  // assert to test lock held?

  std::shared_ptr<Action> ret_ptr;

  for (auto action_it=_actionRegistry.begin();
       _actionRegistry.end() != action_it && !ret_ptr; ++action_it) {
    if ((*action_it)->id() == id) {
      ret_ptr=*action_it;
    } // if
  } // for

  return ret_ptr;

} // MaintenanceFeature::findActionIdNoLock


std::shared_ptr<Action> MaintenanceFeature::findReadyAction() {
  std::shared_ptr<Action> ret_ptr;

  while(!_isShuttingDown && !ret_ptr) {

    // scan for ready action (and purge any that are done waiting)
    {
      WRITE_LOCKER(wLock, _actionRegistryLock);

      for (auto loop=_actionRegistry.begin(); _actionRegistry.end()!=loop && !ret_ptr; ) {
        auto state = (*loop)->getState();
        if (state == maintenance::READY) {
          ret_ptr=*loop;
          ret_ptr->setState(maintenance::EXECUTING);
        } else if ((*loop)->done()) {
          loop = _actionRegistry.erase(loop);
        } else {
          ++loop;
        } // else
      } // for
    } // WRITE

    // no pointer ... wait 5 second
    if (!_isShuttingDown && !ret_ptr) {
    CONDITION_LOCKER(cLock, _actionRegistryCond);
      _actionRegistryCond.wait(100000);
    } // if

  } // while

  return ret_ptr;

} // MaintenanceFeature::findReadyAction


VPackBuilder MaintenanceFeature::toVelocyPack() const {
  VPackBuilder vb;
  READ_LOCKER(rLock, _actionRegistryLock);

  { VPackArrayBuilder ab(&vb);
    for (auto const& action : _actionRegistry ) {
      action->toVelocyPack(vb);
    } // for

  }
  return vb;

} // MaintenanceFeature::toVelocyPack
#if 0
std::string MaintenanceFeature::toJson(VPackBuilder & builder) {
} // MaintenanceFeature::toJson
#endif

std::string const SLASH("/");

arangodb::Result MaintenanceFeature::storeDBError (
  std::string const& database, std::shared_ptr<VPackBuffer<uint8_t>> error) {

  MUTEX_LOCKER(guard, _dbeLock);
  auto const it = _dbErrors.find(database);
  if (it != _dbErrors.end()) {
    std::stringstream error;
    error << "database " << database << " already has pending error";
    LOG_TOPIC(DEBUG, Logger::MAINTENANCE) << error.str();
    return Result(TRI_ERROR_FAILED, error.str());
  }

  try {
    _dbErrors.emplace(database,error);
  } catch (std::exception const& e) {
    return Result(TRI_ERROR_FAILED, e.what());
  }

  return Result();
  
}

arangodb::Result MaintenanceFeature::dbError (
  std::string const& database, std::shared_ptr<VPackBuffer<uint8_t>>& error) const {

  MUTEX_LOCKER(guard, _dbeLock);
  auto const it = _dbErrors.find(database);
  error = (it != _dbErrors.end()) ? it->second : nullptr;
  return Result();
  
}

arangodb::Result MaintenanceFeature::removeDBError (
  std::string const& database) {

  try {
    MUTEX_LOCKER(guard, _seLock);
    _shardErrors.erase(database);
  } catch (std::exception const& e) {
    std::stringstream error;
    error << "erasing dataabse error for " << database << " failed";
    LOG_TOPIC(DEBUG, Logger::MAINTENANCE) << error.str();
    return Result(TRI_ERROR_FAILED, error.str());
  }

  return Result();
  
}

arangodb::Result MaintenanceFeature::storeShardError (
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
    _shardErrors.emplace(key,error);
  } catch (std::exception const& e) {
    return Result(TRI_ERROR_FAILED, e.what());
  }

  return Result();
  
}

arangodb::Result MaintenanceFeature::shardError (
  std::string const& database, std::string const& collection,
  std::string const& shard, std::shared_ptr<VPackBuffer<uint8_t>>& error) const {

  std::string key = database + SLASH + collection + SLASH + shard;

  MUTEX_LOCKER(guard, _seLock);
  auto const it = _shardErrors.find(key);
  error = (it != _shardErrors.end()) ? it->second : nullptr;
  return Result();
  
}

arangodb::Result MaintenanceFeature::removeShardError (std::string const& key) {

  try {
    MUTEX_LOCKER(guard, _seLock);
    _shardErrors.erase(key);
  } catch (std::exception const& e) {
    std::stringstream error;
    error << "erasing shard error for " << key << " failed";
    LOG_TOPIC(DEBUG, Logger::MAINTENANCE) << error.str();
    return Result(TRI_ERROR_FAILED, error.str());
  }
  
  return Result();
  
}

arangodb::Result MaintenanceFeature::removeShardError (
  std::string const& database, std::string const& collection,
  std::string const& shard) {
  return removeShardError(database + SLASH + collection + SLASH + shard);
}


arangodb::Result MaintenanceFeature::storeIndexError (
  std::string const& database, std::string const& collection,
    std::string const& shard, std::string const& indexId,
    std::shared_ptr<VPackBuffer<uint8_t>> error) {

  using buffer_t = std::shared_ptr<VPackBuffer<uint8_t>>;
  std::string key = database + SLASH + collection + SLASH + shard;
  
  MUTEX_LOCKER(guard, _ieLock);
  
  auto errorsIt = _indexErrors.find(key);
  if (errorsIt == _indexErrors.end()) {
    try {
      _indexErrors.emplace(key,std::map<std::string,buffer_t>());
    } catch (std::exception const& e) {
      return Result(TRI_ERROR_FAILED, e.what());
    }
  }
  auto& errors = _indexErrors.find(key)->second;
  auto const it = errors.find(indexId);

  if (it != errors.end()) {
    std::stringstream error;
    error << "index " << indexId << " for shard "
          << key << " already has pending error";
    LOG_TOPIC(DEBUG, Logger::MAINTENANCE) << error.str();
    return Result(TRI_ERROR_FAILED, error.str());
  }

  try {
    errors.emplace(indexId,error);
  } catch (std::exception const& e) {
    return Result(TRI_ERROR_FAILED, e.what());
  }

  return Result();
  
}

arangodb::Result MaintenanceFeature::indexErrors (
  std::string const& database, std::string const& collection,
  std::string const& shard,
  std::map<std::string,std::shared_ptr<VPackBuffer<uint8_t>>>& error) const {

  std::string key = database + SLASH + collection + SLASH + shard;

  MUTEX_LOCKER(guard, _ieLock);
  auto const& it = _indexErrors.find(key);
  if (it != _indexErrors.end()) {
    error = it->second;
  }
  
  return Result();
  
}

template<typename T>
std::ostream& operator<<(std::ostream& os, std::set<T>const& st) {
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


arangodb::Result MaintenanceFeature::removeIndexErrors (
  std::string const& key, std::unordered_set<std::string> indexIds) {

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
  } catch (std::exception const& e) {
    std::stringstream error;
    error << "erasing index errors " << indexIds << " for " << key << " failed";
    LOG_TOPIC(DEBUG, Logger::MAINTENANCE) << error.str();
    return Result(TRI_ERROR_FAILED, error.str());
  }

  return Result();

}

arangodb::Result MaintenanceFeature::removeIndexErrors (
  std::string const& database, std::string const& collection,
  std::string const& shard, std::unordered_set<std::string> indexIds) {

  return removeIndexErrors(
    database + SLASH + collection + SLASH + shard, indexIds);
  
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


