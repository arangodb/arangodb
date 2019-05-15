////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_SCHEDULER_SCHEDULER_H
#define ARANGOD_SCHEDULER_SCHEDULER_H 1

#include <condition_variable>
#include <mutex>
#include <queue>

#include "GeneralServer/RequestLane.h"
#include "Logger/Logger.h"

namespace arangodb {

namespace velocypack {
class Builder;
}

class Scheduler;
class SchedulerThread;
class SchedulerCronThread;

class Scheduler {
 public:
  explicit Scheduler();
  virtual ~Scheduler();

  // ---------------------------------------------------------------------------
  // Scheduling and Task Queuing - the relevant stuff
  // ---------------------------------------------------------------------------
 public:
  class WorkItem;
  typedef std::chrono::steady_clock clock;
  typedef std::shared_ptr<WorkItem> WorkHandle;

  // Enqueues a task - this is implemented on the specific scheduler
  virtual bool queue(RequestLane lane, std::function<void()>) = 0;

  // Enqueues a task after delay - this uses the queue functions above.
  // WorkHandle is a shared_ptr to a WorkItem. If all references the WorkItem
  // are dropped, the task is canceled.
  virtual WorkHandle queueDelay(RequestLane lane, clock::duration delay,
                                std::function<void(bool canceled)> handler);

  class WorkItem final {
   public:
    ~WorkItem() { 
      try {
        cancel(); 
      } catch (...) {
        // destructor... no exceptions allowed here
      }
    }

    // Cancels the WorkItem
    void cancel() { executeWithCancel(true); }

    // Runs the WorkItem immediately
    void run() { executeWithCancel(false); }

    explicit WorkItem(std::function<void(bool canceled)>&& handler,
                      RequestLane lane, Scheduler* scheduler)
        : _handler(std::move(handler)), _lane(lane), _disable(false), _scheduler(scheduler){};

   private:
    // This is not copyable or movable
    WorkItem(WorkItem const&) = delete;
    WorkItem(WorkItem&&) = delete;
    void operator=(WorkItem const&) const = delete;

    inline void executeWithCancel(bool arg) {
      bool disabled = _disable.exchange(true);
      // If exchange returns false, the item was not yet scheduled.
      // Hence we are the first dealing with this WorkItem
      if (disabled == false) {
        // The following code moves the _handler into the Scheduler.
        // Thus any reference to class to self in the _handler will be released
        // as soon as the scheduler executed the _handler lambda.
        _scheduler->queue(_lane, [handler = std::move(_handler), arg]() {
          handler(arg);
        });
      }
    }
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    bool isDisabled() const { return _disable.load(); }
    friend class Scheduler;
#endif

   private:
    std::function<void(bool)> _handler;
    RequestLane _lane;
    std::atomic<bool> _disable;
    Scheduler* _scheduler;
  };

  // ---------------------------------------------------------------------------
  // CronThread and delayed tasks
  // ---------------------------------------------------------------------------
 private:
  // The priority queue is managed by a CronThread. It wakes up on a regular basis (10ms currently)
  // and looks at queue.top(). It the _expire time is smaller than now() and the task is not canceled
  // it is posted on the scheduler. The next sleep time is computed depending on queue top.
  //
  // Note that tasks that have a delay of less than 1ms are posted directly.
  // For tasks above 50ms the CronThread is woken up to potentially update its sleep time, which
  // could now be shorter than before.

  // Entry point for the CronThread
  void runCronThread();
  friend class SchedulerCronThread;

  // Removed all tasks from the priority queue and cancels them
  void cancelAllCronTasks();

  typedef std::pair<clock::time_point, std::weak_ptr<WorkItem>> CronWorkItem;

  struct CronWorkItemCompare {
    bool operator()(CronWorkItem const& left, CronWorkItem const& right) const {
      // Reverse order, because std::priority_queue is a max heap.
      return right.first < left.first;
    }
  };

  std::priority_queue<CronWorkItem, std::vector<CronWorkItem>, CronWorkItemCompare> _cronQueue;

  std::mutex _cronQueueMutex;
  std::condition_variable _croncv;
  std::unique_ptr<SchedulerCronThread> _cronThread;

  // ---------------------------------------------------------------------------
  // Statistics stuff
  // ---------------------------------------------------------------------------
 public:
  struct QueueStatistics {
    uint64_t _running;
    uint64_t _working;
    uint64_t _queued;
    uint64_t _fifo1;
    uint64_t _fifo2;
    uint64_t _fifo3;
  };

  virtual void addQueueStatistics(velocypack::Builder&) const = 0;
  virtual QueueStatistics queueStatistics() const = 0;
  virtual std::string infoStatus() const = 0;

  // ---------------------------------------------------------------------------
  // Start/Stop/IsRunning stuff
  // ---------------------------------------------------------------------------
 public:
  virtual bool start();
  virtual void shutdown();

 protected:
  // You wondering why Scheduler::isStopping() no longer works for you?
  // Go away and use `application_features::ApplicationServer::isStopping()`
  // It is made for people that want to know if the should stop doing things.
  virtual bool isStopping() = 0;

 private:
  Scheduler(Scheduler const&) = delete;
  Scheduler(Scheduler&&) = delete;
  void operator=(Scheduler const&) = delete;
};

}  // namespace arangodb

#endif
