////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "RestServer/arangod.h"
#include "Scheduler/Scheduler.h"

#include <functional>
#include <memory>

namespace arangodb {

class SchedulerFeature final : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept { return "Scheduler"; }

  static Scheduler* SCHEDULER;

  explicit SchedulerFeature(Server& server);
  ~SchedulerFeature();

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void stop() override final;
  void unprepare() override final;

  // -------------------------------------------------------------------------
  // UNRELATED SECTION STARTS HERE: Signals and other things crept into Sched
  // -------------------------------------------------------------------------
  void buildControlCHandler();
  void buildHangupHandler();

  uint64_t maximalThreads() const noexcept;

 private:
  void signalStuffInit();
  void signalStuffDeinit();

  uint64_t _nrMinimalThreads = 4;
  uint64_t _nrMaximalThreads = 0;
  uint64_t _queueSize = 4096;
  uint64_t _fifo1Size = 4096;
  uint64_t _fifo2Size = 4096;
  uint64_t _fifo3Size = 4096;
  double _ongoingLowPriorityMultiplier = 4.0;
  double _unavailabilityQueueFillGrade = 0.75;

  std::unique_ptr<Scheduler> _scheduler;

  struct AsioHandler;
  std::unique_ptr<AsioHandler> _asioHandler;
};

}  // namespace arangodb
