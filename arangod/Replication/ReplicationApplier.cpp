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
#include "Basics/ReadLocker.h"
#include "Basics/Thread.h"
#include "Basics/WriteLocker.h"
#include "Basics/files.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "VocBase/replication-common.h"

using namespace arangodb;

ReplicationApplier::ReplicationApplier(ReplicationApplierConfiguration const& configuration,
                                       std::string&& databaseName) 
      : _configuration(configuration),
        _terminateThread(false),
        _databaseName(std::move(databaseName)) {
    setProgress(std::string("applier initially created for ") + _databaseName);
}

ReplicationApplier::~ReplicationApplier() {}

/// @brief test if the replication applier is running
bool ReplicationApplier::isRunning() const {
  READ_LOCKER(readLocker, _statusLock);
  return _state._active;
}

/// @brief block the replication applier from starting
int ReplicationApplier::preventStart() {
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

/// @brief unblock the replication applier from starting
int ReplicationApplier::allowStart() {
  WRITE_LOCKER(writeLocker, _statusLock);

  if (!_state._preventStart) {
    return TRI_ERROR_INTERNAL;
  }

  _state._stopInitialSynchronization = false;
  _state._preventStart = false;

  return TRI_ERROR_NO_ERROR;
}

/// @brief whether or not autostart option was set
bool ReplicationApplier::autoStart() const {
  READ_LOCKER(readLocker, _statusLock);
  return _configuration._autoStart;
}

/// @brief check whether the initial synchronization should be stopped
bool ReplicationApplier::stopInitialSynchronization() const {
  READ_LOCKER(readLocker, _statusLock);

  return _state._stopInitialSynchronization;
}

/// @brief stop the initial synchronization
void ReplicationApplier::stopInitialSynchronization(bool value) {
  WRITE_LOCKER(writeLocker, _statusLock);
  _state._stopInitialSynchronization = value;
}

/// @brief shuts down the replication applier
void ReplicationApplier::shutdown() {
  {
    WRITE_LOCKER(writeLocker, _statusLock);

    if (!_state._active) {
      // nothing to do
      return;
    }

    _state._active = false;
    _state.clearError();

    setTermination(true);
    setProgressNoLock("applier stopped");
  }

  // join the thread without holding the status lock 
  // (otherwise it would probably not join)
  TRI_ASSERT(_thread);
  _thread.reset();

  setTermination(false);

  LOG_TOPIC(INFO, Logger::REPLICATION)
      << "shut down replication applier for " << _databaseName;
}

void ReplicationApplier::removeState() {
  WRITE_LOCKER(writeLocker, _statusLock);
  _state.reset();

  std::string const filename = getStateFilename();

  if (TRI_ExistsFile(filename.c_str())) {
    LOG_TOPIC(TRACE, Logger::REPLICATION) << "removing replication state file '"
                                          << filename << "'";
    int res = TRI_UnlinkFile(filename.c_str());

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION_MESSAGE(res, std::string("unable to remove replication state file '") + filename + "'");
    }
  }
} 
  
void ReplicationApplier::reconfigure(ReplicationApplierConfiguration const& configuration) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  
  if (configuration._endpoint.empty()) {
    // no endpoint
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION, "no endpoint configured");
  }

  WRITE_LOCKER(writeLocker, _statusLock);

  if (_state._active) {
    // cannot change the configuration while the replication is still running
    THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_RUNNING);
  }

  _configuration = configuration;
  storeConfiguration(true);
}

/// @brief register an applier error
int ReplicationApplier::setError(int errorCode, char const* msg) {
  return setErrorNoLock(errorCode, std::string(msg));
}

int ReplicationApplier::setError(int errorCode, std::string const& msg) {
  WRITE_LOCKER(writeLocker, _statusLock);
  return setErrorNoLock(errorCode, msg);
}

/// @brief set the progress 
void ReplicationApplier::setProgress(char const* msg) {
  return setProgress(std::string(msg));
}

void ReplicationApplier::setProgress(std::string const& msg) {
  WRITE_LOCKER(writeLocker, _statusLock);
  setProgressNoLock(msg);
}

/// @brief register an applier error
int ReplicationApplier::setErrorNoLock(int errorCode, std::string const& msg) {
  // log error message
  if (errorCode != TRI_ERROR_REPLICATION_APPLIER_STOPPED) {
    LOG_TOPIC(ERR, Logger::REPLICATION)
        << "replication applier error for " << _databaseName << ": "
        << (msg.empty() ? TRI_errno_string(errorCode) : msg);
  }

  _state.setError(errorCode, msg.empty() ? TRI_errno_string(errorCode) : msg);
  return errorCode;
}

void ReplicationApplier::setProgressNoLock(std::string const& msg) {
  _state._progressMsg = msg;

  // write time into buffer
  TRI_GetTimeStampReplication(_state._progressTime, sizeof(_state._progressTime) - 1);
}
