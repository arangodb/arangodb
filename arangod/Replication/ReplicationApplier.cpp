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

#include "ReplicationApplier.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/files.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Replication/InitialSyncer.h"
#include "Replication/TailingSyncer.h"
#include "Replication/common-defines.h"
#include "Rest/Version.h"
#include "RestServer/ServerIdFeature.h"

using namespace arangodb;
namespace StringUtils = arangodb::basics::StringUtils;

/// @brief common replication applier
struct ApplierThread : public Thread {
 public:
  ApplierThread(application_features::ApplicationServer& server,
                ReplicationApplier* applier, std::shared_ptr<Syncer> syncer)
      : Thread(server, "ReplicationApplier"),
        _applier(applier),
        _syncer(std::move(syncer)) {
    TRI_ASSERT(_syncer);
  }

  ~ApplierThread() = default; // shutdown is called by derived implementations!

  void run() override {
    TRI_ASSERT(_syncer != nullptr);
    TRI_ASSERT(_applier != nullptr);

    try {
      setAborted(false);
      Result res = runApplier();
      if (res.fail() && res.isNot(TRI_ERROR_REPLICATION_APPLIER_STOPPED)) {
        LOG_TOPIC("6fe50", ERR, Logger::REPLICATION)
            << "error while running applier thread for "
            << _applier->databaseName() << ": " << res.errorMessage();
      }
    } catch (std::exception const& ex) {
      LOG_TOPIC("f6e01", WARN, Logger::REPLICATION)
          << "caught exception in ApplierThread for "
          << _applier->databaseName() << ": " << ex.what();
    } catch (...) {
      LOG_TOPIC("24dd2", WARN, Logger::REPLICATION)
          << "caught unknown exception in ApplyThread for "
          << _applier->databaseName();
    }

    {
      MUTEX_LOCKER(locker, _syncerMutex);
      _syncer->setAborted(false);
      _syncer.reset();
    }

    _applier->markThreadStopped();
  }

  virtual Result runApplier() = 0;

  void setAborted(bool value) {
    MUTEX_LOCKER(locker, _syncerMutex);
    if (_syncer) {
      _syncer->setAborted(value);
    }
  }

 protected:
  ReplicationApplier* _applier;
  Mutex _syncerMutex;
  std::shared_ptr<Syncer> _syncer;
};

/// @brief sync thread class
struct FullApplierThread final : public ApplierThread {
  FullApplierThread(application_features::ApplicationServer& server,
                    ReplicationApplier* applier, std::shared_ptr<InitialSyncer>&& syncer)
      : ApplierThread(server, applier, std::move(syncer)) {}

  ~FullApplierThread() { shutdown(); }

  Result runApplier() override {
    TRI_ASSERT(_syncer != nullptr);
    TRI_ASSERT(_applier != nullptr);

    InitialSyncer* initSync = static_cast<InitialSyncer*>(_syncer.get());
    // start initial synchronization
    bool allowIncremental = _applier->configuration()._incremental;
    Result r = initSync->run(allowIncremental);
    if (r.fail() || initSync->isAborted()) {
      return r;
    }
    TRI_voc_tick_t lastLogTick = initSync->getLastLogTick();

    {
      MUTEX_LOCKER(locker, _syncerMutex);
      auto tailer = _applier->buildTailingSyncer(lastLogTick, true);
      _syncer.reset();
      _syncer = std::move(tailer);
    }

    _applier->markThreadTailing();

    TRI_ASSERT(_syncer);
    return static_cast<TailingSyncer*>(_syncer.get())->run();
  }
};

/// @brief applier thread class. run only the tailing code
struct TailingApplierThread final : public ApplierThread {
  TailingApplierThread(application_features::ApplicationServer& server,
                       ReplicationApplier* applier, std::shared_ptr<TailingSyncer>&& syncer)
      : ApplierThread(server, applier, std::move(syncer)) {}

  ~TailingApplierThread() { shutdown(); }

  Result runApplier() override {
    TRI_ASSERT(dynamic_cast<TailingSyncer*>(_syncer.get()) != nullptr);
    return static_cast<TailingSyncer*>(_syncer.get())->run();
  }
};

ReplicationApplier::ReplicationApplier(ReplicationApplierConfiguration const& configuration,
                                       std::string&& databaseName)
    : _configuration(configuration), _databaseName(std::move(databaseName)) {
  setProgress(std::string("applier initially created for ") + _databaseName);
}

/// @brief test if the replication applier is running
bool ReplicationApplier::isActive() const {
  READ_LOCKER_EVENTUAL(readLocker, _statusLock);
  return _state.isActive();
}

/// @brief test if the repication applier is performing initial sync
bool ReplicationApplier::isInitializing() const {
  READ_LOCKER_EVENTUAL(readLocker, _statusLock);
  return _state.isInitializing();
}

/// @brief test if the replication applier is shutting down
bool ReplicationApplier::isShuttingDown() const {
  READ_LOCKER_EVENTUAL(readLocker, _statusLock);
  return _state.isShuttingDown();
}

/// @brief block the replication applier from starting
Result ReplicationApplier::preventStart() {
  WRITE_LOCKER_EVENTUAL(writeLocker, _statusLock);

  if (_state.isTailing()) {
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

/// @brief set the applier state to tailing
void ReplicationApplier::markThreadTailing() {
  WRITE_LOCKER_EVENTUAL(writeLocker, _statusLock);
  _state._phase = ReplicationApplierState::ActivityPhase::TAILING;
  setProgressNoLock("applier started tailing");

  LOG_TOPIC("e00c1", INFO, Logger::REPLICATION)
      << "started tailing in replication applier for " << _databaseName;
}

void ReplicationApplier::markThreadStopped() {
  WRITE_LOCKER_EVENTUAL(writeLocker, _statusLock);
  _state._phase = ReplicationApplierState::ActivityPhase::INACTIVE;
  setProgressNoLock("applier shut down");

  LOG_TOPIC("21c52", INFO, Logger::REPLICATION) << "stopped replication applier for " << _databaseName;
}

/// Perform some common ops for startReplication / startTailing
void ReplicationApplier::doStart(std::function<void()>&& cb,
                                 ReplicationApplierState::ActivityPhase activity) {
  WRITE_LOCKER_EVENTUAL(writeLocker, _statusLock);

  if (_state._preventStart) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_LOCKED,
        StringUtils::concatT("cannot start replication applier for ", _databaseName,
                             ": ", TRI_errno_string(TRI_ERROR_LOCKED)));
  }

  if (_state.isActive()) {
    // already started
    return;
  }

  while (_state.isShuttingDown()) {
    // another instance is still around
    writeLocker.unlock();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    writeLocker.lock();
  }

  TRI_ASSERT(!_state.isTailing() && !_state.isShuttingDown());

  if (_configuration._endpoint.empty()) {
    Result r(TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION,
             "no endpoint configured");
    setErrorNoLock(r);
    THROW_ARANGO_EXCEPTION(r);
  }

  if (!isGlobal() && _configuration._database.empty()) {
    Result r(TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION,
             "no database configured");
    setErrorNoLock(r);
    THROW_ARANGO_EXCEPTION(r);
  }

  {  // Debug output
    VPackBuilder b;
    b.openObject();
    _configuration.toVelocyPack(b, false, false);
    b.close();

    LOG_TOPIC("63158", DEBUG, Logger::REPLICATION)
        << "starting applier with configuration " << b.slice().toJson();
  }

  // reset error
  _state._lastError.reset();

  {
    // steal thread
    std::unique_ptr<Thread> t = std::move(_thread);
    TRI_ASSERT(_thread == nullptr);
    // now _thread is empty
    // and release the write lock so when "thread" goes
    // out of scope, it actually can call the thread
    // deleter without holding the write lock (which would
    // deadlock)
    writeLocker.unlock();
  }

  if (_configuration._server.isStopping()) {
    // dont build a new applier if we shutting down
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }

  // reacquire the lock
  writeLocker.lockEventual(); 
 
  // build a new instance in _thread 
  cb();

  TRI_ASSERT(_thread != nullptr);

  if (!_thread->start()) {
    _thread.reset();
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "could not start ApplyThread");
  }

  while (!_thread->hasStarted()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  TRI_ASSERT(!_state.isActive() && !_state.isShuttingDown());
  _state._phase = activity;
}

/// @brief perform a complete replication dump and then tail continiously
void ReplicationApplier::startReplication() {
  if (!applies()) {
    return;
  }

  doStart(
      [&]() {
        std::shared_ptr<InitialSyncer> syncer = buildInitialSyncer();
        _thread.reset(new FullApplierThread(_configuration._server, this, std::move(syncer)));
      },
      ReplicationApplierState::ActivityPhase::INITIAL);
}

/// @brief start the replication applier
void ReplicationApplier::startTailing(TRI_voc_tick_t initialTick, bool useTick) {
  if (!applies()) {
    return;
  }
  doStart(
      [&]() {
        LOG_TOPIC("9917a", DEBUG, Logger::REPLICATION)
            << "requesting replication applier start for " << _databaseName
            << ". initialTick: " << initialTick << ", useTick: " << useTick;
        std::shared_ptr<TailingSyncer> syncer =
            buildTailingSyncer(initialTick, useTick);
        _thread.reset(new TailingApplierThread(_configuration._server, this,
                                               std::move(syncer)));
      },
      ReplicationApplierState::ActivityPhase::TAILING);

  if (useTick) {
    LOG_TOPIC("a9913", INFO, Logger::REPLICATION)
        << "started replication applier for " << _databaseName << ", endpoint '"
        << _configuration._endpoint << "' from tick " << initialTick;
  } else {
    LOG_TOPIC("b681e", INFO, Logger::REPLICATION)
        << "re-started replication applier for " << _databaseName
        << ", endpoint '" << _configuration._endpoint << "' from previous state";
  }
}

/// @brief stop the replication applier
void ReplicationApplier::stop(Result const& r) { doStop(r, false); }

void ReplicationApplier::stop() { doStop(Result(), false); }

/// @brief stop the replication applier and join the apply thread
void ReplicationApplier::stopAndJoin() { doStop(Result(), true); }

/// @brief sleeps for the specific number of microseconds if the
/// applier is still active, and returns true. if the applier is not
/// active anymore, returns false
bool ReplicationApplier::sleepIfStillActive(uint64_t sleepTime) {
  while (sleepTime > 0) {
    if (!isActive()) {
      // already terminated
      return false;
    }

    // now sleep
    uint64_t sleepChunk = 250 * 1000;
    if (sleepChunk > sleepTime) {
      sleepChunk = sleepTime;
    }
    std::this_thread::sleep_for(std::chrono::microseconds(sleepChunk));
    sleepTime -= sleepChunk;
  }

  return isActive();
}

void ReplicationApplier::removeState() {
  if (!applies()) {
    return;
  }

  std::string const filename = getStateFilename();
  if (filename.empty()) {
    // will happen during testing and for coordinator engine
    return;
  }

  WRITE_LOCKER_EVENTUAL(writeLocker, _statusLock);
  _state.reset(false);

  if (TRI_ExistsFile(filename.c_str())) {
    LOG_TOPIC("87a61", TRACE, Logger::REPLICATION) << "removing replication state file '"
                                          << filename << "' for " << _databaseName;
    auto res = TRI_UnlinkFile(filename.c_str());

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          res, std::string("unable to remove replication state file '") +
                   filename + "'");
    }
  }
}

Result ReplicationApplier::resetState(bool reducedSet) {
  // make sure the both vars below match your needs
  static const bool resetPhase = false;
  static const bool doSync = false;

  if (!applies()) {
    return Result{};
  }
  std::string const filename = getStateFilename();

  WRITE_LOCKER_EVENTUAL(writeLocker, _statusLock);
  _state.reset(resetPhase, reducedSet);

  if (!filename.empty() && TRI_ExistsFile(filename.c_str())) {
    LOG_TOPIC("2914f", TRACE, Logger::REPLICATION) << "removing replication state file '"
                                          << filename << "' for " << _databaseName;
    auto res = TRI_UnlinkFile(filename.c_str());

    if (res != TRI_ERROR_NO_ERROR) {
      return Result{res, std::string("unable to remove replication state file '") + filename + "'"};
    }
  }

  LOG_TOPIC("87584", DEBUG, Logger::REPLICATION)
    << "stopped replication applier for database '" << _databaseName
    << "' with lastProcessedContinuousTick: " << _state._lastProcessedContinuousTick
    << ", lastAppliedContinuousTick: " << _state._lastAppliedContinuousTick
    << ", safeResumeTick: " << _state._safeResumeTick;

  return persistStateResult(doSync);
}

void ReplicationApplier::reconfigure(ReplicationApplierConfiguration const& configuration) {
  if (!applies()) {
    return;
  }

  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  if (configuration._endpoint.empty()) {
    // no endpoint
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION,
                                   "no endpoint configured");
  }

  WRITE_LOCKER_EVENTUAL(writeLocker, _statusLock);

  if (_state.isActive()) {
    // cannot change the configuration while the replication is still running
    THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_RUNNING);
  }

  _configuration = configuration;
  storeConfiguration(true);
}

/// @brief load the applier state from persistent storage
bool ReplicationApplier::loadState() {
  WRITE_LOCKER_EVENTUAL(readLocker, _statusLock);
  return loadStateNoLock();
}

/// @brief load the applier state from persistent storage
/// must currently be called while holding the write-lock
/// returns whether a previous state was found
bool ReplicationApplier::loadStateNoLock() {
  if (!applies()) {
    // unsupported
    return false;
  }

  std::string const filename = getStateFilename();
  if (filename.empty()) {
    // will happen during testing and for coordinator engine
    return false;
  }

  LOG_TOPIC("d946f", TRACE, Logger::REPLICATION)
      << "looking for replication state file '" << filename << "' for " << _databaseName;

  if (!TRI_ExistsFile(filename.c_str())) {
    // no existing state found
    return false;
  }

  LOG_TOPIC("3e515", DEBUG, Logger::REPLICATION) << "replication state file '" << filename
                                        << "' found for " << _databaseName;

  VPackBuilder builder;
  try {
    builder = basics::VelocyPackHelper::velocyPackFromFile(filename);
  } catch (...) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_REPLICATION_INVALID_APPLIER_STATE,
        std::string("cannot read replication applier state from file '") +
            filename + "'");
  }

  VPackSlice const slice = builder.slice();
  if (!slice.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_REPLICATION_INVALID_APPLIER_STATE,
        std::string("invalid replication applier state found in file '") +
            filename + "'");
  }

  _state.reset(false);

  // read the server id
  VPackSlice const serverId = slice.get("serverId");
  if (!serverId.isString()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_INVALID_APPLIER_STATE);
  }
  _state._serverId =
      ServerId(arangodb::basics::StringUtils::uint64(serverId.copyString()));

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

  std::string const filename = getStateFilename();
  if (filename.empty()) {
    // will happen during testing and for coordinator engine
    return;
  }

  VPackBuilder builder;
  _state.toVelocyPack(builder, false);

  LOG_TOPIC("8771f", TRACE, Logger::REPLICATION)
      << "saving replication applier state to file '" << filename << "' for "
      << _databaseName;

  if (!basics::VelocyPackHelper::velocyPackToFile(filename, builder.slice(), doSync)) {
    THROW_ARANGO_EXCEPTION(TRI_errno());
  }
}

Result ReplicationApplier::persistStateResult(bool doSync) {
  LOG_TOPIC("fa5ea", TRACE, Logger::REPLICATION)
    << "saving replication applier state. last applied continuous tick: "
    << this->_state._lastAppliedContinuousTick
    << ", safe resume tick: " << this->_state._safeResumeTick;

  Result rv{};

  try {
    persistState(doSync);
  } catch (basics::Exception const& ex) {
    std::string errorMsg = std::string("unable to save replication applier state: ") + ex.what();
    LOG_TOPIC("a98dc", WARN, Logger::REPLICATION) << errorMsg;
    rv.reset(ex.code(), errorMsg);
  } catch (std::exception const& ex) {
    std::string errorMsg = std::string("unable to save replication applier state: ") + ex.what();
    LOG_TOPIC("0d891", WARN, Logger::REPLICATION) << errorMsg;
    rv.reset(TRI_ERROR_INTERNAL, errorMsg);
  } catch (...) {
    std::string errorMsg = std::string("caught unknown exception while saving applier state");
    LOG_TOPIC("2f0c1", WARN, Logger::REPLICATION) << errorMsg;
    rv.reset(TRI_ERROR_INTERNAL, errorMsg);
  }

  return rv;
}

/// @brief store the current applier state in the passed vpack builder
void ReplicationApplier::toVelocyPack(arangodb::velocypack::Builder& result) const {
  TRI_ASSERT(!result.isClosed());

  ReplicationApplierConfiguration configuration(_configuration._server);
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
  result.add("serverId", VPackValue(std::to_string(ServerIdFeature::getId().id())));
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

/// @brief return last persisted tick
TRI_voc_tick_t ReplicationApplier::lastTick() const {
  READ_LOCKER_EVENTUAL(readLocker, _statusLock);
  return std::max(_state._lastAppliedContinuousTick, _state._lastProcessedContinuousTick);
}

/// @brief register an applier error
void ReplicationApplier::setError(arangodb::Result const& r) {
  WRITE_LOCKER_EVENTUAL(writeLocker, _statusLock);
  setErrorNoLock(r);
}

Result ReplicationApplier::lastError() const {
  READ_LOCKER_EVENTUAL(writeLocker, _statusLock);
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
    LOG_TOPIC("ab64e", ERR, Logger::REPLICATION) << "replication applier error for " << _databaseName
                                        << ": " << rr.errorMessage();
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

  if (!_state.isActive() || _state.isShuttingDown()) {
    // not active or somebody else is shutting us down
    return;
  }

  LOG_TOPIC("73c1a", DEBUG, Logger::REPLICATION)
      << "requesting replication applier stop for " << _databaseName;

  _state._phase = ReplicationApplierState::ActivityPhase::SHUTDOWN;
  _state.setError(r.errorNumber(), r.errorMessage());

  if (_thread != nullptr) {
    static_cast<ApplierThread*>(_thread.get())->setAborted(true);
  }

  if (joinThread) {
    auto start = std::chrono::steady_clock::now();
    while (_state.isShuttingDown()) {
      writeLocker.unlock();
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      if (std::chrono::steady_clock::now() - start > std::chrono::minutes(3)) {
        LOG_TOPIC("0b9c8", ERR, Logger::REPLICATION)
            << "replication applier is not stopping";
        TRI_ASSERT(false);
        start = std::chrono::steady_clock::now();
      }
      writeLocker.lock();
    }

    TRI_ASSERT(!_state.isActive() && !_state.isShuttingDown());
    // wipe aborted flag. this will be passed on to the syncer
    static_cast<ApplierThread*>(_thread.get())->setAborted(false);

    // steal thread
    std::unique_ptr<Thread> t = std::move(_thread);
    TRI_ASSERT(_thread == nullptr);
    // now _thread is empty
    // and release the write lock so when "thread" goes
    // out of scope, it actually can call the thread
    // deleter without holding the write lock (which would
    // deadlock)
    writeLocker.unlock();
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

    dst = static_cast<TRI_voc_tick_t>(
        arangodb::basics::StringUtils::uint64(tick.copyString()));
  }
}
