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

#include "SupervisedScheduler.h"
#include "Scheduler.h"

#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/cpu-relax.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/Acceptor.h"
#include "GeneralServer/RestHandler.h"
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"
#include "Rest/GeneralResponse.h"
#include "Statistics/RequestStatistics.h"

using namespace arangodb;
using namespace arangodb::basics;

namespace {
uint64_t getTickCount_ns() {
  auto now = std::chrono::high_resolution_clock::now();

  return std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch())
      .count();
}

bool isDirectDeadlockLane(RequestLane lane) {
  // Some lane have tasks deadlock because they hold a mutex while calling queue that must be locked to execute the handler.
  // Those tasks can not be executed directly.
  return lane == RequestLane::TASK_V8
    || lane == RequestLane::CLIENT_V8
    || lane == RequestLane::CLUSTER_V8
    || lane == RequestLane::INTERNAL_LOW
    || lane == RequestLane::SERVER_REPLICATION
    || lane == RequestLane::CLUSTER_ADMIN
    || lane == RequestLane::CLUSTER_INTERNAL
    || lane == RequestLane::AGENCY_CLUSTER
    || lane == RequestLane::CLIENT_AQL;
}

}  // namespace

namespace {
  typedef std::chrono::time_point<std::chrono::steady_clock> time_point;

  // value initialise these arrays, otherwise mac will crash
  thread_local time_point conditionQueueFullSince{};
  thread_local uint_fast32_t queueWarningTick{};

  time_point lastWarningQueue = std::chrono::steady_clock::now();
  int64_t queueWarningEvents = 0;
  std::mutex queueWarningMutex;

  time_point lastQueueFullWarning[3];
  int64_t fullQueueEvents[3] = {0, 0, 0};
  std::mutex fullQueueWarningMutex[3];

  void logQueueWarningEveryNowAndThen(int64_t events) {
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
      LOG_TOPIC("dead2", WARN, Logger::THREADS) << "Scheduler queue" <<
        " is filled more than 50% in last " << sinceLast.count()
        << "s. (happened " << totalEvents << " times since last message)";
    }
  }

  void logQueueFullEveryNowAndThen(int64_t fifo) {
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
      LOG_TOPIC("dead1", WARN, Logger::THREADS) << "Scheduler queue " << fifo << " is full. (happened " << events << " times since last message)";
    }
  }
}


namespace arangodb {

class SupervisedSchedulerThread : virtual public Thread {
 public:
  explicit SupervisedSchedulerThread(SupervisedScheduler& scheduler)
      : Thread("Scheduler"), _scheduler(scheduler) {}
  ~SupervisedSchedulerThread() {} // shutdown is called by derived implementation!

 protected:
  SupervisedScheduler& _scheduler;
};

class SupervisedSchedulerManagerThread final : public SupervisedSchedulerThread {
 public:
  explicit SupervisedSchedulerManagerThread(SupervisedScheduler& scheduler)
      : Thread("SchedMan"), SupervisedSchedulerThread(scheduler) {}
  ~SupervisedSchedulerManagerThread() { shutdown(); }
  void run() override { _scheduler.runSupervisor(); };
};

class SupervisedSchedulerWorkerThread final : public SupervisedSchedulerThread {
 public:
  explicit SupervisedSchedulerWorkerThread(SupervisedScheduler& scheduler)
      : Thread("SchedWorker"), SupervisedSchedulerThread(scheduler) {}
  ~SupervisedSchedulerWorkerThread() { shutdown(); }
  void run() override { _scheduler.runWorker(); };
};

}  // namespace arangodb

SupervisedScheduler::SupervisedScheduler(uint64_t minThreads, uint64_t maxThreads,
                                         uint64_t maxQueueSize,
                                         uint64_t fifo1Size, uint64_t fifo2Size)
    : _numWorkers(0),
      _stopping(false),
      _jobsSubmitted(0),
      _jobsDequeued(0),
      _jobsDone(0),
      _jobsDirectExec(0),
      _wakeupQueueLength(5),
      _wakeupTime_ns(1000),
      _definitiveWakeupTime_ns(100000),
      _maxNumWorker(maxThreads),
      _numIdleWorker(minThreads),
      _maxFifoSize(maxQueueSize) {
  _queue[0].reserve(maxQueueSize);
  _queue[1].reserve(fifo1Size);
  _queue[2].reserve(fifo2Size);
}

SupervisedScheduler::~SupervisedScheduler() {}

bool SupervisedScheduler::queue(RequestLane lane, std::function<void()> handler, bool allowDirectHandling) {
  if (!isDirectDeadlockLane(lane) &&
      allowDirectHandling &&
      !ServerState::instance()->isClusterRole() &&
      (_jobsSubmitted - _jobsDone) < 2) {
    _jobsSubmitted.fetch_add(1, std::memory_order_relaxed);
    _jobsDequeued.fetch_add(1, std::memory_order_relaxed);
    _jobsDirectExec.fetch_add(1, std::memory_order_release);
    try {
      handler();
      _jobsDone.fetch_add(1, std::memory_order_release);
      return true;
    } catch (...) {
      _jobsDone.fetch_add(1, std::memory_order_release);
      throw;
    }
  }

  size_t queueNo = static_cast<size_t>(PriorityRequestLane(lane));

  TRI_ASSERT(queueNo <= 2);
  TRI_ASSERT(isStopping() == false);

  auto work = std::make_unique<WorkItem>(std::move(handler));

  if (!_queue[queueNo].push(work.get())) {
    logQueueFullEveryNowAndThen(queueNo);
    return false;
  }
  // queue now has ownership for the WorkItem
  work.release();

  static thread_local uint64_t lastSubmitTime_ns;

  // use memory order release to make sure, pushed item is visible
  uint64_t jobsSubmitted = _jobsSubmitted.fetch_add(1, std::memory_order_release);
  uint64_t approxQueueLength = jobsSubmitted - _jobsDone;
  uint64_t now_ns = getTickCount_ns();
  uint64_t sleepyTime_ns = now_ns - lastSubmitTime_ns;
  lastSubmitTime_ns = now_ns;

  if (approxQueueLength > _maxFifoSize / 2) {
    if ((queueWarningTick++ & 0xFF) == 0) {
      auto const& now = std::chrono::steady_clock::now();
      if (conditionQueueFullSince == time_point{}) {
        logQueueWarningEveryNowAndThen(queueWarningTick);
        conditionQueueFullSince = now;
      } else if (now - conditionQueueFullSince > std::chrono::seconds(5)) {
        logQueueWarningEveryNowAndThen(queueWarningTick);
        queueWarningTick = 0;
        conditionQueueFullSince = now;
      }
    }
  } else {
    queueWarningTick = 0;
    conditionQueueFullSince = time_point{};
  }


  bool doNotify = false;
  if (sleepyTime_ns > _definitiveWakeupTime_ns.load(std::memory_order_relaxed)) {
    doNotify = true;
  } else if (sleepyTime_ns > _wakeupTime_ns &&
             approxQueueLength > _wakeupQueueLength.load(std::memory_order_relaxed)) {
    doNotify = true;
  }

  if (doNotify) {
    _conditionWork.notify_one();
  }

  return true;
}

bool SupervisedScheduler::start() {
  _manager.reset(new SupervisedSchedulerManagerThread(*this));
  if (!_manager->start()) {
    LOG_TOPIC("00443", ERR, Logger::THREADS) << "could not start supervisor thread";
    return false;
  }

  return Scheduler::start();
}

void SupervisedScheduler::shutdown() {
  // THIS IS WHAT WE SHOULD AIM FOR, BUT NOBODY CARES
  // TRI_ASSERT(_jobsSubmitted <= _jobsDone);

  {
    std::unique_lock<std::mutex> guard(_mutex);
    _stopping = true;
    _conditionWork.notify_all();
  }

  Scheduler::shutdown();

  while (true) {
    auto jobsSubmitted = _jobsSubmitted.load();
    auto jobsDone = _jobsDone.load();

    if (jobsSubmitted <= jobsDone) {
      break;
    }

    LOG_TOPIC("a1690", WARN, Logger::THREADS)
        << "Scheduler received shutdown, but there are still tasks on the "
        << "queue: jobsSubmitted=" << jobsSubmitted << " jobsDone=" << jobsDone;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

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
      << "Scheduler received shutdown, but there are still abandoned threads";
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
}

void SupervisedScheduler::runWorker() {
  uint64_t id;

  std::shared_ptr<WorkerState> state;

  {
    std::lock_guard<std::mutex> guard(_mutexSupervisor);
    id = _numWorkers++;  // increase the number of workers here, to obtain the id
    // copy shared_ptr with worker state
    state = _workerStates.back();
    // inform the supervisor that this thread is alive
    _conditionSupervisor.notify_one();
  }

  state->_sleepTimeout_ms = 20 * (id + 1);
  // cap the timeout to some boundary value
  if (state->_sleepTimeout_ms >= 1000) {
    state->_sleepTimeout_ms = 1000;
  }

  if (id < 32) {
    // 512 >> 32 => undefined behavior
    state->_queueRetryCount = (512 >> id) + 3;
  } else {
    // we want at least 3 retries
    state->_queueRetryCount = 3;
  }

  while (true) {
    std::unique_ptr<WorkItem> work = getWork(state);
    if (work == nullptr) {
      break;
    }

    _jobsDequeued++;

    try {
      state->_lastJobStarted = clock::now();
      state->_working = true;
      work->_handler();
      state->_working = false;
    } catch (std::exception const& ex) {
      LOG_TOPIC("a235e", ERR, Logger::THREADS)
          << "scheduler loop caught exception: " << ex.what();
    } catch (...) {
      LOG_TOPIC("d4121", ERR, Logger::THREADS)
          << "scheduler loop caught unknown exception";
    }

    _jobsDone.fetch_add(1, std::memory_order_release);
  }
}

void SupervisedScheduler::runSupervisor() {
  while (_numWorkers < _numIdleWorker) {
    startOneThread();
  }

  uint64_t lastJobsDone = 0, lastJobsSubmitted = 0;
  uint64_t jobsStallingTick = 0, lastQueueLength = 0;

  while (!_stopping) {
    uint64_t jobsDone = _jobsDone.load(std::memory_order_acquire);
    uint64_t jobsSubmitted = _jobsSubmitted.load(std::memory_order_acquire);
    uint64_t jobsDequeued = _jobsDequeued.load(std::memory_order_acquire);

    if (jobsDone == lastJobsDone && (jobsDequeued < jobsSubmitted)) {
      jobsStallingTick++;
    } else if (jobsStallingTick != 0) {
      jobsStallingTick--;
    }

    uint64_t queueLength = jobsSubmitted - jobsDequeued;

    bool doStartOneThread = (((queueLength >= 3 * _numWorkers) &&
                              ((lastQueueLength + _numWorkers) < queueLength)) ||
                             (lastJobsSubmitted > jobsDone)) &&
                            (queueLength != 0);

    bool doStopOneThread = ((((lastQueueLength < 10) || (lastQueueLength >= queueLength)) &&
                             (lastJobsSubmitted <= jobsDone)) ||
                            ((queueLength == 0) && (lastQueueLength == 0))) &&
                           ((rand() & 0x0F) == 0);

    lastJobsDone = jobsDone;
    lastQueueLength = queueLength;
    lastJobsSubmitted = jobsSubmitted;

    if (doStartOneThread && _numWorkers < _maxNumWorker) {
      jobsStallingTick = 0;
      startOneThread();
    } else if (doStopOneThread && _numWorkers > _numIdleWorker) {
      stopOneThread();
    }

    cleanupAbandonedThreads();
    sortoutLongRunningThreads();

    std::unique_lock<std::mutex> guard(_mutexSupervisor);

    if (_stopping) {
      break;
    }

    _conditionSupervisor.wait_for(guard, std::chrono::milliseconds(100));
  }
}

bool SupervisedScheduler::cleanupAbandonedThreads() {
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

  auto now = clock::now();
  auto i = _workerStates.begin();

  while (i != _workerStates.end()) {
    auto& state = *i;

    if (!state->_working) {
      i++;
      continue;
    }

    if ((now - state->_lastJobStarted) > std::chrono::seconds(5)) {
      LOG_TOPIC("efcaa", TRACE, Logger::THREADS) << "Detach long running thread.";

      {
        std::unique_lock<std::mutex> guard(_mutex);
        state->_stop = true;
      }

      // Move that thread to the abandoned thread
      _abandonedWorkerStates.push_back(std::move(state));
      i = _workerStates.erase(i);
      _numWorkers--;

      // and now start another thread!
      startOneThread();
    } else {
      i++;
    }
  }
}

std::unique_ptr<SupervisedScheduler::WorkItem> SupervisedScheduler::getWork(
    std::shared_ptr<WorkerState>& state) {
  WorkItem* work;

  while (!state->_stop) {
    uint64_t triesCount = 0;
    while (triesCount < state->_queueRetryCount) {
      // must keep some real or potential threads reserved for high priority
      if ((0 == (triesCount % 3)) || ((_jobsDequeued - _jobsDone) < (_maxNumWorker / 2))) {
        // access queue via 0 1 2 0 1 2 0 1 ...
        if (_queue[triesCount % 3].pop(work)) {
          return std::unique_ptr<WorkItem>(work);
        }
      } // if

      triesCount++;
      cpu_relax();
    } // while

    std::unique_lock<std::mutex> guard(_mutex);

    if (state->_stop) {
      break;
    }

    if (state->_sleepTimeout_ms == 0) {
      _conditionWork.wait(guard);
    } else {
      _conditionWork.wait_for(guard, std::chrono::milliseconds(state->_sleepTimeout_ms));
    }
  } // while

  return nullptr;
}

void SupervisedScheduler::startOneThread() {
  // TRI_ASSERT(_numWorkers < _maxNumWorker);
  if (_numWorkers + _abandonedWorkerStates.size() >= _maxNumWorker) {
    return;  // do not add more threads, than maximum allows
  }

  std::unique_lock<std::mutex> guard(_mutexSupervisor);

  // start a new thread

  //wait for windows fix or implement operator new
  #if (_MSC_VER >= 1)
  #pragma warning(push)
  #pragma warning(disable : 4316) // Object allocated on the heap may not be aligned for this type
  #endif
  _workerStates.emplace_back(std::make_shared<WorkerState>(*this));
  #if (_MSC_VER >= 1)
  #pragma warning(pop)
  #endif

  if (!_workerStates.back()->start()) {
    // failed to start a worker
    _workerStates.pop_back();  // pop_back deletes shared_ptr, which deletes thread
    LOG_TOPIC("913b5", ERR, Logger::THREADS)
        << "could not start additional worker thread";

  } else {
    LOG_TOPIC("f9de8", TRACE, Logger::THREADS) << "Started new thread";
    _conditionSupervisor.wait(guard);
  }
}

void SupervisedScheduler::stopOneThread() {
  TRI_ASSERT(_numWorkers > 0);

  // copy shared_ptr
  auto state = _workerStates.back();
  _workerStates.pop_back();

  {
    std::unique_lock<std::mutex> guard(_mutex);
    state->_stop = true;
    _conditionWork.notify_all();
    // _stop is set under the mutex, then all worker threads are notified.
    // in any case, the stopped thread should notice that it is stoped.
  }

  // However the thread may be working on a long job. Hence we enqueue it on
  // the cleanup list and wait for its termination.
  //
  // Since the thread is effectively taken out of the pool, decrease the number of worker.
  _numWorkers--;

  if (state->_thread->isRunning()) {
    LOG_TOPIC("73365", TRACE, Logger::THREADS) << "Abandon one thread.";
    _abandonedWorkerStates.push_back(std::move(state));
  } else {
    state.reset();  // reset the shared_ptr. At this point we delete the thread object
                    // Since the thread is already STOPPED, the join is a no op.
  }
}

SupervisedScheduler::WorkerState::WorkerState(SupervisedScheduler::WorkerState&& that) noexcept
    : _queueRetryCount(that._queueRetryCount),
      _sleepTimeout_ms(that._sleepTimeout_ms),
      _stop(that._stop.load()),
      _working(false),
      _thread(std::move(that._thread)) {}

SupervisedScheduler::WorkerState::WorkerState(SupervisedScheduler& scheduler)
    : _queueRetryCount(100),
      _sleepTimeout_ms(100),
      _stop(false),
      _working(false),
      _thread(new SupervisedSchedulerWorkerThread(scheduler)) {}

bool SupervisedScheduler::WorkerState::start() { return _thread->start(); }

// ---------------------------------------------------------------------------
// Statistics Stuff
// ---------------------------------------------------------------------------

Scheduler::QueueStatistics SupervisedScheduler::queueStatistics() const {
  // we need to read multiple atomics here. as all atomic reads happen independently
  // without a mutex outside, the overall picture may be inconsistent

  uint64_t const numWorkers = _numWorkers.load(std::memory_order_relaxed);

  // read _jobsDone first, so the differences of the counters cannot get negative
  uint64_t const jobsDone = _jobsDone.load(std::memory_order_relaxed);
  uint64_t const jobsDequeued = _jobsDequeued.load(std::memory_order_relaxed);
  uint64_t const jobsSubmitted = _jobsSubmitted.load(std::memory_order_relaxed);

  uint64_t const queued = jobsSubmitted - jobsDone;
  uint64_t const working = jobsDequeued - jobsDone;
  
  uint64_t const directExec = _jobsDirectExec.load(std::memory_order_relaxed);

  return QueueStatistics{numWorkers, 0, queued, working, directExec};
}

void SupervisedScheduler::toVelocyPack(velocypack::Builder& b) const {
  QueueStatistics qs = queueStatistics(); 

  b.add("scheduler-threads", VPackValue(qs._running)); // numWorkers
  b.add("blocked", VPackValue(qs._blocked)); // obsolete
  b.add("queued", VPackValue(qs._queued));
  b.add("in-progress", VPackValue(qs._working));
  b.add("direct-exec", VPackValue(qs._directExec));
}
