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
#include "Logger/Logger.h"
#include "Rest/GeneralResponse.h"
#include "Scheduler/JobQueue.h"
#include "Scheduler/Task.h"

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
    _scheduler->incRunning();
    LOG_TOPIC(DEBUG, Logger::THREADS) << "running (" << _scheduler->infoStatus()
                                      << ")";

    auto start = std::chrono::steady_clock::now();

    try {
      static size_t EVERY_LOOP = 1000;
      static double MIN_SECONDS = 30;

      size_t counter = 0;
      while (!_scheduler->isStopping()) {
        _service->run_one();

        if (++counter > EVERY_LOOP) {
          auto now = std::chrono::steady_clock::now();
          std::chrono::duration<double> diff = now - start;

          if (diff.count() > MIN_SECONDS) {
            if (_scheduler->stopThread()) {
              auto n = _scheduler->decRunning();

              if (n <= 2) {
                _scheduler->incRunning();
              } else {
                break;
              }
            }

            start = std::chrono::steady_clock::now();
          }
        }
      }

      LOG_TOPIC(DEBUG, Logger::THREADS) << "stopped ("
                                        << _scheduler->infoStatus() << ")";
    } catch (...) {
      LOG_TOPIC(ERR, Logger::THREADS)
          << "scheduler loop caught an error, restarting";
      _scheduler->decRunning();
      _scheduler->startNewThread();
    }

    _scheduler->threadDone(this);
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

Scheduler::Scheduler(size_t nrThreads, size_t maxQueueSize)
    : _nrThreads(nrThreads),
      _maxQueueSize(maxQueueSize),
      _stopping(false),
      _nrBusy(0),
      _nrWorking(0),
      _nrBlocked(0),
      _nrRunning(0),
      _nrMinimal(0),
      _nrMaximal(0),
      _nrRealMaximum(0),
      _lastThreadWarning(0) {
  // setup signal handlers
  initializeSignalHandlers();
}

Scheduler::~Scheduler() { deleteOldThreads(); }

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

bool Scheduler::start(ConditionVariable* cv) {
  // start the I/O
  startIoService();

  // initialize thread handling
  if (_nrMaximal <= 0) {
    _nrMaximal = _nrThreads;
  }

  if (_nrRealMaximum <= 0) {
    _nrRealMaximum = 4 * _nrMaximal;
  }

  for (size_t i = 0; i < 2; ++i) {
    startNewThread();
  }

  startManagerThread();
  startRebalancer();

  // initialize the queue handling
  _jobQueue.reset(new JobQueue(_maxQueueSize, _ioService.get()));
  _jobQueue->start();

  // done
  LOG(TRACE) << "all scheduler threads are up and running";
  return true;
}

void Scheduler::startIoService() {
  _ioService.reset(new boost::asio::io_service());
  _serviceGuard.reset(new boost::asio::io_service::work(*_ioService));

  _managerService.reset(new boost::asio::io_service());
  _managerGuard.reset(new boost::asio::io_service::work(*_managerService));
}

void Scheduler::startRebalancer() {
  std::chrono::milliseconds interval(500);
  _threadManager.reset(new boost::asio::steady_timer(*_managerService));

  _threadHandler = [this, interval](const boost::system::error_code& error) {
    if (error || isStopping()) {
      return;
    }

    rebalanceThreads();

    _threadManager->expires_from_now(interval);
    _threadManager->async_wait(_threadHandler);
  };

  _threadManager->expires_from_now(interval);
  _threadManager->async_wait(_threadHandler);

  _lastThreadWarning.store(TRI_microtime());
}

void Scheduler::startManagerThread() {
  MUTEX_LOCKER(guard, _threadsLock);

  auto thread = new SchedulerManagerThread(this, _managerService.get());

  _threads.emplace(thread);
  thread->start();
}

void Scheduler::startNewThread() {
  MUTEX_LOCKER(guard, _threadsLock);

  auto thread = new SchedulerThread(this, _ioService.get());

  _threads.emplace(thread);
  thread->start();
}

bool Scheduler::stopThread() {
  if (_nrRunning <= _nrMinimal) {
    return false;
  }
  
  if (_nrRunning >= 3) {
    int64_t low = ((_nrRunning <= 4) ? 0 : (_nrRunning * 1 / 4)) - _nrBlocked;

    if (_nrBusy <= low && _nrWorking <= low) {
      return true;
    }
  }

  return false;
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
  static double const MIN_WARN_INTERVAL = 10;
  static double const MIN_ERR_INTERVAL = 300;

  int64_t high = (_nrRunning <= 4) ? 1 : (_nrRunning * 11 / 16);
  int64_t working = (_nrBusy > _nrWorking) ? _nrBusy : _nrWorking;

  LOG_TOPIC(DEBUG, Logger::THREADS) << "rebalancing threads, high: " << high
                                    << ", working: " << working << " ("
                                    << infoStatus() << ")";

  if (working >= high) {
    if (_nrRunning < _nrMaximal + _nrBlocked &&
        _nrRunning < _nrRealMaximal) {   // added by Max 22.12.2016
      // otherwise we exceed the total maximum
      startNewThread();
      return;
    }
  }

  if (working >= _nrMaximal + _nrBlocked || _nrRunning < _nrMinimal) {
    double ltw = _lastThreadWarning.load();
    double now = TRI_microtime();

    if (_nrRunning >= _nrRealMaximum) {
      if (ltw - now > MIN_ERR_INTERVAL) {
        LOG_TOPIC(ERR, Logger::THREADS) << "too many threads (" << infoStatus()
                                        << ")";
        _lastThreadWarning.store(now);
      }
    } else {
      if (_nrRunning >= _nrRealMaximum * 3 / 4) {
        if (ltw - now > MIN_WARN_INTERVAL) {
          LOG_TOPIC(WARN, Logger::THREADS)
              << "number of threads is reaching a critical limit ("
              << infoStatus() << ")";
          _lastThreadWarning.store(now);
        }
      }

      LOG_TOPIC(DEBUG, Logger::THREADS) << "overloading threads ("
                                        << infoStatus() << ")";

      startNewThread();
    }
  }
}

void Scheduler::beginShutdown() {
  if (_stopping) {
    return;
  }

  _jobQueue->beginShutdown();

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
    MUTEX_LOCKER(guard, _threadsLock);
    done = _threads.empty();
  }

  deleteOldThreads();

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
    LOG(ERR) << "cannot initialize signal handlers for pipe";
  }
#endif
}
