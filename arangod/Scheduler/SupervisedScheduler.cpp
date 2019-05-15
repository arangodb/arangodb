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
#include "GeneralServer/Acceptor.h"
#include "GeneralServer/RestHandler.h"
#include "GeneralServer/Task.h"
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"
#include "Rest/GeneralResponse.h"
#include "Statistics/RequestStatistics.h"

using namespace arangodb;
using namespace arangodb::basics;

namespace {
static uint64_t getTickCount_ns() {
  auto now = std::chrono::high_resolution_clock::now();

  return std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch())
      .count();
}
}  // namespace

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
    : _numWorker(0),
      _stopping(false),
      _jobsSubmitted(0),
      _jobsDequeued(0),
      _jobsDone(0),
      _jobsDirectExec(0),
      _wakeupQueueLength(5),
      _wakeupTime_ns(1000),
      _definitiveWakeupTime_ns(100000),
      _maxNumWorker(maxThreads),
      _numIdleWorker(minThreads) {
  _queue[0].reserve(maxQueueSize);
  _queue[1].reserve(fifo1Size);
  _queue[2].reserve(fifo2Size);
}

SupervisedScheduler::~SupervisedScheduler() {
  LOG_DEVEL << "direct exec: " << _jobsDirectExec;
}

namespace {
  bool isDirectDeadlockLane(RequestLane lane) {
    // Some lane have tasks that deadlock because they hold a mutex whil calling queue that must be locked to execute the handler.
    // Those tasks can not be executed directly.
    //return true;
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
}

bool SupervisedScheduler::queue(RequestLane lane, std::function<void()> handler) {
  size_t queueNo = (size_t)PriorityRequestLane(lane);

  TRI_ASSERT(queueNo <= 2);
  TRI_ASSERT(isStopping() == false);

  static thread_local uint64_t lastSubmitTime_ns;
  bool doNotify = false;


  uint64_t approxQueueLength = _jobsSubmitted - _jobsDone;
  if (!isDirectDeadlockLane(lane) && approxQueueLength < 10) {
    _jobsDirectExec.fetch_add(1, std::memory_order_relaxed);
    handler();
    return true;
  }


  WorkItem* work = new WorkItem(std::move(handler));

  if (!_queue[queueNo].push(work)) {
    delete work;
    return false;
  }

  // use memory order release to make sure, pushed item is visible
  uint64_t jobsSubmitted = _jobsSubmitted.fetch_add(1, std::memory_order_release);
  approxQueueLength = jobsSubmitted - _jobsDone;
  uint64_t now_ns = getTickCount_ns();
  uint64_t sleepyTime_ns = now_ns - lastSubmitTime_ns;
  lastSubmitTime_ns = now_ns;

  if (sleepyTime_ns > _definitiveWakeupTime_ns.load(std::memory_order_relaxed)) {
    doNotify = true;

  } else if (sleepyTime_ns > _wakeupTime_ns) {
    if (approxQueueLength > _wakeupQueueLength.load(std::memory_order_relaxed)) {
      doNotify = true;
    }
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

  while (_numWorker > 0) {
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
    id = _numWorker++;  // increase the number of workers here, to obtains the id
    // copy shared_ptr with worker state
    state = _workerStates.back();
    // inform the supervisor that this thread is alive
    _conditionSupervisor.notify_one();
  }

  state->_sleepTimeout_ms = 20 * (id + 1);
  state->_queueRetryCount = (512 >> id) + 3;

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
  while (_numWorker < _numIdleWorker) {
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

    bool doStartOneThread = (((queueLength >= 3 * _numWorker) &&
                              ((lastQueueLength + _numWorker) < queueLength)) ||
                             (lastJobsSubmitted > jobsDone)) &&
                            (queueLength != 0);

    bool doStopOneThread = ((((lastQueueLength < 10) || (lastQueueLength >= queueLength)) &&
                             (lastJobsSubmitted <= jobsDone)) ||
                            ((queueLength == 0) && (lastQueueLength == 0))) &&
                           ((rand() & 0x0F) == 0);

    lastJobsDone = jobsDone;
    lastQueueLength = queueLength;
    lastJobsSubmitted = jobsSubmitted;

    if (doStartOneThread && _numWorker < _maxNumWorker) {
      jobsStallingTick = 0;
      startOneThread();
    } else if (doStopOneThread && _numWorker > _numIdleWorker) {
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
      _numWorker--;

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
      // access queue via 0 1 2 0 1 2 0 1 ...
      if (_queue[triesCount % 3].pop(work)) {
        return std::unique_ptr<WorkItem>(work);
      }

      triesCount++;
      cpu_relax();
    }

    std::unique_lock<std::mutex> guard(_mutex);

    if (state->_stop) {
      break;
    }

    if (state->_sleepTimeout_ms == 0) {
      _conditionWork.wait(guard);
    } else {
      _conditionWork.wait_for(guard, std::chrono::milliseconds(state->_sleepTimeout_ms));
    }
  }

  return nullptr;
}

void SupervisedScheduler::startOneThread() {
  // TRI_ASSERT(_numWorker < _maxNumWorker);
  if (_numWorker + _abandonedWorkerStates.size() >= _maxNumWorker) {
    return;  // do not add more threads, than maximum allows
  }

  std::unique_lock<std::mutex> guard(_mutexSupervisor);

  // start a new thread
  _workerStates.emplace_back(std::make_shared<WorkerState>(*this));
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
  TRI_ASSERT(_numWorker > 0);

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
  _numWorker--;

  if (state->_thread->isRunning()) {
    LOG_TOPIC("73365", TRACE, Logger::THREADS) << "Abandon one thread.";
    _abandonedWorkerStates.push_back(std::move(state));
  } else {
    state.reset();  // reset the shared_ptr. At this point we delete the thread object
                    // Since the thread is already STOPPED, the join is a no op.
  }
}

SupervisedScheduler::WorkerState::WorkerState(SupervisedScheduler::WorkerState&& that)
    : _queueRetryCount(that._queueRetryCount),
      _sleepTimeout_ms(that._sleepTimeout_ms),
      _stop(that._stop.load()),
      _thread(std::move(that._thread)) {}

SupervisedScheduler::WorkerState::WorkerState(SupervisedScheduler& scheduler)
    : _queueRetryCount(100),
      _sleepTimeout_ms(100),
      _stop(false),
      _thread(new SupervisedSchedulerWorkerThread(scheduler)),
      _padding() {}

bool SupervisedScheduler::WorkerState::start() { return _thread->start(); }

// ---------------------------------------------------------------------------
// Statistics Stuff
// ---------------------------------------------------------------------------
std::string SupervisedScheduler::infoStatus() const {
  // TODO: compare with old output format
  // Does some code rely on that string or is it for humans?
  uint64_t numWorker = _numWorker.load(std::memory_order_relaxed);
  uint64_t queueLength = _jobsSubmitted.load(std::memory_order_relaxed) -
                         _jobsDone.load(std::memory_order_relaxed);

  return "scheduler threads " + std::to_string(numWorker) + " (" +
         std::to_string(_numIdleWorker) + "<" + std::to_string(_maxNumWorker) +
         ") queued " + std::to_string(queueLength);
}

Scheduler::QueueStatistics SupervisedScheduler::queueStatistics() const {
  uint64_t numWorker = _numWorker.load(std::memory_order_relaxed);
  uint64_t queueLength = _jobsSubmitted.load(std::memory_order_relaxed) -
                         _jobsDone.load(std::memory_order_relaxed);

  return QueueStatistics{numWorker, numWorker, queueLength, 0, 0, 0};
}

void SupervisedScheduler::addQueueStatistics(velocypack::Builder& b) const {
  uint64_t numWorker = _numWorker.load(std::memory_order_relaxed);
  uint64_t queueLength = _jobsSubmitted.load(std::memory_order_relaxed) -
                         _jobsDone.load(std::memory_order_relaxed);

  // TODO: previous scheduler filled out a lot more fields, relevant?
  b.add("scheduler-threads", VPackValue(static_cast<int32_t>(numWorker)));
  b.add("queued", VPackValue(static_cast<int32_t>(queueLength)));
}
