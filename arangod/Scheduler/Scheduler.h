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

#ifndef ARANGOD_SCHEDULER_SCHEDULER_H
#define ARANGOD_SCHEDULER_SCHEDULER_H 1

#include "Basics/Common.h"

#include "Basics/Mutex.h"
#include "Basics/socket-utils.h"
#include "Scheduler/EventLoop.h"
#include "Scheduler/Job.h"
#include "Scheduler/JobQueue.h"
#include "Scheduler/Socket.h"

namespace arangodb {
class JobGuard;

namespace velocypack {
class Builder;
}

class ListenTask;

namespace rest {

class GeneralCommTask;
class SocketTask;

class Scheduler {
  Scheduler(Scheduler const&) = delete;
  Scheduler& operator=(Scheduler const&) = delete;

  friend class arangodb::JobGuard;
  friend class arangodb::rest::GeneralCommTask;
  friend class arangodb::rest::SocketTask;
  friend class arangodb::ListenTask;

 public:
  Scheduler(uint64_t nrMinimum, uint64_t nrDesired, uint64_t nrMaximum,
            uint64_t maxQueueSize);
  virtual ~Scheduler();

 public:
  asio_ns::io_context* ioContext() const { return _ioContext.get(); }
  asio_ns::io_context* managerService() const { return _managerService.get(); }

  EventLoop eventLoop() {
    // cannot use
    //   return EventLoop{._ioService = *_ioService.get(), ._scheduler = this};
    // because windows complains ...
    return EventLoop{_ioContext.get(), this};
  }

  void post(std::function<void()> callback);

  bool start();
  bool isRunning() const { return numRunning(_counters) > 0; }

  void beginShutdown();
  void stopRebalancer() noexcept;
  bool isStopping() { return (_counters & (1ULL << 63)) != 0; }
  void shutdown();

 public:
  // decrements the nrRunning counter for the thread
  void stopThread();

  // check if the current thread should be stopped
  // returns true if yes, otherwise false. when the function returns
  // true, it has already decremented the nrRunning counter!
  bool stopThreadIfTooMany(double now);

  bool shouldQueueMore() const;
  bool shouldExecuteDirect() const;

  // queue processing of an async rest job
  bool queue(std::unique_ptr<Job> job);

  std::string infoStatus();

  uint64_t minimum() const { return _nrMinimum; }

  // number of queued handlers
  inline uint64_t getCounters() const noexcept { return _counters; }

  // number of running threads
  static uint64_t numRunning(uint64_t value) noexcept {
    return value & 0xFFFFULL;
  }

  // number of working threads
  static uint64_t numWorking(uint64_t value) noexcept {
    return (value >> 16) & 0xFFFFULL;
  }

  // number of blocked threads
  static uint64_t numBlocked(uint64_t value) noexcept {
    return (value >> 32) & 0xFFFFULL;
  }

  inline void wakeupJobQueue() noexcept {
    _jobQueue->wakeup();
  }

 private:
  void startNewThread();

  static void initializeSignalHandlers();

 private:
  // we store most of the threads status info in a single atomic uint64_t
  // the encoding of the values inside this variable is (left to right means
  // high to low bytes):
  //
  //   AA BB CC DD
  //
  // we use the lowest 2 bytes (DD) to store the number of running threads
  // the next lowest bytes (CC) are used to store the number of currently
  // working threads
  // the next bytes (BB) are used to store the number of currently blocked
  // threads
  // the highest bytes (AA) are used only to encode a stopping bit. when this
  // bit is
  // set, the scheduler is stopping (or already stopped)
  inline void setStopping() noexcept { _counters |= (1ULL << 63); }

  inline void incRunning() noexcept { _counters += 1ULL << 0; }
  inline void decRunning() noexcept {
    TRI_ASSERT((_counters & 0xFFFFUL) > 0);
    _counters -= 1ULL << 0;
  }

  inline void workThread() noexcept { _counters += 1ULL << 16; }
  inline void unworkThread() noexcept {
    TRI_ASSERT(((_counters & 0XFFFF0000UL) >> 16) > 0);
    _counters -= 1ULL << 16;
  }

  inline void blockThread() noexcept { _counters += 1ULL << 32; }
  inline void unblockThread() noexcept {
    TRI_ASSERT(((_counters & 0XFFFF00000000UL) >> 32) > 0);
    _counters -= 1ULL << 32;
  }

  inline bool isStopping(uint64_t value) const noexcept {
    return (value & (1ULL << 63)) != 0;
  }

  void startIoService();
  void startRebalancer();
  void startManagerThread();
  void rebalanceThreads();

 private:
  // maximal number of outstanding user requests
  uint64_t const _maxQueueSize;

  // minimum number of running SchedulerThreads
  uint64_t const _nrMinimum;

  // maximal number of outstanding user requests
  uint64_t const _nrMaximum;

  // current counters. refer to the above description of the
  // meaning of its individual bits
  std::atomic<uint64_t> _counters;

  std::unique_ptr<JobQueue> _jobQueue;

  std::shared_ptr<asio_ns::io_context::work> _serviceGuard;
  std::unique_ptr<asio_ns::io_context> _ioContext;

  std::shared_ptr<asio_ns::io_context::work> _managerGuard;
  std::unique_ptr<asio_ns::io_context> _managerService;

  std::unique_ptr<asio_ns::steady_timer> _threadManager;
  std::function<void(const asio_ns::error_code&)> _threadHandler;

  mutable Mutex _threadCreateLock;
  double _lastAllBusyStamp;
};
}
}

#endif
