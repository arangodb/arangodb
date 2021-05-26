////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/asio_ns.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Scheduler/SupervisedScheduler.h"

namespace arangodb {

class SoftShutdownTracker;

class SchedulerFeature final : public application_features::ApplicationFeature {
 public:
  static SupervisedScheduler* SCHEDULER;

  explicit SchedulerFeature(application_features::ApplicationServer& server);
  ~SchedulerFeature();

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void stop() override final;
  void unprepare() override final;

 private:
  uint64_t _nrMinimalThreads = 4;
  uint64_t _nrMaximalThreads = 0;
  uint64_t _queueSize = 4096;
  uint64_t _fifo1Size = 4096;
  uint64_t _fifo2Size = 4096;
  uint64_t _fifo3Size = 4096;
  double _ongoingLowPriorityMultiplier = 4.0;
  double _unavailabilityQueueFillGrade = 0.75;

  std::unique_ptr<Scheduler> _scheduler;

  // -------------------------------------------------------------------------
  // UNRELATED SECTION STARTS HERE: Signals and other things crept into Sched
  // -------------------------------------------------------------------------

 public:
  void buildControlCHandler();
  void buildHangupHandler();
  SoftShutdownTracker& softShutdownTracker() const {
    TRI_ASSERT(_softShutdownTracker != nullptr);
    return *_softShutdownTracker;
  }

 private:
  void signalStuffInit();
  void signalStuffDeinit();

  std::function<void(const asio_ns::error_code&, int)> _signalHandler;
  std::function<void(const asio_ns::error_code&, int)> _exitHandler;
  std::shared_ptr<asio_ns::signal_set> _exitSignals;

  std::function<void(const asio_ns::error_code&, int)> _hangupHandler;
  std::shared_ptr<asio_ns::signal_set> _hangupSignals;

  std::shared_ptr<SoftShutdownTracker> _softShutdownTracker;
};

class SoftShutdownTracker : public std::enable_shared_from_this<SoftShutdownTracker> {
  // This is a class which tracks the proceedings in case of a soft shutdown.
  // Soft shutdown is a means to shut down a coordinator gracefully. It
  // means that certain things are allowed to run to completion but
  // new instances are no longer allowed to start. This class tracks
  // the number of these things in flight, so that the real shut down
  // can be triggered, once all tracked activity has ceased.
  // This class has customers, like the CursorRepositories of each vocbase,
  // these customers can on creation get a reference to an atomic counter,
  // which they can increase and decrease, the highest bit in the counter
  // is initially set, soft shutdown state is indicated when the highest
  // bit in each counter is reset. Then no new activity should be begun.

 public:

  static constexpr uint64_t HighBit = 1ULL << 63;

  // Each customer has an index here:
  enum Indexes : size_t {
    IndexAQLCursors = 0,
    NrCounters = 1,
  };

 private:
  
  application_features::ApplicationServer& _server;
  std::atomic<bool> _softShutdownOngoing; // flag, if soft shutdown is ongoing
  std::mutex _workItemMutex;
  Scheduler::WorkHandle _workItem;    // used for soft shutdown checker
  std::function<void(bool)> _checkFunc;
  std::atomic<uint64_t> _counters[NrCounters];

 public:

  SoftShutdownTracker(application_features::ApplicationServer& server);
  ~SoftShutdownTracker() {};

  void initiateSoftShutdown();

  bool softShutdownOngoing() const {
    return _softShutdownOngoing.load(std::memory_order_relaxed);
  }

  std::atomic<bool> const* getSoftShutdownFlag() const {
    return &_softShutdownOngoing;
  }

  std::atomic<uint64_t>* getCounterPointer(size_t index) {
    TRI_ASSERT(index < NrCounters);
    if (index < NrCounters) {
      return &_counters[index];
    } else {
      return nullptr;
    }
  }

 private:
  // Need mutex to call all of the following:

  void check() const;
  void initiateActualShutdown() const;

};

}  // namespace arangodb

