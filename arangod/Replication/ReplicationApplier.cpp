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
#include "Basics/ReadLocker.h"
#include "Basics/Thread.h"
#include "Basics/WriteLocker.h"
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
