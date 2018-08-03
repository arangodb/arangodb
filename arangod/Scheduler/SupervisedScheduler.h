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

#ifndef ARANGOD_SUPERIVSED_SCHEDULER_SCHEDULER_H
#define ARANGOD_SUPERIVSED_SCHEDULER_SCHEDULER_H 1

#include "Scheduler/Scheduler.h"

namespace arangodb {

namespace rest {

class SupervisedSchedulerWorkerThread;
class SupervisedSchedulerManagerThread;

class SupervisedScheduler : public Scheduler {

 public:
  SupervisedScheduler(uint64_t minThreads, uint64_t maxThreads, uint64_t maxQueueSize,
            uint64_t fifo1Size, uint64_t fifo2Size);
  virtual ~SupervisedScheduler();

  void post(std::function<void()> const& callback) override;
  bool queue(RequestPriority prio, std::function<void()> const&) override;

private:
  std::atomic<size_t> _numWorker;
  std::atomic<bool> _stopping;

public:
  bool isRunning() const override { return _numWorker > 0; };
  bool isStopping() const noexcept override { return _stopping; };

  bool start() override;
  void beginShutdown() override;
  void shutdown() override;

  void addQueueStatistics(velocypack::Builder&) const;
  Scheduler::QueueStatistics queueStatistics() const;
  std::string infoStatus() const;

 private:
  friend class SupervisedSchedulerManagerThread;
  friend class SupervisedSchedulerWorkerThread;

  struct WorkItem {
    std::function<void()> _handler;

    WorkItem(std::function<void()> const& handler) : _handler(handler) {}
    WorkItem(std::function<void()> && handler) : _handler(std::move(handler)) {}
    virtual ~WorkItem() {}

    virtual void operator() () { _handler(); }
  };

  // Since the lockfree queue can only handle PODs, one has to wrap lambdas
  // in a container class and store pointers. -- Maybe there is a better way?
  boost::lockfree::queue<WorkItem*> _queue[3];

  std::atomic<uint64_t> _jobsSubmitted, _jobsDone;

  // During a queue operation there a two reasons to manually wake up a worker
  //  1. the queue length is bigger than _wakeupQueueLength and the last submit time
  //      is bigger than _wakeupTime_ns.
  //  2. the last submit time is bigger than _definitiveWakeupTime_ns.
  //
  // The last submit time is a thread local variable that stores the time of the last
  // queue operation.
  std::atomic<uint64_t> _wakeupQueueLength;                        // q1
  std::atomic<uint64_t> _wakeupTime_ns, _definitiveWakeupTime_ns;  // t3, t4


  // each worker thread has a state block which contains configuration values.
  // _queueRetryCount is the number of spins this particular thread should perform
  // before going to sleep.
  // _sleepTimeout_ms is the amount of ms the thread should sleep before waking
  // up again. Note that each worker wakes up constantly, even if there is no work.
  //
  // All those values are maintained by the supervisor thread.
  // Currently they are set once and for all the same, however a future
  // implementation my alter those values for each thread individually.
  struct WorkerState {
    uint64_t _queueRetryCount;     // t1
    uint64_t _sleepTimeout_ms;    // t2
    std::atomic<bool> _stop;
    std::unique_ptr<SupervisedSchedulerWorkerThread> _thread;
    char padding[40];

    // initialize with harmless defaults: spin once, sleep forever
    WorkerState(SupervisedScheduler &scheduler);
    WorkerState(WorkerState &&that);

    bool start();
  };

  size_t _maxNumWorker;
  size_t _numIdleWorker;
  std::vector<WorkerState> _workerStates;

  std::mutex _mutex;
  std::condition_variable _conditionWork;

  void runWorker();
  void runSupervisor();

  std::mutex _mutexSupervisor;
  std::condition_variable _conditionSupervisor;
  std::unique_ptr<SupervisedSchedulerManagerThread> _manager;

  std::unique_ptr<WorkItem> getWork (WorkerState &state);

  void startOneThread();
  void stopOneThread();

};
}
}

#endif
