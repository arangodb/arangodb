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
#include "GeneralServer/RequestLane.h"

namespace arangodb {
class JobGuard;
class ListenTask;
class SchedulerThread;

namespace velocypack {
class Builder;
}

namespace rest {
class GeneralCommTask;
class SocketTask;

class Scheduler : public std::enable_shared_from_this<Scheduler> {
  Scheduler(Scheduler const&) = delete;
  Scheduler& operator=(Scheduler const&) = delete;

  friend class arangodb::JobGuard;
  friend class arangodb::rest::GeneralCommTask;
  friend class arangodb::rest::SocketTask;
  friend class arangodb::ListenTask;
  friend class arangodb::SchedulerThread;

 public:
  Scheduler(uint64_t minThreads, uint64_t maxThreads,
            uint64_t fifo1Size, uint64_t fifo2Size);
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
  // that are not yet worked on.
  //
  // `numWorking`returns the number of jobs currently worked on.
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
    uint64_t _queued;
    uint64_t _fifo1;
    uint64_t _fifo2;
    uint64_t _fifo3;
  };

  bool queue(RequestPriority prio, std::function<void()> const&);
  void post(asio_ns::io_context::strand&, std::function<void()> const callback);

  void addQueueStatistics(velocypack::Builder&) const;
  QueueStatistics queueStatistics() const;
  std::string infoStatus();

  bool isRunning() const { return numRunning(_counters) > 0; }
  bool isStopping() const noexcept { return (_counters & (1ULL << 63)) != 0; }

 private:
  void post(std::function<void()> const callback);
  void drain();

  inline void setStopping() noexcept { _counters |= (1ULL << 63); }

  inline bool isStopping(uint64_t value) const noexcept {
    return (value & (1ULL << 63)) != 0;
  }

  bool canPostDirectly(RequestPriority prio) const noexcept;

  static uint64_t numRunning(uint64_t value) noexcept {
    return value & 0xFFFFULL;
  }

  inline void incRunning() noexcept { _counters += 1ULL << 0; }

  inline void decRunning() noexcept {
    TRI_ASSERT((_counters & 0xFFFFUL) > 0);
    _counters -= 1ULL << 0;
  }

  static uint64_t numQueued(uint64_t value) noexcept {
    return (value >> 32) & 0xFFFFULL;
  }

  inline void incQueued() noexcept { _counters += 1ULL << 32; }

  inline void decQueued() noexcept {
    TRI_ASSERT(((_counters & 0XFFFF00000000UL) >> 32) > 0);
    _counters -= 1ULL << 32;
  }

  static uint64_t numWorking(uint64_t value) noexcept {
    return (value >> 16) & 0xFFFFULL;
  }

  inline void incWorking() noexcept { _counters += 1ULL << 16; }

  inline void decWorking() noexcept {
    TRI_ASSERT(((_counters & 0XFFFF0000UL) >> 16) > 0);
    _counters -= 1ULL << 16;
  }


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

  std::atomic<uint64_t> _counters;

  inline uint64_t getCounters() const noexcept { return _counters; }

  // the fifos will collect the outstand requests in case the Scheduler
  // queue is full

  struct FifoJob {
    FifoJob(std::function<void()> const& callback)
        : _callback(callback) {}
    std::function<void()> _callback;
  };

  bool pushToFifo(int64_t fifo, std::function<void()> const& callback);
  bool popFifo(int64_t fifo);

  static constexpr int64_t NUMBER_FIFOS = 3;
  static constexpr int64_t FIFO1 = 0;
  static constexpr int64_t FIFO2 = 1;
  static constexpr int64_t FIFO3 = 2;

  uint64_t const _maxFifoSize[NUMBER_FIFOS];
  std::atomic<int64_t> _fifoSize[NUMBER_FIFOS];

  boost::lockfree::queue<FifoJob*> _fifo1;
  boost::lockfree::queue<FifoJob*> _fifo2;
  boost::lockfree::queue<FifoJob*> _fifo3;
  boost::lockfree::queue<FifoJob*>* _fifos[NUMBER_FIFOS];

  // the following methds create tasks in the `io_context`.
  // The `io_context` itself is not exposed because everything
  // should use the method `post` of the Scheduler.

 public:
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

#ifndef _WIN32
  asio_ns::local::stream_protocol::acceptor* newDomainAcceptor() {
    return new asio_ns::local::stream_protocol::acceptor(*_ioContext);
  }
#endif

  asio_ns::ip::tcp::socket* newSocket() {
    return new asio_ns::ip::tcp::socket(*_ioContext);
  }

#ifndef _WIN32
  asio_ns::local::stream_protocol::socket* newDomainSocket() {
    return new asio_ns::local::stream_protocol::socket(*_ioContext);
  }
#endif

  asio_ns::ssl::stream<asio_ns::ip::tcp::socket>* newSslSocket(
      asio_ns::ssl::context& context) {
    return new asio_ns::ssl::stream<asio_ns::ip::tcp::socket>(*_ioContext,
                                                              context);
  }

  asio_ns::ip::tcp::resolver* newResolver() {
    return new asio_ns::ip::tcp::resolver(*_ioContext);
  }

  asio_ns::signal_set* newSignalSet() {
    return new asio_ns::signal_set(*_managerContext);
  }

 private:
  static void initializeSignalHandlers();

  // `start`will start the Scheduler threads
  // `stopRebalancer` will stop the rebalancer thread
  // `beginShutdown` will begin to shut down the Scheduler threads
  // `shutdown` will shutdown the Scheduler threads

 public:
  bool start();
  void beginShutdown();
  void shutdown();

 private:
  void startIoService();
  void startManagerThread();
  void startRebalancer();

 public:
  void stopRebalancer() noexcept;

 private:
  std::shared_ptr<asio_ns::io_context::work> _serviceGuard;
  std::unique_ptr<asio_ns::io_context> _ioContext;

  std::shared_ptr<asio_ns::io_context::work> _managerGuard;
  std::unique_ptr<asio_ns::io_context> _managerContext;

  std::unique_ptr<asio_ns::steady_timer> _threadManager;
  std::function<void(const asio_ns::error_code&)> _threadHandler;

  // There will never be less than `_minThread` Scheduler threads.
  // There will never be more than `_maxThread` Scheduler threads.

 private:
  void rebalanceThreads();

 public:
  // decrements the nrRunning counter for the thread
  void threadHasStopped();

  // check if the current thread should be stopped returns true if
  // yes, otherwise false. when the function returns true, it has
  // already decremented the nrRunning counter!
  bool threadShouldStop(double now);

 private:
  void startNewThread();

 private:
  uint64_t const _minThreads;
  uint64_t const _maxThreads;

  mutable Mutex _threadCreateLock;
  double _lastAllBusyStamp;
};
}  // namespace rest
}  // namespace arangodb

#endif
