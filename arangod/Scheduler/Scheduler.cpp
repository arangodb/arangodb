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
  SchedulerManagerThread(std::shared_ptr<Scheduler> const& scheduler, asio_ns::io_context* service)
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
    _scheduler.reset();
  }

 private:
  std::shared_ptr<Scheduler> _scheduler;
  asio_ns::io_context* _service;
};
}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                   SchedulerThread
// -----------------------------------------------------------------------------

class arangodb::SchedulerThread : public Thread {
 public:
  SchedulerThread(std::shared_ptr<Scheduler> const& scheduler, asio_ns::io_context* service)
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

    _scheduler.reset();
  }

 private:
  std::shared_ptr<Scheduler> _scheduler;
  asio_ns::io_context* _service;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                         Scheduler
// -----------------------------------------------------------------------------

Scheduler::Scheduler(uint64_t nrMinimum, uint64_t nrMaximum,
                     uint64_t fifo1Size, uint64_t fifo2Size)
    : _counters(0),
      _maxFifoSize{fifo1Size, fifo2Size, fifo2Size},
      _fifo1(_maxFifoSize[FIFO1]),
      _fifo2(_maxFifoSize[FIFO2]),
      _fifo3(_maxFifoSize[FIFO3]),
      _fifos{&_fifo1, &_fifo2, &_fifo3},
      _minThreads(nrMinimum),
      _maxThreads(nrMaximum),
      _lastAllBusyStamp(0.0) {
  LOG_TOPIC(DEBUG, Logger::THREADS) << "Scheduler configuration min: " << nrMinimum << " max: " << nrMaximum;
  _fifoSize[FIFO1] = 0;
  _fifoSize[FIFO2] = 0;
  _fifoSize[FIFO3] = 0;

  // setup signal handlers
  initializeSignalHandlers();
}

Scheduler::~Scheduler() {
  stopRebalancer();

  _managerGuard.reset();
  _managerContext.reset();

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
void Scheduler::post(std::function<void()> const callback) {
  // increment number of queued and guard against exceptions
  incQueued();

  auto guardQueue = scopeGuard([this]() { decQueued(); });

  // capture without self, ioContext will not live longer than scheduler
  _ioContext->post([this, callback]() {
    // start working
    JobGuard jobGuard(this);
    jobGuard.work();

    // reduce number of queued now
    decQueued();

    callback();
  });

  // no exception happened, cancel guard
  guardQueue.cancel();
}

// do not pass callback by reference, might get deleted before execution
void Scheduler::post(asio_ns::io_context::strand& strand,
                     std::function<void()> const callback) {
  incQueued();

  auto guardQueue = scopeGuard([this]() { decQueued(); });

  strand.post([this, callback]() {
    JobGuard guard(this);
    guard.work();

    decQueued();
    callback();
  });

  // no exception happened, cancel guard
  guardQueue.cancel();
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
        ok = pushToFifo(FIFO1, callback);
      } else {
        post(callback);
      }
      break;

    // If there is anything in the fifo1, fifo2, fifo8
    // or if the scheduler queue is already full, then
    // append it to the fifo2. Otherewise directly queue
    // it.
    case RequestPriority::MED:
      if (0 < _fifoSize[FIFO1] || 0 < _fifoSize[FIFO2] || !canPostDirectly(prio)) {
        ok = pushToFifo(FIFO2, callback);
      } else {
        post(callback);
      }
      break;

    // If there is anything in the fifo1, fifo2, fifo3
    // or if the scheduler queue is already full, then
    // append it to the fifo2. Otherwise directly queue
    // it.
    case RequestPriority::LOW:
      if (0 < _fifoSize[FIFO1] || 0 < _fifoSize[FIFO2] || 0 < _fifoSize[FIFO3] || !canPostDirectly(prio)) {
        ok = pushToFifo(FIFO3, callback);
      } else {
        post(callback);
      }
      break;

    default:
      TRI_ASSERT(false);
      break;
  }

  // THIS IS A UGLY HACK TO SUPPORT THE NEW IO CONTEXT INFRASTRUCTURE
  //  This is needed, since a post on the scheduler does no longer result in a
  //  drain immerdiately. The reason for that is, that no worker thread returns
  //  from `run_once`.
  this->drain();

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

    if (!found) {
      found = popFifo(FIFO3);
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
  b.add("current-fifo1", VPackValue(_fifoSize[FIFO1]));
  b.add("fifo1-size", VPackValue(_maxFifoSize[FIFO1]));
  b.add("current-fifo2", VPackValue(_fifoSize[FIFO2]));
  b.add("fifo2-size", VPackValue(_maxFifoSize[FIFO2]));
  b.add("current-fifo3", VPackValue(_fifoSize[FIFO3]));
  b.add("fifo3-size", VPackValue(_maxFifoSize[FIFO3]));
}

Scheduler::QueueStatistics Scheduler::queueStatistics() const {
  auto counters = getCounters();

  return QueueStatistics{numRunning(counters),
                         numWorking(counters),
                         numQueued(counters),
                         static_cast<uint64_t>(_fifoSize[FIFO1]),
                         static_cast<uint64_t>(_fifoSize[FIFO2]),
                         static_cast<uint64_t>(_fifoSize[FIFO3])};
}

std::string Scheduler::infoStatus() {
  uint64_t const counters = _counters.load();

  return "scheduler " + std::to_string(numRunning(counters)) + " (" +
         std::to_string(_minThreads) + "<" + std::to_string(_maxThreads) +
         ") in-progress " + std::to_string(numWorking(counters)) + " queued " +
         std::to_string(numQueued(counters)) +
         " F1 " + std::to_string(_fifoSize[FIFO1]) +
         " (<=" + std::to_string(_maxFifoSize[FIFO1]) + ") F2 " +
         std::to_string(_fifoSize[FIFO2]) +
         " (<=" + std::to_string(_maxFifoSize[FIFO2]) + ") F3 " +
         std::to_string(_fifoSize[FIFO3]) +
         " (<=" + std::to_string(_maxFifoSize[FIFO3]) + ")";
}

bool Scheduler::canPostDirectly(RequestPriority prio) const noexcept {
  auto counters = getCounters();
  auto nrWorking = numWorking(counters);
  auto nrQueued = numQueued(counters);

  switch (prio) {
    case RequestPriority::HIGH:
      return nrWorking + nrQueued < _maxThreads;

    // the "/ 2" is an assumption that HIGH is typically responses to our outbound messages
    //  where MED & LOW are incoming requests.  Keep half the threads processing our work and half their work.
    case RequestPriority::MED:
    case RequestPriority::LOW:
      return nrWorking + nrQueued < _maxThreads / 2;
  }

  return false;
}

bool Scheduler::pushToFifo(int64_t fifo, std::function<void()> const& callback) {
  LOG_TOPIC(TRACE, Logger::THREADS) << "Push element on fifo: " << fifo;
  TRI_ASSERT(0 <= fifo && fifo < NUMBER_FIFOS);

  size_t p = static_cast<size_t>(fifo);
  auto job = std::make_unique<FifoJob>(callback);

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
      post([] {
          LOG_TOPIC(DEBUG, Logger::THREADS) << "Wakeup alarm";
          /*wakeup call for scheduler thread*/
      });
    }
  } catch (...) {
    return false;
  }

  return true;
}

bool Scheduler::popFifo(int64_t fifo) {
  LOG_TOPIC(TRACE, Logger::THREADS) << "Popping a job from fifo: " << fifo;
  TRI_ASSERT(0 <= fifo && fifo < NUMBER_FIFOS);

  size_t p = static_cast<size_t>(fifo);
  FifoJob* job = nullptr;

  bool ok = _fifos[p]->pop(job) && job != nullptr;

  if (ok) {
    auto guard = scopeGuard([job]() {
      if (job) {
        delete job;
      }
    });

   post(job->_callback);

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

  for (uint64_t i = 0; i < _minThreads; ++i) {
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

  // One has to clean up the ioContext here, because there could a lambda
  // in its queue, that requires for it finalization some object (for example vocbase)
  // that would already be destroyed
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
  auto thread = new SchedulerManagerThread(shared_from_this(), _managerContext.get());
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


//
// This routine tries to keep only the most likely needed count of threads running:
//  - asio io_context runs less efficiently if it has too many threads, but
//  - there is a latency hit to starting a new thread.
//
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

  // decrement nrRunning by one already in here while holding the lock
  decRunning();
  return true;
}

void Scheduler::startNewThread() {
  TRI_IF_FAILURE("Scheduler::startNewThread") {
    LOG_TOPIC(WARN, Logger::FIXME) << "Debug: preventing thread from starting";
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  auto thread = new SchedulerThread(shared_from_this(), _ioContext.get());
  if (!thread->start()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FAILED,
                                   "unable to start scheduler thread");
  }
}
