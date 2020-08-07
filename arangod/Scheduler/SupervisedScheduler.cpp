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

#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

#include <Basics/StaticStrings.h>
#include <memory>

#include "Scheduler.h"
#include "SupervisedScheduler.h"

#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/cpu-relax.h"
#include "GeneralServer/Acceptor.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"
#include "Statistics/RequestStatistics.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "RestServer/MetricsFeature.h"

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

time_point lastQueueFullWarning[3];
int64_t fullQueueEvents[3] = {0, 0, 0};
std::mutex fullQueueWarningMutex[3];

void logQueueWarningEveryNowAndThen(int64_t events, uint64_t maxQueueSize) {
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
        << " is filled more than 50% in last " << sinceLast.count()
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

SupervisedScheduler::SupervisedScheduler(application_features::ApplicationServer& server,
                                         uint64_t minThreads, uint64_t maxThreads,
                                         uint64_t maxQueueSize,
                                         uint64_t fifo1Size, uint64_t fifo2Size)
    : Scheduler(server),
      _numWorkers(0),
      _stopping(false),
      _acceptingNewJobs(true),
      _jobsSubmitted(0),
      _jobsDequeued(0),
      _jobsDone(0),
      _jobsDirectExec(0),
      _wakeupQueueLength(5),
      _wakeupTime_ns(1000),
      _definitiveWakeupTime_ns(100000),
      _minNumWorker(minThreads),
      _maxNumWorker(maxThreads),
      _nrWorking(0),
      _nrAwake(0),
      _maxFifoSize(maxQueueSize),
      _fifo1Size(fifo1Size),
      _fifo2Size(fifo2Size),
      _metricsQueueLength(server.getFeature<arangodb::MetricsFeature>().gauge<uint64_t>(
          StaticStrings::SchedulerQueueLength, 0,
          "Servers internal queue length")),
      _metricsAwakeThreads(server.getFeature<arangodb::MetricsFeature>().gauge<uint64_t>(
          StaticStrings::SchedulerAwakeWorkers, 0,
          "Number of awake worker threads")),
      _metricsNumWorkerThreads(server.getFeature<arangodb::MetricsFeature>().gauge<uint64_t>(
          StaticStrings::SchedulerNumWorker, 0,
          "Number of worker threads")) {
  _queues[0].reserve(maxQueueSize);
  _queues[1].reserve(fifo1Size);
  _queues[2].reserve(fifo2Size);
}

SupervisedScheduler::~SupervisedScheduler() = default;

bool SupervisedScheduler::queue(RequestLane lane, fu2::unique_function<void()> handler) {
  if (!_acceptingNewJobs.load(std::memory_order_relaxed)) {
    return false;
  }

  auto work = std::make_unique<WorkItem>(std::move(handler));

  // use memory order acquire to make sure, pushed item is visible
  uint64_t const jobsDone = _jobsDone.load(std::memory_order_acquire);
  uint64_t const jobsSubmitted = _jobsSubmitted.fetch_add(1, std::memory_order_relaxed);

  // to make sure the queue length hasn't underflowed
  TRI_ASSERT(jobsDone <= jobsSubmitted);

  uint64_t const approxQueueLength = jobsSubmitted - jobsDone;

  auto const queueNo = static_cast<size_t>(PriorityRequestLane(lane));

  TRI_ASSERT(queueNo <= 2);
  TRI_ASSERT(isStopping() == false);

  if (!_queues[queueNo].bounded_push(work.get())) {
    _jobsSubmitted.fetch_sub(1, std::memory_order_release);

    uint64_t maxSize = _maxFifoSize;
    if (queueNo == 1) {
      maxSize = _fifo1Size;
    } else if (queueNo == 2) {
      maxSize = _fifo2Size;
    }
    LOG_TOPIC("98d94", DEBUG, Logger::THREADS)
        << "unable to push job to scheduler queue: queue is full";
    logQueueFullEveryNowAndThen(queueNo, maxSize);
    return false;
  }

  // queue now has ownership for the WorkItem
  (void)work.release();  // intentionally ignore return value

  if (approxQueueLength > _maxFifoSize / 2) {
    if ((::queueWarningTick++ & 0xFFu) == 0) {
      auto const& now = std::chrono::steady_clock::now();
      if (::conditionQueueFullSince == time_point{}) {
        logQueueWarningEveryNowAndThen(::queueWarningTick, _maxFifoSize);
        ::conditionQueueFullSince = now;
      } else if (now - ::conditionQueueFullSince > std::chrono::seconds(5)) {
        logQueueWarningEveryNowAndThen(::queueWarningTick, _maxFifoSize);
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
  // sleeping, we also do not wake up anybody (i.e. _nrAwake >= _numWorker). If
  // there is a spinning worker (i.e. _nrAwake > _nrWorking), then we do not try
  // to wake up anybody, however, we need to actually see a spinning worker in
  // this case. Otherwise, we walk through the threads, and wake up the first we
  // see which is sleeping.
  uint64_t awake = _nrAwake.load(std::memory_order_relaxed);
  if (awake == _numWorkers) {
    // Everybody labouring away, no need to wake nobody up.
    return true;
  }
  if (awake > _nrWorking.load(std::memory_order_relaxed)) {
    // This indicates that one is spinning, let's actually see this
    // one with out own eyes, if not, go on.
    // Without this additional loop we run the risk that a thread which
    // is currently spinning has already decided to go to sleep and will
    // not look at the queue again before doing so. Since we check the
    // _sleeping state under the mutex and the worker checks the queues
    // again after having indicates that it sleeps, we are good.
    std::unique_lock<std::mutex> guard(_mutex);  // protect _workerStates
    for (auto& state : _workerStates) {
      std::unique_lock<std::mutex> guard2(state->_mutex);
      if (!state->_sleeping && !state->_working) {
        // Got the spinning one, good:
        return true;
      }
    }
  }
  std::unique_lock<std::mutex> guard(_mutex);  // protect _workerStates
  for (auto& state : _workerStates) {
    std::unique_lock<std::mutex> guard2(state->_mutex);
    if (state->_sleeping) {
      guard2.unlock();
      state->_conditionWork.notify_one();
      break;
    }
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
    state = _workerStates.back();
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
  state->_ready = true;
  {
    std::lock_guard<std::mutex> guard(_mutexSupervisor);
  }
  _conditionSupervisor.notify_one();

  _nrAwake.fetch_add(1, std::memory_order_relaxed);
  while (true) {
    try {
      std::unique_ptr<WorkItem> work = getWork(state);
      if (work == nullptr) {
        break;
      }

      _jobsDequeued.fetch_add(1, std::memory_order_relaxed);

      state->_lastJobStarted = clock::now();
      state->_working = true;
      _nrWorking.fetch_add(1, std::memory_order_relaxed);
      work->_handler();
      state->_working = false;
      _nrWorking.fetch_sub(1, std::memory_order_relaxed);
    } catch (std::exception const& ex) {
      LOG_TOPIC("a235e", ERR, Logger::THREADS)
          << "scheduler loop caught exception: " << ex.what();
    } catch (...) {
      LOG_TOPIC("d4121", ERR, Logger::THREADS)
          << "scheduler loop caught unknown exception";
    }

    _jobsDone.fetch_add(1, std::memory_order_release);
  }
  _nrAwake.fetch_sub(1, std::memory_order_relaxed);
}

void SupervisedScheduler::runSupervisor() {
  while (_numWorkers < _minNumWorker) {
    startOneThread();
  }

  uint64_t lastJobsDone = 0, lastJobsSubmitted = 0;
  uint64_t jobsStallingTick = 0, lastQueueLength = 0;

  uint64_t roundCount = 0;

  while (!_stopping) {
    uint64_t jobsDone = _jobsDone.load(std::memory_order_acquire);
    uint64_t jobsSubmitted = _jobsSubmitted.load(std::memory_order_relaxed);
    uint64_t jobsDequeued = _jobsDequeued.load(std::memory_order_relaxed);

    if (jobsDone == lastJobsDone && (jobsDequeued < jobsSubmitted)) {
      jobsStallingTick++;
    } else if (jobsStallingTick != 0) {
      jobsStallingTick--;
    }

    uint64_t queueLength = jobsSubmitted - jobsDequeued;

    uint64_t awake = _nrAwake.load(std::memory_order_relaxed);
    uint64_t numWorker = _numWorkers.load(std::memory_order_relaxed);
    bool sleeperFound = (awake < numWorker);

    bool doStartOneThread = (((queueLength >= 3 * _numWorkers) &&
                              ((lastQueueLength + _numWorkers) < queueLength)) ||
                             (lastJobsSubmitted > jobsDone) || (!sleeperFound)) &&
                            (queueLength != 0);
    bool doStopOneThread = ((((lastQueueLength < 10) || (lastQueueLength >= queueLength)) &&
                             (lastJobsSubmitted <= jobsDone)) ||
                            ((queueLength == 0) && (lastQueueLength == 0))) &&
                           ((rand() & 0x3F) == 0) && sleeperFound;

    lastJobsDone = jobsDone;
    lastQueueLength = queueLength;
    lastJobsSubmitted = jobsSubmitted;

    if (roundCount % 8 == 0) {
      // approx every 0.8s update the metrics
      _metricsQueueLength.operator=(queueLength);
      _metricsAwakeThreads.operator=(awake);
      _metricsNumWorkerThreads.operator=(numWorker);
    }

    try {
      if (doStartOneThread && _numWorkers < _maxNumWorker) {
        jobsStallingTick = 0;
        startOneThread();
      } else if (doStopOneThread && _numWorkers > _minNumWorker) {
        stopOneThread();
      }

      cleanupAbandonedThreads();
      sortoutLongRunningThreads();

      std::unique_lock<std::mutex> guard(_mutexSupervisor);

      if (_stopping) {
        break;
      }

      _conditionSupervisor.wait_for(guard, std::chrono::milliseconds(100));
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

void SupervisedScheduler::sortoutLongRunningThreads() {
  // Detaching a thread always implies starting a new thread. Hence check here
  // if we can start a new thread.

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

      if ((now - state->_lastJobStarted) > std::chrono::seconds(5)) {
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
    startOneThread();
  }
}

bool SupervisedScheduler::canPullFromQueue(uint64_t queueIndex) const {
  if (queueIndex == 0) {
    // We can always! pull from high priority
    return true;
  }

  // This function should ensure the following thread reservation:
  // 25% reserved for FastLane only
  // upto 75% of work can go on MedLane and FastLane
  // uptop 50% of work can go on Slow, Med and FastLane
  TRI_ASSERT(_maxNumWorker >= 2);

  // The ordering of Done and dequeued is important, hence acquire.
  // Otherwise we might have the unlucky case that we first check dequeued,
  // then a job gets done fast (eg dequeued++, done++)
  // and then we read done.
  uint64_t jobsDone = _jobsDone.load(std::memory_order_acquire);
  uint64_t jobsDequeued = _jobsDequeued.load(std::memory_order_relaxed);
  TRI_ASSERT(jobsDequeued >= jobsDone);

  if (queueIndex == 1) {
    // We can work on med if less than 75% of the workers are busy
    return (jobsDequeued - jobsDone) < (_maxNumWorker * 3 / 4);
  }

  // We can work on low if less than 50% of the workers are busy
  return (jobsDequeued - jobsDone) < (_maxNumWorker / 2);
}

std::unique_ptr<SupervisedScheduler::WorkItem> SupervisedScheduler::getWork(
    std::shared_ptr<WorkerState>& state) {
  WorkItem* work;

  auto checkAllQueues = [this]() -> WorkItem* {
    WorkItem* res = nullptr;
    for (uint64_t i = 0; i < 3; ++i) {
      if (this->canPullFromQueue(i) && this->_queues[i].pop(res)) {
        return res;
      }
    }
    // Please note that _queues[i].pop(res) can modify res even if it does
    // not return `true`. Therefore it is crucial that we return nullptr
    // here and not res! We have been there and do not want to go back!
    return nullptr;
  };

  while (!state->_stop) {
    uint64_t triesCount = 0;
    auto loopStart = std::chrono::steady_clock::now();
    uint64_t timeOutForNow = state->_queueRetryTime_us;
    if (loopStart - state->_lastJobStarted > std::chrono::seconds(1)) {
      timeOutForNow = 0;
    }
    do {
      work = checkAllQueues();
      if (work != nullptr) {
        return std::unique_ptr<WorkItem>(work);
      }
      triesCount++;
      cpu_relax();
    } while ((std::chrono::steady_clock::now() - loopStart) <
             std::chrono::microseconds(timeOutForNow));

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
    _nrAwake.fetch_sub(1, std::memory_order_relaxed);

    work = checkAllQueues();
    if (work != nullptr) {
      // Fix the sleep indicators:
      state->_sleeping = false;
      _nrAwake.fetch_add(1, std::memory_order_relaxed);
      return std::unique_ptr<WorkItem>(work);
    }

    if (state->_sleepTimeout_ms == 0) {
      state->_conditionWork.wait(guard);
    } else {
      state->_conditionWork.wait_for(guard, std::chrono::milliseconds(state->_sleepTimeout_ms));
    }
    state->_sleeping = false;
    _nrAwake.fetch_add(1, std::memory_order_relaxed);
  }  // while

  return nullptr;
}

void SupervisedScheduler::startOneThread() {
  std::shared_ptr<WorkerState> state;
  {
    std::unique_lock<std::mutex> guard(_mutex);

    // TRI_ASSERT(_numWorkers < _maxNumWorker);
    if (_numWorkers + _abandonedWorkerStates.size() >= _maxNumWorker) {
      return;  // do not add more threads than maximum allows
    }

    // start a new thread
    _workerStates.emplace_back(std::make_shared<WorkerState>(*this));
    state = _workerStates.back();
  }

  if (!state->start()) {
    // failed to start a worker
    _workerStates.pop_back();  // pop_back deletes shared_ptr, which deletes thread
    LOG_TOPIC("913b5", ERR, Logger::THREADS)
        << "could not start additional worker thread";
    return;
  }

  // sync with runWorker()
  std::unique_lock<std::mutex> guard2(_mutexSupervisor);
  _conditionSupervisor.wait(guard2, [&state]() { return state->_ready; });
  LOG_TOPIC("f9de8", TRACE, Logger::THREADS) << "Started new thread";
}

void SupervisedScheduler::stopOneThread() {
  TRI_ASSERT(_numWorkers > 0);

  // copy shared_ptr
  std::shared_ptr<WorkerState> state;
  {
    std::unique_lock<std::mutex> guard(_mutex);
    state = _workerStates.back();
    _workerStates.pop_back();
    // Since the thread is effectively taken out of the pool, decrease the number of worker.
    _numWorkers--;
  }

  {
    std::unique_lock<std::mutex> guard(state->_mutex);
    state->_stop = true;
    // _stop is set under the mutex, then the worker thread is notified.
  }
  state->_conditionWork.notify_one();

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

  uint64_t const directExec = _jobsDirectExec.load(std::memory_order_relaxed);

  return QueueStatistics{numWorkers, 0, queued, working, directExec};
}

void SupervisedScheduler::toVelocyPack(velocypack::Builder& b) const {
  QueueStatistics qs = queueStatistics();

  b.add("scheduler-threads", VPackValue(qs._running));  // numWorkers
  b.add("blocked", VPackValue(qs._blocked));            // obsolete
  b.add("queued", VPackValue(qs._queued));
  b.add("in-progress", VPackValue(qs._working));
  b.add("direct-exec", VPackValue(qs._directExec));
}
