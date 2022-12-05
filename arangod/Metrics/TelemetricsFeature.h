////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <chrono>

#include "ApplicationFeatures/ApplicationFeature.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/arangod.h"
#include "Scheduler/SchedulerFeature.h"
#include "Logger/LogMacros.h"

namespace arangodb::metrics {

struct ITelemetricsSender {
 public:
  virtual ~ITelemetricsSender() = default;
  virtual void send(arangodb::velocypack::Builder&) = 0;
};

class TelemetricsSender : public ITelemetricsSender {
  void send(arangodb::velocypack::Builder& result) override {
    LOG_TOPIC("affd3", WARN, Logger::STATISTICS) << result.slice().toJson();
  }
};

class TelemetricsFeature final : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept { return "Telemetrics"; }

  explicit TelemetricsFeature(
      Server& server, std::unique_ptr<ITelemetricsSender> telemetricsSender);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) final;
  void validateOptions(std::shared_ptr<options::ProgramOptions> options) final;
  void start() final;
  void stop() final;
  void beginShutdown() final;
  void setRescheduleInterval(uint64_t newInerval) {
    _rescheduleInterval = newInerval;
  }
  void setInterval(uint64_t newInterval) { _interval = newInterval; }

 private:
  bool _enable;
  uint64_t _interval;
  uint64_t _rescheduleInterval;
  uint64_t _prepareDeadline;
  void sendTelemetrics();
  bool handleLastUpdateInDoc(bool isCoordinator, std::string& oldRev,
                             uint64_t& lastUpdate);
  void doLastUpdate(std::string_view oldRev, uint64_t lastUpadte);
  std::function<void(bool)> _telemetricsEnqueue;
  std::mutex _workItemMutex;
  Scheduler::WorkHandle _workItem;
  std::unique_ptr<ITelemetricsSender> _sender;
};

}  // namespace arangodb::metrics
