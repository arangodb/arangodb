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

#ifndef ARANGOD_SCHEDULER_JOB_QUEUE_H
#define ARANGOD_SCHEDULER_JOB_QUEUE_H 1

#include "Basics/Common.h"

#include <boost/lockfree/queue.hpp>

#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"
#include "Scheduler/Job.h"

namespace arangodb {
namespace rest {
class Scheduler;
}

class JobQueueThread;

class JobQueue {
  friend class JobQueueThread;
 public:
  // ordered by priority (highst prio first)
  static size_t const AQL_QUEUE = 1;
  static size_t const STANDARD_QUEUE = 2;
  static size_t const BACKGROUND_QUEUE = 3;

 public:
  JobQueue(size_t maxQueueSize, rest::Scheduler*);

 public:
  void start();
  void beginShutdown();

  int64_t queueSize() const { return _queueSize; }

  bool queue(std::unique_ptr<Job> job) {
    try {
      if (0 < _maxQueueSize && _maxQueueSize <= _queueSize) {
        wakeup();
        return false;
      }

      if (!_queue.push(job.get())) {
        wakeup();
        return false;
      }

      job.release();
      ++_queueSize;
    } catch (...) {
      wakeup();
      return false;
    }

    wakeup();
    return true;
  }
  
private:

  bool pop(Job*& job) {
    bool ok = _queue.pop(job) && job != nullptr;

    if (ok) {
      --_queueSize;
    }

    return ok;
  }
  
 public:
  void wakeup();
  
 private:
  int64_t const _maxQueueSize;
  boost::lockfree::queue<Job*> _queue;
  std::atomic<int64_t> _queueSize;

  basics::ConditionVariable _queueCondition;

  std::shared_ptr<JobQueueThread> _queueThread;
};
}

#endif
