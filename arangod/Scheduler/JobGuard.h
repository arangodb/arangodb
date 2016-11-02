////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_SCHEDULER_JOB_GUARD_H
#define ARANGOD_SCHEDULER_JOB_GUARD_H 1

#include "Basics/Common.h"

#include "Scheduler/EventLoop.h"
#include "Scheduler/Scheduler.h"

namespace arangodb {
namespace rest {
class Scheduler;
}

class JobGuard {
 public:
  explicit JobGuard(EventLoop const& loop) : _scheduler(loop._scheduler) {}
  explicit JobGuard(rest::Scheduler* scheduler) : _scheduler(scheduler) {}
  ~JobGuard() { release(); }

 public:
  bool isIdle() {
    return _scheduler->isIdle();
  }

  void busy() {
    if (0 == _isBusy) {
      _scheduler->enterThread();
    }

    ++_isBusy;
  }

  void work() {
    if (0 == _isWorking) {
      _scheduler->workThread();
    }

    ++_isWorking;
  }

  void block() {
    if (0 == _isBlocked) {
      _scheduler->blockThread();
    }
    
    ++_isBlocked;
  }

  void release() {
    if (0 < _isBusy) {
      --_isBusy;

      if (0 == _isBusy) {
        _scheduler->unenterThread();
      }
    }
    
    if (0 < _isWorking) {
      --_isWorking;

      if (0 == _isWorking) {
        _scheduler->unworkThread();
      }
    }

    if (0 < _isBlocked) {
      --_isBlocked;

      if (0 == _isBlocked) {
        _scheduler->unblockThread();
      }
    }
  }

 private:
  rest::Scheduler* _scheduler;

  static thread_local size_t _isBusy;
  static thread_local size_t _isWorking;
  static thread_local size_t _isBlocked;
};
}

#endif
