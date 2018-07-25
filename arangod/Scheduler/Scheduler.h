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

class SchedulerContextThread;
class SchedulerWorkerThread;
class SchedulerManagerThread;

class Scheduler {
  Scheduler(Scheduler const&) = delete;
  Scheduler& operator=(Scheduler const&) = delete;



 public:
  Scheduler(uint64_t minThreads, uint64_t maxThreads, uint64_t maxQueueSize,
            uint64_t fifo1Size, uint64_t fifo2Size);
  virtual ~Scheduler();

  struct QueueStatistics {
    uint64_t _running;
    uint64_t _working;
    uint64_t _queued;
  };

  void post(std::function<void()> const& callback);
  bool queue(RequestPriority prio, std::function<void()> const&);

  //void post(uint64_t time, std::function<void()> const& callback);

  void addQueueStatistics(velocypack::Builder&) const;
  QueueStatistics queueStatistics() const;
  std::string infoStatus();

private:
  std::atomic<size_t> _numWorker;
  std::atomic<bool> _stopping;

public:
  bool isRunning() const { return _numWorker > 0; }
  bool isStopping() const noexcept { return _stopping; }

  bool start();
  void beginShutdown();
  void shutdown();


  template <typename T>
  asio_ns::deadline_timer* newDeadlineTimer(T timeout) {
    return new asio_ns::deadline_timer(_obsoleteContext, timeout);
  }

  asio_ns::steady_timer* newSteadyTimer() {
    return new asio_ns::steady_timer(_obsoleteContext);
  }

  asio_ns::signal_set* newSignalSet() {
    return new asio_ns::signal_set(_obsoleteContext);
  }



 private:
  friend class SchedulerManagerThread;
  friend class SchedulerWorkerThread;
  friend class SchedulerContextThread;



  struct WorkItem {
    std::function<void()> _handler;

    WorkItem(std::function<void()> const& handler) : _handler(handler) {}
    WorkItem(std::function<void()> && handler) : _handler(std::move(handler)) {}
    void operator() () { _handler(); }
  };

  // Since the lockfree queue can only handle PODs, one has to wrap lambdas
  // in a container class and store pointers. -- Maybe there is a better way?
  boost::lockfree::queue<WorkItem*> _queue[3];

  std::atomic<uint64_t> _jobsSubmitted, _jobsDone;

  std::atomic<uint64_t> _wakeupQueueLength;                        // q1
  std::atomic<uint64_t> _wakeupTime_ns, _definitiveWakeupTime_ns;  // t3, t4


  struct WorkerState {
    uint64_t _queueRetryCount;     // t1
    uint64_t _sleepTimeout_ms;    // t2
    bool _stop;

    std::unique_ptr<SchedulerWorkerThread> _thread;

    // initialise with harmless defaults: spin once, sleep forever
    WorkerState(Scheduler &scheduler);
  };

  size_t _maxNumWorker;
  size_t _numIdleWorker;
  std::vector<WorkerState> _workerStates;

  std::mutex _mutex;
  std::condition_variable _conditionWork;

  // This asio context is here to provide functionallity
  // for signal handlers, deadline and steady timer.
  // It runs in a seperate thread.
  //
  // However in feature this should be replaced:
  //  Steady and deadline timer could be implemented using a priority queue
  asio_ns::io_context _obsoleteContext;
  asio_ns::io_context::work _obsoleteWork;

  void runWorker();
  void runSupervisor();

  std::mutex _mutexSupervisor;
  std::condition_variable _conditionSupervisor;
  std::unique_ptr<SchedulerManagerThread> _manager;

  bool getWork (WorkItem *&, WorkerState &);

  void startOneThread();
  void stopOneThread();

  std::unique_ptr<SchedulerContextThread> _contextThread;
};
}
}

#endif
