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

#include "Basics/SameThreadAsserter.h"
#include "Scheduler/EventLoop.h"
#include "Scheduler/Scheduler.h"

namespace arangodb {
namespace rest {
class Scheduler;
}

class JobGuard : public SameThreadAsserter {
 public:
  JobGuard(JobGuard const&) = delete;
  JobGuard& operator=(JobGuard const&) = delete;

  explicit JobGuard(EventLoop const& loop) : SameThreadAsserter(), _scheduler(loop._scheduler) {}
  explicit JobGuard(rest::Scheduler* scheduler) : SameThreadAsserter(), _scheduler(scheduler) {}
  ~JobGuard() { release(); }

 public:
  void work() {
    TRI_ASSERT(!_isWorkingFlag);

    if (0 == _isWorking++) {
      _scheduler->workThread();
    }

    _isWorkingFlag = true;
  }

  void block() {
    TRI_ASSERT(!_isBlockedFlag);

    if (0 == _isBlocked++) {
      _scheduler->blockThread();
    }

    _isBlockedFlag = true;
  }

 private:
  void release() {
    if (_isWorkingFlag) {
      _isWorkingFlag = false;

      if (0 == --_isWorking) {
        // if this is the last JobGuard we inform the
        // scheduler that the thread is back to idle
        _scheduler->unworkThread();
      }
    }

    if (_isBlockedFlag) {
      _isBlockedFlag = false;

      if (0 == --_isBlocked) {
        // if this is the last JobGuard we inform the
        // scheduler that the thread is now unblocked
        _scheduler->unblockThread();
      }
    }
  }

 private:
  rest::Scheduler* _scheduler;

  bool _isWorkingFlag = false;
  bool _isBlockedFlag = false;

  static thread_local size_t _isWorking;
  static thread_local size_t _isBlocked;
};
}

#endif
