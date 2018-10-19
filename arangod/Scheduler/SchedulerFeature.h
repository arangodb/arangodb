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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_SCHEDULER_SCHEDULER_FEATURE_H
#define ARANGOD_SCHEDULER_SCHEDULER_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

#include "Scheduler/Socket.h"

namespace arangodb {

namespace rest {

class Scheduler;

}

class SchedulerFeature final : public application_features::ApplicationFeature {
 public:
  static rest::Scheduler* SCHEDULER;

  explicit SchedulerFeature(application_features::ApplicationServer& server);
  ~SchedulerFeature();

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void start() override final;
  void beginShutdown() override final;
  void stop() override final;
  void unprepare() override final;

  uint64_t queueSize() const { return _queueSize; }

 private:
  uint64_t _nrMinimalThreads = 0;
  uint64_t _nrMaximalThreads = 0;
  uint64_t _queueSize = 128;
  uint64_t _fifo1Size = 1024 * 1024;
  uint64_t _fifo2Size = 4096;

 public:
  size_t concurrency() const { return static_cast<size_t>(_nrMaximalThreads); }
  void buildControlCHandler();
  void buildHangupHandler();

 private:
  /// @brief return the default number of threads to use (upper bound)
  size_t defaultNumberOfThreads() const;
  void buildScheduler();

  std::shared_ptr<rest::Scheduler> _scheduler;

  std::function<void(const asio_ns::error_code&, int)> _signalHandler;
  std::function<void(const asio_ns::error_code&, int)> _exitHandler;
  std::shared_ptr<asio_ns::signal_set> _exitSignals;

  std::function<void(const asio_ns::error_code&, int)> _hangupHandler;
  std::shared_ptr<asio_ns::signal_set> _hangupSignals;
};

}

#endif
