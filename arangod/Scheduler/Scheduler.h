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

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>

#include "Futures/Future.h"
#include "Futures/Unit.h"
#include "Futures/Utilities.h"

#include "Basics/Exceptions.h"
#include "Basics/system-compiler.h"
#include "GeneralServer/RequestLane.h"

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
namespace velocypack {
class Builder;
}

class LogTopic;
class SchedulerThread;
class SchedulerCronThread;

class Scheduler {
 public:
  explicit Scheduler(application_features::ApplicationServer&);
  virtual ~Scheduler();

  // ---------------------------------------------------------------------------
  // Scheduling and Task Queuing - the relevant stuff
  // ---------------------------------------------------------------------------
 public:
  class WorkItem;
  typedef std::chrono::steady_clock clock;
  typedef std::shared_ptr<WorkItem> WorkHandle;

  // Enqueues a task - this is implemented on the specific scheduler
  // May throw.
  virtual bool queue(RequestLane lane, fu2::unique_function<void()>) ADB_WARN_UNUSED_RESULT = 0;

  // Enqueues a task after delay - this uses the queue functions above.
  // WorkHandle is a shared_ptr to a WorkItem. If all references the WorkItem
  // are dropped, the task is canceled. It will return true if queued, false
  // otherwise
  virtual std::pair<bool, WorkHandle> queueDelay(RequestLane lane, clock::duration delay,
                                                 fu2::unique_function<void(bool canceled)> handler);

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

    explicit WorkItem(fu2::unique_function<void(bool canceled)>&& handler,
                      RequestLane lane, Scheduler* scheduler)
        : _handler(std::move(handler)), _lane(lane), _disable(false), _scheduler(scheduler) {}

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
        bool queued = _scheduler->queue(_lane, [handler = std::move(_handler),
                                                arg]() mutable  {
                                                  handler(arg);
                                                });
        if (!queued) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_QUEUE_FULL);
        }
      }
    }
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    bool isDisabled() const { return _disable.load(); }
    friend class Scheduler;
#endif

   private:
    fu2::unique_function<void(bool)> _handler;
    RequestLane _lane;
    std::atomic<bool> _disable;
    Scheduler* _scheduler;
  };

 protected:
  application_features::ApplicationServer& _server;

 public:
    // delay Future returns a future that will be fulfilled after the given duration
    // requires scheduler
    // If d is zero, the future is fulfilled immediately. Throws a logic error
    // if delay was cancelled.
    futures::Future<futures::Unit> delay(clock::duration d) {
      if (d == clock::duration::zero()) {
        return futures::makeFuture();
      }

      futures::Promise<bool> p;
      futures::Future<bool> f = p.getFuture();

      auto item = queueDelay(RequestLane::DELAYED_FUTURE, d,
        [pr = std::move(p)](bool cancelled) mutable { pr.setValue(cancelled); });

      return std::move(f).thenValue([item = std::move(item)](bool cancelled) {
        if (cancelled) {
          throw std::logic_error("delay was cancelled");
        }
      });
    }

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
    uint64_t _running; // numWorkers
    uint64_t _queued;
    uint64_t _working;
  };

  virtual void toVelocyPack(velocypack::Builder&) const = 0;
  virtual QueueStatistics queueStatistics() const = 0;

  /// @brief approximate fill grade of the scheduler's queue (in %)
  virtual double approximateQueueFillGrade() const = 0;
  
  /// @brief fill grade of the scheduler's queue (in %) from which onwards
  /// the server is considered unavailable (because of overload)
  virtual double unavailabilityQueueFillGrade() const = 0;

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
