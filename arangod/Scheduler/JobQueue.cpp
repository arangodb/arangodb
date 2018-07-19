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
#include "Statistics/RequestStatistics.h"

using namespace arangodb;

namespace arangodb {
class JobQueueThread final
    : public Thread,
      public std::enable_shared_from_this<JobQueueThread> {
 public:
  JobQueueThread(JobQueue* server, rest::Scheduler* scheduler)
      : Thread("JobQueueThread"), _jobQueue(server), _scheduler(scheduler) {}

  ~JobQueueThread() { shutdown(); }

  void beginShutdown() override {
    Thread::beginShutdown();
    _jobQueue->wakeup();
  }

 public:
  void run() override {
    int idleTries = 0;

    auto self = shared_from_this();

    CONDITION_LOCKER(guard, _jobQueue->_queueCondition);

    // iterate until we are shutting down
    while (!isStopping()) {
      ++idleTries;

      LOG_TOPIC(TRACE, Logger::THREADS)
          << "size of job queue: " << _jobQueue->queueSize();

      while (_scheduler->shouldQueueMore()) {
        guard.unlock();
        Job* jobPtr = nullptr;

        if (!_jobQueue->pop(jobPtr)) {
          guard.lock();
          break;
        }

        LOG_TOPIC(TRACE, Logger::THREADS) << "starting next queued job";

        idleTries = 0;
        std::shared_ptr<Job> job(jobPtr);

        _scheduler->post([self, job]() {
          RequestStatistics::SET_QUEUE_END(job->_handler->statistics());

          try {
            job->_callback(std::move(job->_handler));
          } catch (std::exception const& e) {
            LOG_TOPIC(WARN, Logger::THREADS)
                << "caught exception while executing job callback: " << e.what();
          } catch (...) {
            LOG_TOPIC(WARN, Logger::THREADS)
                << "caught unknown exception while executing job callback";
          }
        });

        guard.lock();
      }

      if (idleTries >= 2) {
        LOG_TOPIC(TRACE, Logger::THREADS) << "queue manager going to sleep";
        static uint64_t WAIT_TIME = 1000 * 1000;
        guard.wait(WAIT_TIME);
      }
    }

    // clear all non-processed jobs
    Job* job = nullptr;

    while (_jobQueue->pop(job)) {
      delete job;
    }
  }

 private:
  JobQueue* _jobQueue;
  rest::Scheduler* _scheduler;
};
}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

JobQueue::JobQueue(size_t maxQueueSize, rest::Scheduler* scheduler)
    : _maxQueueSize(static_cast<int64_t>(maxQueueSize)),
      _queue(maxQueueSize == 0 ? 512 : maxQueueSize),
      _queueSize(0),
      _queueThread(new JobQueueThread(this, scheduler)) {}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

void JobQueue::start() { 
  if (!_queueThread->start()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FAILED, "unable to start jobqueue thread");
  }
}

void JobQueue::beginShutdown() { _queueThread->beginShutdown(); }

void JobQueue::wakeup() {
  CONDITION_LOCKER(guard, _queueCondition);
  guard.signal();
}
