////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "ReplicationApplier.h"

#include "Basics/Exceptions.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/files.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Replication/common-defines.h"

using namespace arangodb;
namespace StringUtils = arangodb::basics::StringUtils;

ReplicationApplier::ReplicationApplier(
    ReplicationApplierConfiguration const& configuration,
    std::string&& databaseName)
    : _configuration(configuration), _databaseName(std::move(databaseName)) {
  setProgress(std::string("applier initially created for ") + _databaseName);
}

/// @brief block the replication applier from starting
Result ReplicationApplier::preventStart() {
  WRITE_LOCKER_EVENTUAL(writeLocker, _statusLock);

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

/// @brief stop the replication applier, resets the error message
void ReplicationApplier::stop() { doStop(Result(), false); }

/// @brief stop the replication applier and join the apply thread
void ReplicationApplier::stopAndJoin() { doStop(Result(), true); }

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
      << "looking for replication state file '" << filename << "' for "
      << _databaseName;

  if (!TRI_ExistsFile(filename.c_str())) {
    // no existing state found
    return false;
  }

  LOG_TOPIC("3e515", DEBUG, Logger::REPLICATION)
      << "replication state file '" << filename << "' found for "
      << _databaseName;

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
  readTick(slice, "lastAppliedContinuousTick",
           _state._lastAppliedContinuousTick, false);

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

  if (!basics::VelocyPackHelper::velocyPackToFile(filename, builder.slice(),
                                                  doSync)) {
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
    std::string errorMsg =
        std::string("unable to save replication applier state: ") + ex.what();
    LOG_TOPIC("a98dc", WARN, Logger::REPLICATION) << errorMsg;
    rv.reset(ex.code(), errorMsg);
  } catch (std::exception const& ex) {
    std::string errorMsg =
        std::string("unable to save replication applier state: ") + ex.what();
    LOG_TOPIC("0d891", WARN, Logger::REPLICATION) << errorMsg;
    rv.reset(TRI_ERROR_INTERNAL, errorMsg);
  } catch (...) {
    std::string errorMsg =
        std::string("caught unknown exception while saving applier state");
    LOG_TOPIC("2f0c1", WARN, Logger::REPLICATION) << errorMsg;
    rv.reset(TRI_ERROR_INTERNAL, errorMsg);
  }

  return rv;
}

/// @brief set the progress
void ReplicationApplier::setProgress(char const* msg) {
  return setProgress(std::string(msg));
}

void ReplicationApplier::setProgress(std::string const& msg) {
  WRITE_LOCKER_EVENTUAL(writeLocker, _statusLock);
  setProgressNoLock(msg);
}

void ReplicationApplier::setProgressNoLock(std::string const& msg) {
  _state._progressMsg = msg;

  // write time into buffer
  TRI_GetTimeStampReplication(_state._progressTime,
                              sizeof(_state._progressTime) - 1);
}

/// @brief stop the replication applier
void ReplicationApplier::doStop(Result const& r, bool joinThread) {
  if (!applies()) {
    return;
  }

  WRITE_LOCKER_EVENTUAL(writeLocker, _statusLock);

  // always stop initial synchronization
  _state._stopInitialSynchronization = true;

  if (!joinThread) {
    return;
  }

  LOG_TOPIC("73c1a", DEBUG, Logger::REPLICATION)
      << "requesting replication applier stop for " << _databaseName;

  _state.setError(r.errorNumber(), r.errorMessage());
}

/// @brief read a tick value from a VelocyPack struct
void ReplicationApplier::readTick(VPackSlice const& slice,
                                  char const* attributeName,
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
