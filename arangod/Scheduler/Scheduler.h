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

namespace arangodb {
class JobGuard;
class ListenTask;

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
  };

  // Enqueues a task at highest priority
  virtual void post(std::function<void()> const& callback) = 0;
  // Enqueues a task at given priority
  virtual bool queue(RequestPriority prio, std::function<void()> const&) = 0;

  // A spinning threads looks at the queues in a cyclic way, i.e.
  // 0 1 2 0 1 2 0 1 2 0 1 2 ..

  typedef std::chrono::steady_clock clock;

private:
  struct DelayedWorkItem {
    std::function<void(bool cancelled)> _handler;
    const clock::time_point _due;
    std::atomic<bool> _cancelled;

    DelayedWorkItem() {};

    DelayedWorkItem(std::function<void(bool cancelled)> const& handler, clock::duration delay) :
      _handler(handler), _due(clock::now() + delay), _cancelled(false) {}

    DelayedWorkItem(std::function<void(bool cancelled)> && handler, clock::duration delay) :
      _handler(std::move(handler)), _due(clock::now() + delay), _cancelled(false) {}

    void cancel() { _cancelled = true; };
    bool operator <(const DelayedWorkItem & rhs) const
    {
      return _due < rhs._due;
    }
  };

  class WorkGuard {
    std::shared_ptr<DelayedWorkItem> _item;
  public:
    WorkGuard() {};
    WorkGuard(std::shared_ptr<DelayedWorkItem> &item) : _item(item) {}
    ~WorkGuard() { cancel(); }
    void cancel() { if(_item) { _item->cancel(); } }
    void detach() { _item.reset(); }
  };

public:
  class WorkHandle {
    std::shared_ptr<WorkGuard> _guard;
  public:
    WorkHandle() {};
    WorkHandle(std::shared_ptr<DelayedWorkItem> &item) : _guard(std::make_shared<WorkGuard>(item)) {}
    WorkHandle(WorkHandle const &handle) : _guard(handle._guard) {}
    void cancel() { _guard->cancel(); }
    void detach() { _guard->detach(); }
  };

  // postDelay returns a WorkHandler. You can cancel the job by calling cancel. The job is also
  // cancelled if all WorkHandles are destructed. To disable this behavior call detach.
  WorkHandle postDelay(clock::duration delay, std::function<void(bool cancelled)> const& callback);

  // TODO: This method is unintuitive to use. If you call it and you are not interested in canceling
  // you have to call detach(). Otherwise the WorkHandle goes out of scope and the task is canceled
  // immediately.

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
  friend class SchedulerCronThread;

  // The priority queue is managed by a Cron Thread. It wakes up on a regular basis (10ms currently)
  // and looks at queue.top(). It the _due time is smaller than now() and the task is not canceled
  // it is posted on the scheduler. The next sleep time is computed depending on queue top.
  //
  // Note that tasks that have a delay of less than 1ms are posted directly.
  // For tasks above 50ms the Cron Thread is woken up to potentially update its sleep time, which
  // could now be shorter than before.

  std::priority_queue<std::shared_ptr<DelayedWorkItem>,
                      std::vector<std::shared_ptr<DelayedWorkItem>>,
                      std::greater<std::shared_ptr<DelayedWorkItem>>> _priorityQueue;
  std::mutex _priorityQueueMutex;
  std::condition_variable _conditionCron;

  void runCron();

  std::unique_ptr<SchedulerCronThread> _cronThread;
};
}
}

#endif
