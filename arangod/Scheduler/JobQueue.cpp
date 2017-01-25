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

#include "JobQueue.h"

#include "Basics/ConditionLocker.h"
#include "GeneralServer/RestHandler.h"
#include "Logger/Logger.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"

using namespace arangodb;

namespace {
class JobQueueThread final : public Thread {
 public:
  JobQueueThread(JobQueue* server, boost::asio::io_service* ioService)
      : Thread("JobQueueThread"), _jobQueue(server), _ioService(ioService) {}

  ~JobQueueThread() { shutdown(); }

  void beginShutdown() {
    Thread::beginShutdown();
    _jobQueue->wakeup();
  }

 public:
  void run() {
    int idleTries = 0;
    auto jobQueue = _jobQueue;

    // iterate until we are shutting down
    while (!isStopping()) {
      ++idleTries;

      for (size_t i = 0; i < JobQueue::SYSTEM_QUEUE_SIZE; ++i) {
        LOG_TOPIC(TRACE, Logger::THREADS) << "size of queue #" << i << ": "
                                          << _jobQueue->queueSize(i);

        while (_jobQueue->tryActive()) {
          Job* job = nullptr;

          if (!_jobQueue->pop(i, job)) {
            _jobQueue->releaseActive();
            break;
          }

          LOG_TOPIC(TRACE, Logger::THREADS)
              << "starting next queued job, number currently active "
              << _jobQueue->active();

          idleTries = 0;

          _ioService->dispatch([jobQueue, job]() {
            std::unique_ptr<Job> guard(job);

            job->_callback(std::move(job->_handler));
            jobQueue->releaseActive();
            jobQueue->wakeup();
          });
        }
      }

      // we need to check again if more work has arrived after we have
      // aquired the lock. The lockfree queue and _nrWaiting are accessed
      // using "memory_order_seq_cst", this guarantees that we do not
      // miss a signal.

      if (idleTries >= 2) {
        LOG_TOPIC(TRACE, Logger::THREADS) << "queue manager going to sleep";
        _jobQueue->waitForWork();
      }
    }

    // clear all non-processed jobs
    for (size_t i = 0; i < JobQueue::SYSTEM_QUEUE_SIZE; ++i) {
      Job* job = nullptr;
      while (_jobQueue->pop(i, job)) {
        delete job;
      }
    }
  }

 private:
  JobQueue* _jobQueue;
  boost::asio::io_service* _ioService;
};
}

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

JobQueue::JobQueue(size_t queueSize, boost::asio::io_service* ioService)
    : _queueAql(queueSize),
      _queueRequeue(queueSize),
      _queueStandard(queueSize),
      _queueUser(queueSize),
      _queues{&_queueRequeue, &_queueAql, &_queueStandard, &_queueUser},
      _active(0),
      _ioService(ioService),
      _queueThread(new JobQueueThread(this, _ioService)) {
  for (size_t i = 0; i < SYSTEM_QUEUE_SIZE; ++i) {
    _queuesSize[i].store(0);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

void JobQueue::start() {
  _queueThread->start();
}

void JobQueue::beginShutdown() {
  _queueThread->beginShutdown();
}

bool JobQueue::tryActive() {
  if (!SchedulerFeature::SCHEDULER->tryBlocking()) {
    return false;
  }

  ++_active;
  return true;
}

void JobQueue::releaseActive() {
  SchedulerFeature::SCHEDULER->unworkThread();
  --_active;
}

void JobQueue::wakeup() {
  CONDITION_LOCKER(guard, _queueCondition);
  guard.signal();
}

void JobQueue::waitForWork() {
  static uint64_t WAIT_TIME = 1000 * 1000;

  CONDITION_LOCKER(guard, _queueCondition);
  guard.wait(WAIT_TIME);
}
