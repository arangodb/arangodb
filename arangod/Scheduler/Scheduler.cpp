////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/WorkMonitor.h"
#include "GeneralServer/RestHandler.h"
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"
#include "Rest/GeneralResponse.h"
#include "Scheduler/JobGuard.h"
#include "Scheduler/JobQueue.h"
#include "Scheduler/Task.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include <thread>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

namespace {    
constexpr double MIN_SECONDS = 30.0;
}

// -----------------------------------------------------------------------------
// --SECTION--                                            SchedulerManagerThread
// -----------------------------------------------------------------------------

namespace {
class SchedulerManagerThread : public Thread {
 public:
  SchedulerManagerThread(Scheduler* scheduler, boost::asio::io_service* service)
      : Thread("SchedulerManager", true), _scheduler(scheduler), _service(service) {}

  ~SchedulerManagerThread() { shutdown(); }

 public:
  void run() {
    while (!_scheduler->isStopping()) {
      try {
        _service->run_one();
      } catch (...) {
        LOG_TOPIC(ERR, Logger::THREADS)
            << "manager loop caught an error, restarting";
      }
    }
  }

 private:
  Scheduler* _scheduler;
  boost::asio::io_service* _service;
};
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   SchedulerThread
// -----------------------------------------------------------------------------

namespace {
class SchedulerThread : public Thread {
 public:
  SchedulerThread(Scheduler* scheduler, boost::asio::io_service* service)
    : Thread("Scheduler", true), _scheduler(scheduler), _service(service) {}

  ~SchedulerThread() { shutdown(); }

 public:
  void run() {
    constexpr size_t EVERY_LOOP = size_t(MIN_SECONDS);
    
    // when we enter this method, 
    // _nrRunning has already been increased for this thread
    LOG_TOPIC(DEBUG, Logger::THREADS) << "started thread ("
                                      << _scheduler->infoStatus() << ")";
  
    // some random delay value to avoid all initial threads checking for
    // their deletion at the very same time
    double const randomWait =
        static_cast<double>(RandomGenerator::interval(int64_t(0), static_cast<int64_t>(MIN_SECONDS * 0.5)));

    double start = TRI_microtime() + randomWait;
    size_t counter = 0;
    bool doDecrement = true;

    while (!_scheduler->isStopping()) {
      try {
        _service->run_one();
      } catch (std::exception const& ex) {
        LOG_TOPIC(ERR, Logger::THREADS)
            << "scheduler loop caught exception: " << ex.what();
      } catch (...) {
        LOG_TOPIC(ERR, Logger::THREADS)
            << "scheduler loop caught unknown exception";
      }
          
      if (++counter > EVERY_LOOP) {
        counter = 0;

        double const now = TRI_microtime();

        if (now - start > MIN_SECONDS) {
          // test if we should stop this thread
          // if this returns true, nrRunning will have been
          // decremented by one already
          if (_scheduler->stopThreadIfTooMany(now)) {
            // nrRunning was decremented already. now exit thread
            // main loop
            doDecrement = false;
            break;
          }
         
          // use new start time 
          start = now;
        }
      }
    }

    LOG_TOPIC(DEBUG, Logger::THREADS) << "stopped ("
                                      << _scheduler->infoStatus() << ")";

    if (doDecrement) {
      // only decrement here if this wasn't already done above
      _scheduler->stopThread();
    }
  }

 private:
  Scheduler* _scheduler;
  boost::asio::io_service* _service;
};
}

// -----------------------------------------------------------------------------
// --SECTION--                                                         Scheduler
// -----------------------------------------------------------------------------

Scheduler::Scheduler(uint64_t nrMinimum, uint64_t /*nrDesired*/, 
                     uint64_t nrMaximum, uint64_t maxQueueSize)
    : _maxQueueSize(maxQueueSize),
      _nrMinimum(nrMinimum),
      _nrMaximum(nrMaximum),
      _counters(0),
      _nrQueued(0),
      _lastAllBusyStamp(0.0) {
  // setup signal handlers
  initializeSignalHandlers();
}

Scheduler::~Scheduler() {
  stopRebalancer();
}

void Scheduler::post(std::function<void()> callback) {
  ++_nrQueued;

  _ioService.get()->post([this, callback]() {
    --_nrQueued;

    JobGuard guard(this);
    guard.work();

    callback();
  });
}

bool Scheduler::start(ConditionVariable* cv) {
  // start the I/O
  startIoService();

  TRI_ASSERT(0 < _nrMinimum);
  TRI_ASSERT(_nrMinimum <= _nrMaximum);

  for (uint64_t i = 0; i < _nrMinimum; ++i) {
    {
      MUTEX_LOCKER(locker, _threadCreateLock);
      incRunning();
    }
    try {
      startNewThread();
    } catch (...) {
      MUTEX_LOCKER(locker, _threadCreateLock);
      decRunning();
      throw;
    }
  }

  startManagerThread();
  startRebalancer();

  // initialize the queue handling
  _jobQueue.reset(new JobQueue(_maxQueueSize, this));
  _jobQueue->start();

  // done
  LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "all scheduler threads are up and running";
  return true;
}

void Scheduler::startIoService() {
  _ioService.reset(new boost::asio::io_service());
  _serviceGuard.reset(new boost::asio::io_service::work(*_ioService));

  _managerService.reset(new boost::asio::io_service());
  _managerGuard.reset(new boost::asio::io_service::work(*_managerService));
}

void Scheduler::startRebalancer() {
  std::chrono::milliseconds interval(100);
  _threadManager.reset(new boost::asio::steady_timer(*_managerService));

  _threadHandler = [this, interval](const boost::system::error_code& error) {
    if (error || isStopping()) {
      return;
    }

    rebalanceThreads();

    if (_threadManager != nullptr) {
      _threadManager->expires_from_now(interval);
      _threadManager->async_wait(_threadHandler);
    }
  };

  _threadManager->expires_from_now(interval);
  _threadManager->async_wait(_threadHandler);
}

void Scheduler::stopRebalancer() noexcept {
  if (_threadManager != nullptr) {
    try {
      _threadManager->cancel();
    } catch (...) {
    }
  }
}

void Scheduler::startManagerThread() {
  auto thread = new SchedulerManagerThread(this, _managerService.get());
  thread->start();
}

void Scheduler::startNewThread() {
  auto thread = new SchedulerThread(this, _ioService.get());
  thread->start();
}
    
void Scheduler::stopThread() {
  MUTEX_LOCKER(locker, _threadCreateLock);
  decRunning();
}

// check if the current thread should be stopped
// returns true if yes, otherwise false. when the function returns
// true, it has already decremented the nrRunning counter!
bool Scheduler::stopThreadIfTooMany(double now) {
  // make sure no extra threads are created while we check the timestamp
  // and while we modify nrRunning
  
  uint64_t const queueCap = std::max(uint64_t(1), uint64_t(_nrMaximum / 4));
  uint64_t const nrQueued = std::min(_nrQueued.load(), queueCap);
  
  MUTEX_LOCKER(locker, _threadCreateLock);
  
  // fetch all counters in one atomic operation
  uint64_t counters = _counters.load();
  uint64_t const nrRunning = numRunning(counters);
  uint64_t const nrBlocked = numBlocked(counters);
  uint64_t const nrWorking = numWorking(counters);

  if (nrRunning <= _nrMinimum + nrBlocked) {
    // don't stop a thread if we already reached the minimum 
    // number of threads
    _lastAllBusyStamp = now;
    return false;
  }

  if (nrRunning <= nrWorking + nrQueued) {
    return false;
  }
  
  if (_lastAllBusyStamp + 1.25 * MIN_SECONDS >= now) {
    // last time all threads were busy is less than x seconds ago
    return false;
  }
  
  // set the all busy stamp. this avoids that we shut down all threads
  // at the same time
  if (_lastAllBusyStamp < now - MIN_SECONDS / 2.0) {
    _lastAllBusyStamp = now - MIN_SECONDS / 2.0;
  }

  // decrement nrRunning by one already in here while holding the lock
  decRunning();
  return true;
}

bool Scheduler::shouldQueueMore() const {
  uint64_t const counters = _counters.load();
  uint64_t const nrWorking = numWorking(counters);

  if (nrWorking + _nrQueued < _nrMaximum) {
    return true;
  }

  return false;
}

bool Scheduler::hasQueueCapacity() const {
  if (!shouldQueueMore()) {
    return false;
  }

  auto jobQueue = _jobQueue.get();
  auto queueSize = (jobQueue == nullptr) ? 0 : jobQueue->queueSize();

  return queueSize == 0;
}

bool Scheduler::queue(std::unique_ptr<Job> job) {
  auto jobQueue = _jobQueue.get();
  auto queueSize = (jobQueue == nullptr) ? 0 : jobQueue->queueSize();
  RequestStatistics::SET_QUEUE_START(job->_handler->statistics(), queueSize);

  return _jobQueue->queue(std::move(job));
}

std::string Scheduler::infoStatus() {
  auto jobQueue = _jobQueue.get();
  auto queueSize = (jobQueue == nullptr) ? 0 : jobQueue->queueSize();

  uint64_t const counters = _counters.load();
  return "working: " + std::to_string(numWorking(counters)) + ", queued: " +
         std::to_string(_nrQueued) + ", blocked: " +
         std::to_string(numBlocked(counters)) + ", running: " +
         std::to_string(numRunning(counters)) + ", outstanding: " +
         std::to_string(queueSize) + ", min/max: " +
         std::to_string(_nrMinimum) + "/" + std::to_string(_nrMaximum);
}

void Scheduler::rebalanceThreads() {
  static uint64_t count = 0;

  ++count;

  if (count % 50 == 0) {
    LOG_TOPIC(DEBUG, Logger::THREADS) << "rebalancing threads: " << infoStatus();
  } else if (count % 5 == 0) {
    LOG_TOPIC(TRACE, Logger::THREADS) << "rebalancing threads: " << infoStatus();
  }
      
  uint64_t const queueCap = std::max(uint64_t(1), uint64_t(_nrMaximum / 4));

  while (true) {
    {
      double const now = TRI_microtime();
      
      uint64_t const nrQueued = std::min(_nrQueued.load(), queueCap);

      MUTEX_LOCKER(locker, _threadCreateLock);

      uint64_t const counters = _counters.load();
      uint64_t const nrRunning = numRunning(counters);
      uint64_t const nrWorking = numWorking(counters);
      uint64_t const nrBlocked = numBlocked(counters);

      if (nrRunning >= std::max(_nrMinimum, nrWorking + nrQueued)) { // + std::min(nrBlocked, uint64_t(8)))) {
        // all threads are working, and none are blocked. so there is no
        // need to start a new thread now
        if (nrWorking == nrRunning) {
          // still note that all threads are maxed out
          _lastAllBusyStamp = now;
        }
        break;
      }

      if (nrRunning >= _nrMaximum + nrBlocked) {
        // reached the maximum now
        break;
      }
    
      if (isStopping(counters)) {
        // do not start any new threads in case we are already shutting down
        break;
      }
      
      // LOG_TOPIC(ERR, Logger::THREADS) << "starting new thread. nrRunning: " << nrRunning << ", nrWorking: " << nrWorking << ", nrBlocked: " << nrBlocked << ", nrQueued: " << nrQueued;

      // all threads are maxed out
      _lastAllBusyStamp = now;
      // increase nrRunning by one here already, while holding the lock
      incRunning();
    }

    // create thread and sleep without holding the mutex
    try {
      // actually start the new thread
      startNewThread();
    } catch (...) {
      // if it fails, we have to rollback the increase of nrRunning again
      MUTEX_LOCKER(locker, _threadCreateLock);
      decRunning();
      throw;
    }

    usleep(5000);
  }
}

void Scheduler::beginShutdown() {
  if (isStopping()) {
    return;
  }
  
  _jobQueue->beginShutdown();

  stopRebalancer();
  _threadManager.reset();

  _managerGuard.reset();
  _managerService->stop();

  _serviceGuard.reset();
  _ioService->stop();

  // set the flag AFTER stopping the threads
  setStopping();
}

void Scheduler::shutdown() {
  while (true) {
    uint64_t const counters = _counters.load();
    
    if (numRunning(counters) == 0 && numWorking(counters) == 0) {
      break;
    }

    std::this_thread::yield();
  }

  // remove all queued work descriptions in the work monitor first
  // before freeing the io service a few lines later
  // this is required because the work descriptions may have captured
  // HttpCommTasks etc. which have references to the io service and
  // access it in their destructors
  // so the proper shutdown order is:
  // - stop accepting further requests (already done by GeneralServerFeature::stop)
  // - cancel all running scheduler tasks
  // - free all work descriptions in work monitor
  // - delete io service
  WorkMonitor::clearWorkDescriptions();

  _managerService.reset();
  _ioService.reset();
}

void Scheduler::initializeSignalHandlers() {
#ifdef _WIN32
// Windows does not support POSIX signal handling
#else
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  sigfillset(&action.sa_mask);

  // ignore broken pipes
  action.sa_handler = SIG_IGN;

  int res = sigaction(SIGPIPE, &action, 0);

  if (res < 0) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot initialize signal handlers for pipe";
  }
#endif
}
