////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include "RestServer/Metrics.h"
#include "Scheduler/Scheduler.h"

namespace arangodb {
class SupervisedSchedulerWorkerThread;
class SupervisedSchedulerManagerThread;

class SupervisedScheduler final : public Scheduler {
 public:
  SupervisedScheduler(application_features::ApplicationServer& server,
                      uint64_t minThreads, uint64_t maxThreads, uint64_t maxQueueSize,
                      uint64_t fifo1Size, uint64_t fifo2Size);
  virtual ~SupervisedScheduler();

  bool queue(RequestLane lane, fu2::unique_function<void()>) override ADB_WARN_UNUSED_RESULT;

 private:
  std::atomic<size_t> _numWorkers;
  std::atomic<bool> _stopping;
  std::atomic<bool> _acceptingNewJobs;

 protected:
  bool isStopping() override { return _stopping; }

 public:
  bool start() override;
  void shutdown() override;

  void toVelocyPack(velocypack::Builder&) const override;
  Scheduler::QueueStatistics queueStatistics() const override;

 private:
  friend class SupervisedSchedulerManagerThread;
  friend class SupervisedSchedulerWorkerThread;

  struct WorkItem final {
    fu2::unique_function<void()> _handler;

    explicit WorkItem(fu2::unique_function<void()>&& handler)
        : _handler(std::move(handler)) {}
    ~WorkItem() = default;

    void operator()() { _handler(); }
  };

  // Since the lockfree queue can only handle PODs, one has to wrap lambdas
  // in a container class and store pointers. -- Maybe there is a better way?
  boost::lockfree::queue<WorkItem*> _queues[3];

  // aligning required to prevent false sharing - assumes cache line size is 64
  alignas(64) std::atomic<uint64_t> _jobsSubmitted;
  alignas(64) std::atomic<uint64_t> _jobsDequeued;
  alignas(64) std::atomic<uint64_t> _jobsDone;

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
  // _queueRetryTime_us is the number of microseconds this particular
  // thread should spin before going to sleep. Note that this spinning is only
  // done if the thread has actually started to work on a request less than
  // 1 seconds ago.
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

  struct WorkerState {
    uint64_t _queueRetryTime_us; // t1
    uint64_t _sleepTimeout_ms;  // t2
    std::atomic<bool> _stop, _working, _sleeping;
    // _ready = false means the Worker is not properly initialized
    // _ready = true means it is initialized and can be used to dispatch tasks to
    // _ready is protected by the Scheduler's condition variable & mutex
    bool _ready;
    clock::time_point _lastJobStarted;
    std::unique_ptr<SupervisedSchedulerWorkerThread> _thread;
    std::mutex _mutex;
    std::condition_variable _conditionWork;

    // initialize with harmless defaults: spin once, sleep forever
    explicit WorkerState(SupervisedScheduler& scheduler);
    WorkerState(WorkerState const&) = delete;
    WorkerState& operator=(WorkerState const&) = delete;

    // cppcheck-suppress missingOverride
    bool start();
  };
  size_t const _minNumWorker;
  size_t const _maxNumWorker;
  std::list<std::shared_ptr<WorkerState>> _workerStates;
  std::list<std::shared_ptr<WorkerState>> _abandonedWorkerStates;
  std::atomic<uint64_t> _nrWorking;   // Number of threads actually working
  std::atomic<uint64_t> _nrAwake;     // Number of threads working or spinning
                                      // (i.e. not sleeping)

  // The following mutex protects the lists _workerStates and
  // _abandonedWorkerStates, whenever one accesses any of these two
  // lists, this mutex is needed. Note that if you need a mutex of one
  // of the workers, always acquire _mutex first and then the worker
  // mutex, never, the other way round. You may acquire only the
  // worker's mutex.
  std::mutex _mutex;

  void runWorker();
  void runSupervisor();

  std::mutex _mutexSupervisor;
  std::condition_variable _conditionSupervisor;
  std::unique_ptr<SupervisedSchedulerManagerThread> _manager;

  uint64_t const _maxFifoSize;
  uint64_t const _fifo1Size;
  uint64_t const _fifo2Size;

  std::unique_ptr<WorkItem> getWork(std::shared_ptr<WorkerState>& state);

  void startOneThread();
  void stopOneThread();

  bool cleanupAbandonedThreads();
  /// @brief returns whether or not a new thread was started by
  /// the method
  bool sortoutLongRunningThreads();

  // Check if we are allowed to pull from a queue with the given index
  // This is used to give priority to "FAST" and "MED" lanes accordingly.
  bool canPullFromQueue(uint64_t queueIdx) const;

  Gauge<uint64_t>& _metricsQueueLength;
  Gauge<uint64_t>& _metricsAwakeThreads;
  Gauge<uint64_t>& _metricsNumWorkerThreads;
  Counter& _metricsThreadsStarted;
  Counter& _metricsThreadsStopped;
  Counter& _metricsQueueFull;
};

}  // namespace arangodb

#endif
