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

#include "Scheduler.h"
#include "SupervisedScheduler.h"

#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/cpu-relax.h"
#include "GeneralServer/RestHandler.h"
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"
#include "Rest/GeneralResponse.h"
#include "GeneralServer/Acceptor.h"
#include "GeneralServer/Task.h"
#include "Statistics/RequestStatistics.h"


using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

namespace {
static uint64_t getTickCount_ns () {
  auto now = std::chrono::high_resolution_clock::now();

  return std::chrono::duration_cast<std::chrono::nanoseconds>
    (now.time_since_epoch()).count();
}
}

namespace arangodb {
namespace rest {

class SupervisedSchedulerThread : virtual public Thread {
public:
  SupervisedSchedulerThread(SupervisedScheduler &scheduler) :
   Thread("Scheduler"), _scheduler(scheduler) {}
  ~SupervisedSchedulerThread() { shutdown(); }
protected:
  SupervisedScheduler &_scheduler;
};

class SupervisedSchedulerManagerThread final : public SupervisedSchedulerThread {
public:
  SupervisedSchedulerManagerThread(SupervisedScheduler &scheduler) :
   Thread("SchedMan"), SupervisedSchedulerThread(scheduler) {}
  void run() { _scheduler.runSupervisor(); };
};

class SupervisedSchedulerWorkerThread final : public SupervisedSchedulerThread {
public:
  SupervisedSchedulerWorkerThread(SupervisedScheduler &scheduler) :
   Thread("SchedWorker"), SupervisedSchedulerThread(scheduler) {}
  void run() { _scheduler.runWorker(); };
};

}
}

SupervisedScheduler::SupervisedScheduler(uint64_t minThreads,
                     uint64_t maxThreads,
                     uint64_t maxQueueSize,
                     uint64_t fifo1Size,
                     uint64_t fifo2Size
) :
  _numWorker(0),
  _stopping(false),
  _jobsSubmitted(0),
  _jobsDequeued(0),
  _jobsDone(0),
  _wakeupQueueLength(5),
  _wakeupTime_ns(1000),
  _definitiveWakeupTime_ns(100000),
  _maxNumWorker(maxThreads),
  _numIdleWorker(minThreads)
{
  _queue[0].reserve(maxQueueSize);
  _queue[1].reserve(fifo1Size);
  _queue[2].reserve(fifo2Size);
}

SupervisedScheduler::~SupervisedScheduler() {}

void SupervisedScheduler::post(std::function<void()> const& callback)
{
  queue(RequestPriority::POST, callback);
}

bool SupervisedScheduler::queue(RequestPriority prio, std::function<void()> const&handler)
{
  size_t queueNo = (size_t) prio;

  TRI_ASSERT(/*0 <= queueNo &&*/ queueNo <= 2);

  static thread_local uint64_t lastSubmitTime_ns;
  bool doNotify = false;

  WorkItem *work = new WorkItem(handler);

  if (!_queue[queueNo].push(work)) {
    delete work;
    return false;
  }

  // use memory order release to make sure, pushed item is visible
  uint64_t jobsSubmitted = _jobsSubmitted.fetch_add(1, std::memory_order_release);
  uint64_t approxQueueLength = _jobsDone - jobsSubmitted;

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


std::string  SupervisedScheduler::infoStatus() const {
  // TODO: compare with old output format
  // Does some code rely on that string or is it for humans?
  uint64_t numWorker = _numWorker.load(std::memory_order_relaxed);
  uint64_t queueLength = _jobsSubmitted.load(std::memory_order_relaxed)
    - _jobsDone.load(std::memory_order_relaxed);

  return "scheduler threads " + std::to_string(numWorker) + " (" +
         std::to_string(_numIdleWorker) + "<" + std::to_string(_maxNumWorker) +
         ") queued " + std::to_string(queueLength);
}

Scheduler::QueueStatistics SupervisedScheduler::queueStatistics() const
{
  uint64_t numWorker = _numWorker.load(std::memory_order_relaxed);
  uint64_t queueLength = _jobsSubmitted.load(std::memory_order_relaxed)
    - _jobsDone.load(std::memory_order_relaxed);

  return QueueStatistics{numWorker, numWorker, queueLength};
}

void SupervisedScheduler::addQueueStatistics(velocypack::Builder& b) const {
  uint64_t numWorker = _numWorker.load(std::memory_order_relaxed);
  uint64_t queueLength = _jobsSubmitted.load(std::memory_order_relaxed)
    - _jobsDone.load(std::memory_order_relaxed);

  // TODO: previous scheduler filled out a lot more fields, relevant?
  b.add("scheduler-threads",
        VPackValue(static_cast<int32_t>(numWorker)));
  b.add("queued", VPackValue(static_cast<int32_t>(queueLength)));
}

bool SupervisedScheduler::start() {

  _manager.reset(new SupervisedSchedulerManagerThread(*this));
  if (!_manager->start()) {
    LOG_TOPIC(ERR, Logger::SUPERVISION) << "could not start supervisor thread";
    return false;
  }

  return Scheduler::start();
}

void SupervisedScheduler::beginShutdown() {
  std::unique_lock<std::mutex> guard(_mutex);
  _stopping = true;
  _conditionWork.notify_all();
  Scheduler::beginShutdown();
}

void SupervisedScheduler::shutdown () {
  // call the destructor of all threads
  _manager.reset();

  while (_numWorker > 0) {
    stopOneThread();
  }

  Scheduler::shutdown();
}

void SupervisedScheduler::runWorker()
{
  uint64_t id;

  {
    // inform the supervisor that this thread is alive
    std::lock_guard<std::mutex> guard(_mutexSupervisor);
    id = _numWorker++;  // increase the number of workers here, to obtains the id
    _conditionSupervisor.notify_one();
  }

  // copy shared_ptr
  auto state = _workerStates.back();

  state->_sleepTimeout_ms = 20 * (id + 1);
  state->_queueRetryCount = (512 >> id) + 3;

  while (true) {
    std::unique_ptr<WorkItem> work = getWork(state);
    if (work == nullptr) {
      break ;
    }

    _jobsDequeued++;

    try {
      state->_lastJobStarted = clock::now();
      state->_working = true;
      work->_handler();
      state->_working = false;
    } catch (std::exception const& ex) {
      LOG_TOPIC(ERR, Logger::THREADS) << "scheduler loop caught exception: "
                                      << ex.what();
    } catch (...) {
      LOG_TOPIC(ERR, Logger::THREADS)
          << "scheduler loop caught unknown exception";
    }

    _jobsDone.fetch_add(1, std::memory_order_release);
  }
}

void SupervisedScheduler::runSupervisor()
{
  while (_numWorker < _numIdleWorker) {
    startOneThread();
  }

  uint64_t jobsDone, lastJobsDone = 0, jobsSubmitted, lastJobsSubmitted = 0;
  uint64_t jobsStallingTick = 0, queueLength, lastQueueLength = 0;
  uint64_t jobsDequeued;

  while (!_stopping) {
    jobsDone = _jobsDone.load(std::memory_order_acquire);
    jobsSubmitted = _jobsSubmitted.load(std::memory_order_acquire);
    jobsDequeued = _jobsDequeued.load(std::memory_order_acquire);

    if (jobsDone == lastJobsDone && (jobsDequeued < jobsSubmitted)) {
      jobsStallingTick++;
    } else if(jobsStallingTick != 0) {
      jobsStallingTick--;
    }

    queueLength = jobsSubmitted - jobsDequeued;

    bool doStartOneThread =
      ((/*(_numWorker < numCpuCores) &&*/ (queueLength >= 3 * _numWorker) && ((lastQueueLength + _numWorker) < queueLength))
      || (lastJobsSubmitted > jobsDone)) && (queueLength != 0);


    bool doStopOneThread = (
          (((lastQueueLength < 10) || (lastQueueLength >= queueLength))
        &&
          (lastJobsSubmitted <= jobsDone))
      ||
        ((queueLength == 0) && (lastQueueLength == 0)))
    &&
      ((rand() & 0x0F) == 0);




    lastJobsDone = jobsDone;
    lastQueueLength = queueLength;
    lastJobsSubmitted = jobsSubmitted;

    if (doStartOneThread && _numWorker < _maxNumWorker) {
      jobsStallingTick = 0;
      startOneThread();
    } else if(doStopOneThread && _numWorker > _numIdleWorker) {
      stopOneThread();
    }

    cleanupAbandonedThreads();
    sortoutLongRunningThreads();

    std::unique_lock<std::mutex> guard(_mutexSupervisor);

    if (_stopping) {
      break ;
    }

    _conditionSupervisor.wait_for(guard, std::chrono::milliseconds(100));
  }
}

void SupervisedScheduler::cleanupAbandonedThreads() {

  for (auto i = _abandonedWorkerStates.begin();
       i != _abandonedWorkerStates.end(); i++) {

    auto &state = *i;

    if (!state->_thread->isRunning()) {
      _abandonedWorkerStates.erase(i);
    }
  }
}

void SupervisedScheduler::sortoutLongRunningThreads() {

  // Detaching a thread always implies starting a new thread. Hence check here
  // if we can start a new thread.

  auto now = clock::now();

  for (auto i = _workerStates.begin(); i != _workerStates.end(); i++) {

    auto &state = *i;

    if (!state->_working) {
      continue ;
    }

    if ((now - state->_lastJobStarted) > std::chrono::seconds(5)) {
      LOG_TOPIC(ERR, Logger::SUPERVISION) << "Detach long running thread.";

      {
        std::unique_lock<std::mutex> guard(_mutex);
        state->_stop = true;
      }

      // Move that thread to the abandoned thread
      _abandonedWorkerStates.push_back(std::move(state));
      _workerStates.erase(i);
      _numWorker--;

      // and now start another thread!
      startOneThread();
    }
  }
}

std::unique_ptr<SupervisedScheduler::WorkItem> SupervisedScheduler::getWork (std::shared_ptr<WorkerState> &state)
{
  WorkItem *work;

  while (!_stopping && !state->_stop) {

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

    if (_stopping || state->_stop) {
      break ;
    }

    if (state->_sleepTimeout_ms == 0) {
      _conditionWork.wait(guard);
    } else {
      _conditionWork.wait_for(guard, std::chrono::milliseconds(state->_sleepTimeout_ms));
    }
  }

  return nullptr;
}

void SupervisedScheduler::startOneThread()
{
  //TRI_ASSERT(_numWorker < _maxNumWorker);
  if (_numWorker + _abandonedWorkerStates.size() >= _maxNumWorker) {
    return ;  // do not add more threads, than maximum allows
  }

  std::unique_lock<std::mutex> guard(_mutexSupervisor);

  // start a new thread
  _workerStates.emplace_back(std::make_shared<WorkerState>(*this));
  if (!_workerStates.back()->start()) {
    // failed to start a worker
    _workerStates.pop_back();   // pop_back deletes shared_ptr, which deletes thread
    LOG_TOPIC(ERR, Logger::SUPERVISION) << "could not start additional worker thread";

  } else {
    _conditionSupervisor.wait(guard);
  }
}

void SupervisedScheduler::stopOneThread()
{
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
      LOG_TOPIC(ERR, Logger::SUPERVISION) << "Abandon one thread.";
    _abandonedWorkerStates.push_back(std::move(state));
  } else {
    state.reset();  // reset the shared_ptr. At this point we delete the thread object
                    // Since the thread is already STOPPED, the join is a no op.
  }
}

SupervisedScheduler::WorkerState::WorkerState(SupervisedScheduler::WorkerState &&that) :
  _queueRetryCount(that._queueRetryCount),
  _sleepTimeout_ms(that._sleepTimeout_ms),
  _stop(that._stop.load()),
  _thread(std::move(that._thread)) {}

SupervisedScheduler::WorkerState::WorkerState(SupervisedScheduler &scheduler) :
  _queueRetryCount(100),
  _sleepTimeout_ms(100),
  _stop(false),
  _thread(new SupervisedSchedulerWorkerThread(scheduler)) {}

bool SupervisedScheduler::WorkerState::start() {
  return _thread->start();
}
