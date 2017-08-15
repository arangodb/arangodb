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

#ifndef ARANGOD_SCHEDULER_SCHEDULER_H
#define ARANGOD_SCHEDULER_SCHEDULER_H 1

#include "Basics/Common.h"

#include <boost/asio/steady_timer.hpp>

#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/asio-helper.h"
#include "Basics/socket-utils.h"
#include "Logger/Logger.h"
#include "Scheduler/EventLoop.h"
#include "Scheduler/Job.h"

namespace arangodb {
class JobQueue;
class JobGuard;

namespace basics {
class ConditionVariable;
}
namespace velocypack {
class Builder;
}

namespace rest {

class Scheduler {
  Scheduler(Scheduler const&) = delete;
  Scheduler& operator=(Scheduler const&) = delete;

  friend class arangodb::JobGuard;

 public:
  Scheduler(uint64_t nrMinimum, uint64_t nrDesired, uint64_t nrMaximum,
            uint64_t maxQueueSize);
  virtual ~Scheduler();

 public:
  boost::asio::io_service* ioService() const { return _ioService.get(); }
  boost::asio::io_service* managerService() const {
    return _managerService.get();
  }

  EventLoop eventLoop() {
    // return EventLoop{._ioService = *_ioService.get(), ._scheduler = this};
    // windows complains ...
    return EventLoop{_ioService.get(), this};
  }

  void post(std::function<void()> callback);

  bool start(basics::ConditionVariable*);
  bool isRunning() { return _nrRunning.load() > 0; }

  void beginShutdown();
  bool isStopping() { return _stopping; }
  void shutdown();

 private:
  static void initializeSignalHandlers();

 public:
  bool shouldStopThread() const;
  bool shouldQueueMore() const;
  bool hasQueueCapacity() const;

  bool queue(std::unique_ptr<Job> job);

  uint64_t minimum() const { return _nrMinimum; }

  uint64_t incRunning() { return ++_nrRunning; }
  uint64_t decRunning() { return --_nrRunning; }

  std::string infoStatus();

  void startNewThread();
  void threadDone(Thread*);
  void deleteOldThreads();
  
  void stopRebalancer() noexcept;

 private:
  void workThread() { ++_nrWorking; }
  void unworkThread() { --_nrWorking; }

  void blockThread() { ++_nrBlocked; }
  void unblockThread() { --_nrBlocked; }

  void startIoService();
  void startRebalancer();
  void startManagerThread();
  void rebalanceThreads();

 private:
  std::atomic<bool> _stopping;

  // maximal number of outstanding user requests
  uint64_t const _maxQueueSize;

  // minimum number of running SchedulerThreads
  uint64_t const _nrMinimum;

  // maximal number of outstanding user requests
  uint64_t const _nrMaximum;

  // number of jobs currently been worked on
  // use signed values just in case we have an underflow
  std::atomic<uint64_t> _nrWorking;

  // number of jobs that are currently been queued, but not worked on
  std::atomic<uint64_t> _nrQueued;

  // number of jobs that entered a potentially blocking situation
  std::atomic<uint64_t> _nrBlocked;

  // number of SchedulerThread that are running
  std::atomic<uint64_t> _nrRunning;

  std::unique_ptr<JobQueue> _jobQueue;

  boost::shared_ptr<boost::asio::io_service::work> _serviceGuard;
  std::unique_ptr<boost::asio::io_service> _ioService;

  boost::shared_ptr<boost::asio::io_service::work> _managerGuard;
  std::unique_ptr<boost::asio::io_service> _managerService;

  std::unique_ptr<boost::asio::steady_timer> _threadManager;
  std::function<void(const boost::system::error_code&)> _threadHandler;

  Mutex _threadsLock;
  std::unordered_set<Thread*> _threads;
  std::unordered_set<Thread*> _deadThreads;
};
}
}

#endif
