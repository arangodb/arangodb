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
#include "Scheduler/Scheduler.h"

namespace arangodb {
namespace rest {
class Scheduler;
}

class JobGuard : public SameThreadAsserter {
 public:
  JobGuard(JobGuard const&) = delete;
  JobGuard& operator=(JobGuard const&) = delete;

  explicit JobGuard(rest::Scheduler* scheduler)
      : SameThreadAsserter(), _scheduler(scheduler) {}
  ~JobGuard() { release(); }

 public:
  void work() noexcept {
    TRI_ASSERT(!_isWorkingFlag);

    if (0 == _isWorking++) {
      _scheduler->incWorking();
    }

    _isWorkingFlag = true;
  }

 private:
  void release() noexcept {
    if (_isWorkingFlag) {
      _isWorkingFlag = false;

      TRI_ASSERT(_isWorking > 0);
      if (0 == --_isWorking) {
        // if this is the last JobGuard we inform the
        // scheduler that the thread is back to idle
        _scheduler->decWorking();
      }
    }
  }

 private:
  rest::Scheduler* _scheduler;

  bool _isWorkingFlag = false;

  static thread_local size_t _isWorking;
};
}

#endif
