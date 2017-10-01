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

#include "DatabaseReplicationApplier.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "Replication/DatabaseTailingSyncer.h"
#include "RestServer/ServerIdFeature.h"
#include "Rest/Version.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

/// @brief applier thread class
class ApplyThread : public Thread {
 public:
  explicit ApplyThread(std::unique_ptr<DatabaseTailingSyncer> syncer)
      : Thread("ReplicationApplier"), _syncer(std::move(syncer)) {}

  ~ApplyThread() { shutdown(); }

 public:
  void run() {
    TRI_ASSERT(_syncer);

    try {
      _syncer->run();
    } catch (std::exception const& ex) {
      LOG_TOPIC(WARN, Logger::REPLICATION) << "caught exception in ApplyThread: " << ex.what();
    } catch (...) {
      LOG_TOPIC(WARN, Logger::REPLICATION) << "caught unknown exception in ApplyThread";
    }
  }

 private:
  std::unique_ptr<DatabaseTailingSyncer> _syncer;
};


namespace {
/// @brief read a tick value from a VelocyPack struct
static void readTick(VPackSlice const& slice, char const* attributeName,
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
}

/// @brief replication applier for a single database, without configuration
DatabaseReplicationApplier::DatabaseReplicationApplier(TRI_vocbase_t* vocbase)
    : DatabaseReplicationApplier(ReplicationApplierConfiguration(), vocbase) {}

/// @brief replication applier for a single database, with configuration
DatabaseReplicationApplier::DatabaseReplicationApplier(ReplicationApplierConfiguration const& configuration,
                                                       TRI_vocbase_t* vocbase)
    : ReplicationApplier(configuration, std::string("database '") + vocbase->name() + "'"), 
      _vocbase(vocbase) {}

DatabaseReplicationApplier::~DatabaseReplicationApplier() {
  stop(true, false);
}

/// @brief configure the replication applier
void DatabaseReplicationApplier::reconfigure(ReplicationApplierConfiguration const& configuration) {
  if (_vocbase->type() == TRI_VOCBASE_TYPE_COORDINATOR) {
    // unsupported
    return;
  }
  
  if (configuration._database.empty()) {
    // no database
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION, "no database configured");
  }
 
  ReplicationApplier::reconfigure(configuration); 
}

/// @brief remove the replication application state file
void DatabaseReplicationApplier::removeState() {
  if (_vocbase->type() == TRI_VOCBASE_TYPE_COORDINATOR) {
    // unsupported
    return; 
  }

  ReplicationApplier::removeState();
}

/// @brief load the applier state from persistent storage
/// must currently be called while holding the write-lock
/// returns whether a previous state was found
bool DatabaseReplicationApplier::loadState() {
  if (_vocbase->type() == TRI_VOCBASE_TYPE_COORDINATOR) {
    // unsupported
    return false;
  }

  std::string const filename = getStateFilename();
  LOG_TOPIC(TRACE, Logger::REPLICATION)
      << "looking for replication state file '" << filename << "'";

  if (!TRI_ExistsFile(filename.c_str())) {
    // no existing state found
    return false;
  }

  LOG_TOPIC(DEBUG, Logger::REPLICATION) << "replication state file '" << filename << "' found";

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

  _state.reset();

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
void DatabaseReplicationApplier::persistState(bool doSync) {
  if (_vocbase->type() == TRI_VOCBASE_TYPE_COORDINATOR) {
    // unsupported
    return;
  }

  VPackBuilder builder;
  _state.toVelocyPack(builder, false);

  std::string const filename = getStateFilename();
  LOG_TOPIC(TRACE, Logger::REPLICATION)
      << "saving replication applier state to file '" << filename << "'";

  if (!basics::VelocyPackHelper::velocyPackToFile(filename, builder.slice(), doSync)) {
    THROW_ARANGO_EXCEPTION(TRI_errno());
  }
}

/// @brief return the current configuration
ReplicationApplierConfiguration DatabaseReplicationApplier::configuration() const {
  READ_LOCKER(readLocker, _statusLock);
  return _configuration;
}

/// @brief store the current applier state in the passed vpack builder 
/// expects builder to be in an open Object state
void DatabaseReplicationApplier::toVelocyPack(arangodb::velocypack::Builder& result) const {
  TRI_ASSERT(!result.isClosed());

  ReplicationApplierConfiguration configuration;
  ReplicationApplierState state;

  {
    // copy current config and state under the lock
    READ_LOCKER(readLocker, _statusLock);
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

void DatabaseReplicationApplier::start(TRI_voc_tick_t initialTick, bool useTick, TRI_voc_tick_t barrierId) {
  if (_vocbase->type() == TRI_VOCBASE_TYPE_COORDINATOR) {
    // unsupported
    return;
  }

  LOG_TOPIC(DEBUG, Logger::REPLICATION)
      << "requesting replication applier start. initialTick: " << initialTick
      << ", useTick: " << useTick;

  // wait until previous applier thread is shut down
  while (!wait(10 * 1000));

  WRITE_LOCKER(writeLocker, _statusLock);
  
  if (_state._preventStart) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_LOCKED);
  }

  if (_state._active) {
    // already started
    return;
  }
  
  if (_configuration._endpoint.empty() || _configuration._database.empty()) {
    setErrorNoLock(TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION, "no endpoint configured");
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION, "no endpoint configured");
  }

  auto syncer = std::make_unique<arangodb::DatabaseTailingSyncer>(_vocbase, &_configuration,
                                                                  initialTick, useTick, barrierId);

  // reset error
  _state._lastError.reset();

  setTermination(false);
  _state._active = true;
 
  _thread.reset(new ApplyThread(std::move(syncer))); 
  if (!_thread->start()) {
    _thread.reset();
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "could not start ApplyThread");
  }

  while (!_thread->hasStarted()) {
    usleep(20000);
  }

  if (useTick) {
    LOG_TOPIC(INFO, Logger::REPLICATION)
        << "started replication applier for database '" << _vocbase->name()
        << "', endpoint '" << _configuration._endpoint << "' from tick "
        << initialTick;
  } else {
    LOG_TOPIC(INFO, Logger::REPLICATION)
        << "re-started replication applier for database '"
        << _vocbase->name() << "', endpoint '" << _configuration._endpoint
        << "'";
  }
}
  
/// @brief stop the replication applier
void DatabaseReplicationApplier::stop(bool resetError, bool joinThread) {
  if (_vocbase->type() == TRI_VOCBASE_TYPE_COORDINATOR) {
    // unsupported
    return; 
  }

  {
    WRITE_LOCKER(writeLocker, _statusLock);

    // always stop initial synchronization
    _state._stopInitialSynchronization = true;

    if (!_state._active) {
      // not active
      return;
    }

    _state._active = false;

    setTermination(true);
    setProgressNoLock("applier shut down");

    if (resetError) {
      _state.clearError();
    }
  }

  // join the thread without holding the status lock 
  // (otherwise it would probably not join)
  if (joinThread) {
    TRI_ASSERT(_thread);
//    _thread->join();
    _thread.reset();
  }

  setTermination(false);

  LOG_TOPIC(INFO, Logger::REPLICATION)
      << "stopped replication applier for database '" << _vocbase->name() << "'";
}

/// @brief shut down the replication applier
void DatabaseReplicationApplier::shutdown() {
  if (_vocbase->type() == TRI_VOCBASE_TYPE_COORDINATOR) {
    // unsupported
    return;
  }

  ReplicationApplier::shutdown();
}

/// @brief stop the applier and "forget" everything
void DatabaseReplicationApplier::forget() {
  if (_vocbase->type() == TRI_VOCBASE_TYPE_COORDINATOR) {
    // unsupported
    return;
  }

  stop(true, true);

  removeState();

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  engine->removeReplicationApplierConfiguration(_vocbase);
  _configuration.reset();
}

/// @brief factory function for creating a database-specific replication applier
DatabaseReplicationApplier* DatabaseReplicationApplier::create(TRI_vocbase_t* vocbase) {
  std::unique_ptr<DatabaseReplicationApplier> applier;

  if (vocbase->type() == TRI_VOCBASE_TYPE_NORMAL) {
    applier = std::make_unique<DatabaseReplicationApplier>(DatabaseReplicationApplier::loadConfiguration(vocbase), vocbase);
    applier->loadState();
  } else {
    applier = std::make_unique<DatabaseReplicationApplier>(vocbase);
  }

  return applier.release();
}

/// @brief load a persisted configuration for the applier
ReplicationApplierConfiguration DatabaseReplicationApplier::loadConfiguration(TRI_vocbase_t* vocbase) {
  ReplicationApplierConfiguration configuration;
  
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  int res = TRI_ERROR_INTERNAL;
  VPackBuilder builder = engine->getReplicationApplierConfiguration(vocbase, res);

  if (res == TRI_ERROR_FILE_NOT_FOUND) {
    // file not found
    TRI_ASSERT(builder.isEmpty());
    return configuration;
  }

  TRI_ASSERT(!builder.isEmpty());

  VPackSlice const slice = builder.slice();
  
  // read the database name
  VPackSlice value = slice.get("database");
  if (!value.isString()) {
    configuration._database = vocbase->name();
  } else {
    configuration._database = value.copyString();
  }

  // read username / password
  value = slice.get("username");
  bool hasUsernamePassword = false;
  if (value.isString()) {
    hasUsernamePassword = true;
    configuration._username = value.copyString();
  }

  value = slice.get("password");
  if (value.isString()) {
    hasUsernamePassword = true;
    configuration._password = value.copyString();
  }

  if (!hasUsernamePassword) {
    value = slice.get("jwt");
    if (value.isString()) {
      configuration._jwt = value.copyString();
    }
  }

  value = slice.get("requestTimeout");
  if (value.isNumber()) {
    configuration._requestTimeout = value.getNumber<double>();
  }

  value = slice.get("connectTimeout");
  if (value.isNumber()) {
    configuration._connectTimeout = value.getNumber<double>();
  }

  value = slice.get("maxConnectRetries");
  if (value.isNumber()) {
    configuration._maxConnectRetries = value.getNumber<int64_t>();
  }
  
  value = slice.get("lockTimeoutRetries");
  if (value.isNumber()) {
    configuration._lockTimeoutRetries = value.getNumber<int64_t>();
  }

  value = slice.get("sslProtocol");
  if (value.isNumber()) {
    configuration._sslProtocol = value.getNumber<uint32_t>();
  }

  value = slice.get("chunkSize");
  if (value.isNumber()) {
    configuration._chunkSize = value.getNumber<uint64_t>();
  }

  value = slice.get("autoStart");
  if (value.isBoolean()) {
    configuration._autoStart = value.getBoolean();
  }

  value = slice.get("adaptivePolling");
  if (value.isBoolean()) {
    configuration._adaptivePolling = value.getBoolean();
  }

  value = slice.get("autoResync");
  if (value.isBoolean()) {
    configuration._autoResync = value.getBoolean();
  }

  value = slice.get("includeSystem");
  if (value.isBoolean()) {
    configuration._includeSystem = value.getBoolean();
  }

  value = slice.get("requireFromPresent");
  if (value.isBoolean()) {
    configuration._requireFromPresent = value.getBoolean();
  }

  value = slice.get("verbose");
  if (value.isBoolean()) {
    configuration._verbose = value.getBoolean();
  }

  value = slice.get("incremental");
  if (value.isBoolean()) {
    configuration._incremental = value.getBoolean();
  }

  value = slice.get("useCollectionId");
  if (value.isBoolean()) {
    configuration._useCollectionId = value.getBoolean();
  }

  value = slice.get("ignoreErrors");
  if (value.isNumber()) {
    configuration._ignoreErrors = value.getNumber<uint64_t>();
  } else if (value.isBoolean()) {
    if (value.getBoolean()) {
      configuration._ignoreErrors = UINT64_MAX;
    } else {
      configuration._ignoreErrors = 0;
    }
  }

  value = slice.get("restrictType");
  if (value.isString()) {
    configuration._restrictType = value.copyString();
  }

  value = slice.get("restrictCollections");
  if (value.isArray()) {
    configuration._restrictCollections.clear();

    for (auto const& it : VPackArrayIterator(value)) {
      if (it.isString()) {
        configuration._restrictCollections.emplace(it.copyString(), true);
      }
    }
  }

  value = slice.get("connectionRetryWaitTime");
  if (value.isNumber()) {
    configuration._connectionRetryWaitTime = static_cast<uint64_t>(value.getNumber<double>() * 1000.0 * 1000.0);
  }

  value = slice.get("initialSyncMaxWaitTime");
  if (value.isNumber()) {
    configuration._initialSyncMaxWaitTime = static_cast<uint64_t>(value.getNumber<double>() * 1000.0 * 1000.0);
  }

  value = slice.get("idleMinWaitTime");
  if (value.isNumber()) {
    configuration._idleMinWaitTime = static_cast<uint64_t>(value.getNumber<double>() * 1000.0 * 1000.0);
  }

  value = slice.get("idleMaxWaitTime");
  if (value.isNumber()) {
    configuration._idleMaxWaitTime = static_cast<uint64_t>(value.getNumber<double>() * 1000.0 * 1000.0);
  }

  value = slice.get("autoResyncRetries");
  if (value.isNumber()) {
    configuration._autoResyncRetries = value.getNumber<uint64_t>();
  }

  // read the endpoint
  value = slice.get("endpoint");
  if (!value.isString()) {
    // we haven't found an endpoint. now don't let the start fail but continue
    configuration._autoStart = false;
  } else {
    configuration._endpoint = value.copyString();
  }

  return configuration;
}

/// @brief store the configuration for the applier
void DatabaseReplicationApplier::storeConfiguration(bool doSync) {
  if (_vocbase->type() == TRI_VOCBASE_TYPE_COORDINATOR) {
    // unsupported
    return;
  }

  VPackBuilder builder;
  builder.openObject();
  _configuration.toVelocyPack(builder, true);
  builder.close();

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  int res = engine->saveReplicationApplierConfiguration(_vocbase, builder.slice(), doSync);
  
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
}
  
std::string DatabaseReplicationApplier::getStateFilename() const {
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  return arangodb::basics::FileUtils::buildFilename(engine->databasePath(_vocbase), "REPLICATION-APPLIER-STATE");
}
