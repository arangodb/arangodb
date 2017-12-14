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

#include "ReplicationApplier.h"
#include "Basics/Exceptions.h"
#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/Thread.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/files.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "Replication/TailingSyncer.h"
#include "Rest/Version.h"
#include "RestServer/ServerIdFeature.h"
#include "VocBase/replication-common.h"

using namespace arangodb;

/// @brief applier thread class
class ApplyThread : public Thread {
 public:
  ApplyThread(ReplicationApplier* applier, std::unique_ptr<TailingSyncer>&& syncer)
      : Thread("ReplicationApplier"), _applier(applier), _syncer(std::move(syncer)) {}

  ~ApplyThread() {
    {
      MUTEX_LOCKER(locker, _syncerMutex);
      _syncer.reset();
    }
    
    shutdown(); 
  }

 public:
  void setAborted(bool value) {
    MUTEX_LOCKER(locker, _syncerMutex);

    if (_syncer) {
      _syncer->setAborted(value);
    }
  }
  
  void run() {
    TRI_ASSERT(_syncer != nullptr);
    TRI_ASSERT(_applier != nullptr);

    try {
      setAborted(false);
      Result res = _syncer->run();
      if (res.fail() && res.isNot(TRI_ERROR_REPLICATION_APPLIER_STOPPED)) {
        LOG_TOPIC(ERR, Logger::REPLICATION) << "error while running applier for " << _applier->databaseName() << ": "
          << res.errorMessage();
      }
    } catch (std::exception const& ex) {
      LOG_TOPIC(WARN, Logger::REPLICATION) << "caught exception in ApplyThread for " << _applier->databaseName() << ": " << ex.what();
    } catch (...) {
      LOG_TOPIC(WARN, Logger::REPLICATION) << "caught unknown exception in ApplyThread for " << _applier->databaseName();
    }

    {
      MUTEX_LOCKER(locker, _syncerMutex);
      // will make the syncer remove its barrier too
      _syncer->setAborted(false);
      _syncer.reset();
    }

    _applier->markThreadStopped();
  }

 private:
  ReplicationApplier* _applier;
  Mutex _syncerMutex;
  std::unique_ptr<TailingSyncer> _syncer;
};


ReplicationApplier::ReplicationApplier(ReplicationApplierConfiguration const& configuration,
                                       std::string&& databaseName) 
      : _configuration(configuration),
        _databaseName(std::move(databaseName)) {
    setProgress(std::string("applier initially created for ") + _databaseName);
}

/// @brief test if the replication applier is running
bool ReplicationApplier::isRunning() const {
  READ_LOCKER_EVENTUAL(readLocker, _statusLock);
  return _state.isRunning();
}

/// @brief test if the replication applier is shutting down
bool ReplicationApplier::isShuttingDown() const {
  READ_LOCKER_EVENTUAL(readLocker, _statusLock);
  return _state.isShuttingDown();
}

void ReplicationApplier::markThreadStopped() {
  WRITE_LOCKER_EVENTUAL(writeLocker, _statusLock);
  _state._state = ReplicationApplierState::ActivityState::INACTIVE;
  setProgressNoLock("applier shut down");
  
  LOG_TOPIC(INFO, Logger::REPLICATION) << "stopped replication applier for " << _databaseName;
}

/// @brief block the replication applier from starting
Result ReplicationApplier::preventStart() {
  WRITE_LOCKER_EVENTUAL(writeLocker, _statusLock);

  if (_state.isRunning()) {
    // already running
    return Result(TRI_ERROR_REPLICATION_RUNNING);
  }

  if (_state._preventStart) {
    // someone else requested start prevention
    return Result(TRI_ERROR_LOCKED);
  }

  _state._stopInitialSynchronization = false;
  _state._preventStart = true;

  return Result();
}

/// @brief unblock the replication applier from starting
void ReplicationApplier::allowStart() {
  WRITE_LOCKER_EVENTUAL(writeLocker, _statusLock);

  if (!_state._preventStart) {
    return;
  }

  _state._stopInitialSynchronization = false;
  _state._preventStart = false;
}

/// @brief whether or not autostart option was set
bool ReplicationApplier::autoStart() const {
  READ_LOCKER_EVENTUAL(readLocker, _statusLock);
  return _configuration._autoStart;
}

/// @brief whether or not the applier has a state already
bool ReplicationApplier::hasState() const {
  READ_LOCKER_EVENTUAL(readLocker, _statusLock);
  return _state.hasProcessedSomething();
}

/// @brief check whether the initial synchronization should be stopped
bool ReplicationApplier::stopInitialSynchronization() const {
  READ_LOCKER_EVENTUAL(readLocker, _statusLock);

  return _state._stopInitialSynchronization;
}

/// @brief stop the initial synchronization
void ReplicationApplier::stopInitialSynchronization(bool value) {
  WRITE_LOCKER_EVENTUAL(writeLocker, _statusLock);
  _state._stopInitialSynchronization = value;
}
  
/// @brief start the replication applier
void ReplicationApplier::start(TRI_voc_tick_t initialTick, bool useTick, TRI_voc_tick_t barrierId) {
  if (!applies()) {
    return;
  }

  WRITE_LOCKER_EVENTUAL(writeLocker, _statusLock);
  
  if (_state._preventStart) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_LOCKED, 
                                   std::string("cannot start replication applier for ") + 
                                   _databaseName + ": " + TRI_errno_string(TRI_ERROR_LOCKED));
  }
  
  if (_state.isRunning()) {
    // already started
    return;
  }
  
  while (_state.isShuttingDown()) {
    // another instance is still around
    writeLocker.unlock();
    usleep(50 * 1000);
    writeLocker.lock();
  }

  TRI_ASSERT(!_state.isRunning() && !_state.isShuttingDown());
  
  LOG_TOPIC(DEBUG, Logger::REPLICATION)
      << "requesting replication applier start for " << _databaseName << ". initialTick: " << initialTick
      << ", useTick: " << useTick;

  if (_configuration._endpoint.empty()) {
    Result r(TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION, "no endpoint configured");
    setErrorNoLock(r);
    THROW_ARANGO_EXCEPTION(r);
  }
    
  if (!isGlobal() && _configuration._database.empty()) {
    Result r(TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION, "no database configured");
    setErrorNoLock(r);
    THROW_ARANGO_EXCEPTION(r);
  }

  // reset error
  _state._lastError.reset();
 
  _thread.reset(new ApplyThread(this, buildSyncer(initialTick, useTick, barrierId)));
  
  if (!_thread->start()) {
    _thread.reset();
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "could not start ApplyThread");
  }
  
  while (!_thread->hasStarted()) {
    usleep(20000);
  }
  
  TRI_ASSERT(!_state.isRunning() && !_state.isShuttingDown());
  _state._state = ReplicationApplierState::ActivityState::RUNNING;
  
  if (useTick) {
    LOG_TOPIC(INFO, Logger::REPLICATION)
        << "started replication applier for " << _databaseName
        << ", endpoint '" << _configuration._endpoint << "' from tick "
        << initialTick;
  } else {
    LOG_TOPIC(INFO, Logger::REPLICATION)
        << "re-started replication applier for "
        << _databaseName << ", endpoint '" << _configuration._endpoint
        << "' from previous state";
  }
}

/// @brief stop the replication applier
void ReplicationApplier::stop(Result const& r) {
  doStop(r, false);
}

void ReplicationApplier::stop() {
  doStop(Result(), false);
}

/// @brief stop the replication applier and join the apply thread
void ReplicationApplier::stopAndJoin() {
  doStop(Result(), true);
}

/// @brief sleeps for the specific number of microseconds if the
/// applier is still active, and returns true. if the applier is not
/// active anymore, returns false
bool ReplicationApplier::sleepIfStillActive(uint64_t sleepTime) {
  while (sleepTime > 0) {
    if (!isRunning()) {
      // already terminated
      return false; 
    }
  
    // now sleep  
    uint64_t sleepChunk = 250 * 1000;
    if (sleepChunk > sleepTime) {
      sleepChunk = sleepTime;
    }
    usleep(static_cast<TRI_usleep_t>(sleepChunk));
    sleepTime -= sleepChunk;
  }
  
  return isRunning();
}

void ReplicationApplier::removeState() {
  if (!applies()) {
    return;
  }

  WRITE_LOCKER_EVENTUAL(writeLocker, _statusLock);
  _state.reset(false);

  std::string const filename = getStateFilename();

  if (TRI_ExistsFile(filename.c_str())) {
    LOG_TOPIC(TRACE, Logger::REPLICATION) << "removing replication state file '"
                                          << filename << "' for " << _databaseName;
    int res = TRI_UnlinkFile(filename.c_str());

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION_MESSAGE(res, std::string("unable to remove replication state file '") + filename + "'");
    }
  }
} 
  
void ReplicationApplier::reconfigure(ReplicationApplierConfiguration const& configuration) {
  if (!applies()) {
    return;
  }

  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  
  if (configuration._endpoint.empty()) {
    // no endpoint
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION, "no endpoint configured");
  }

  WRITE_LOCKER_EVENTUAL(writeLocker, _statusLock);

  if (_state.isRunning()) {
    // cannot change the configuration while the replication is still running
    THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_RUNNING);
  }

  _configuration = configuration;
  storeConfiguration(true);
}

/// @brief load the applier state from persistent storage
/// must currently be called while holding the write-lock
/// returns whether a previous state was found
bool ReplicationApplier::loadState() {
  if (!applies()) {
    // unsupported
    return false;
  }

  std::string const filename = getStateFilename();
  LOG_TOPIC(TRACE, Logger::REPLICATION)
      << "looking for replication state file '" << filename << "' for " << _databaseName;

  if (!TRI_ExistsFile(filename.c_str())) {
    // no existing state found
    return false;
  }

  LOG_TOPIC(DEBUG, Logger::REPLICATION) << "replication state file '" << filename << "' found for " << _databaseName;

  VPackBuilder builder;
  try {
    builder = basics::VelocyPackHelper::velocyPackFromFile(filename);
  } catch (...) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_REPLICATION_INVALID_APPLIER_STATE, std::string("cannot read replication applier state from file '") + filename + "'");
  }

  VPackSlice const slice = builder.slice();
  if (!slice.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_REPLICATION_INVALID_APPLIER_STATE, std::string("invalid replication applier state found in file '") + filename + "'");
  }

  _state.reset(false);

  // read the server id
  VPackSlice const serverId = slice.get("serverId");
  if (!serverId.isString()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_INVALID_APPLIER_STATE);
  }
  _state._serverId = arangodb::basics::StringUtils::uint64(serverId.copyString());

  // read the ticks
  readTick(slice, "lastAppliedContinuousTick", _state._lastAppliedContinuousTick, false);

  // set processed = applied
  _state._lastProcessedContinuousTick = _state._lastAppliedContinuousTick;

  // read the safeResumeTick. note: this is an optional attribute
  _state._safeResumeTick = 0;
  readTick(slice, "safeResumeTick", _state._safeResumeTick, true);

  return true;
}
  
/// @brief store the applier state in persistent storage
/// must currently be called while holding the write-lock
void ReplicationApplier::persistState(bool doSync) {
  if (!applies()) {
    return;
  }

  VPackBuilder builder;
  _state.toVelocyPack(builder, false);

  std::string const filename = getStateFilename();
  LOG_TOPIC(TRACE, Logger::REPLICATION)
      << "saving replication applier state to file '" << filename << "' for " << _databaseName;

  if (!basics::VelocyPackHelper::velocyPackToFile(filename, builder.slice(), doSync)) {
    THROW_ARANGO_EXCEPTION(TRI_errno());
  }
}
  
/// @brief store the current applier state in the passed vpack builder 
void ReplicationApplier::toVelocyPack(arangodb::velocypack::Builder& result) const {
  TRI_ASSERT(!result.isClosed());

  ReplicationApplierConfiguration configuration;
  ReplicationApplierState state;

  {
    // copy current config and state under the lock
    READ_LOCKER_EVENTUAL(readLocker, _statusLock);
    configuration = _configuration;
    state = _state;
  }

  // add state
  result.add(VPackValue("state"));
  state.toVelocyPack(result, true);

  // add server info
  result.add("server", VPackValue(VPackValueType::Object));
  result.add("version", VPackValue(ARANGODB_VERSION));
  result.add("serverId", VPackValue(std::to_string(ServerIdFeature::getId())));
  result.close();  // server
  
  if (!configuration._endpoint.empty()) {
    result.add("endpoint", VPackValue(configuration._endpoint));
  }
  if (!configuration._database.empty()) {
    result.add("database", VPackValue(configuration._database));
  }
}
  
/// @brief return the current configuration
ReplicationApplierConfiguration ReplicationApplier::configuration() const {
  READ_LOCKER_EVENTUAL(readLocker, _statusLock);
  return _configuration;
}

/// @brief return the current configuration
std::string ReplicationApplier::endpoint() const {
  READ_LOCKER_EVENTUAL(readLocker, _statusLock);
  return _configuration._endpoint;
}

/// @brief register an applier error
void ReplicationApplier::setError(arangodb::Result const& r) {
  WRITE_LOCKER_EVENTUAL(writeLocker, _statusLock);
  setErrorNoLock(r);
}

Result ReplicationApplier::lastError() const {
  WRITE_LOCKER_EVENTUAL(writeLocker, _statusLock);
  return Result(_state._lastError.code, _state._lastError.message);
}

/// @brief set the progress 
void ReplicationApplier::setProgress(char const* msg) {
  return setProgress(std::string(msg));
}

void ReplicationApplier::setProgress(std::string const& msg) {
  WRITE_LOCKER_EVENTUAL(writeLocker, _statusLock);
  setProgressNoLock(msg);
}

/// @brief register an applier error
void ReplicationApplier::setErrorNoLock(arangodb::Result const& rr) {
  // log error message
  if (rr.isNot(TRI_ERROR_REPLICATION_APPLIER_STOPPED)) {
    LOG_TOPIC(ERR, Logger::REPLICATION)
        << "replication applier error for " << _databaseName << ": "
        << rr.errorMessage();
  }

  _state.setError(rr.errorNumber(), rr.errorMessage());
}

void ReplicationApplier::setProgressNoLock(std::string const& msg) {
  _state._progressMsg = msg;

  // write time into buffer
  TRI_GetTimeStampReplication(_state._progressTime, sizeof(_state._progressTime) - 1);
}

/// @brief stop the replication applier
void ReplicationApplier::doStop(Result const& r, bool joinThread) {
  if (!applies()) {
    return;
  }

  WRITE_LOCKER_EVENTUAL(writeLocker, _statusLock);

  // always stop initial synchronization
  _state._stopInitialSynchronization = true;
  
  if (!_state.isRunning() || _state.isShuttingDown()) {
    // not active or somebody else is shutting us down
    return;
  }
  
  LOG_TOPIC(DEBUG, Logger::REPLICATION)
      << "requesting replication applier stop for " << _databaseName;
  
  _state._state = ReplicationApplierState::ActivityState::SHUTTING_DOWN;
  _state.setError(r.errorNumber(), r.errorMessage());

  if (_thread != nullptr) {
    static_cast<ApplyThread*>(_thread.get())->setAborted(true);
  }

  if (joinThread) {
    while (_state.isShuttingDown()) {
      writeLocker.unlock();
      usleep(50 * 1000);
      writeLocker.lock();
    }
    
    TRI_ASSERT(!_state.isRunning() && !_state.isShuttingDown());
  
    // wipe aborted flag. this will be passed on to the syncer
    static_cast<ApplyThread*>(_thread.get())->setAborted(false);

    // steal thread
    Thread* t = _thread.release();
    // now _thread is empty
    // and release the write lock so when "thread" goes
    // out of scope, it actually can call the thread
    // deleter without holding the write lock (which would
    // deadlock)
    writeLocker.unlock();

    // now delete without holding the lock
    delete t;
  }
}
  
/// @brief read a tick value from a VelocyPack struct
void ReplicationApplier::readTick(VPackSlice const& slice, char const* attributeName,
                                  TRI_voc_tick_t& dst, bool allowNull) {
  TRI_ASSERT(slice.isObject());

  VPackSlice const tick = slice.get(attributeName);

  if ((tick.isNull() || tick.isNone()) && allowNull) {
    dst = 0;
  } else { 
    if (!tick.isString()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_INVALID_APPLIER_STATE);
    }

    dst = static_cast<TRI_voc_tick_t>(arangodb::basics::StringUtils::uint64(tick.copyString()));
  }
}
