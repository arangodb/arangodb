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

#include <memory>

#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

#include "SupervisedScheduler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/SharedPRNGFeature.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/cpu-relax.h"
#include "GeneralServer/Acceptor.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Network/NetworkFeature.h"
#include "Random/RandomGenerator.h"
#include "RestServer/MetricsFeature.h"
#include "Scheduler/Scheduler.h"
#include "Statistics/RequestStatistics.h"

using namespace arangodb;
using namespace arangodb::basics;

namespace {
typedef std::chrono::time_point<std::chrono::steady_clock> time_point;

// value-initialize these arrays, otherwise mac will crash
thread_local time_point conditionQueueFullSince{};
thread_local uint_fast32_t queueWarningTick{};

time_point lastWarningQueue = std::chrono::steady_clock::now();
int64_t queueWarningEvents = 0;
std::mutex queueWarningMutex;

time_point lastQueueFullWarning[SupervisedScheduler::NumberOfQueues];
int64_t fullQueueEvents[SupervisedScheduler::NumberOfQueues] = {0};
std::mutex fullQueueWarningMutex[SupervisedScheduler::NumberOfQueues];

void logQueueWarningEveryNowAndThen(int64_t events, uint64_t maxQueueSize, uint64_t approxQueueLength) {
  auto const now = std::chrono::steady_clock::now();
  uint64_t totalEvents;
  bool printLog = false;
  std::chrono::duration<double> sinceLast;

  {
    std::unique_lock<std::mutex> guard(queueWarningMutex);
    totalEvents = queueWarningEvents += events;
    sinceLast = now - lastWarningQueue;
    if (sinceLast > std::chrono::seconds(10)) {
      printLog = true;
      lastWarningQueue = now;
      queueWarningEvents = 0;
    }
  }

  if (printLog) {
    LOG_TOPIC("dead2", WARN, Logger::THREADS)
        << "Scheduler queue with max capacity " << maxQueueSize
        << " has approximately " << approxQueueLength
        << " tasks and is filled more than 50% in last " << sinceLast.count()
        << "s (happened " << totalEvents << " times since last message)";
  }
}

void logQueueFullEveryNowAndThen(int64_t fifo, uint64_t maxQueueSize) {
  auto const& now = std::chrono::steady_clock::now();
  uint64_t events;
  bool printLog = false;

  {
    std::unique_lock<std::mutex> guard(fullQueueWarningMutex[fifo]);
    events = ++fullQueueEvents[fifo];
    if (now - lastQueueFullWarning[fifo] > std::chrono::seconds(10)) {
      printLog = true;
      lastQueueFullWarning[fifo] = now;
      fullQueueEvents[fifo] = 0;
    }
  }

  if (printLog) {
    LOG_TOPIC("dead1", WARN, Logger::THREADS)
        << "Scheduler queue " << fifo << " with max capacity " << maxQueueSize
        << " is full (happened " << events << " times since last message)";
  }
}
}  // namespace

namespace arangodb {

class SupervisedSchedulerThread : virtual public Thread {
 public:
  explicit SupervisedSchedulerThread(application_features::ApplicationServer& server,
                                     SupervisedScheduler& scheduler)
      : Thread(server, "Scheduler"), _scheduler(scheduler) {}
  ~SupervisedSchedulerThread() = default;  // shutdown is called by derived implementation!

 protected:
  SupervisedScheduler& _scheduler;
};

class SupervisedSchedulerManagerThread final : public SupervisedSchedulerThread {
 public:
  explicit SupervisedSchedulerManagerThread(application_features::ApplicationServer& server,
                                            SupervisedScheduler& scheduler)
      : Thread(server, "SchedMan"), SupervisedSchedulerThread(server, scheduler) {}
  ~SupervisedSchedulerManagerThread() { shutdown(); }
  void run() override { _scheduler.runSupervisor(); }
};

class SupervisedSchedulerWorkerThread final : public SupervisedSchedulerThread {
 public:
  explicit SupervisedSchedulerWorkerThread(application_features::ApplicationServer& server,
                                           SupervisedScheduler& scheduler)
      : Thread(server, "SchedWorker"), SupervisedSchedulerThread(server, scheduler) {}
  ~SupervisedSchedulerWorkerThread() { shutdown(); }
  void run() override { _scheduler.runWorker(); }
};

}  // namespace arangodb

DECLARE_GAUGE(arangodb_scheduler_num_awake_threads, uint64_t,
              "Number of awake worker threads");
DECLARE_LEGACY_GAUGE(arangodb_scheduler_jobs_dequeued, uint64_t,
              "Total number of jobs dequeued");
DECLARE_LEGACY_GAUGE(arangodb_scheduler_jobs_done, uint64_t,
              "Total number of queue jobs done");
DECLARE_LEGACY_GAUGE(arangodb_scheduler_jobs_submitted, uint64_t,
              "Total number of jobs submitted to the scheduler");
DECLARE_COUNTER(arangodb_scheduler_jobs_done_total,
              "Total number of queue jobs done");
DECLARE_COUNTER(arangodb_scheduler_jobs_submitted_total,
              "Total number of jobs submitted to the scheduler");
DECLARE_COUNTER(arangodb_scheduler_jobs_dequeued_total,
              "Total number of jobs dequeued");
DECLARE_GAUGE(
    arangodb_scheduler_high_prio_queue_length, uint64_t,
    "Current queue length of the high priority queue in the scheduler");
DECLARE_GAUGE(arangodb_scheduler_low_prio_queue_last_dequeue_time, uint64_t,
              "Last recorded dequeue time for a low priority queue item [ms]");
DECLARE_GAUGE(
    arangodb_scheduler_low_prio_queue_length, uint64_t,
    "Current queue length of the low priority queue in the scheduler");
DECLARE_GAUGE(
    arangodb_scheduler_maintenance_prio_queue_length, uint64_t,
    "Current queue length of the maintenance priority queue in the scheduler");
DECLARE_GAUGE(
    arangodb_scheduler_medium_prio_queue_length, uint64_t,
    "Current queue length of the medium priority queue in the scheduler");
DECLARE_GAUGE(arangodb_scheduler_num_working_threads, uint64_t,
              "Number of working threads");
DECLARE_GAUGE(arangodb_scheduler_num_worker_threads, uint64_t,
              "Number of worker threads");
DECLARE_GAUGE(
    arangodb_scheduler_ongoing_low_prio, uint64_t,
    "Total number of ongoing RestHandlers coming from the low prio queue");
DECLARE_COUNTER(arangodb_scheduler_queue_full_failures_total,
                "Tasks dropped and not added to internal queue");
DECLARE_GAUGE(arangodb_scheduler_queue_length, uint64_t,
              "Server's internal queue length");
DECLARE_COUNTER(arangodb_scheduler_threads_started_total,
                "Number of scheduler threads started");
DECLARE_COUNTER(arangodb_scheduler_threads_stopped_total,
                "Number of scheduler threads stopped");

SupervisedScheduler::SupervisedScheduler(application_features::ApplicationServer& server,
                                         uint64_t minThreads, uint64_t maxThreads,
                                         uint64_t maxQueueSize, uint64_t fifo1Size,
                                         uint64_t fifo2Size, uint64_t fifo3Size,
                                         double ongoingMultiplier,
                                         double unavailabilityQueueFillGrade)
    : Scheduler(server),
      _nf(server.getFeature<NetworkFeature>()),
      _sharedPRNG(server.getFeature<SharedPRNGFeature>()),
      _numWorkers(0),
      _stopping(false),
      _acceptingNewJobs(true),
      _jobsSubmitted(0),
      _jobsDequeued(0),
      _jobsDone(0),
      _minNumWorkers(minThreads),
      _maxNumWorkers(maxThreads),
      _maxFifoSizes{maxQueueSize, fifo1Size, fifo2Size, fifo3Size},
      _ongoingLowPriorityLimit(static_cast<std::size_t>(ongoingMultiplier * _maxNumWorkers)),
      _unavailabilityQueueFillGrade(unavailabilityQueueFillGrade),
      _numWorking(0),
      _numAwake(0),
      _metricsQueueLength(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_scheduler_queue_length{})),
      _metricsJobsDone(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_scheduler_jobs_done{})),
      _metricsJobsSubmitted(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_scheduler_jobs_submitted{})),
      _metricsJobsDequeued(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_scheduler_jobs_dequeued{})),
      _metricsJobsDoneTotal(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_scheduler_jobs_done_total{})),
      _metricsJobsSubmittedTotal(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_scheduler_jobs_submitted_total{})),
      _metricsJobsDequeuedTotal(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_scheduler_jobs_dequeued_total{})),
      _metricsNumAwakeThreads(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_scheduler_num_awake_threads{})),
      _metricsNumWorkingThreads(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_scheduler_num_working_threads{})),
      _metricsNumWorkerThreads(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_scheduler_num_worker_threads{})),
      _metricsThreadsStarted(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_scheduler_threads_started_total{})),
      _metricsThreadsStopped(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_scheduler_threads_stopped_total{})),
      _metricsQueueFull(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_scheduler_queue_full_failures_total{})),
      _ongoingLowPriorityGauge(_server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_scheduler_ongoing_low_prio{})),
      _metricsLastLowPriorityDequeueTime(
          _server.getFeature<arangodb::MetricsFeature>().add(
              arangodb_scheduler_low_prio_queue_last_dequeue_time{})),
      _metricsQueueLengths{_server.getFeature<arangodb::MetricsFeature>().add(
                               arangodb_scheduler_maintenance_prio_queue_length{}),
                           _server.getFeature<arangodb::MetricsFeature>().add(
                               arangodb_scheduler_high_prio_queue_length{}),
                           _server.getFeature<arangodb::MetricsFeature>().add(
                               arangodb_scheduler_medium_prio_queue_length{}),
                           _server.getFeature<arangodb::MetricsFeature>().add(
                               arangodb_scheduler_low_prio_queue_length{})} {
  _queues[0].reserve(maxQueueSize);
  _queues[1].reserve(fifo1Size);
  _queues[2].reserve(fifo2Size);
  _queues[3].reserve(fifo3Size);
  TRI_ASSERT(fifo3Size > 0);
}

SupervisedScheduler::~SupervisedScheduler() = default;

bool SupervisedScheduler::queueItem(RequestLane lane, std::unique_ptr<WorkItemBase> work) {
  if (!_acceptingNewJobs.load(std::memory_order_relaxed)) {
    return false;
  }

  // use memory order acquire to make sure, pushed item is visible
  uint64_t const jobsDone = _jobsDone.load(std::memory_order_acquire);
  uint64_t const jobsSubmitted = _jobsSubmitted.fetch_add(1, std::memory_order_relaxed);

  // to make sure the queue length hasn't underflowed
  TRI_ASSERT(jobsDone <= jobsSubmitted);

  uint64_t const approxQueueLength = jobsSubmitted - jobsDone;

  auto const queueNo = static_cast<size_t>(PriorityRequestLane(lane));

  TRI_ASSERT(queueNo < NumberOfQueues);
  TRI_ASSERT(isStopping() == false);

  if (!_queues[queueNo].bounded_push(work.get())) {
    _jobsSubmitted.fetch_sub(1, std::memory_order_release);

    uint64_t maxSize = _maxFifoSizes[queueNo];

    LOG_TOPIC("98d94", DEBUG, Logger::THREADS)
        << "unable to push job to scheduler queue: queue is full";
    logQueueFullEveryNowAndThen(queueNo, maxSize);
    ++_metricsQueueFull;
    return false;
  }

  _metricsQueueLengths[queueNo].get() += 1;

  // queue now has ownership for the WorkItemBase
  (void)work.release();  // intentionally ignore return value

  if (approxQueueLength > _maxFifoSizes[3] / 2) {
    if ((::queueWarningTick++ & 0xFFu) == 0) {
      auto const& now = std::chrono::steady_clock::now();
      if (::conditionQueueFullSince == time_point{}) {
        logQueueWarningEveryNowAndThen(::queueWarningTick, _maxFifoSizes[LowPriorityQueue], approxQueueLength);
        ::conditionQueueFullSince = now;
      } else if (now - ::conditionQueueFullSince > std::chrono::seconds(5)) {
        logQueueWarningEveryNowAndThen(::queueWarningTick, _maxFifoSizes[LowPriorityQueue], approxQueueLength);
        ::queueWarningTick = 0;
        ::conditionQueueFullSince = now;
      }
    }
  } else {
    ::queueWarningTick = 0;
    ::conditionQueueFullSince = time_point{};
  }

  // PLEASE LEAVE THESE EXPLANATIONS IN THE CODE, SINCE WE HAVE HAD MANY
  // PROBLEMS IN THIS AREA IN THE PAST, AND WE MIGHT FORGET AGAIN WHAT WE
  // INVESTIGATED IF IT IS NOT WRITTEN ANYWHERE. Waking up a sleeping thread is
  // very expensive (order of magnitude of a microsecond), therefore, we do not
  // want to do it unnecessarily. However, if we push work to a queue, we do not
  // want a sleeping worker to sleep for much longer, rather, we would like to
  // have the work done. Therefore, we follow this algorithm: If nobody is
  // sleeping, we also do not wake up anybody (i.e. _numAwake >= _numWorkers). If
  // there is a spinning worker (i.e. _numAwake > _numWorking), then we do not try
  // to wake up anybody, however, we need to actually see a spinning worker in
  // this case. Otherwise, we walk through the threads, and wake up the first we
  // see which is sleeping.
  uint64_t numAwake = _numAwake.load(std::memory_order_relaxed);
  if (numAwake == _numWorkers) {
    // Everybody laboring away, no need to wake anybody up.
    return true;
  }


  // This indicates that one is spinning, let's actually see this
  // one with our own eyes, if not, go on.
  // Without this additional loop we run the risk that a thread which
  // is currently spinning has already decided to go to sleep and will
  // not look at the queue again before doing so. Since we check the
  // _sleeping state under the mutex and the worker checks the queues
  // again after having indicates that it sleeps, we are good.
  bool const checkSpinning = (numAwake > _numWorking.load(std::memory_order_relaxed));

  // a candidate thread to notify
  decltype(_workerStates)::iterator notifyCandidate = _workerStates.end();

  std::unique_lock<std::mutex> guard(_mutex);  // protect _workerStates

  // make a single pass over all workerStates, to find a workerState
  // which is either spinning (which we won't notify), or one that is
  // sleeping (which we will notify).
  for (auto it = _workerStates.begin(); it != _workerStates.end(); ++it) {
    auto& state = (*it);

    std::unique_lock<std::mutex> guard2(state->_mutex);

    if (checkSpinning && !state->_sleeping && !state->_working) {
      // Got the spinning one, good:
      // DONT'T NOTIFY IT!
      notifyCandidate = _workerStates.end();
      break;
    }
    // look out for a sleeping workerState. the first one we find will
    // become a candidate for being notified. however, we will only notify
    // it if we don't look out for and find a spinning working thead.
    if (state->_sleeping && notifyCandidate == _workerStates.end()) {
      notifyCandidate = it;
      if (!checkSpinning) {
        // just use the first thing we find
        break;
      }
    }
  }

  if (notifyCandidate != _workerStates.end()) {
    (*notifyCandidate)->_conditionWork.notify_one();
  }

  return true;
}

bool SupervisedScheduler::start() {
  _manager = std::make_unique<SupervisedSchedulerManagerThread>(_server, *this);
  if (!_manager->start()) {
    LOG_TOPIC("00443", ERR, Logger::THREADS)
        << "could not start supervisor thread";
    return false;
  }

  return Scheduler::start();
}

void SupervisedScheduler::shutdown() {
  // First do not accept any more jobs:
  {
    std::unique_lock<std::mutex> guard(_mutex);
    _acceptingNewJobs = false;
  }

  // Now wait until all are finished:
  while (true) {
    auto jobsDone = _jobsDone.load(std::memory_order_acquire);
    auto jobsSubmitted = _jobsSubmitted.load(std::memory_order_relaxed);

    if (jobsSubmitted <= jobsDone) {
      break;
    }

    LOG_TOPIC("a1690", WARN, Logger::THREADS)
        << "Scheduler received shutdown, but there are still tasks on the "
        << "queue: jobsSubmitted=" << jobsSubmitted << " jobsDone=" << jobsDone;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  // Now we can shut down the worker threads:
  {
    std::unique_lock<std::mutex> guard(_mutex);
    _stopping = true;
    for (auto& state : _workerStates) {
      std::unique_lock<std::mutex> guard2(state->_mutex);
      state->_stop = true;
      guard2.unlock();
      state->_conditionWork.notify_one();
    }
  }

  // And the cron thread:
  Scheduler::shutdown();

  // call the destructor of all threads
  _manager.reset();

  while (_numWorkers > 0) {
    stopOneThread();
  }

  int tries = 0;
  while (!cleanupAbandonedThreads()) {
    if (++tries > 5 * 5) {
      // spam only after some time (5 seconds here)
      LOG_TOPIC("ed0b2", WARN, Logger::THREADS)
          << "Scheduler received shutdown, but there are still abandoned "
             "threads";
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
}

void SupervisedScheduler::runWorker() {
  uint64_t id;

  std::shared_ptr<WorkerState> state;

  {
    std::lock_guard<std::mutex> guard1(_mutex);
    id = _numWorkers++;  // increase the number of workers here, to obtain the id
    // copy shared_ptr with worker state
    // obtaining the state from the end of the _workerStates list
    // is (only) safe here because there is only one thread
    // (SupervisedScheduler) that modifies _workerStates and
    // that blocks until we (in this thread) have set the _ready
    // flag on the state
    state = _workerStates.back();
    TRI_ASSERT(!state->_ready);
  }

  state->_sleepTimeout_ms = 20 * (id + 1);
  // cap the timeout to some boundary value
  if (state->_sleepTimeout_ms >= 1000) {
    state->_sleepTimeout_ms = 1000;
  }

  if (id < 5U) {
    state->_queueRetryTime_us = (uint64_t(32) >> id) + 1;
  } else {
    state->_queueRetryTime_us = 0;
  }

  // inform the supervisor that this thread is alive
  {
    std::lock_guard<std::mutex> guard(_mutexSupervisor);
    state->_ready = true;
  }
  _conditionSupervisor.notify_one();

  _numAwake.fetch_add(1, std::memory_order_relaxed);
  while (true) {
    try {
      std::unique_ptr<WorkItemBase> work = getWork(state);
      if (work == nullptr) {
        break;
      }

      _jobsDequeued.fetch_add(1, std::memory_order_relaxed);

      state->_lastJobStarted.store(clock::now(), std::memory_order_release);
      state->_working = true;
      _numWorking.fetch_add(1, std::memory_order_relaxed);
      try {
        work->invoke();
        state->_working = false;
        _numWorking.fetch_sub(1, std::memory_order_relaxed);
      } catch (...) {
        state->_working = false;
        _numWorking.fetch_sub(1, std::memory_order_relaxed);
        throw;
      }
    } catch (std::exception const& ex) {
      LOG_TOPIC("a235e", ERR, Logger::THREADS)
          << "scheduler loop caught exception: " << ex.what();
    } catch (...) {
      LOG_TOPIC("d4121", ERR, Logger::THREADS)
          << "scheduler loop caught unknown exception";
    }

    _jobsDone.fetch_add(1, std::memory_order_release);
  }
  _numAwake.fetch_sub(1, std::memory_order_relaxed);
}

void SupervisedScheduler::runSupervisor() {
  while (_numWorkers < _minNumWorkers) {
    startOneThread();
  }

  uint64_t lastJobsSubmitted = 0;
  uint64_t lastQueueLength = 0;
  uint64_t roundCount = 0;

  while (!_stopping) {
    uint64_t jobsSubmitted = _jobsSubmitted.load(std::memory_order_acquire);
    uint64_t jobsDone = _jobsDone.load(std::memory_order_acquire);
    uint64_t jobsDequeued = _jobsDequeued.load(std::memory_order_relaxed);
    uint64_t queueLength = jobsSubmitted - jobsDequeued;

    uint64_t numAwake = _numAwake.load(std::memory_order_relaxed);
    uint64_t numWorkers = _numWorkers.load(std::memory_order_relaxed);
    uint64_t numWorking = _numWorking.load(std::memory_order_relaxed);
    bool sleeperFound = (numAwake < numWorkers);

    bool doStartOneThread = (((queueLength >= 3 * _numWorkers) &&
                              ((lastQueueLength + _numWorkers) < queueLength)) ||
                             (lastJobsSubmitted > jobsDone) || (!sleeperFound)) &&
                            (queueLength != 0);
    bool doStopOneThread = ((((lastQueueLength < 10) || (lastQueueLength >= queueLength)) &&
                             (lastJobsSubmitted <= jobsDone)) ||
                            ((queueLength == 0) && (lastQueueLength == 0))) &&
                           ((rand() & 0x3F) == 0) && sleeperFound;

    lastQueueLength = queueLength;
    lastJobsSubmitted = jobsSubmitted;

    if (roundCount++ >= 5) {
      // approx every 0.5s update the metrics
      _metricsQueueLength.operator=(queueLength);
      _metricsJobsDone.operator=(jobsDone);
      _metricsJobsSubmitted.operator=(jobsSubmitted);
      _metricsJobsDequeued.operator=(jobsDequeued);
      _metricsJobsDoneTotal.operator=(jobsDone);
      _metricsJobsSubmittedTotal.operator=(jobsSubmitted);
      _metricsJobsDequeuedTotal.operator=(jobsDequeued);
      _metricsNumAwakeThreads.operator=(numAwake);
      _metricsNumWorkingThreads.operator=(numWorking);
      _metricsNumWorkerThreads.operator=(numWorkers);
      roundCount = 0;
    }

    try {
      bool haveStartedThread = false;

      if (doStartOneThread && _numWorkers < _maxNumWorkers) {
        startOneThread();
        haveStartedThread = true;
      } else if (doStopOneThread && _numWorkers > _minNumWorkers) {
        stopOneThread();
      }

      cleanupAbandonedThreads();
      haveStartedThread |= sortoutLongRunningThreads();

      std::unique_lock<std::mutex> guard(_mutexSupervisor);

      if (_stopping) {
        break;
      }

      // use a reduced wait time if we just started a new thread...
      // it is possible that more work is coming so we should react
      // quickly.
      _conditionSupervisor.wait_for(guard,  std::chrono::milliseconds(haveStartedThread ? 50 : 100));
    } catch (std::exception const& ex) {
      LOG_TOPIC("3318c", WARN, Logger::THREADS)
          << "scheduler supervisor thread caught exception: " << ex.what();
    }
  }
}

bool SupervisedScheduler::cleanupAbandonedThreads() {
  std::unique_lock<std::mutex> guard(_mutex);
  auto i = _abandonedWorkerStates.begin();

  while (i != _abandonedWorkerStates.end()) {
    auto& state = *i;
    if (!state->_thread->isRunning()) {
      i = _abandonedWorkerStates.erase(i);
    } else {
      i++;
    }
  }

  return _abandonedWorkerStates.empty();
}

bool SupervisedScheduler::sortoutLongRunningThreads() {
  // Detaching a thread always implies starting a new thread. Hence check here
  // if we can start a new thread.

  bool haveStartedThread = true;
  size_t newThreadsNeeded = 0;
  {
    std::unique_lock<std::mutex> guard(_mutex);  // protect _workerStates
    auto now = clock::now();
    auto i = _workerStates.begin();

    while (i != _workerStates.end()) {
      auto& state = *i;

      if (!state->_working) {
        i++;
        continue;
      }

      if ((now - state->_lastJobStarted.load(std::memory_order_acquire)) >
          std::chrono::seconds(5)) {
        LOG_TOPIC("efcaa", TRACE, Logger::THREADS)
            << "Detach long running thread.";

        {
          std::unique_lock<std::mutex> guard2(state->_mutex);
          state->_stop = true;
        }

        // Move that thread to the abandoned thread
        _abandonedWorkerStates.push_back(std::move(state));
        i = _workerStates.erase(i);
        _numWorkers--;

        // and now start another thread!
        ++newThreadsNeeded;
      } else {
        i++;
      }
    }
  }

  while (newThreadsNeeded--) {
    haveStartedThread = true;
    startOneThread();
  }
  return haveStartedThread;
}

bool SupervisedScheduler::canPullFromQueue(uint64_t queueIndex) const noexcept {
  if (queueIndex == 0) {
    // We can always! pull from maintenance priority
    return true;
  }

  // This function should ensure the following thread reservation:
  // 12.5% (but at least 1) reserved for MAINTENANCE prio
  // 25% (but at least 2) reserved for HIGH and MAINTENANCE only
  // upto 75% of work can do MEDIUM and LOW
  // uptop 50% of work can do LOW
  TRI_ASSERT(_maxNumWorkers >= 4);

  // The ordering of Done and dequeued is important, hence acquire.
  // Otherwise we might have the unlucky case that we first check dequeued,
  // then a job gets done fast (eg dequeued++, done++)
  // and then we read done.
  uint64_t jobsDone = _jobsDone.load(std::memory_order_acquire);
  uint64_t jobsDequeued = _jobsDequeued.load(std::memory_order_relaxed);
  TRI_ASSERT(jobsDequeued >= jobsDone);
  uint64_t threadsWorking = jobsDequeued - jobsDone;

  if (queueIndex == HighPriorityQueue) {
    // We can work on high if less than 87.5% of the workers are busy
    size_t limit = (_maxNumWorkers >= 8) ? (_maxNumWorkers * 7 / 8) : _maxNumWorkers - 1;
    return threadsWorking < limit;
  }

  if (queueIndex == MediumPriorityQueue) {
    // We can work on med if less than 75% of the workers are busy
    size_t limit = (_maxNumWorkers >= 8) ? (_maxNumWorkers * 3 / 4) : _maxNumWorkers - 2;
    return threadsWorking < limit;
  }

  TRI_ASSERT(queueIndex == LowPriorityQueue);

  // We limit the number of ongoing low priority jobs to prevent a cluster
  // from getting overwhelmed
  std::size_t const ongoing = _ongoingLowPriorityGauge.load();
  if (ongoing >= _ongoingLowPriorityLimit) {
    return false;
  }

  // Because jobs may fan out to multiple servers and shards, we also limit
  // dequeuing based on the number of internal requests in flight
  if (_nf.isSaturated()) {
    return false;
  }

  // We can work on low if less than 50% of the workers are busy
  size_t limit = (_maxNumWorkers >= 8) ? (_maxNumWorkers / 2) : _maxNumWorkers - 3;
  return threadsWorking < limit;
}

std::unique_ptr<SupervisedScheduler::WorkItemBase> SupervisedScheduler::getWork(
    std::shared_ptr<WorkerState>& state) {
  auto checkAllQueues = [this](uint64_t& maxCheckedQueue) -> WorkItemBase* {
    for (uint64_t i = 0; i < NumberOfQueues; ++i) {
      if (!this->canPullFromQueue(i)) {
        // if we can't pull from high prio, then we will not be able to
        // pull from medium prio.
        // if we can't pull from medium prio, then we will not be able to
        // pull from low prio.
        // so we can as well abort the search here and exit.
        break;
      }
      maxCheckedQueue = i;
      WorkItemBase* res;
      if (this->_queues[i].pop(res)) {
        _metricsQueueLengths[i].get() -= 1;
        return res;
      }
    }
    // Please note that _queues[i].pop(res) can modify res even if it does
    // not return `true`. Therefore it is crucial that we return nullptr
    // here and not res! We have been there and do not want to go back!
    return nullptr;
  };

  // how often did we check for new work without success
  uint64_t iterations = 0;
  uint64_t maxCheckedQueue = 0;

  while (!state->_stop) {
    // do the first check for new work without calculating any timeouts etc.
    // only if we find nothing to do, we will invest time to calculate a 
    // timeout
    WorkItemBase* work = checkAllQueues(maxCheckedQueue);
    if (work != nullptr) {
      return std::unique_ptr<WorkItemBase>(work);
    }
    
    ++iterations;
    
    auto loopStart = std::chrono::steady_clock::now();
    uint64_t timeoutForNow = state->_queueRetryTime_us;
    if (loopStart - state->_lastJobStarted.load(std::memory_order_acquire) >
        std::chrono::seconds(1)) {
      timeoutForNow = 0;
    }

    do {
      cpu_relax();
      
      work = checkAllQueues(maxCheckedQueue);
      if (work != nullptr) {
        return std::unique_ptr<WorkItemBase>(work);
      }
    } while ((std::chrono::steady_clock::now() - loopStart) <
             std::chrono::microseconds(timeoutForNow));

    std::unique_lock<std::mutex> guard(state->_mutex);
    // Now let's one more time check all the queues under the mutex before we
    // actually go to sleep, we already indicate that we are sleeping. Note that
    // both are important, otherwise we run the risk that the queue() call
    // thinks we are spinning when in fact we are already going to sleep!
    // This could lead to a request lying around on the queue and everybody is
    // sleeping, which would cause random rare delays of some 20ms.

    if (state->_stop) {
      break;
    }

    state->_sleeping = true;
    _numAwake.fetch_sub(1, std::memory_order_relaxed);

    work = checkAllQueues(maxCheckedQueue);
    if (work != nullptr) {
      // Fix the sleep indicators:
      state->_sleeping = false;
      _numAwake.fetch_add(1, std::memory_order_relaxed);
      return std::unique_ptr<WorkItemBase>(work);
    }
    
    // nothing to do for a long time, but the previously stored dequeue time 
    // is still set to something > 5ms (note: we use 5 here because a deque time 
    // of > 0ms is not very unlikely for any request)
    if (maxCheckedQueue == LowPriorityQueue &&
        iterations >= 10 &&
        _metricsLastLowPriorityDequeueTime.load(std::memory_order_relaxed) > 5) {
      // set the dequeue time back to 0.
      setLastLowPriorityDequeueTime(0);
    }

    if (state->_sleepTimeout_ms == 0) {
      state->_conditionWork.wait(guard);
    } else {
      state->_conditionWork.wait_for(guard, std::chrono::milliseconds(state->_sleepTimeout_ms));
    }
    state->_sleeping = false;
    _numAwake.fetch_add(1, std::memory_order_relaxed);
  }  // while

  return nullptr;
}

void SupervisedScheduler::startOneThread() {
  std::shared_ptr<WorkerState> state;
  {
    std::unique_lock<std::mutex> guard(_mutex);

    if (_numWorkers + _abandonedWorkerStates.size() >= _maxNumWorkers) {
      return;  // do not add more threads than maximum allows
    }

    // start a new thread
    _workerStates.emplace_back(std::make_shared<WorkerState>(*this));
    state = _workerStates.back();
  }

  if (!state->start()) {
    // failed to start a worker
    
    // now remove the worker again from the list of workers
    // we need to grab the lock for this and scan the entire list, because
    // some other operation can have modified the list in between.
    // this is expensive, but a rare edge cases (threads don't start)
    {
      std::unique_lock<std::mutex> guard(_mutex);

      // removing deletes shared_ptr, which deletes thread
      _workerStates.erase(std::remove_if(_workerStates.begin(), _workerStates.end(), [&state](auto const& s) {
        return (s.get() == state.get());
      }), _workerStates.end());
    }
    LOG_TOPIC("913b5", WARN, Logger::THREADS)
        << "could not start additional worker thread";
  } else {
    // sync with runWorker()
    {
      std::unique_lock<std::mutex> guard2(_mutexSupervisor);
      _conditionSupervisor.wait(guard2, [&state]() { return state->_ready; });
    }
    ++_metricsThreadsStarted;
    LOG_TOPIC("f9de8", TRACE, Logger::THREADS) << "Started new thread";
  }
}

void SupervisedScheduler::stopOneThread() {
  TRI_ASSERT(_numWorkers > 0);

  // copy shared_ptr
  std::shared_ptr<WorkerState> state;
  {
    std::unique_lock<std::mutex> guard(_mutex);
    state = _workerStates.back();
    _workerStates.pop_back();
    // Since the thread is effectively taken out of the pool, decrease the number of workers.
    --_numWorkers;
  }

  {
    std::unique_lock<std::mutex> guard(state->_mutex);
    state->_stop = true;
    // _stop is set under the mutex, then the worker thread is notified.
  }
  state->_conditionWork.notify_one();

  ++_metricsThreadsStopped;

  // However the thread may be working on a long job. Hence we enqueue it on
  // the cleanup list and wait for its termination.

  if (state->_thread->isRunning()) {
    LOG_TOPIC("73365", TRACE, Logger::THREADS) << "Abandon one thread.";
    {
      std::unique_lock<std::mutex> guard(_mutex);
      _abandonedWorkerStates.push_back(std::move(state));
    }
  } else {
    state.reset();  // reset the shared_ptr. At this point we delete the thread object
                    // Since the thread is already STOPPED, the join is a no op.
  }
}

SupervisedScheduler::WorkerState::WorkerState(SupervisedScheduler& scheduler)
    : _queueRetryTime_us(10),
      _sleepTimeout_ms(100),
      _stop(false),
      _working(false),
      _sleeping(false),
      _ready(false),
      _lastJobStarted(clock::now()),
      _thread(new SupervisedSchedulerWorkerThread(scheduler._server, scheduler)) {}

bool SupervisedScheduler::WorkerState::start() { return _thread->start(); }

// ---------------------------------------------------------------------------
// Statistics Stuff
// ---------------------------------------------------------------------------

Scheduler::QueueStatistics SupervisedScheduler::queueStatistics() const {
  // we need to read multiple atomics here. as all atomic reads happen independently
  // without a mutex outside, the overall picture may be inconsistent

  uint64_t const numWorkers = _numWorkers.load(std::memory_order_relaxed);

  // read _jobsDone first, so the differences of the counters cannot get negative
  uint64_t const jobsDone = _jobsDone.load(std::memory_order_acquire);
  uint64_t const jobsDequeued = _jobsDequeued.load(std::memory_order_relaxed);
  uint64_t const jobsSubmitted = _jobsSubmitted.load(std::memory_order_relaxed);

  uint64_t const queued = jobsSubmitted - jobsDone;
  uint64_t const working = jobsDequeued - jobsDone;

  return QueueStatistics{numWorkers, queued, working};
}

void SupervisedScheduler::toVelocyPack(velocypack::Builder& b) const {
  QueueStatistics qs = queueStatistics();

  b.add("scheduler-threads", VPackValue(qs._running)); // numWorkers
  b.add("blocked", VPackValue(0)); // obsolete
  b.add("queued", VPackValue(qs._queued)); // scheduler queue length
  b.add("in-progress", VPackValue(qs._working)); // number of working (non-idle) threads
  b.add("direct-exec", VPackValue(0)); // obsolete
}

void SupervisedScheduler::trackBeginOngoingLowPriorityTask() {
  if (!_server.isStopping()) {
    ++_ongoingLowPriorityGauge;
  }
}

void SupervisedScheduler::trackEndOngoingLowPriorityTask() {
  if (!_server.isStopping()) {
    --_ongoingLowPriorityGauge;
  }
}

void SupervisedScheduler::setLastLowPriorityDequeueTime(uint64_t time) noexcept {
  // update only probabilistically, in order to reduce contention on the gauge
  if ((_sharedPRNG.rand() & 7) == 0) {
    _metricsLastLowPriorityDequeueTime.operator=(time);
  }
}

double SupervisedScheduler::approximateQueueFillGrade() const {
  uint64_t const maxLength = _maxFifoSizes[3];
  uint64_t const qLength = std::min<uint64_t>(maxLength, _metricsQueueLength.load());
  return static_cast<double>(qLength) / static_cast<double>(maxLength);
}

double SupervisedScheduler::unavailabilityQueueFillGrade() const {
  return _unavailabilityQueueFillGrade;
}
