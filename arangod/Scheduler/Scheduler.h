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

#include "Basics/Common.h"

#include <boost/lockfree/queue.hpp>

#include <mutex>
#include <condition_variable>
#include <queue>

#include "Basics/Mutex.h"
#include "Basics/asio_ns.h"
#include "Basics/socket-utils.h"
#include "Endpoint/Endpoint.h"
#include "GeneralServer/RequestLane.h"
#include "Scheduler/SchedulerFeature.h"

namespace arangodb {
class ListenTask;
class SchedulerThread;

namespace velocypack {
class Builder;
}

namespace rest {
class GeneralCommTask;
class SocketTask;

class SchedulerCronThread;

class Scheduler {
  Scheduler(Scheduler const&) = delete;
  Scheduler& operator=(Scheduler const&) = delete;

 public:
  Scheduler();
  virtual ~Scheduler();

  struct QueueStatistics {
    uint64_t _running;
    uint64_t _working;
    uint64_t _queued;
    uint64_t _fifo1;
    uint64_t _fifo2;
    uint64_t _fifo3;
  };


public:
  virtual void addQueueStatistics(velocypack::Builder&) const = 0;
  virtual QueueStatistics queueStatistics() const = 0;
  virtual std::string infoStatus() const = 0;

private:
  std::atomic<size_t> _numWorker;
  std::atomic<bool> _stopping;

public:
  virtual bool isRunning() const = 0;
  virtual bool isStopping() const noexcept = 0;

  virtual bool start();
  virtual void beginShutdown();
  virtual void shutdown();


private:
  class WorkItem;
public:
  typedef std::chrono::steady_clock clock;
  typedef std::shared_ptr<WorkItem> WorkHandle;

  // Enqueues a task at given priority
  virtual bool queue(RequestLane lane, std::function<void()> const&) = 0;

  // postDelay returns a WorkHandler. You can cancel the job by calling cancel. The job is also
  // cancelled if all WorkHandles are destructed. To disable this behavior call detach.
  WorkHandle queueDelay(
    RequestLane lane,
    clock::duration delay,
    std::function<void(bool cancelled)> const& handler
  );

  // TODO: This method is unintuitive to use. If you call it and you are not interested in canceling
  // you have to call detach(). Otherwise the WorkHandle goes out of scope and the task is canceled
  // immediately.


  // A spinning threads looks at the queues in a cyclic way, i.e.
  // 0 1 2 0 1 2 0 1 2 0 1 2 ..


private:
  class WorkItem : std::enable_shared_from_this<WorkItem> {
    std::function<void(bool)> _handler;
    RequestLane _lane;
    std::atomic<bool> _disable;
    Scheduler *_scheduler;

  public:
    // Default constructor for weak_ptr::lock to return a "disabled" work item
    WorkItem() :
      _disable(true),
      _scheduler(nullptr)
    {}

    // This is not copyable or moveable
    WorkItem(WorkItem const&) = delete;
    WorkItem(WorkItem &&) = delete;
    void operator=(WorkItem const&) const = delete;

    WorkItem(std::function<void(bool cancelled)> const& handler, RequestLane lane, Scheduler *scheduler) :
      _handler(handler),
      _lane(lane),
      _disable(false),
      _scheduler(scheduler)
    {}

    WorkItem(std::function<void(bool cancelled)> && handler, RequestLane lane, Scheduler *scheduler) :
      _handler(std::move(handler)),
      _lane(lane),
      _disable(false),
      _scheduler(scheduler)
    {}

    ~WorkItem() { cancel(); }

  private:
    void doWith(bool argCancelled) {
      bool disabled = false;
      if (_disable.compare_exchange_strong(disabled, true)) {
        auto self(shared_from_this());
        if (!disabled) {
          // Here we need the RequestLane to do the right queue call
          _scheduler->queue(_lane, [this, self, argCancelled]() {
            this->_handler(argCancelled);
          });
        }
      }
    }

    void cancel() {
      doWith(true);
    }

    void run() {
      doWith(false);
    }

    friend Scheduler;
  };


 private:
  friend class SchedulerCronThread;

  // The priority queue is managed by a Cron Thread. It wakes up on a regular basis (10ms currently)
  // and looks at queue.top(). It the _expire time is smaller than now() and the task is not canceled
  // it is posted on the scheduler. The next sleep time is computed depending on queue top.
  //
  // Note that tasks that have a delay of less than 1ms are posted directly.
  // For tasks above 50ms the Cron Thread is woken up to potentially update its sleep time, which
  // could now be shorter than before.
  void runCron();

  typedef std::pair<
    clock::time_point,
    std::weak_ptr<WorkItem>
  > CronWorkItem;

  struct CronWorkItemCompare {
    bool operator()(CronWorkItem const &left,
      CronWorkItem const &right) {
      // Reverse order, because std::priority_queue is a max heap.
      return right.first < left.first;
    }
  };

  std::priority_queue<
    CronWorkItem,
    std::vector<CronWorkItem>,
    CronWorkItemCompare
  > _cronQueue;

  std::mutex _cronQueueMutex;
  std::condition_variable _croncv;

  std::unique_ptr<SchedulerCronThread> _cronThread;
};
}  // namespace rest
}  // namespace arangodb

#endif
