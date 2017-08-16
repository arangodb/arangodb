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

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

#include "Scheduler.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/WorkMonitor.h"
#include "GeneralServer/RestHandler.h"
#include "Logger/Logger.h"
#include "Rest/GeneralResponse.h"
#include "Scheduler/JobGuard.h"
#include "Scheduler/JobQueue.h"
#include "Scheduler/Task.h"

#include <thread>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                            SchedulerManagerThread
// -----------------------------------------------------------------------------

namespace {
class SchedulerManagerThread : public Thread {
 public:
  SchedulerManagerThread(Scheduler* scheduler, boost::asio::io_service* service)
      : Thread("SchedulerManager"), _scheduler(scheduler), _service(service) {}

  ~SchedulerManagerThread() { shutdown(); }

 public:
  void run() {
    while (!_scheduler->isStopping()) {
      try {
        _service->run_one();
        _scheduler->deleteOldThreads();
      } catch (...) {
        LOG_TOPIC(ERR, Logger::THREADS)
            << "manager loop caught an error, restarting";
      }
    }

    _scheduler->threadDone(this);
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
      : Thread("Scheduler"), _scheduler(scheduler), _service(service) {}

  ~SchedulerThread() { shutdown(); }

 public:
  void run() {
    try {
      _scheduler->incRunning();
      TRI_DEFER(_scheduler->threadDone(this));

      LOG_TOPIC(DEBUG, Logger::THREADS) << "started thread ("
                                        << _scheduler->infoStatus() << ")";

      auto start = std::chrono::steady_clock::now();

      try {
        static size_t EVERY_LOOP = 1000;
        static double MIN_SECONDS = 30;

        size_t counter = 0;
        bool doDecrement = true;

        while (!_scheduler->isStopping()) {
          _service->run_one();

          if (++counter > EVERY_LOOP) {
            counter = 0;

            auto now = std::chrono::steady_clock::now();
            std::chrono::duration<double> diff = now - start;

            if (diff.count() > MIN_SECONDS) {
              start = std::chrono::steady_clock::now();

              if (_scheduler->shouldStopThread()) {
                auto n = _scheduler->decRunning();

                if (n <= _scheduler->minimum()) {
                  _scheduler->incRunning();
                } else {
                  doDecrement = false;
                  break;
                }
              }
            }
          }
        }

        if (doDecrement) {
          _scheduler->decRunning();
        }

        LOG_TOPIC(DEBUG, Logger::THREADS) << "stopped ("
                                          << _scheduler->infoStatus() << ")";
      } catch (std::exception const& ex) {
        LOG_TOPIC(ERR, Logger::THREADS)
            << "restarting scheduler loop after caught exception: " << ex.what();
        _scheduler->decRunning();
        _scheduler->startNewThread();
      } catch (...) {
        LOG_TOPIC(ERR, Logger::THREADS)
            << "restarting scheduler loop after unknown exception";
        _scheduler->decRunning();
        _scheduler->startNewThread();
      }
    } catch (...) {
      // better not throw from here, as this is a thread main loop
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

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

Scheduler::Scheduler(uint64_t nrMinimum, uint64_t /*nrDesired*/, 
                     uint64_t nrMaximum, uint64_t maxQueueSize)
    : _stopping(false),
      _maxQueueSize(maxQueueSize),
      _nrMinimum(nrMinimum),
      _nrMaximum(nrMaximum),
      _nrWorking(0),
      _nrQueued(0),
      _nrBlocked(0),
      _nrRunning(0) {
  // setup signal handlers
  initializeSignalHandlers();
}

Scheduler::~Scheduler() {
  stopRebalancer();

  try {
    deleteOldThreads();
  } catch (...) {
    // probably out of memory here...
    // must not throw in the dtor
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "unable to delete old scheduler threads";
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

void Scheduler::post(std::function<void()> callback) {
  ++_nrQueued;

  _ioService.get()->post([this, callback]() {
    JobGuard guard(this);
    guard.work();

    --_nrQueued;

    callback();
  });
}

bool Scheduler::start(ConditionVariable* cv) {
  // start the I/O
  startIoService();

  TRI_ASSERT(0 < _nrMinimum);
  TRI_ASSERT(_nrMinimum <= _nrMaximum);

  for (uint64_t i = 0; i < _nrMinimum; ++i) {
    startNewThread();
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
  MUTEX_LOCKER(guard, _threadsLock);

  auto thread = new SchedulerManagerThread(this, _managerService.get());

  try {
    _threads.emplace(thread);
  } catch (...) {
    delete thread;
    throw;
  }
    
  thread->start();
}

void Scheduler::startNewThread() {
  MUTEX_LOCKER(guard, _threadsLock);

  auto thread = new SchedulerThread(this, _ioService.get());
  
  try {
    _threads.emplace(thread);
  } catch (...) {
    delete thread;
    throw;
  }
    
  thread->start();
}

bool Scheduler::shouldStopThread() const {
  if (_nrRunning <= _nrWorking + _nrQueued + _nrMinimum) {
    return false;
  }

  if (_nrMinimum + _nrBlocked < _nrRunning) {
    return true;
  }

  return false;
}

bool Scheduler::shouldQueueMore() const {
  if (_nrWorking + _nrQueued + _nrMinimum < _nrMaximum) {
    return true;
  }

  return false;
}

bool Scheduler::hasQueueCapacity() const {
  if (_nrWorking + _nrQueued + _nrMinimum >= _nrMaximum) {
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

  return "working: " + std::to_string(_nrWorking) + ", queued: " +
         std::to_string(_nrQueued) + ", blocked: " +
         std::to_string(_nrBlocked) + ", running: " +
         std::to_string(_nrRunning) + ", outstanding: " +
         std::to_string(queueSize) + ", min/max: " +
         std::to_string(_nrMinimum) + "/" + std::to_string(_nrMaximum);
}

void Scheduler::threadDone(Thread* thread) {
  MUTEX_LOCKER(guard, _threadsLock);

  _threads.erase(thread);
  _deadThreads.insert(thread);
}

void Scheduler::deleteOldThreads() {
  // delete old thread objects
  std::unordered_set<Thread*> deadThreads;

  {
    MUTEX_LOCKER(guard, _threadsLock);

    if (_deadThreads.empty()) {
      return;
    }

    deadThreads.swap(_deadThreads);
  }

  for (auto thread : deadThreads) {
    try {
      delete thread;
    } catch (...) {
      LOG_TOPIC(ERR, Logger::THREADS) << "cannot delete thread";
    }
  }
}

void Scheduler::rebalanceThreads() {
  static uint64_t count = 0;

  ++count;

  if ((count % 5) == 0) {
    LOG_TOPIC(DEBUG, Logger::THREADS) << "rebalancing threads: " << infoStatus();
  } else {
    LOG_TOPIC(TRACE, Logger::THREADS) << "rebalancing threads: " << infoStatus();
  }

  while (_nrRunning < _nrWorking + _nrQueued + _nrMinimum) {
    if (_stopping) {
      // do not start any new threads in case we are already shutting down
      break;
    }
    startNewThread();
    usleep(5000);
  }
}

void Scheduler::beginShutdown() {
  if (_stopping) {
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
  _stopping = true;
}

void Scheduler::shutdown() {
  bool done = false;

  while (!done) {
    {
      MUTEX_LOCKER(guard, _threadsLock);
      done = _threads.empty();
    }
    std::this_thread::yield();
  }

  deleteOldThreads();

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
