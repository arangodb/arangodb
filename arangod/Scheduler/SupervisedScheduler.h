////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include <array>
#include <condition_variable>
#include <functional>
#include <list>
#include <mutex>

#include <boost/lockfree/queue.hpp>

#include "RestServer/Metrics.h"
#include "Scheduler/Scheduler.h"

namespace arangodb {
class NetworkFeature;
class SharedPRNGFeature;
class SupervisedSchedulerWorkerThread;
class SupervisedSchedulerManagerThread;

class SupervisedScheduler final : public Scheduler {
 public:
  SupervisedScheduler(application_features::ApplicationServer& server,
                      uint64_t minThreads, uint64_t maxThreads, uint64_t maxQueueSize,
                      uint64_t fifo1Size, uint64_t fifo2Size, uint64_t fifo3Size,
                      double ongoingMultiplier, double unavailabilityQueueFillGrade);
  ~SupervisedScheduler() final;

  bool start() override;
  void shutdown() override;

  void toVelocyPack(velocypack::Builder&) const override;
  Scheduler::QueueStatistics queueStatistics() const override;

  void trackBeginOngoingLowPriorityTask();
  void trackEndOngoingLowPriorityTask();

  /// @brief set the time it took for the last low prio item to be dequeued
  /// (time between queuing and dequeing) [ms]
  void setLastLowPriorityDequeueTime(uint64_t time) noexcept;

  constexpr static uint64_t const NumberOfQueues = 4;

  constexpr static uint64_t const HighPriorityQueue = 1;
  constexpr static uint64_t const MediumPriorityQueue = 2;
  constexpr static uint64_t const LowPriorityQueue = 3;

  static_assert(HighPriorityQueue < NumberOfQueues);
  static_assert(MediumPriorityQueue < NumberOfQueues);
  static_assert(LowPriorityQueue < NumberOfQueues);
  
  static_assert(HighPriorityQueue < MediumPriorityQueue);
  static_assert(MediumPriorityQueue < LowPriorityQueue);

  /// @brief approximate fill grade of the scheduler's queue (in %)
  double approximateQueueFillGrade() const override;
  
  /// @brief fill grade of the scheduler's queue (in %) from which onwards
  /// the server is considered unavailable (because of overload)
  double unavailabilityQueueFillGrade() const override;
 
 protected:
  bool isStopping() override { return _stopping; }

 private:
  friend class SupervisedSchedulerManagerThread;
  friend class SupervisedSchedulerWorkerThread;

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
    std::atomic<clock::time_point> _lastJobStarted;
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

  struct WorkItem final {
    fu2::unique_function<void()> _handler;

    explicit WorkItem(fu2::unique_function<void()>&& handler)
        : _handler(std::move(handler)) {}
    ~WorkItem() = default;

    void operator()() { _handler(); }
  };

  std::unique_ptr<WorkItemBase> getWork(std::shared_ptr<WorkerState>& state);
  void startOneThread();
  void stopOneThread();

  bool cleanupAbandonedThreads();
  /// @brief returns whether or not a new thread was started by
  /// the method
  bool sortoutLongRunningThreads();

  // Check if we are allowed to pull from a queue with the given index
  // This is used to give priority to "FAST" and "MED" lanes accordingly.
  bool canPullFromQueue(uint64_t queueIdx) const noexcept;
  
  void runWorker();
  void runSupervisor();

  [[nodiscard]] bool queueItem(RequestLane lane, std::unique_ptr<WorkItemBase> item) override;

 private:
  NetworkFeature& _nf;
  SharedPRNGFeature& _sharedPRNG;

  std::atomic<uint64_t> _numWorkers;
  std::atomic<bool> _stopping;
  std::atomic<bool> _acceptingNewJobs;

  // Since the lockfree queue can only handle PODs, one has to wrap lambdas
  // in a container class and store pointers. -- Maybe there is a better way?
  boost::lockfree::queue<WorkItemBase*> _queues[NumberOfQueues];

  // aligning required to prevent false sharing - assumes cache line size is 64
  alignas(64) std::atomic<uint64_t> _jobsSubmitted;
  alignas(64) std::atomic<uint64_t> _jobsDequeued;
  alignas(64) std::atomic<uint64_t> _jobsDone;

  size_t const _minNumWorkers;
  size_t const _maxNumWorkers;
  uint64_t const _maxFifoSizes[NumberOfQueues];
  size_t const _ongoingLowPriorityLimit;

  /// @brief fill grade of the scheduler's queue (in %) from which onwards
  /// the server is considered unavailable (because of overload)
  double const _unavailabilityQueueFillGrade;

  std::list<std::shared_ptr<WorkerState>> _workerStates;
  std::list<std::shared_ptr<WorkerState>> _abandonedWorkerStates;
  std::atomic<uint64_t> _numWorking;   // Number of threads actually working
  std::atomic<uint64_t> _numAwake;     // Number of threads working or spinning
                                       // (i.e. not sleeping)

  // The following mutex protects the lists _workerStates and
  // _abandonedWorkerStates, whenever one accesses any of these two
  // lists, this mutex is needed. Note that if you need a mutex of one
  // of the workers, always acquire _mutex first and then the worker
  // mutex, never, the other way round. You may acquire only the
  // worker's mutex.
  std::mutex _mutex;

  std::mutex _mutexSupervisor;
  std::condition_variable _conditionSupervisor;
  std::unique_ptr<SupervisedSchedulerManagerThread> _manager;

  Gauge<uint64_t>& _metricsQueueLength;
  Gauge<uint64_t>& _metricsJobsDone;
  Gauge<uint64_t>& _metricsJobsSubmitted;
  Gauge<uint64_t>& _metricsJobsDequeued;
  Counter& _metricsJobsDoneTotal;
  Counter& _metricsJobsSubmittedTotal;
  Counter& _metricsJobsDequeuedTotal;
  Gauge<uint64_t>& _metricsNumAwakeThreads;
  Gauge<uint64_t>& _metricsNumWorkingThreads;
  Gauge<uint64_t>& _metricsNumWorkerThreads;
  
  Counter& _metricsThreadsStarted;
  Counter& _metricsThreadsStopped;
  Counter& _metricsQueueFull;
  Gauge<uint64_t>& _ongoingLowPriorityGauge;
  
  /// @brief amount of time it took for the last low prio item to be dequeued
  /// (time between queuing and dequeing) [ms].
  /// this metric is only updated probabilistically
  Gauge<uint64_t>& _metricsLastLowPriorityDequeueTime;

  std::array<std::reference_wrapper<Gauge<uint64_t>>, NumberOfQueues> _metricsQueueLengths;
};

}  // namespace arangodb

#endif
