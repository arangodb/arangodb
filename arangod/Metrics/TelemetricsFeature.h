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
/// @author Julia Casarin Puget
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <chrono>

#include "ApplicationFeatures/ApplicationFeature.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestHandler/RestSupportInfoHandler.h"
#include "RestServer/arangod.h"
#include "Scheduler/SchedulerFeature.h"
#include "Statistics/ServerStatistics.h"
#include "Utils/SingleCollectionTransaction.h"

using namespace std::chrono_literals;
using namespace arangodb::velocypack;

namespace arangodb::metrics {

class TelemetricsFeature final : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept { return "Telemetrics"; }

  explicit TelemetricsFeature(Server& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) final;
  void validateOptions(std::shared_ptr<options::ProgramOptions> options) final;
  void start() final;
  void beginShutdown() final;

  bool isEnabled() { return _enable; }
  long getInterval() { return _interval; }

 private:
  static constexpr char kCollName[] = "_statistics";
  static constexpr char kKeyValue[] = "telemetrics";
  static constexpr char kAttrName[] = "latestUpdate";
  bool _enable;
  long _interval;
  uint64_t _latestUpdate;
  void sendTelemetrics();
  bool storeLastUpdate(bool isInsert);
  //  std::shared_ptr<std::function<void()>> _telemetricsEnqueue;
  std::function<void(bool)> _telemetricsEnqueue;
  std::mutex _workItemMutex;
  Scheduler::WorkHandle _workItem;
  SupportInfoMessageHandler _infoHandler;
};

}  // namespace arangodb::metrics
