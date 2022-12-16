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

#include "velocypack/Slice.h"

namespace arangodb::metrics {

/*
struct ITelemetricsSender {
 public:
  virtual ~ITelemetricsSender() = default;
  virtual void send(arangodb::velocypack::Builder&) = 0;
};
*/

struct ITelemetricsSender {
  virtual ~ITelemetricsSender() = default;
  virtual void send(arangodb::velocypack::Slice result) const = 0;
};

struct TelemetricsSender : public ITelemetricsSender {
  void send(velocypack::Slice result) const override {
    LOG_TOPIC("affd3", WARN, Logger::STATISTICS) << result.toJson();
  }
};

struct LastUpdateHandler {
 public:
  LastUpdateHandler(ArangodServer& server, std::uint64_t prepareDeadline = 30)
      : _sender(std::make_unique<TelemetricsSender>()),
        _prepareDeadline(prepareDeadline),
        _server(server) {}
  virtual ~LastUpdateHandler() = default;
  virtual bool handleLastUpdatePersistance(bool isCoordinator,
                                           std::string& oldRev,
                                           uint64_t& lastUpdate,
                                           uint64_t interval);
  virtual void sendTelemetrics();
  void doLastUpdate(std::string_view oldRev, uint64_t lastUpdate);
  void setTelemetricsSender(std::unique_ptr<ITelemetricsSender> sender) {
    _sender = std::move(sender);
  }
  ITelemetricsSender* getSender() { return _sender.get(); }
  ArangodServer& server() const { return _server; }
  void setPrepareDeadline(std::uint64_t newPrepareDeadline) {
    _prepareDeadline = newPrepareDeadline;
  }

 private:
  std::unique_ptr<ITelemetricsSender> _sender;
  std::uint64_t _prepareDeadline;
  ArangodServer& _server;
};

class TelemetricsFeature : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept { return "Telemetrics"; }

  explicit TelemetricsFeature(Server& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) final;
  void start() final;
  void stop() final;
  void beginShutdown() final;
  void setRescheduleInterval(uint64_t newInerval) {
    _rescheduleInterval = newInerval;
  }
  void setInterval(uint64_t newInterval) { _interval = newInterval; }
  void setUpdateHandler(std::unique_ptr<LastUpdateHandler> updateHandler) {
    _updateHandler = std::move(updateHandler);
  }

 private:
  bool _enable;
  uint64_t _interval;
  uint64_t _rescheduleInterval;

  std::function<void(bool)> _telemetricsEnqueue;
  std::mutex _workItemMutex;
  Scheduler::WorkHandle _workItem;
  std::unique_ptr<LastUpdateHandler> _updateHandler;
};

}  // namespace arangodb::metrics
