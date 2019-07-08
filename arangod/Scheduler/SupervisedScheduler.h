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

#include <boost/lockfree/queue.hpp>
#include <condition_variable>
#include <list>
#include <mutex>
#include <queue>

#include "Logger/Logger.h"
#include "Scheduler/Scheduler.h"

namespace arangodb {
class SupervisedSchedulerWorkerThread;
class SupervisedSchedulerManagerThread;

class SupervisedScheduler final : public Scheduler {
 public:
  SupervisedScheduler(uint64_t minThreads, uint64_t maxThreads, uint64_t maxQueueSize,
                      uint64_t fifo1Size, uint64_t fifo2Size);
  virtual ~SupervisedScheduler();

  bool queue(RequestLane lane, std::function<void()>, bool allowDirectHandling = false) override;

 private:
  std::atomic<size_t> _numWorker;
  std::atomic<bool> _stopping;

 protected:
  bool isStopping() override { return _stopping; }

 public:
  bool start() override;
  void shutdown() override;

  void addQueueStatistics(velocypack::Builder&) const override;
  Scheduler::QueueStatistics queueStatistics() const override;
  std::string infoStatus() const override;

 private:
  friend class SupervisedSchedulerManagerThread;
  friend class SupervisedSchedulerWorkerThread;

  struct WorkItem final {
    std::function<void()> _handler;
    double _startTime;
    bool _called;

    explicit WorkItem(std::function<void()> const& handler)
        : _handler(handler), _startTime(TRI_microtime()), _called(false) {}
    explicit WorkItem(std::function<void()>&& handler)
        : _handler(std::move(handler)), _startTime(TRI_microtime()), _called(false) {}
    ~WorkItem() {
      if (!_called && (TRI_microtime() - _startTime) > 0.001) {
        LOG_TOPIC("hunde", ERR, arangodb::Logger::REPLICATION)
            << "Work item forgotten, created at " << _startTime << " that is "
            << (TRI_microtime() - _startTime) << "s ago.";
      }
    }

    void operator()() {
      _called = true;
      auto waittime = TRI_microtime() - _startTime;
      if (waittime > 0.4) {
        LOG_TOPIC("hunde", ERR, arangodb::Logger::REPLICATION)
            << "Long queue wait time: " << waittime << "s";
      }
      _handler();
    }
  };

  // Since the lockfree queue can only handle PODs, one has to wrap lambdas
  // in a container class and store pointers. -- Maybe there is a better way?
  boost::lockfree::queue<WorkItem*> _queue[3];

  // aligning required to prevent false sharing - assumes cache line size is 64
  alignas(64) std::atomic<uint64_t> _jobsSubmitted;
  alignas(64) std::atomic<uint64_t> _jobsDequeued;
  alignas(64) std::atomic<uint64_t> _jobsDone;
  alignas(64) std::atomic<uint64_t> _jobsDirectExec;

  // During a queue operation there a two reasons to manually wake up a worker
  //  1. the queue length is bigger than _wakeupQueueLength and the last submit time
  //      is bigger than _wakeupTime_ns.
  //  2. the last submit time is bigger than _definitiveWakeupTime_ns.
  //
  // The last submit time is a thread local variable that stores the time of the last
  // queue operation.
  alignas(64) std::atomic<uint64_t> _wakeupQueueLength;            // q1
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
  //
  // _lastJobStarted is the timepoint when the last job in this thread was started.
  // _working indicates if the thread is currently processing a job.
  //    Hence if you want to know, if the thread has a long running job, test for
  //    _working && (now - _lastJobStarted) > eps

  struct alignas(64) WorkerState {
    uint64_t _queueRetryCount;  // t1
    uint64_t _sleepTimeout_ms;  // t2
    std::atomic<bool> _stop, _working;
    clock::time_point _lastJobStarted;
    std::unique_ptr<SupervisedSchedulerWorkerThread> _thread;

    // initialize with harmless defaults: spin once, sleep forever
    explicit WorkerState(SupervisedScheduler& scheduler);
    WorkerState(WorkerState&& that) noexcept;

    bool start();
  };
  size_t _maxNumWorker;
  size_t _numIdleWorker;
  std::list<std::shared_ptr<WorkerState>> _workerStates;
  std::list<std::shared_ptr<WorkerState>> _abandonedWorkerStates;

  std::mutex _mutex;
  std::condition_variable _conditionWork;

  void runWorker();
  void runSupervisor();

  std::mutex _mutexSupervisor;
  std::condition_variable _conditionSupervisor;
  std::unique_ptr<SupervisedSchedulerManagerThread> _manager;

  std::unique_ptr<WorkItem> getWork(std::shared_ptr<WorkerState>& state);

  void startOneThread();
  void stopOneThread();

  bool cleanupAbandonedThreads();
  void sortoutLongRunningThreads();
};

}  // namespace arangodb

#endif
