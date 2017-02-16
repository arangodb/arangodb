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

#include "Basics/asio-helper.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"
#include "Scheduler/Job.h"

namespace arangodb {
class JobQueue {
 public:
  // ordered by priority (highst prio first)
  static size_t const REQUEUED_QUEUE = 0;
  static size_t const AQL_QUEUE = 1;
  static size_t const STANDARD_QUEUE = 2;
  static size_t const USER_QUEUE = 3;
  static size_t const SYSTEM_QUEUE_SIZE = 4;

 public:
  JobQueue(size_t queueSize, boost::asio::io_service* ioService);

 public:
  void start();
  void beginShutdown();

  int64_t queueSize(size_t i) const { return _queuesSize[i]; }

  bool queue(size_t i, std::unique_ptr<Job> job) {
    if (i >= SYSTEM_QUEUE_SIZE) {
      return false;
    }

    try {
      if (!_queues[i]->push(job.get())) {
        throw "failed to add to queue";
      }

      job.release();
      ++_queuesSize[i];
    } catch (...) {
      wakeup();
      return false;
    }

    wakeup();
    return true;
  }

  bool pop(size_t i, Job*& job) {
    if (i >= SYSTEM_QUEUE_SIZE) {
      return false;
    }

    bool ok = _queues[i]->pop(job) && job != nullptr;

    if (ok) {
      --_queuesSize[i];
    }

    return ok;
  }

  void wakeup();
  void waitForWork();

  size_t active() const { return _active.load(); }
  bool tryActive();
  void releaseActive();

 private:
  boost::lockfree::queue<Job*> _queueAql;
  boost::lockfree::queue<Job*> _queueRequeue;
  boost::lockfree::queue<Job*> _queueStandard;
  boost::lockfree::queue<Job*> _queueUser;
  boost::lockfree::queue<Job*>* _queues[SYSTEM_QUEUE_SIZE];
  std::atomic<int64_t> _queuesSize[SYSTEM_QUEUE_SIZE];
  std::atomic<size_t> _active;

  basics::ConditionVariable _queueCondition;

  boost::asio::io_service* _ioService;
  std::unique_ptr<Thread> _queueThread;
};
}

#endif
