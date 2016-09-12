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

#ifndef ARANGOD_WAL_ALLOCATOR_THREAD_H
#define ARANGOD_WAL_ALLOCATOR_THREAD_H 1

#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/ReadLocker.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/Thread.h"
#include "Basics/WriteLocker.h"

namespace arangodb {
namespace wal {

class LogfileManager;

class AllocatorThread final : public Thread {
  AllocatorThread(AllocatorThread const&) = delete;
  AllocatorThread& operator=(AllocatorThread const&) = delete;

 public:
  explicit AllocatorThread(LogfileManager*);
  ~AllocatorThread() { shutdown(); }

 public:
  void beginShutdown() override final;

 public:
  /// @brief signal the creation of a new logfile
  void signal(uint32_t);

  /// @brief wait for allocator result
  int waitForResult(uint64_t);

  /// @brief tell the thread that the recovery phase is over
  void recoveryDone() {
    WRITE_LOCKER(writeLocker, _recoveryLock);
    _inRecovery = false;
  }

  /// @brief whether or not we are in recovery
  bool inRecovery() {
    READ_LOCKER(readLocker, _recoveryLock);
    return _inRecovery;
  }

 protected:
  /// @brief main loop
  void run() override;

 private:
  /// @brief creates a new reserve logfile
  int createReserveLogfile(uint32_t);

 private:
  /// @brief the logfile manager
  LogfileManager* _logfileManager;

  /// @brief condition variable for the allocator thread
  basics::ConditionVariable _condition;

  /// @brief lock for _inRecovery
  basics::ReadWriteLock _recoveryLock;

  /// @brief requested logfile size
  uint32_t _requestedSize;

  /// @brief whether or not we are in the recovery mode
  bool _inRecovery;

  /// @brief condition variable for the allocator thread
  basics::ConditionVariable _allocatorResultCondition;

  /// @brief allocation result
  int _allocatorResult;

  /// @brief wait interval for the allocator thread when idle
  static uint64_t const Interval;
};
}
}

#endif
