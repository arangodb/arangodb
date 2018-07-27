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

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include <thread>

#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/cpu-relax.h"
#include "GeneralServer/RestHandler.h"
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"
#include "Rest/GeneralResponse.h"
#include "Scheduler/Acceptor.h"
#include "Scheduler/JobGuard.h"
#include "Scheduler/Task.h"
#include "Statistics/RequestStatistics.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

namespace arangodb {
namespace rest {
class SchedulerThread : virtual public Thread {
public:
  SchedulerThread(Scheduler &scheduler) : Thread("scheduler"), _scheduler(scheduler) {}
  ~SchedulerThread() { shutdown(); }
protected:
  Scheduler &_scheduler;
};

class SchedulerManagerThread final : public SchedulerThread {
public:
  SchedulerManagerThread(Scheduler &scheduler) : Thread("sched-man"), SchedulerThread(scheduler) {}
  void run() { _scheduler.runSupervisor(); };
};

class SchedulerWorkerThread final : public SchedulerThread {
public:
  SchedulerWorkerThread(Scheduler &scheduler) : Thread("sched-work"), SchedulerThread(scheduler) {}
  void run() { _scheduler.runWorker(); };
};

class SchedulerContextThread final : public SchedulerThread {
public:
  SchedulerContextThread(Scheduler &scheduler) : Thread("sched-ctx"), SchedulerThread(scheduler) {}
  void run() { _scheduler._obsoleteContext.run(); };
};

class SchedulerCronThread : public SchedulerThread {
public:
  SchedulerCronThread(Scheduler &scheduler) : Thread("sched-cron"), SchedulerThread(scheduler) {}
  void run() { _scheduler.runCron(); };
};

}
}

static uint64_t getTickCount_ns ()
{
  auto now = std::chrono::high_resolution_clock::now();

  return std::chrono::duration_cast<std::chrono::nanoseconds>
    (now.time_since_epoch()).count();
}

Scheduler::Scheduler(uint64_t minThreads,
                     uint64_t maxThreads,
                     uint64_t maxQueueSize,
                     uint64_t fifo1Size,
                     uint64_t fifo2Size
) :
  _numWorker(0),
  _stopping(false),
  _jobsSubmitted(0),
  _jobsDone(0),
  _wakeupQueueLength(5),
  _wakeupTime_ns(1000),
  _definitiveWakeupTime_ns(100000),
  _maxNumWorker(maxThreads),
  _numIdleWorker(minThreads),
  _obsoleteWork(_obsoleteContext)
{
  _queue[0].reserve(maxQueueSize);
  _queue[1].reserve(fifo1Size);
  _queue[2].reserve(fifo2Size);
  _workerStates.reserve(maxThreads);

#ifdef _WIN32
// Windows does not support POSIX signal handling
#else
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  sigfillset(&action.sa_mask);

  // ignore broken pipes
  action.sa_handler = SIG_IGN;

  int res = sigaction(SIGPIPE, &action, nullptr);

  if (res < 0) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "cannot initialize signal handlers for pipe";
  }
#endif
}

Scheduler::~Scheduler() {

}

void Scheduler::post(std::function<void()> const& callback)
{
  queue(RequestPriority::POST, callback);
}

bool Scheduler::queue(RequestPriority prio, std::function<void()> const&handler)
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


std::string  Scheduler::infoStatus() {
  uint64_t numWorker = _numWorker.load(std::memory_order_relaxed);
  uint64_t queueLength = _jobsSubmitted.load(std::memory_order_relaxed)
    - _jobsDone.load(std::memory_order_relaxed);

  return "scheduler threads " + std::to_string(numWorker) + " (" +
         std::to_string(_numIdleWorker) + "<" + std::to_string(_maxNumWorker) +
         ") queued " + std::to_string(queueLength);
}

Scheduler::QueueStatistics Scheduler::queueStatistics() const
{
  uint64_t numWorker = _numWorker.load(std::memory_order_relaxed);
  uint64_t queueLength = _jobsSubmitted.load(std::memory_order_relaxed)
    - _jobsDone.load(std::memory_order_relaxed);

  return QueueStatistics{numWorker, numWorker, queueLength};
}

void Scheduler::addQueueStatistics(velocypack::Builder& b) const {
  uint64_t numWorker = _numWorker.load(std::memory_order_relaxed);
  uint64_t queueLength = _jobsSubmitted.load(std::memory_order_relaxed)
    - _jobsDone.load(std::memory_order_relaxed);

  b.add("scheduler-threads",
        VPackValue(static_cast<int32_t>(numWorker)));
  b.add("queued", VPackValue(static_cast<int32_t>(queueLength)));
}

bool Scheduler::start() {

  _contextThread.reset(new SchedulerContextThread(*this));
  _contextThread->start();

  _manager.reset(new SchedulerManagerThread(*this));
  _manager->start();

  _cronThread.reset(new SchedulerCronThread(*this));
  _cronThread->start();

  postDelay(std::chrono::seconds(5), []() {
    LOG_TOPIC(INFO, arangodb::Logger::FIXME)
        << "Delayed post works!";
  }).detach();

  return true;
}

void Scheduler::beginShutdown() {
  std::unique_lock<std::mutex> guard(_mutex);
  std::unique_lock<std::mutex> guard2(_priorityQueueMutex);
  _stopping = true;
  _conditionWork.notify_all();
  _conditionCron.notify_one();
  _obsoleteContext.stop();
}

void Scheduler::shutdown () {
  // call the destructor of all threads
  _contextThread.reset();
  _manager.reset();
  _cronThread.reset();

  while (_numWorker > 0) {
    stopOneThread();
  }

}

Scheduler::WorkerState::WorkerState(Scheduler &scheduler) :
  _queueRetryCount(100),
  _sleepTimeout_ms(100),
  _stop(false),
  _thread(new SchedulerWorkerThread(scheduler))
{
  _thread->start();
}

void Scheduler::runWorker()
{
  uint64_t id;

  {
    // inform the supervisor that this thread is alive
    std::lock_guard<std::mutex> guard(_mutexSupervisor);
    id = _numWorker++;
    _conditionSupervisor.notify_one();
  }

  auto &state = _workerStates[id];
  WorkItem *work;

  while (getWork(work, state)) {
    work->_handler();
    delete work;
    _jobsDone.fetch_add(1, std::memory_order_release);
  }

  _numWorker--;
}

void Scheduler::runSupervisor()
{
  while (_numWorker < _numIdleWorker) {
    startOneThread();
  }

  uint64_t jobsDone, lastJobsDone = 0, jobsSubmitted;
  uint64_t jobsStallingTick = 0, queueLength, lastQueueLength = 0;

  while (!_stopping) {
    jobsDone = _jobsDone.load(std::memory_order_acquire);
    jobsSubmitted = _jobsSubmitted.load(std::memory_order_acquire);

    if (jobsDone == lastJobsDone && (jobsDone < jobsSubmitted)) {
      jobsStallingTick++;
    } else {
      jobsStallingTick = jobsStallingTick == 0 ? 0 : jobsStallingTick - 1;
    }

    queueLength = jobsSubmitted - jobsDone;

    bool doStartOneThread =
      (jobsStallingTick > 5) || ((queueLength >= 50) && (1.5 * lastQueueLength < queueLength));

    bool doStopOneThread =
      (queueLength == 0);

    lastJobsDone = jobsDone;
    lastQueueLength = queueLength;

    if (doStartOneThread && _numWorker < _maxNumWorker) {
      jobsStallingTick = 0;
      startOneThread();
    } else if(doStopOneThread && _numWorker > _numIdleWorker) {
      stopOneThread();
    }

    std::unique_lock<std::mutex> guard(_mutexSupervisor);

    if (_stopping) {
      break ;
    }

    auto status = _conditionSupervisor.wait_for(guard, std::chrono::milliseconds(10));
    if (status != std::cv_status::timeout) {
      break ;
    }
  }
}

bool Scheduler::getWork (WorkItem *&work, WorkerState &state)
{
  while (!_stopping && !state._stop) {

    uint64_t triesCount = 0;
    while (triesCount < state._queueRetryCount) {

      // access queue via 0 1 2 0 1 2 0 1 ...
      if (_queue[triesCount % 3].pop(work)) {
        return true;
      }

      triesCount++;
      cpu_relax();
    }

    std::unique_lock<std::mutex> guard(_mutex);

    if (_stopping || state._stop) {
      break ;
    }

    if (state._sleepTimeout_ms == 0) {
      _conditionWork.wait(guard);
    } else {
      _conditionWork.wait_for(guard, std::chrono::milliseconds(state._sleepTimeout_ms));
    }
  }

  return false;
}

void Scheduler::startOneThread()
{
  TRI_ASSERT(_numWorker < _maxNumWorker);
  std::unique_lock<std::mutex> guard(_mutexSupervisor);

  // start a new thread
  _workerStates.emplace_back(*this);
  _conditionSupervisor.wait(guard);
}

void Scheduler::stopOneThread()
{
  TRI_ASSERT(_numWorker > 0);

  {
    std::unique_lock<std::mutex> guard(_mutex);
    _workerStates.back()._stop = true;
    _conditionWork.notify_all();
  }

  // wait for the thread to terminate
  _workerStates.pop_back();
}

Scheduler::WorkHandle Scheduler::postDelay(clock::duration delay,
  std::function<void()> const& callback) {

  if (delay < std::chrono::milliseconds(1)) {
    post(callback);
    return WorkHandle{};
  }

  std::unique_lock<std::mutex> guard(_priorityQueueMutex);

  auto handle = std::make_shared<Scheduler::DelayedWork>(callback, delay);
  _priorityQueue.push(handle);

  if (delay < std::chrono::milliseconds(50)) {
    // wakeup thread
    _conditionCron.notify_one();
  }

  return handle;
}

void Scheduler::runCron() {


  while (!_stopping) {

    auto now = clock::now();

    std::unique_lock<std::mutex> guard(_priorityQueueMutex);

    clock::duration sleepTime = std::chrono::milliseconds(50);

    while (_priorityQueue.size() > 0) {
      auto &top = _priorityQueue.top();

      if (top->_cancelled) {
        _priorityQueue.pop();
      } else if (top->_due < now) {
        post(top->_handler);
        _priorityQueue.pop();
      } else {
        auto then = (top->_due - now);

        sleepTime = sleepTime > then ? then : sleepTime;
        break ;
      }
    }

    _conditionCron.wait_for(guard, sleepTime);

  }

}

