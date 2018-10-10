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

namespace {
constexpr double MIN_SECONDS = 30.0;
}

// -----------------------------------------------------------------------------
// --SECTION--                                            SchedulerManagerThread
// -----------------------------------------------------------------------------

namespace {
class SchedulerManagerThread final : public Thread {
 public:
  SchedulerManagerThread(Scheduler* scheduler, asio_ns::io_context* service)
      : Thread("SchedulerManager", true),
        _scheduler(scheduler),
        _service(service) {}

  ~SchedulerManagerThread() { shutdown(); }

 public:
  void run() override {
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
  asio_ns::io_context* _service;
};
}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                   SchedulerThread
// -----------------------------------------------------------------------------

namespace {
class SchedulerThread : public Thread {
 public:
  SchedulerThread(Scheduler* scheduler, asio_ns::io_context* service)
      : Thread("Scheduler", true), _scheduler(scheduler), _service(service) {}

  ~SchedulerThread() { shutdown(); }

 public:
  void run() override {
    constexpr size_t everyXSeconds = size_t(MIN_SECONDS);

    // when we enter this method,
    // _nrRunning has already been increased for this thread
    LOG_TOPIC(DEBUG, Logger::THREADS)
        << "started thread: " << _scheduler->infoStatus();

    // some random delay value to avoid all initial threads checking for
    // their deletion at the very same time
    double const randomWait = static_cast<double>(RandomGenerator::interval(
        int64_t(0), static_cast<int64_t>(MIN_SECONDS * 0.5)));

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

      try {
        _scheduler->drain();
      } catch (...) {
      }

      if (++counter > everyXSeconds) {
        counter = 0;

        double const now = TRI_microtime();

        if (now - start > MIN_SECONDS) {
          // test if we should stop this thread, if this returns true,
          // numRunning
          // will have been decremented by one already
          if (_scheduler->threadShouldStop(now)) {
            // nrRunning was decremented already. now exit thread
            doDecrement = false;
            break;
          }

          // use new start time
          start = now;
        }
      }
    }

    LOG_TOPIC(DEBUG, Logger::THREADS)
        << "stopped (" << _scheduler->infoStatus() << ")";

    if (doDecrement) {
      // only decrement here if this wasn't already done above
      _scheduler->threadHasStopped();
    }
  }

 private:
  Scheduler* _scheduler;
  asio_ns::io_context* _service;
};
}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                         Scheduler
// -----------------------------------------------------------------------------

Scheduler::Scheduler(uint64_t nrMinimum, uint64_t nrMaximum,
                     uint64_t maxQueueSize, uint64_t fifo1Size,
                     uint64_t fifo2Size)
    : _queuedV8(0),
      _maxQueuedV8(std::max(static_cast<uint64_t>(1), nrMaximum - nrMinimum)),
      _maxQueueSize(maxQueueSize),
      _counters(0),
      _maxFifoSize{fifo1Size, fifo2Size, fifo2Size},
      _fifo1(_maxFifoSize[FIFO1]),
      _fifo2(_maxFifoSize[FIFO2]),
      _fifo8(_maxFifoSize[FIFO8]),
      _fifos{&_fifo1, &_fifo2, &_fifo8},
      _minThreads(nrMinimum),
      _maxThreads(nrMaximum),
      _lastAllBusyStamp(0.0) {
  _fifoSize[FIFO1] = 0;
  _fifoSize[FIFO2] = 0;
  _fifoSize[FIFO8] = 0;

  // setup signal handlers
  initializeSignalHandlers();
}

Scheduler::~Scheduler() {
  stopRebalancer();

  _managerGuard.reset();
  _managerContext.reset();

  _serviceGuard.reset();
  _ioContext.reset();

  FifoJob* job = nullptr;

  for (int i = 0; i < NUMBER_FIFOS; ++i) {
    _fifoSize[i] = 0;

    while (_fifos[i]->pop(job) && job != nullptr) {
      delete job;
      job = nullptr;
    }
  }
}

// do not pass callback by reference, might get deleted before execution
void Scheduler::post(std::function<void()> const callback, bool isV8,
                     uint64_t timeout) {
  // increment number of queued and guard against exceptions
  incQueued();

  auto guardQueue = scopeGuard([this]() { decQueued(); });

  // increment number of queued V8 jobs and guard against exceptions
  if (isV8) {
    ++_queuedV8;
  }

  auto guardV8 = scopeGuard([this, isV8]() {
    if (isV8) {
      --_queuedV8;
    }
  });

  // capture without self, ioContext will not live longer than scheduler
  _ioContext->post([this, callback, isV8, timeout]() {
    // at the end (either success or exception),
    // reduce number of queued V8
    auto guard = scopeGuard([this, isV8]() {
      if (isV8) {
        --_queuedV8;
      }
    });

    // reduce number of queued now
    decQueued();

    // start working
    JobGuard jobGuard(this);
    jobGuard.work();

    if (isV8 && _queuedV8 > _maxQueuedV8 &&
        numWorking(getCounters()) >= static_cast<uint64_t>(_maxQueuedV8)) {
      // this must be done before requeuing the job
      guard.fire();

      // in case we queued more V8 jobs in the scheduler than desired this
      // job is put back into the scheduler queue. An exponential backoff is
      // used with a maximum of 256ms. Initial the timeout will be zero.
      auto t = timeout;

      if (t == 0) {
        t = 1;
      } else if (t <= 200) {
        t *= 2;
      }

      std::shared_ptr<asio_ns::deadline_timer> timer(
          newDeadlineTimer(boost::posix_time::millisec(timeout)));
      timer->async_wait(
          [this, callback, isV8, t, timer](const asio::error_code& error) {
            if (error != asio::error::operation_aborted) {
              post(callback, isV8, t);
            }
          });

      return;
    }

    callback();
  });

  // no exception happened, cancel guards
  guardV8.cancel();
  guardQueue.cancel();
}

// do not pass callback by reference, might get deleted before execution
void Scheduler::post(asio_ns::io_context::strand& strand,
                     std::function<void()> const callback) {
  incQueued();

  try {
    // capture without self, ioContext will not live longer than scheduler
    strand.post([this, callback]() {
      decQueued();

      JobGuard guard(this);
      guard.work();

      callback();
    });
  } catch (...) {
    decQueued();
    throw;
  }
}

bool Scheduler::queue(RequestPriority prio,
                      std::function<void()> const& callback) {
  bool ok = true;

  switch (prio) {
    // If there is anything in the fifo1 or if the scheduler
    // queue is already full, then append it to the fifo1.
    // Otherwise directly queue it.
    //
    // This does not care if there is anything in fifo2 or
    // fifo8 because these queue have lower priority.
    case RequestPriority::HIGH:
      if (0 < _fifoSize[FIFO1] || !canPostDirectly(prio)) {
        ok = pushToFifo(FIFO1, callback, false);
      } else {
        post(callback, false);
      }
      break;

    // If there is anything in the fifo1, fifo2, fifo8
    // or if the scheduler queue is already full, then
    // append it to the fifo2. Otherewise directly queue
    // it.
    case RequestPriority::LOW:
      if (0 < _fifoSize[FIFO1] || 0 < _fifoSize[FIFO8] ||
          0 < _fifoSize[FIFO2] || !canPostDirectly(prio)) {
        ok = pushToFifo(FIFO2, callback, false);
      } else {
        post(callback, false);
      }
      break;

    // Also push V8 requests to the fifo2. Even if we could
    // queue directly.
    case RequestPriority::V8:
      ok = pushToFifo(FIFO2, callback, true);
      break;

    default:
      TRI_ASSERT(false);
      break;
  }

  return ok;
}

void Scheduler::drain() {
  bool found = true;

  while (found && canPostDirectly(RequestPriority::HIGH)) {
    found = popFifo(FIFO1);
  }

  found = true;

  while (found && canPostDirectly(RequestPriority::LOW)) {
    found = popFifo(FIFO1);

    if (!found) {
      found = popFifo(FIFO2);
    }
  }
}

void Scheduler::addQueueStatistics(velocypack::Builder& b) const {
  auto counters = getCounters();

  // if you change the names of these attributes, please make sure to
  // also change them in StatisticsWorker.cpp:computePerSeconds
  b.add("scheduler-threads", VPackValue(numRunning(counters)));
  b.add("in-progress", VPackValue(numWorking(counters)));
  b.add("queued", VPackValue(numQueued(counters)));
  b.add("queue-size", VPackValue(_maxQueueSize));
  b.add("current-fifo1", VPackValue(_fifoSize[FIFO1]));
  b.add("fifo1-size", VPackValue(_maxFifoSize[FIFO1]));
  b.add("current-fifo2", VPackValue(_fifoSize[FIFO2]));
  b.add("fifo2-size", VPackValue(_maxFifoSize[FIFO2]));
  b.add("current-fifo8", VPackValue(_fifoSize[FIFO8]));
  b.add("fifo8-size", VPackValue(_maxFifoSize[FIFO8]));
}

Scheduler::QueueStatistics Scheduler::queueStatistics() const {
  auto counters = getCounters();

  return QueueStatistics{numRunning(counters),
                         numWorking(counters),
                         numQueued(counters),
                         static_cast<uint64_t>(_fifoSize[FIFO1]),
                         static_cast<uint64_t>(_fifoSize[FIFO8]),
                         static_cast<uint64_t>(_fifoSize[FIFO8]),
                         static_cast<uint64_t>(_queuedV8)};
}

std::string Scheduler::infoStatus() {
  uint64_t const counters = _counters.load();

  return "scheduler " + std::to_string(numRunning(counters)) + " (" +
         std::to_string(_minThreads) + "<" + std::to_string(_maxThreads) +
         ") in-progress " + std::to_string(numWorking(counters)) + " queued " +
         std::to_string(numQueued(counters)) +
         " (<=" + std::to_string(_maxQueueSize) + ") V8 " +
         std::to_string(_queuedV8) + " (<=" + std::to_string(_maxQueuedV8) +
         ") F1 " + std::to_string(_fifoSize[FIFO1]) +
         " (<=" + std::to_string(_maxFifoSize[FIFO1]) + ") F2 " +
         std::to_string(_fifoSize[FIFO2]) +
         " (<=" + std::to_string(_maxFifoSize[FIFO2]) + ") F8 " +
         std::to_string(_fifoSize[FIFO8]) +
         " (<=" + std::to_string(_maxFifoSize[FIFO8]) + ")";
}

bool Scheduler::canPostDirectly(RequestPriority prio) const noexcept {
  auto counters = getCounters();
  auto nrWorking = numWorking(counters);
  auto nrQueued = numQueued(counters);

  switch (prio) {
    case RequestPriority::HIGH:
//      return nrWorking + nrQueued <= _maxQueuedHighPrio;
      return nrWorking + nrQueued <= _maxThreads;

    case RequestPriority::LOW:
//      return nrWorking + nrQueued <= _maxQueuedLowPrio;
      return nrWorking + nrQueued <= _maxThreads - _minThreads;
  }

  return false;
}

bool Scheduler::pushToFifo(int64_t fifo, std::function<void()> const& callback,
                           bool isV8) {
  TRI_ASSERT(0 <= fifo && fifo < NUMBER_FIFOS);
  TRI_ASSERT(fifo != FIFO8 || isV8);

  size_t p = static_cast<size_t>(fifo);
  auto job = std::make_unique<FifoJob>(callback, isV8);

  try {
    if (0 < _maxFifoSize[p] && (int64_t)_maxFifoSize[p] <= _fifoSize[p]) {
      return false;
    }

    if (!_fifos[p]->push(job.get())) {
      return false;
    }

    job.release();
    ++_fifoSize[p];

    // then check, otherwise we might miss to wake up a thread
    auto counters = getCounters();
    auto nrWorking = numRunning(counters);
    auto nrQueued = numQueued(counters);

    if (0 == nrWorking + nrQueued) {
      post([] { /*wakeup call for scheduler thread*/ }, false);
    }
  } catch (...) {
    return false;
  }

  return true;
}

bool Scheduler::popFifo(int64_t fifo) {
  TRI_ASSERT(0 <= fifo && fifo < NUMBER_FIFOS);

  if (fifo == FIFO8 && _queuedV8 >= _maxQueuedV8) {
    return false;
  }

  size_t p = static_cast<size_t>(fifo);
  FifoJob* job = nullptr;

  bool ok = _fifos[p]->pop(job) && job != nullptr;

  if (ok) {
    auto guard = scopeGuard([job]() {
      if (job) {
        delete job;
      }
    });

    if (!job->_isV8 || _queuedV8 < _maxQueuedV8) {
      post(job->_callback, job->_isV8);
    } else {
      pushToFifo(FIFO8, job->_callback, job->_isV8);
    }

    --_fifoSize[p];
  }

  return ok;
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

  int res = sigaction(SIGPIPE, &action, nullptr);

  if (res < 0) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "cannot initialize signal handlers for pipe";
  }
#endif
}

bool Scheduler::start() {
  // start the I/O
  startIoService();

  TRI_ASSERT(0 < _minThreads);
  TRI_ASSERT(_minThreads <= _maxThreads);
  TRI_ASSERT(0 < _maxQueueSize);
  TRI_ASSERT(0 < _maxQueuedV8);

  for (uint64_t i = 0; i < _maxThreads; ++i) {
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

  LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
      << "all scheduler threads are up and running";

  return true;
}

void Scheduler::beginShutdown() {
  if (isStopping()) {
    return;
  }

  stopRebalancer();
  _threadManager.reset();

  _managerGuard.reset();
  _managerContext->stop();

  _serviceGuard.reset();
  _ioContext->stop();

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
    // we can be quite generous here with waiting...
    // as we are in the shutdown already, we do not care if we need to wait
    // for a bit longer
    std::this_thread::sleep_for(std::chrono::microseconds(20000));
  }

  _managerContext.reset();
  _ioContext.reset();
}

void Scheduler::startIoService() {
  _ioContext.reset(new asio_ns::io_context());
  _serviceGuard.reset(new asio_ns::io_context::work(*_ioContext));

  _managerContext.reset(new asio_ns::io_context());
  _managerGuard.reset(new asio_ns::io_context::work(*_managerContext));
}

void Scheduler::startManagerThread() {
  auto thread = new SchedulerManagerThread(this, _managerContext.get());
  if (!thread->start()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FAILED,
                                   "unable to start rebalancer thread");
  }
}

void Scheduler::startRebalancer() {
  std::chrono::milliseconds interval(100);
  _threadManager.reset(new asio_ns::steady_timer(*_managerContext));

  _threadHandler = [this, interval](const asio_ns::error_code& error) {
    if (error || isStopping()) {
      return;
    }

    try {
      rebalanceThreads();
    } catch (...) {
      // continue if this fails.
      // we can try rebalancing again in the next round
    }

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

void Scheduler::rebalanceThreads() {
  static uint64_t count = 0;

  ++count;

  if (count % 50 == 0) {
    LOG_TOPIC(DEBUG, Logger::THREADS)
        << "rebalancing threads: " << infoStatus();
  } else if (count % 5 == 0) {
    LOG_TOPIC(TRACE, Logger::THREADS)
        << "rebalancing threads: " << infoStatus();
  }

  while (true) {
    double const now = TRI_microtime();

    {
      MUTEX_LOCKER(locker, _threadCreateLock);

      uint64_t const counters = _counters.load();

      if (isStopping(counters)) {
        // do not start any new threads in case we are already shutting down
        break;
      }

      uint64_t const nrRunning = numRunning(counters);
      uint64_t const nrWorking = numWorking(counters);

      // some threads are idle
      if (nrWorking < nrRunning) {
        break;
      }

      // all threads are maxed out
      _lastAllBusyStamp = now;

      // reached maximum
      if (nrRunning >= _maxThreads) {
        break;
      }

      // increase nrRunning by one here already, while holding the lock
      incRunning();
    }

    // create thread and sleep without holding the mutex
    try {
      startNewThread();
    } catch (...) {
      // cannot create new thread or start new thread
      // if this happens, we have to rollback the increase of nrRunning again
      {
        MUTEX_LOCKER(locker, _threadCreateLock);
        decRunning();
      }
      // add an extra sleep so the system has a chance to recover and provide
      // the needed resources
      std::this_thread::sleep_for(std::chrono::microseconds(20000));
    }

    std::this_thread::sleep_for(std::chrono::microseconds(5000));
  }
}

void Scheduler::threadHasStopped() {
  MUTEX_LOCKER(locker, _threadCreateLock);
  decRunning();
}

bool Scheduler::threadShouldStop(double now) {
  // make sure no extra threads are created while we check the timestamp
  // and while we modify nrRunning

  MUTEX_LOCKER(locker, _threadCreateLock);

  // fetch all counters in one atomic operation
  uint64_t counters = _counters.load();
  uint64_t const nrRunning = numRunning(counters);

  if (nrRunning <= _minThreads) {
    // don't stop a thread if we already reached the minimum
    // number of threads
    return false;
  }

  if (_lastAllBusyStamp + 1.25 * MIN_SECONDS >= now) {
    // last time all threads were busy is less than x seconds ago
    return false;
  }

#if 0  /// testing
  // decrement nrRunning by one already in here while holding the lock
  decRunning();
  return true;
#else
  return false;
#endif
}

void Scheduler::startNewThread() {
  TRI_IF_FAILURE("Scheduler::startNewThread") {
    LOG_TOPIC(WARN, Logger::FIXME) << "Debug: preventing thread from starting";
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  auto thread = new SchedulerThread(this, _ioContext.get());
  if (!thread->start()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FAILED,
                                   "unable to start scheduler thread");
  }
}
