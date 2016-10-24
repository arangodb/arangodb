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

#include "Basics/asio-helper.h"
#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/socket-utils.h"
#include "Logger/Logger.h"
#include "Scheduler/EventLoop.h"

namespace arangodb {
class JobQueue;

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

 public:
  Scheduler(size_t nrThreads, size_t maxQueueSize);
  virtual ~Scheduler();

 public:
  boost::asio::io_service* ioService() const { return _ioService.get(); }
  boost::asio::io_service* managerService() const {
    return _managerService.get();
  }

  EventLoop eventLoop() {
    // return EventLoop{._ioService = *_ioService.get(), ._scheduler = this}; //
    // windows complains ...
    return EventLoop{_ioService.get(), this};
  }

  bool start(basics::ConditionVariable*);
  bool isRunning() { return _nrRunning.load() > 0; }

  void beginShutdown();
  bool isStopping() { return _stopping; }
  void shutdown();

 private:
  static void initializeSignalHandlers();

 public:
  JobQueue* jobQueue() const { return _jobQueue.get(); }

  bool isIdle() {
    if (_nrWorking < _nrRunning && _nrWorking < _nrMaximal) {
      return true;
    }

    return false;
  }

  bool tryBlocking() {
    static int64_t const MIN_FREE = 2;

    if (_nrWorking < (_nrRealMaximum - MIN_FREE)) {
      ++_nrWorking;
      return true;
    }

    return false;
  }

  void enterThread() { ++_nrBusy; }

  void unenterThread() { --_nrBusy; }

  void workThread() { ++_nrWorking; }

  void unworkThread() { --_nrWorking; }

  void blockThread() { ++_nrBlocked; }

  void unblockThread() { --_nrBlocked; }

  uint64_t incRunning() { return ++_nrRunning; }

  uint64_t decRunning() { return --_nrRunning; }

  void setMinimal(int64_t minimal) { _nrMinimal = minimal; }
  void setMaximal(int64_t maximal) { _nrMaximal = maximal; }
  void setRealMaximum(int64_t maximum) { _nrRealMaximum = maximum; }

  std::string infoStatus() {
    return "busy: " + std::to_string(_nrBusy) + ", working: " +
           std::to_string(_nrWorking) + ", blocked: " +
           std::to_string(_nrBlocked) + ", running: " +
           std::to_string(_nrRunning) + ", maximal: " +
           std::to_string(_nrMaximal) + ", real maximum: " +
           std::to_string(_nrRealMaximum);
  }

  void startNewThread();
  bool stopThread();
  void threadDone(Thread*);
  void deleteOldThreads();

 private:
  void startIoService();
  void startRebalancer();
  void startManagerThread();
  void rebalanceThreads();

 private:
  size_t _nrThreads;
  size_t _maxQueueSize;

  std::atomic<bool> _stopping;

  std::atomic<int64_t> _nrBusy;
  std::atomic<int64_t> _nrWorking;
  std::atomic<int64_t> _nrBlocked;
  std::atomic<int64_t> _nrRunning;
  std::atomic<int64_t> _nrMinimal;
  std::atomic<int64_t> _nrMaximal;
  std::atomic<int64_t> _nrRealMaximum;

  std::atomic<double> _lastThreadWarning;

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
