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

#include "MMFilesAllocatorThread.h"
#include "Basics/ConditionLocker.h"
#include "Basics/Exceptions.h"
#include "Logger/Logger.h"
#include "MMFiles/MMFilesLogfileManager.h"

using namespace arangodb;

/// @brief wait interval for the allocator thread when idle
uint64_t const MMFilesAllocatorThread::Interval = 500 * 1000;

MMFilesAllocatorThread::MMFilesAllocatorThread(MMFilesLogfileManager* logfileManager)
    : Thread("WalAllocator"),
      _logfileManager(logfileManager),
      _condition(),
      _recoveryLock(),
      _requestedSize(0),
      _inRecovery(true),
      _allocatorResultCondition(),
      _allocatorResult(TRI_ERROR_LOCKED) {}

/// @brief wait for the collector result
int MMFilesAllocatorThread::waitForResult(uint64_t timeout) {
  CONDITION_LOCKER(guard, _allocatorResultCondition);

  if (_allocatorResult == TRI_ERROR_LOCKED) {
    if (guard.wait(timeout)) {
      return TRI_ERROR_LOCK_TIMEOUT;
    }
  }

  int res = _allocatorResult;

  // convert "locked" into NO_ERROR
  if (res == TRI_ERROR_LOCKED) {
    res = TRI_ERROR_NO_ERROR;
  }

  TRI_ASSERT(res != TRI_ERROR_LOCKED);

  return res;
}

/// @brief begin shutdown sequence
void MMFilesAllocatorThread::beginShutdown() {
  Thread::beginShutdown();

  CONDITION_LOCKER(guard, _condition);
  guard.signal();
}

/// @brief signal the creation of a new logfile
void MMFilesAllocatorThread::signal(uint32_t markerSize) {
  CONDITION_LOCKER(guard, _condition);

  if (_requestedSize == 0 || markerSize > _requestedSize) {
    // logfile must be as big as the requested marker
    _requestedSize = markerSize;
  }

  guard.signal();
}

/// @brief creates a new reserve logfile
int MMFilesAllocatorThread::createReserveLogfile(uint32_t size) {
  return _logfileManager->createReserveLogfile(size);
}

/// @brief main loop
void MMFilesAllocatorThread::run() {
  while (!isStopping()) {
    uint32_t requestedSize = 0;

    {
      CONDITION_LOCKER(guard, _condition);
      requestedSize = _requestedSize;
      _requestedSize = 0;
    }

    int res = TRI_ERROR_NO_ERROR;
    bool worked = false;

    try {
      if (requestedSize == 0 && !inRecovery() && !_logfileManager->hasReserveLogfiles()) {
        // reset allocator status
        {
          CONDITION_LOCKER(guard, _allocatorResultCondition);
          _allocatorResult = TRI_ERROR_LOCKED;
        }

        // only create reserve files if we are not in the recovery mode
        worked = true;
        res = createReserveLogfile(0);
      } else if (requestedSize > 0 && _logfileManager->logfileCreationAllowed(requestedSize)) {
        // reset allocator status
        {
          CONDITION_LOCKER(guard, _allocatorResultCondition);
          _allocatorResult = TRI_ERROR_LOCKED;
        }

        worked = true;
        res = createReserveLogfile(requestedSize);
      }
    } catch (arangodb::basics::Exception const& ex) {
      res = ex.code();
      LOG_TOPIC("47ea3", ERR, arangodb::Logger::ENGINES)
          << "got unexpected error in allocatorThread: " << TRI_errno_string(res);
    } catch (...) {
      res = TRI_ERROR_INTERNAL;
      LOG_TOPIC("8ff31", ERR, arangodb::Logger::ENGINES)
          << "got unspecific error in allocatorThread";
    }

    if (worked) {
      if (res != TRI_ERROR_NO_ERROR) {
        LOG_TOPIC("a8415", ERR, arangodb::Logger::ENGINES)
            << "unable to create new WAL reserve logfile: " << TRI_errno_string(res);
      }

      // broadcast new allocator status
      CONDITION_LOCKER(guard, _allocatorResultCondition);
      _allocatorResult = res;
      guard.broadcast();
    } else {
      TRI_ASSERT(res == TRI_ERROR_NO_ERROR);
    }

    CONDITION_LOCKER(guard, _condition);
    guard.wait(Interval);
  }
}
