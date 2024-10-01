////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <string_view>
#include <utility>

#include "Futures/Future.h"
#include "Futures/Unit.h"
#include "Futures/Utilities.h"

#include "Basics/Exceptions.h"
#include "Basics/system-compiler.h"
#include "GeneralServer/RequestLane.h"
#include "RestServer/arangod.h"
#include "Logger/LogContext.h"

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
  explicit Scheduler(ArangodServer&);
  virtual ~Scheduler();

  // ---------------------------------------------------------------------------
  // Scheduling and Task Queuing - the relevant stuff
  // ---------------------------------------------------------------------------
  class DelayedWorkItem;
  typedef std::chrono::steady_clock clock;
  typedef std::shared_ptr<DelayedWorkItem> WorkHandle;

  // push an item onto the queue. does not indicate success or failure
  // by returning a value, but will throw if the item could not be pushed
  // onto the queue. as the function is marked noexcept, this will also
  // lead to the program aborting with std::terminate.
  template<typename F,
           std::enable_if_t<std::is_class_v<std::decay_t<F>>, int> = 0>
  void queue(RequestLane lane, F&& fn) noexcept {
    // doQueue() will always return true for unbounded queues.
    // in the case of failure, doQueue() will throw
    [[maybe_unused]] bool result =
        doQueue(lane, std::forward<F>(fn), /*bounded*/ false);
    TRI_ASSERT(result);
  }
  template<typename F, typename R = std::invoke_result_t<F>,
           std::enable_if_t<std::is_class_v<std::decay_t<F>>, int> = 0>
  futures::Future<R> queueWithFuture(RequestLane lane, F&& fn) {
    auto p = futures::Promise<R>{};
    auto f = p.getFuture();
    queue(lane, [p = std::move(p), fn = std::forward<F>(fn)]() mutable {
      p.setValue(std::forward<F>(fn)());
    });
    return f;
  }

  // push an item onto the queue. indicates success or failure by returning
  // a boolean value (true = queueing successful, false = queueing failed)
  template<typename F,
           std::enable_if_t<std::is_class_v<std::decay_t<F>>, int> = 0>
  [[nodiscard]] bool tryBoundedQueue(RequestLane lane, F&& fn) noexcept {
    return doQueue(lane, std::forward<F>(fn), true);
  }

  // Enqueues a task after delay - this uses the queue functions above.
  // WorkHandle is a shared_ptr to a DelayedWorkItem. If all references the
  // DelayedWorkItem are dropped, the task is canceled.
  [[nodiscard]] virtual WorkHandle queueDelayed(
      std::string_view name, RequestLane lane, clock::duration delay,
      fu2::unique_function<void(bool canceled)> handler) noexcept;

  // Returns the scheduler's server object
  ArangodServer& server() noexcept { return _server; }

  struct WorkItemBase {
    virtual ~WorkItemBase() { TRI_ASSERT(next == nullptr); }
    virtual void invoke() = 0;

    std::chrono::steady_clock::time_point enqueueTime;

    // used by some schedulers to chain work items
    WorkItemBase* next = nullptr;
  };

  class DelayedWorkItem {
   public:
    ~DelayedWorkItem() {
      try {
        cancel();
      } catch (...) {
        // destructor... no exceptions allowed here
      }
    }

    // Cancels the DelayedWorkItem
    void cancel() { executeWithCancel(true); }

    // Runs the DelayedWorkItem immediately
    void run() { executeWithCancel(false); }

    explicit DelayedWorkItem(
        std::string_view name,
        fu2::unique_function<void(bool canceled)>&& handler, RequestLane lane,
        Scheduler* scheduler)
        : _name(name),
          _handler(std::move(handler)),
          _lane(lane),
          _disable(false),
          _scheduler(scheduler) {}

    // This is not copyable or movable
    DelayedWorkItem(DelayedWorkItem const&) = delete;
    DelayedWorkItem(DelayedWorkItem&&) noexcept = delete;
    void operator=(DelayedWorkItem const&) = delete;
    void operator=(DelayedWorkItem&&) noexcept = delete;

    std::string_view name() const noexcept { return _name; }

   private:
    inline void executeWithCancel(bool arg) {
      bool disabled = _disable.exchange(true);
      // If exchange returns false, the item was not yet scheduled.
      // Hence we are the first dealing with this DelayedWorkItem
      if (disabled == false) {
        // The following code moves the _handler into the Scheduler.
        // Thus any reference to class to self in the _handler will be released
        // as soon as the scheduler executed the _handler lambda.
        _scheduler->queue(_lane, [handler = std::move(_handler),
                                  arg]() mutable { handler(arg); });
      }
    }
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    bool isDisabled() const { return _disable.load(); }
    friend class Scheduler;
#endif

   private:
    std::string_view _name;
    fu2::unique_function<void(bool)> _handler;
    RequestLane _lane;
    std::atomic<bool> _disable;
    Scheduler* _scheduler;
  };

 protected:
  ArangodServer& _server;

  template<typename F>
  struct WorkItem final : WorkItemBase, F {
    explicit WorkItem(F f)
        : F(std::move(f)), logContext(LogContext::current()) {
      schedulerJobMemoryAccounting(static_cast<int64_t>(sizeof(*this)));
    }
    ~WorkItem() {
      schedulerJobMemoryAccounting(-static_cast<int64_t>(sizeof(*this)));
    }
    void invoke() override {
      LogContext::ScopedContext ctxGuard(logContext);
      this->operator()();
    }

   private:
    LogContext logContext;
  };

  // Enqueues a task - this is implemented on the specific scheduler
  // May throw.
  [[nodiscard]] virtual bool queueItem(RequestLane lane,
                                       std::unique_ptr<WorkItemBase> item,
                                       bool bounded) = 0;

 private:
  template<typename F,
           std::enable_if_t<std::is_class_v<std::decay_t<F>>, int> = 0>
  [[nodiscard]] bool doQueue(RequestLane lane, F&& fn, bool bounded) {
    auto item = std::make_unique<Scheduler::WorkItem<std::decay_t<F>>>(
        std::forward<F>(fn));
    auto result = queueItem(lane, std::move(item), bounded);
    ADB_PROD_ASSERT(result || bounded);
    return result;
  }

 public:
  // delay Future. returns a future that will be fulfilled after the given
  // duration expires. If d is zero or we cannot post the future
  // to the scheduler, the future is fulfilled immediately.
  // Throws a logic error if delay was cancelled.
  futures::Future<futures::Unit> delay(std::string_view name,
                                       clock::duration d) {
    if (d == clock::duration::zero()) {
      return futures::makeFuture();
    }

    futures::Promise<bool> p;
    futures::Future<bool> f = p.getFuture();

    auto item = queueDelayed(name, RequestLane::DELAYED_FUTURE, d,
                             [pr = std::move(p)](bool cancelled) mutable {
                               pr.setValue(cancelled);
                             });

    if (item == nullptr) {
      return futures::makeFuture();
    }

    return std::move(f).thenValue([item = std::move(item)](bool cancelled) {
      if (cancelled) {
        throw std::logic_error("delay was cancelled");
      }
    });
  }

  // Yield the current thread
  futures::Future<futures::Unit> yield(
      RequestLane lane = RequestLane::CONTINUATION) {
    struct awaitable {
      bool await_ready() { return false; }
      void await_suspend(std::coroutine_handle<> coro) {
        sched->queue(lane, [coro] { coro.resume(); });
      }
      void await_resume() {}
      Scheduler* sched;
      RequestLane lane;
    };

    co_await awaitable{this, lane};
  }

  // ---------------------------------------------------------------------------
  // CronThread and delayed tasks
  // ---------------------------------------------------------------------------
 private:
  // The priority queue is managed by a CronThread. It wakes up on a regular
  // basis (10ms currently) and looks at queue.top(). It the _expire time is
  // smaller than now() and the task is not canceled it is posted on the
  // scheduler. The next sleep time is computed depending on queue top.
  //
  // Note that tasks that have a delay of less than 1ms are posted directly.
  // For tasks above 50ms the CronThread is woken up to potentially update its
  // sleep time, which could now be shorter than before.

  // Entry point for the CronThread
  void runCronThread();
  friend class SchedulerCronThread;

  typedef std::pair<clock::time_point, std::weak_ptr<DelayedWorkItem>>
      CronWorkItem;

  struct CronWorkItemCompare {
    bool operator()(CronWorkItem const& left, CronWorkItem const& right) const {
      // Reverse order, because std::priority_queue is a max heap.
      return right.first < left.first;
    }
  };

  std::priority_queue<CronWorkItem, std::vector<CronWorkItem>,
                      CronWorkItemCompare>
      _cronQueue;

  std::mutex _cronQueueMutex;
  std::condition_variable _croncv;
  std::unique_ptr<SchedulerCronThread> _cronThread;

  // ---------------------------------------------------------------------------
  // Statistics stuff
  // ---------------------------------------------------------------------------
 public:
  struct QueueStatistics {
    uint64_t _running;  // numWorkers
    uint64_t _queued;
    uint64_t _working;
  };

  virtual void toVelocyPack(velocypack::Builder&) const = 0;
  virtual QueueStatistics queueStatistics() const = 0;

  virtual void trackCreateHandlerTask() noexcept = 0;
  virtual void trackBeginOngoingLowPriorityTask() noexcept = 0;
  virtual void trackEndOngoingLowPriorityTask() noexcept = 0;

  virtual void trackQueueTimeViolation() = 0;
  virtual void trackQueueItemSize(std::int64_t) noexcept = 0;

  /// @brief returns the last stored dequeue time [ms]
  virtual uint64_t getLastLowPriorityDequeueTime() const noexcept = 0;

  /// @brief set the time it took for the last low prio item to be dequeued
  /// (time between queuing and dequeing) [ms]
  virtual void setLastLowPriorityDequeueTime(uint64_t time) noexcept = 0;

  /// @brief get information about low prio queue:
  virtual std::pair<uint64_t, uint64_t> getNumberLowPrioOngoingAndQueued()
      const = 0;

  /// @brief approximate fill grade of the scheduler's queue (in %)
  virtual double approximateQueueFillGrade() const = 0;

  /// @brief fill grade of the scheduler's queue (in %) from which onwards
  /// the server is considered unavailable (because of overload)
  virtual double unavailabilityQueueFillGrade() const = 0;

  // ---------------------------------------------------------------------------
  // Start/Stop/IsRunning stuff
  // ---------------------------------------------------------------------------
  virtual bool start();
  virtual void shutdown();

 protected:
  // You wondering why Scheduler::isStopping() no longer works for you?
  // Go away and use `application_features::ApplicationServer::isStopping()`
  // It is made for people that want to know if the should stop doing things.
  virtual bool isStopping() = 0;

 private:
  static void schedulerJobMemoryAccounting(std::int64_t x) noexcept;

  Scheduler(Scheduler const&) = delete;
  Scheduler(Scheduler&&) = delete;
  void operator=(Scheduler const&) = delete;
};

}  // namespace arangodb
