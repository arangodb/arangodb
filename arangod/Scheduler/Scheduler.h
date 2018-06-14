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

#include <boost/lockfree/queue.hpp>

#include "Basics/Mutex.h"
#include "Basics/asio_ns.h"
#include "Basics/socket-utils.h"
#include "Endpoint/Endpoint.h"

namespace arangodb {
class JobGuard;
class ListenTask;

namespace velocypack {
class Builder;
}

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

  // queue handling:
  //
  // The Scheduler queue is an `io_context` that is server by a number
  // of SchedulerThreads. Jobs can be posted to the `io_context` using
  // the function `post`. A job in the Scheduler queue can be queues,
  // i.e. the executing has not yet been started, or running, i.e.
  // the executing has already been started.
  //
  // `numRunning` returns the number of running threads that are
  // currently serving the `io_context`.
  //
  // `numQueued` returns the number of jobs queued in the io_context
  // that are not yet running.
  //
  // The scheduler also has a number of FIFO where it stores jobs in
  // case that the there are too many jobs in Scheduler queue. The
  // function `queue` will handle the queuing. Depending on the fifos
  // and the number of queues jobs it will either queue the job
  // directly in the Scheduler queue or move it to the corresponding
  // FIFO.

 public:
  struct QueueStatistics {
    uint64_t _running;
    uint64_t _working;
    uint64_t _blocked;
    uint64_t _queued;
  };

  static size_t const INTERNAL_QUEUE = 1;
  static size_t const INTERNAL_AQL_QUEUE = 1;
  static size_t const INTERNAL_V8_QUEUE = 2;
  static size_t const CLIENT_QUEUE = 2;
  static size_t const CLIENT_SLOW_QUEUE = 2;
  static size_t const CLIENT_AQL_QUEUE = 2;
  static size_t const CLIENT_V8_QUEUE = 2;

  void post(std::function<void()> callback);
  void post(asio_ns::io_context::strand&, std::function<void()> callback);

  bool queue(size_t fifo, std::function<void()>);
  void drain();

  void addQueueStatistics(velocypack::Builder&) const;
  QueueStatistics queueStatistics() const;

 private:
  bool canPostDirectly() const noexcept;

  static uint64_t numRunning(uint64_t value) noexcept {
    return value & 0xFFFFULL;
  }

  inline uint64_t numQueued() const noexcept { return _nrQueued; };

  struct FifoJob {
    FifoJob(std::function<void()> callback) : _callback(callback) {}
    std::function<void()> _callback;
  };

  bool pushToFifo(size_t fifo, std::function<void()> callback);
  bool popFifo(size_t fifo);

  // maximal number of outstanding user requests
  uint64_t _maxQueueSize;

  static int64_t const NUMBER_FIFOS = 2;
  uint64_t _maxFifoSize[NUMBER_FIFOS];
  int64_t _fifoSize[NUMBER_FIFOS];
  boost::lockfree::queue<FifoJob*> _fifo1;
  boost::lockfree::queue<FifoJob*> _fifo2;
  boost::lockfree::queue<FifoJob*>* _fifos[NUMBER_FIFOS];

 public:
  // XXX-TODO remove, replace with signal handler
  asio_ns::io_context* managerService() const { return _managerService.get(); }

  bool start();
  bool isRunning() const { return numRunning(_counters) > 0; }

  void beginShutdown();
  void stopRebalancer() noexcept;
  bool isStopping() { return (_counters & (1ULL << 63)) != 0; }
  void shutdown();

  template <typename T>
  asio_ns::deadline_timer* newDeadlineTimer(T timeout) {
    return new asio_ns::deadline_timer(*_ioContext, timeout);
  }

  asio_ns::steady_timer* newSteadyTimer() {
    return new asio_ns::steady_timer(*_ioContext);
  }

  asio_ns::io_context::strand* newStrand() {
    return new asio_ns::io_context::strand(*_ioContext);
  }

  asio_ns::ip::tcp::acceptor* newAcceptor() {
    return new asio_ns::ip::tcp::acceptor(*_ioContext);
  }

  asio_ns::local::stream_protocol::acceptor* newDomainAcceptor() {
    return new asio_ns::local::stream_protocol::acceptor(*_ioContext);
  }

  asio_ns::ip::tcp::socket* newSocket() {
    return new asio_ns::ip::tcp::socket(*_ioContext);
  }

  asio_ns::local::stream_protocol::socket* newDomainSocket() {
    return new asio_ns::local::stream_protocol::socket(*_ioContext);
  }

  asio_ns::ssl::stream<asio_ns::ip::tcp::socket>* newSslSocket(
      asio_ns::ssl::context& context) {
    return new asio_ns::ssl::stream<asio_ns::ip::tcp::socket>(*_ioContext,
                                                              context);
  }

  asio_ns::ip::tcp::resolver* newResolver() {
    return new asio_ns::ip::tcp::resolver(*_ioContext);
  }

 public:
  // decrements the nrRunning counter for the thread
  void stopThread();

  // check if the current thread should be stopped
  // returns true if yes, otherwise false. when the function returns
  // true, it has already decremented the nrRunning counter!
  bool stopThreadIfTooMany(double now);

  bool shouldQueueMore() const;
  bool shouldExecuteDirect() const;

  std::string infoStatus();

  uint64_t minimum() const { return _nrMinimum; }

  inline uint64_t getCounters() const noexcept { return _counters; }

  static uint64_t numWorking(uint64_t value) noexcept {
    return (value >> 16) & 0xFFFFULL;
  }

  static uint64_t numBlocked(uint64_t value) noexcept {
    return (value >> 32) & 0xFFFFULL;
  }

  /*
  inline void queueJob() noexcept { ++_nrQueued; }

  inline void unqueueJob() noexcept {
    if (--_nrQueued == UINT64_MAX) {
      TRI_ASSERT(false);
    }
  }
  */

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
  //
  // the next lowest bytes (CC) are used to store the number of currently
  // working threads
  //
  // the next bytes (BB) are used to store the number of currently blocked
  // threads
  //
  // the highest bytes (AA) are used only to encode a stopping bit. when this
  // bit is set, the scheduler is stopping (or already stopped)
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
  // minimum number of running SchedulerThreads
  uint64_t const _nrMinimum;

  // maximal number of outstanding user requests
  uint64_t const _nrMaximum;

  // current counters. refer to the above description of the
  // meaning of its individual bits
  std::atomic<uint64_t> _counters;

  std::atomic<uint64_t> _nrQueued;

  // std::unique_ptr<JobQueue> _jobQueue;

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
