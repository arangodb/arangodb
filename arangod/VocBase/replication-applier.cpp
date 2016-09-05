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

#include "replication-applier.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/ReadLocker.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StringUtils.h"
#include "Basics/tri-strings.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/conversions.h"
#include "Basics/files.h"
#include "Basics/tri-strings.h"
#include "Logger/Logger.h"
#include "Replication/ContinuousSyncer.h"
#include "Rest/Version.h"
#include "RestServer/ServerIdFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "VocBase/transaction.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;

////////////////////////////////////////////////////////////////////////////////
/// @brief read a tick value from a VelocyPack struct
////////////////////////////////////////////////////////////////////////////////

static int ReadTick(VPackSlice const& slice, char const* attributeName,
                    TRI_voc_tick_t* dst, bool allowNull) {
  TRI_ASSERT(slice.isObject());

  VPackSlice const tick = slice.get(attributeName);

  if ((tick.isNull() || tick.isNone()) && allowNull) {
    *dst = 0;
    return TRI_ERROR_NO_ERROR;
  }

  if (!tick.isString()) {
    return TRI_ERROR_REPLICATION_INVALID_APPLIER_STATE;
  }

  *dst = static_cast<TRI_voc_tick_t>(StringUtils::uint64(tick.copyString()));

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the filename of the replication applier configuration file
////////////////////////////////////////////////////////////////////////////////

static std::string GetConfigurationFilename(TRI_vocbase_t* vocbase) {
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  return arangodb::basics::FileUtils::buildFilename(engine->databasePath(vocbase), "REPLICATION-APPLIER-CONFIG");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief load the replication application configuration from a file
/// this function must be called under the statusLock
////////////////////////////////////////////////////////////////////////////////

static int LoadConfiguration(TRI_vocbase_t* vocbase,
                             TRI_replication_applier_configuration_t* config) {
  // Clear
  std::string const filename = GetConfigurationFilename(vocbase);

  if (!TRI_ExistsFile(filename.c_str())) {
    return TRI_ERROR_FILE_NOT_FOUND;
  }

  std::shared_ptr<VPackBuilder> builder;
  try {
    builder = VelocyPackHelper::velocyPackFromFile(filename);
  }
  catch (...) {
    LOG_TOPIC(ERR, Logger::REPLICATION)
        << "unable to read replication applier configuration from file '"
        << filename << "'";
    return TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION;
  }

  VPackSlice const slice = builder->slice();

  if (!slice.isObject()) {
    LOG_TOPIC(ERR, Logger::REPLICATION)
        << "unable to read replication applier configuration from file '"
        << filename << "'";
    return TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION;
  }

  // read the database name
  VPackSlice value = slice.get("database");

  if (!value.isString()) {
    config->_database = vocbase->name();
  } else {
    config->_database = value.copyString();
  }

  // read username / password
  value = slice.get("username");

  if (value.isString()) {
    config->_username = value.copyString();
  }

  value = slice.get("password");

  if (value.isString()) {
    config->_password = value.copyString();
  }

  value = slice.get("requestTimeout");

  if (value.isNumber()) {
    config->_requestTimeout = value.getNumber<double>();
  }

  value = slice.get("connectTimeout");

  if (value.isNumber()) {
    config->_connectTimeout = value.getNumber<double>();
  }

  value = slice.get("maxConnectRetries");

  if (value.isNumber()) {
    config->_maxConnectRetries = value.getNumber<int64_t>();
  }

  value = slice.get("sslProtocol");

  if (value.isNumber()) {
    config->_sslProtocol = value.getNumber<uint32_t>();
  }

  value = slice.get("chunkSize");

  if (value.isNumber()) {
    config->_chunkSize = value.getNumber<uint64_t>();
  }

  value = slice.get("autoStart");

  if (value.isBoolean()) {
    config->_autoStart = value.getBoolean();
  }

  value = slice.get("adaptivePolling");

  if (value.isBoolean()) {
    config->_adaptivePolling = value.getBoolean();
  }

  value = slice.get("autoResync");

  if (value.isBoolean()) {
    config->_autoResync = value.getBoolean();
  }

  value = slice.get("includeSystem");

  if (value.isBoolean()) {
    config->_includeSystem = value.getBoolean();
  }

  value = slice.get("requireFromPresent");

  if (value.isBoolean()) {
    config->_requireFromPresent = value.getBoolean();
  }

  value = slice.get("verbose");

  if (value.isBoolean()) {
    config->_verbose = value.getBoolean();
  }

  value = slice.get("incremental");

  if (value.isBoolean()) {
    config->_incremental = value.getBoolean();
  }

  value = slice.get("useCollectionId");

  if (value.isBoolean()) {
    config->_useCollectionId = value.getBoolean();
  }

  value = slice.get("ignoreErrors");

  if (value.isNumber()) {
    config->_ignoreErrors = value.getNumber<uint64_t>();
  } else if (value.isBoolean()) {
    if (value.getBoolean()) {
      config->_ignoreErrors = UINT64_MAX;
    } else {
      config->_ignoreErrors = 0;
    }
  }

  value = slice.get("restrictType");

  if (value.isString()) {
    config->_restrictType = value.copyString();
  }

  value = slice.get("restrictCollections");

  if (value.isArray()) {
    config->_restrictCollections.clear();

    for (auto const& it : VPackArrayIterator(value)) {
      if (it.isString()) {
        config->_restrictCollections.emplace(it.copyString(), true);
      }
    }
  }

  value = slice.get("connectionRetryWaitTime");

  if (value.isNumber()) {
    config->_connectionRetryWaitTime = static_cast<uint64_t>(value.getNumber<double>() * 1000.0 * 1000.0);
  }

  value = slice.get("initialSyncMaxWaitTime");

  if (value.isNumber()) {
    config->_initialSyncMaxWaitTime = static_cast<uint64_t>(value.getNumber<double>() * 1000.0 * 1000.0);
  }

  value = slice.get("idleMinWaitTime");

  if (value.isNumber()) {
    config->_idleMinWaitTime = static_cast<uint64_t>(value.getNumber<double>() * 1000.0 * 1000.0);
  }

  value = slice.get("idleMaxWaitTime");

  if (value.isNumber()) {
    config->_idleMaxWaitTime = static_cast<uint64_t>(value.getNumber<double>() * 1000.0 * 1000.0);
  }

  value = slice.get("autoResyncRetries");

  if (value.isNumber()) {
    config->_autoResyncRetries = value.getNumber<uint64_t>();
  }

  // read the endpoint
  value = slice.get("endpoint");

  if (!value.isString()) {
    // we haven't found an endpoint. now don't let the start fail but continue
    config->_autoStart = false;
  } else {
    config->_endpoint = value.copyString();
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the filename of the replication applier state file
////////////////////////////////////////////////////////////////////////////////

static std::string GetStateFilename(TRI_vocbase_t* vocbase) {
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  return arangodb::basics::FileUtils::buildFilename(engine->databasePath(vocbase), "REPLICATION-APPLIER-STATE");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a VPack representation of the replication applier state
////////////////////////////////////////////////////////////////////////////////

static VPackBuilder VPackApplyState(TRI_replication_applier_state_t const* state) {
  VPackBuilder builder;
  builder.openObject();

  builder.add("serverId", VPackValue(std::to_string(state->_serverId))); 
  builder.add("lastProcessedContinuousTick", VPackValue(std::to_string(state->_lastProcessedContinuousTick))); 
  builder.add("lastAppliedContinuousTick", VPackValue(std::to_string(state->_lastAppliedContinuousTick))); 
  builder.add("safeResumeTick", VPackValue(std::to_string(state->_safeResumeTick)));
  builder.close();

  return builder;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief applier thread main function
////////////////////////////////////////////////////////////////////////////////

static void ApplyThread(void* data) {
  auto syncer = static_cast<arangodb::ContinuousSyncer*>(data);

  try {
    syncer->run();
  } catch (...) {
  }
  delete syncer;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a VPack representation of an applier state
////////////////////////////////////////////////////////////////////////////////

static void VPackState(VPackBuilder& builder,
                       TRI_replication_applier_state_t const* state) {
  builder.openObject();

  // add replication state
  builder.add("running", VPackValue(state->_active));

  // lastAppliedContinuousTick
  if (state->_lastAppliedContinuousTick > 0) {
    builder.add("lastAppliedContinuousTick", VPackValue(std::to_string(state->_lastAppliedContinuousTick)));
  } else {
    builder.add("lastAppliedContinuousTick", VPackValue(VPackValueType::Null));
  }

  // lastProcessedContinuousTick
  if (state->_lastProcessedContinuousTick > 0) {
    builder.add("lastProcessedContinuousTick", VPackValue(std::to_string(state->_lastProcessedContinuousTick)));
  } else {
    builder.add("lastProcessedContinuousTick", VPackValue(VPackValueType::Null));
  }

  // lastAvailableContinuousTick
  if (state->_lastAvailableContinuousTick > 0) {
    builder.add("lastAvailableContinuousTick", VPackValue(std::to_string(state->_lastAvailableContinuousTick)));
  } else {
    builder.add("lastAvailableContinuousTick", VPackValue(VPackValueType::Null));
  }

  // safeResumeTick
  if (state->_safeResumeTick > 0) {
    builder.add("safeResumeTick", VPackValue(std::to_string(state->_safeResumeTick)));
  } else {
    builder.add("safeResumeTick", VPackValue(VPackValueType::Null));
  }

  // progress
  builder.add("progress", VPackValue(VPackValueType::Object));
  builder.add("time", VPackValue(state->_progressTime));

  if (state->_progressMsg != nullptr) {
    builder.add("message", VPackValue(state->_progressMsg));
  }

  builder.add("failedConnects", VPackValue(state->_failedConnects));
  builder.close(); // progress

  builder.add("totalRequests", VPackValue(state->_totalRequests));
  builder.add("totalFailedConnects", VPackValue(state->_totalFailedConnects));
  builder.add("totalEvents", VPackValue(state->_totalEvents));
  builder.add("totalOperationsExcluded", VPackValue(state->_skippedOperations));

  // lastError
  builder.add("lastError", VPackValue(VPackValueType::Object));
  builder.add("errorNum", VPackValue(state->_lastError._code));

  if (state->_lastError._code > 0) {
    builder.add("time", VPackValue(state->_lastError._time));
    if (state->_lastError._msg != nullptr) {
      builder.add("errorMessage", VPackValue(state->_lastError._msg));
    }
  }
  builder.close(); // lastError

  char timeString[24];
  TRI_GetTimeStampReplication(timeString, sizeof(timeString) - 1);
  builder.add("time", VPackValue(&timeString[0]));

  builder.close();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a replication applier
////////////////////////////////////////////////////////////////////////////////

TRI_replication_applier_t* TRI_CreateReplicationApplier(TRI_vocbase_t* vocbase) {
  auto applier = std::make_unique<TRI_replication_applier_t>(vocbase);

  TRI_InitStateReplicationApplier(&applier->_state);

  if (vocbase->type() == TRI_VOCBASE_TYPE_NORMAL) {
    int res = LoadConfiguration(vocbase, &applier->_configuration);

    if (res != TRI_ERROR_NO_ERROR && res != TRI_ERROR_FILE_NOT_FOUND) {
      TRI_DestroyStateReplicationApplier(&applier->_state);
      THROW_ARANGO_EXCEPTION(res);
    }

    res = TRI_LoadStateReplicationApplier(vocbase, &applier->_state);

    if (res != TRI_ERROR_NO_ERROR && res != TRI_ERROR_FILE_NOT_FOUND) {
      TRI_DestroyStateReplicationApplier(&applier->_state);
      THROW_ARANGO_EXCEPTION(res);
    }
  }

  applier->setTermination(false);
  applier->setProgress("applier initially created", true);

  return applier.release();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief construct the configuration with default values
////////////////////////////////////////////////////////////////////////////////

TRI_replication_applier_configuration_t::
    TRI_replication_applier_configuration_t() 
    : _endpoint(),
      _database(),
      _username(),
      _password(),
      _requestTimeout(600.0),
      _connectTimeout(10.0),
      _ignoreErrors(0),
      _maxConnectRetries(100),
      _chunkSize(0),
      _connectionRetryWaitTime(15 * 1000 * 1000),
      _idleMinWaitTime(1000 * 1000),
      _idleMaxWaitTime(5 * 500 * 1000),
      _initialSyncMaxWaitTime(300 * 1000 * 1000),
      _autoResyncRetries(2),
      _sslProtocol(0),
      _autoStart(false),
      _adaptivePolling(true),
      _autoResync(false),
      _includeSystem(true),
      _requireFromPresent(false),
      _incremental(false),
      _verbose(false),
      _useCollectionId(true),
      _restrictType(),
      _restrictCollections() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief reset the configuration to defaults
////////////////////////////////////////////////////////////////////////////////

void TRI_replication_applier_configuration_t::reset() {
  TRI_replication_applier_configuration_t empty;
  update(&empty);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a VelocyPack representation
///        Expects builder to be in an open Object state
////////////////////////////////////////////////////////////////////////////////

void TRI_replication_applier_configuration_t::toVelocyPack(
    bool includePassword, VPackBuilder& builder) const {
  if (!_endpoint.empty()) {
    builder.add("endpoint", VPackValue(_endpoint));
  }
  if (!_database.empty()) {
    builder.add("database", VPackValue(_database));
  }
  if (!_username.empty()) {
    builder.add("username", VPackValue(_username));
  }
  if (includePassword) {
    builder.add("password", VPackValue(_password));
  }

  builder.add("requestTimeout", VPackValue(_requestTimeout));
  builder.add("connectTimeout", VPackValue(_connectTimeout));
  builder.add("ignoreErrors", VPackValue(_ignoreErrors));
  builder.add("maxConnectRetries", VPackValue(_maxConnectRetries));
  builder.add("sslProtocol", VPackValue(_sslProtocol));
  builder.add("chunkSize", VPackValue(_chunkSize));
  builder.add("autoStart", VPackValue(_autoStart));
  builder.add("adaptivePolling", VPackValue(_adaptivePolling));
  builder.add("autoResync", VPackValue(_autoResync));
  builder.add("autoResyncRetries", VPackValue(_autoResyncRetries));
  builder.add("includeSystem", VPackValue(_includeSystem));
  builder.add("requireFromPresent", VPackValue(_requireFromPresent));
  builder.add("verbose", VPackValue(_verbose));
  builder.add("incremental", VPackValue(_incremental));
  builder.add("useCollectionId", VPackValue(_useCollectionId));
  builder.add("restrictType", VPackValue(_restrictType));

  builder.add("restrictCollections", VPackValue(VPackValueType::Array));
  for (auto& it : _restrictCollections) {
    builder.add(VPackValue(it.first));
  }
  builder.close();  // restrictCollections

  builder.add("connectionRetryWaitTime",
              VPackValue(static_cast<double>(_connectionRetryWaitTime) /
                         (1000.0 * 1000.0)));
  builder.add("initialSyncMaxWaitTime",
              VPackValue(static_cast<double>(_initialSyncMaxWaitTime) /
                         (1000.0 * 1000.0)));
  builder.add(
      "idleMinWaitTime",
      VPackValue(static_cast<double>(_idleMinWaitTime) / (1000.0 * 1000.0)));
  builder.add(
      "idleMaxWaitTime",
      VPackValue(static_cast<double>(_idleMaxWaitTime) / (1000.0 * 1000.0)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a VelocyPack representation
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<VPackBuilder>
TRI_replication_applier_configuration_t::toVelocyPack(
    bool includePassword) const {
  auto builder = std::make_shared<VPackBuilder>();
  {
    VPackObjectBuilder b(builder.get());
    toVelocyPack(includePassword, *builder);
  }

  return builder;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief configure the replication applier
////////////////////////////////////////////////////////////////////////////////

int TRI_ConfigureReplicationApplier(
    TRI_replication_applier_t* applier,
    TRI_replication_applier_configuration_t const* config) {
  if (applier->_vocbase->type() == TRI_VOCBASE_TYPE_COORDINATOR) {
    return TRI_ERROR_CLUSTER_UNSUPPORTED;
  }

  if (config->_endpoint.empty()) {
    // no endpoint
    return TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION;
  }

  if (config->_database.empty()) {
    // no database
    return TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION;
  }

  WRITE_LOCKER(writeLocker, applier->_statusLock);

  if (applier->_state._active) {
    // cannot change the configuration while the replication is still running
    return TRI_ERROR_REPLICATION_RUNNING;
  }

  int res =
      TRI_SaveConfigurationReplicationApplier(applier->_vocbase, config, true);

  if (res == TRI_ERROR_NO_ERROR) {
    res = LoadConfiguration(applier->_vocbase, &applier->_configuration);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the current replication applier state
////////////////////////////////////////////////////////////////////////////////

int TRI_StateReplicationApplier(TRI_replication_applier_t const* applier,
                                TRI_replication_applier_state_t* state) {
  TRI_InitStateReplicationApplier(state);

  READ_LOCKER(readLocker, applier->_statusLock);

  state->_active = applier->_state._active;
  state->_lastAppliedContinuousTick =
      applier->_state._lastAppliedContinuousTick;
  state->_lastProcessedContinuousTick =
      applier->_state._lastProcessedContinuousTick;
  state->_lastAvailableContinuousTick =
      applier->_state._lastAvailableContinuousTick;
  state->_safeResumeTick = applier->_state._safeResumeTick;
  state->_serverId = applier->_state._serverId;
  state->_lastError._code = applier->_state._lastError._code;
  state->_failedConnects = applier->_state._failedConnects;
  state->_totalRequests = applier->_state._totalRequests;
  state->_totalFailedConnects = applier->_state._totalFailedConnects;
  state->_totalEvents = applier->_state._totalEvents;
  state->_skippedOperations = applier->_state._skippedOperations;
  memcpy(&state->_lastError._time, &applier->_state._lastError._time,
         sizeof(state->_lastError._time));

  if (applier->_state._progressMsg != nullptr) {
    state->_progressMsg =
        TRI_DuplicateString(TRI_CORE_MEM_ZONE, applier->_state._progressMsg);
  } else {
    state->_progressMsg = nullptr;
  }

  memcpy(&state->_progressTime, &applier->_state._progressTime,
         sizeof(state->_progressTime));

  if (applier->_state._lastError._msg != nullptr) {
    state->_lastError._msg =
        TRI_DuplicateString(TRI_CORE_MEM_ZONE, applier->_state._lastError._msg);
  } else {
    state->_lastError._msg = nullptr;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize an applier state struct
////////////////////////////////////////////////////////////////////////////////

void TRI_InitStateReplicationApplier(TRI_replication_applier_state_t* state) {
  memset(state, 0, sizeof(TRI_replication_applier_state_t));

  state->_active = false;

  state->_lastError._code = TRI_ERROR_NO_ERROR;
  state->_lastError._msg = nullptr;
  state->_lastError._time[0] = '\0';
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy an applier state struct
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyStateReplicationApplier(
    TRI_replication_applier_state_t* state) {
  TRI_ASSERT(state != nullptr);

  if (state->_progressMsg != nullptr) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, state->_progressMsg);
  }

  if (state->_lastError._msg != nullptr) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, state->_lastError._msg);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove the replication application state file
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveStateReplicationApplier(TRI_vocbase_t* vocbase) {
  if (vocbase->type() == TRI_VOCBASE_TYPE_COORDINATOR) {
    return TRI_ERROR_CLUSTER_UNSUPPORTED;
  }

  std::string const filename = GetStateFilename(vocbase);

  if (TRI_ExistsFile(filename.c_str())) {
    LOG_TOPIC(TRACE, Logger::REPLICATION) << "removing replication state file '"
                                          << filename << "'";
    return TRI_UnlinkFile(filename.c_str());
  } 
  
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief save the replication application state to a file
////////////////////////////////////////////////////////////////////////////////

int TRI_SaveStateReplicationApplier(
    TRI_vocbase_t* vocbase, TRI_replication_applier_state_t const* state,
    bool doSync) {
  if (vocbase->type() == TRI_VOCBASE_TYPE_COORDINATOR) {
    return TRI_ERROR_CLUSTER_UNSUPPORTED;
  }

  VPackBuilder builder = VPackApplyState(state);

  std::string const filename = GetStateFilename(vocbase);
  LOG_TOPIC(TRACE, Logger::REPLICATION)
      << "saving replication applier state to file '" << filename << "'";

  if (!VelocyPackHelper::velocyPackToFile(filename, builder.slice(), doSync)) {
    return TRI_errno();
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief load the replication application state from a file
////////////////////////////////////////////////////////////////////////////////

int TRI_LoadStateReplicationApplier(TRI_vocbase_t* vocbase,
                                    TRI_replication_applier_state_t* state) {
  if (vocbase->type() == TRI_VOCBASE_TYPE_COORDINATOR) {
    return TRI_ERROR_CLUSTER_UNSUPPORTED;
  }

  TRI_DestroyStateReplicationApplier(state);
  TRI_InitStateReplicationApplier(state);
  std::string const filename = GetStateFilename(vocbase);

  LOG_TOPIC(TRACE, Logger::REPLICATION)
      << "looking for replication state file '" << filename << "'";

  if (!TRI_ExistsFile(filename.c_str())) {
    return TRI_ERROR_FILE_NOT_FOUND;
  }

  LOG_TOPIC(DEBUG, Logger::REPLICATION) << "replication state file '"
                                        << filename << "' found";

  std::shared_ptr<VPackBuilder> builder;
  try {
    builder = VelocyPackHelper::velocyPackFromFile(filename);
  }
  catch (...) {
    return TRI_ERROR_REPLICATION_INVALID_APPLIER_STATE;
  }

  VPackSlice const slice = builder->slice();
  if (!slice.isObject()) {
    return TRI_ERROR_REPLICATION_INVALID_APPLIER_STATE;
  }

  int res = TRI_ERROR_NO_ERROR;

  // read the server id
  VPackSlice const serverId = slice.get("serverId");

  if (!serverId.isString()) {
    res = TRI_ERROR_REPLICATION_INVALID_APPLIER_STATE;
  } else {
    state->_serverId = StringUtils::uint64(serverId.copyString());
  }

  if (res == TRI_ERROR_NO_ERROR) {
    // read the ticks
    res |= ReadTick(slice, "lastAppliedContinuousTick",
                    &state->_lastAppliedContinuousTick, false);

    // set processed = applied
    state->_lastProcessedContinuousTick = state->_lastAppliedContinuousTick;
  }

  if (res == TRI_ERROR_NO_ERROR) {
    // read the safeResumeTick. note: this is an optional attribute
    state->_safeResumeTick = 0;
    ReadTick(slice, "safeResumeTick", &state->_safeResumeTick, true);
  }

  LOG_TOPIC(DEBUG, Logger::REPLICATION)
      << "replication state file read successfully";

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copy an applier configuration
////////////////////////////////////////////////////////////////////////////////

void TRI_replication_applier_configuration_t::update(
    TRI_replication_applier_configuration_t const* src) {
  _endpoint = src->_endpoint;
  _database = src->_database;
  _username = src->_username;
  _password = src->_password;
  _requestTimeout = src->_requestTimeout;
  _connectTimeout = src->_connectTimeout;
  _ignoreErrors = src->_ignoreErrors;
  _maxConnectRetries = src->_maxConnectRetries;
  _sslProtocol = src->_sslProtocol;
  _chunkSize = src->_chunkSize;
  _autoStart = src->_autoStart;
  _adaptivePolling = src->_adaptivePolling;
  _autoResync = src->_autoResync;
  _includeSystem = src->_includeSystem;
  _requireFromPresent = src->_requireFromPresent;
  _verbose = src->_verbose;
  _incremental = src->_incremental;
  _useCollectionId = src->_useCollectionId;
  _restrictType = src->_restrictType;
  _restrictCollections = src->_restrictCollections;
  _connectionRetryWaitTime = src->_connectionRetryWaitTime;
  _initialSyncMaxWaitTime = src->_initialSyncMaxWaitTime;
  _idleMinWaitTime = src->_idleMinWaitTime;
  _idleMaxWaitTime = src->_idleMaxWaitTime;
  _autoResyncRetries = src->_autoResyncRetries;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove the replication application configuration file
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveConfigurationReplicationApplier(TRI_vocbase_t* vocbase) {
  if (vocbase->type() == TRI_VOCBASE_TYPE_COORDINATOR) {
    return TRI_ERROR_CLUSTER_UNSUPPORTED;
  }

  std::string const filename = GetConfigurationFilename(vocbase);

  if (TRI_ExistsFile(filename.c_str())) {
    return TRI_UnlinkFile(filename.c_str());
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief save the replication application configuration to a file
////////////////////////////////////////////////////////////////////////////////

int TRI_SaveConfigurationReplicationApplier(
    TRI_vocbase_t* vocbase,
    TRI_replication_applier_configuration_t const* config, bool doSync) {
  if (vocbase->type() == TRI_VOCBASE_TYPE_COORDINATOR) {
    return TRI_ERROR_CLUSTER_UNSUPPORTED;
  }

  std::shared_ptr<VPackBuilder> builder;
  try {
    builder = config->toVelocyPack(true);
  } catch (...) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  std::string const filename = GetConfigurationFilename(vocbase);

  if (!VelocyPackHelper::velocyPackToFile(filename, builder->slice(), doSync)) {
    return TRI_errno();
  } 
  
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a replication applier
////////////////////////////////////////////////////////////////////////////////

TRI_replication_applier_t::TRI_replication_applier_t(TRI_vocbase_t* vocbase)
    : _databaseName(vocbase->name()),
      _starts(0),
      _vocbase(vocbase),
      _terminateThread(false),
      _state() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a replication applier
////////////////////////////////////////////////////////////////////////////////

TRI_replication_applier_t::~TRI_replication_applier_t() {
  stop(true);
  TRI_DestroyStateReplicationApplier(&_state);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start the replication applier
////////////////////////////////////////////////////////////////////////////////

int TRI_replication_applier_t::start(TRI_voc_tick_t initialTick, bool useTick,
                                     TRI_voc_tick_t barrierId) {
  if (_vocbase->type() == TRI_VOCBASE_TYPE_COORDINATOR) {
    return TRI_ERROR_CLUSTER_UNSUPPORTED;
  }

  LOG_TOPIC(DEBUG, Logger::REPLICATION)
      << "requesting replication applier start. initialTick: " << initialTick
      << ", useTick: " << useTick;

  // wait until previous applier thread is shut down
  while (!wait(10 * 1000))
    ;

  WRITE_LOCKER(writeLocker, _statusLock);

  if (_state._preventStart) {
    return TRI_ERROR_LOCKED;
  }

  if (_state._active) {
    return TRI_ERROR_NO_ERROR;
  }

  if (_configuration._endpoint.empty()) {
    return doSetError(TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION,
                      "no endpoint configured");
  }

  if (_configuration._database.empty()) {
    return doSetError(TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION,
                      "no database configured");
  }

  auto syncer = std::make_unique<arangodb::ContinuousSyncer>(_vocbase, &_configuration, initialTick, useTick, barrierId);

  // reset error
  if (_state._lastError._msg != nullptr) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, _state._lastError._msg);
    _state._lastError._msg = nullptr;
  }

  // save previous counter value
  uint64_t oldStarts = _starts.load();

  _state._lastError._code = TRI_ERROR_NO_ERROR;

  TRI_GetTimeStampReplication(_state._lastError._time,
                              sizeof(_state._lastError._time) - 1);

  setTermination(false);
  _state._active = true;

  TRI_InitThread(&_thread);

  if (!TRI_StartThread(&_thread, nullptr, "Applier", ApplyThread,
                       static_cast<void*>(syncer.get()))) {
    return TRI_ERROR_INTERNAL;
  }

  syncer.release();

  uint64_t iterations = 0;
  while (oldStarts == _starts.load() && ++iterations < 50 * 10) {
    usleep(20000);
  }

  if (useTick) {
    LOG_TOPIC(INFO, Logger::REPLICATION)
        << "started replication applier for database '" << _databaseName
        << "', endpoint '" << _configuration._endpoint << "' from tick "
        << initialTick;
  } else {
    LOG_TOPIC(INFO, Logger::REPLICATION)
        << "re-started replication applier for database '"
        << _databaseName << "', endpoint '" << _configuration._endpoint
        << "'";
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the replication applier is running
////////////////////////////////////////////////////////////////////////////////

bool TRI_replication_applier_t::isRunning() const {
  READ_LOCKER(readLocker, _statusLock);

  return _state._active;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief block the replication applier from starting
////////////////////////////////////////////////////////////////////////////////

int TRI_replication_applier_t::preventStart() {
  WRITE_LOCKER(writeLocker, _statusLock);

  if (_state._active) {
    // already running
    return TRI_ERROR_REPLICATION_RUNNING;
  }

  if (_state._preventStart) {
    // someone else requested start prevention
    return TRI_ERROR_LOCKED;
  }

  _state._stopInitialSynchronization = false;
  _state._preventStart = true;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unblock the replication applier from starting
////////////////////////////////////////////////////////////////////////////////

int TRI_replication_applier_t::allowStart() {
  WRITE_LOCKER(writeLocker, _statusLock);

  if (!_state._preventStart) {
    return TRI_ERROR_INTERNAL;
  }

  _state._stopInitialSynchronization = false;
  _state._preventStart = false;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the initial synchronization should be stopped
////////////////////////////////////////////////////////////////////////////////

bool TRI_replication_applier_t::stopInitialSynchronization() {
  READ_LOCKER(readLocker, _statusLock);

  return _state._stopInitialSynchronization;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the initial synchronization
////////////////////////////////////////////////////////////////////////////////

void TRI_replication_applier_t::stopInitialSynchronization(bool value) {
  WRITE_LOCKER(writeLocker, _statusLock);
  _state._stopInitialSynchronization = value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the replication applier
////////////////////////////////////////////////////////////////////////////////

int TRI_replication_applier_t::stop(bool resetError) {
  if (_vocbase->type() == TRI_VOCBASE_TYPE_COORDINATOR) {
    return TRI_ERROR_CLUSTER_UNSUPPORTED;
  }

  {
    WRITE_LOCKER(writeLocker, _statusLock);

    // always stop initial synchronization
    _state._stopInitialSynchronization = true;

    if (!_state._active) {
      return TRI_ERROR_NO_ERROR;
    }

    _state._active = false;

    setTermination(true);
    setProgress("applier shut down", false);

    if (resetError) {
      if (_state._lastError._msg != nullptr) {
        TRI_FreeString(TRI_CORE_MEM_ZONE, _state._lastError._msg);
        _state._lastError._msg = nullptr;
      }

      _state._lastError._code = TRI_ERROR_NO_ERROR;
    }

    TRI_GetTimeStampReplication(_state._lastError._time,
                                sizeof(_state._lastError._time) - 1);
  }

  // join the thread without the status lock (otherwise it would probably not
  // join)
  int res = TRI_JoinThread(&_thread);

  setTermination(false);

  LOG_TOPIC(INFO, Logger::REPLICATION)
      << "stopped replication applier for database '" << _databaseName
      << "'";

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the applier and "forget" everything
////////////////////////////////////////////////////////////////////////////////

int TRI_replication_applier_t::forget() {
  int res = stop(true);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  TRI_RemoveStateReplicationApplier(_vocbase);
  TRI_DestroyStateReplicationApplier(&_state);
  TRI_InitStateReplicationApplier(&_state);

  TRI_RemoveConfigurationReplicationApplier(_vocbase);
  _configuration.reset();

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shut down the replication applier
////////////////////////////////////////////////////////////////////////////////

int TRI_replication_applier_t::shutdown() {
  if (_vocbase->type() == TRI_VOCBASE_TYPE_COORDINATOR) {
    return TRI_ERROR_CLUSTER_UNSUPPORTED;
  }

  {
    WRITE_LOCKER(writeLocker, _statusLock);

    if (!_state._active) {
      return TRI_ERROR_NO_ERROR;
    }

    _state._active = false;

    setTermination(true);
    setProgress("applier stopped", false);

    if (_state._lastError._msg != nullptr) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, _state._lastError._msg);
      _state._lastError._msg = nullptr;
    }

    _state._lastError._code = TRI_ERROR_NO_ERROR;

    TRI_GetTimeStampReplication(_state._lastError._time,
                                sizeof(_state._lastError._time) - 1);
  }

  // join the thread without the status lock (otherwise it would probably not
  // join)
  int res = TRI_JoinThread(&_thread);

  setTermination(false);

  LOG_TOPIC(INFO, Logger::REPLICATION)
      << "shut down replication applier for database '" << _databaseName
      << "'";

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief pauses and checks whether the apply thread should terminate
////////////////////////////////////////////////////////////////////////////////

bool TRI_replication_applier_t::wait(uint64_t sleepTime) {
  if (isTerminated()) {
    return false;
  }

  if (sleepTime > 0) {
    LOG_TOPIC(TRACE, Logger::REPLICATION)
        << "replication applier going to sleep for " << sleepTime << " ns";

    static uint64_t const SleepChunk = 500 * 1000;

    while (sleepTime >= SleepChunk) {
#ifdef _WIN32
      usleep((unsigned long)SleepChunk);
#else
      usleep((useconds_t)SleepChunk);
#endif

      sleepTime -= SleepChunk;

      if (isTerminated()) {
        return false;
      }
    }

    if (sleepTime > 0) {
#ifdef _WIN32
      usleep((unsigned long)sleepTime);
#else
      usleep((useconds_t)sleepTime);
#endif

      if (isTerminated()) {
        return false;
      }
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the progress with or without a lock
////////////////////////////////////////////////////////////////////////////////

void TRI_replication_applier_t::setProgress(char const* msg, bool lock) {
  char* copy = TRI_DuplicateString(TRI_CORE_MEM_ZONE, msg);

  if (copy == nullptr) {
    return;
  }

  if (lock) {
    WRITE_LOCKER(writeLocker, _statusLock);

    if (_state._progressMsg != nullptr) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, _state._progressMsg);
    }

    _state._progressMsg = copy;

    // write time in buffer
    TRI_GetTimeStampReplication(_state._progressTime,
                                sizeof(_state._progressTime) - 1);
  } else {
    if (_state._progressMsg != nullptr) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, _state._progressMsg);
    }

    _state._progressMsg = copy;

    // write time in buffer
    TRI_GetTimeStampReplication(_state._progressTime,
                                sizeof(_state._progressTime) - 1);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register an applier error
////////////////////////////////////////////////////////////////////////////////

int TRI_replication_applier_t::setError(int errorCode, char const* msg) {
  WRITE_LOCKER(writeLocker, _statusLock);
  return doSetError(errorCode, msg);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a VelocyPack representation
///        Expects builder to be in an open Object state
////////////////////////////////////////////////////////////////////////////////

void TRI_replication_applier_t::toVelocyPack(VPackBuilder& builder) const {
  TRI_ASSERT(!builder.isClosed());
  TRI_replication_applier_state_t state;
  TRI_replication_applier_configuration_t config;

  int res = TRI_StateReplicationApplier(this, &state);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  try {
    READ_LOCKER(readLocker, _statusLock);
    config.update(&_configuration);
  } catch (...) {
    TRI_DestroyStateReplicationApplier(&state);
    throw;
  }

  arangodb::basics::ScopeGuard guard{
      []() -> void {},
      [&state, &config]() -> void {
        TRI_DestroyStateReplicationApplier(&state);
      }};

  // add state
  builder.add(VPackValue("state"));
  VPackState(builder, &state);

  // add server info
  builder.add("server", VPackValue(VPackValueType::Object));
  builder.add("version", VPackValue(ARANGODB_VERSION));
  builder.add("serverId", VPackValue(std::to_string(ServerIdFeature::getId())));
  builder.close();  // server
  
  if (!config._endpoint.empty()) {
    builder.add("endpoint", VPackValue(config._endpoint));
  }
  if (!config._database.empty()) {
    builder.add("database", VPackValue(config._database));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a VelocyPack representation
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<VPackBuilder> TRI_replication_applier_t::toVelocyPack() const {
  auto builder = std::make_shared<VPackBuilder>();
  {
    VPackObjectBuilder b(builder.get());
    toVelocyPack(*builder);
  }
  return builder;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register an applier error
////////////////////////////////////////////////////////////////////////////////

int TRI_replication_applier_t::doSetError(int errorCode, char const* msg) {
  char const* realMsg;

  if (msg == nullptr || strlen(msg) == 0) {
    realMsg = TRI_errno_string(errorCode);
  } else {
    realMsg = msg;
  }

  // log error message
  if (errorCode != TRI_ERROR_REPLICATION_APPLIER_STOPPED) {
    LOG_TOPIC(ERR, Logger::REPLICATION)
        << "replication applier error for database '" << _databaseName
        << "': " << realMsg;
  }

  _state._lastError._code = errorCode;

  TRI_GetTimeStampReplication(_state._lastError._time,
                              sizeof(_state._lastError._time) - 1);

  if (_state._lastError._msg != nullptr) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, _state._lastError._msg);
  }

  _state._lastError._msg = TRI_DuplicateString(TRI_CORE_MEM_ZONE, realMsg);

  return errorCode;
}
