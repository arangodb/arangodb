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
#include "Metrics/TelemetricsFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Agency/AsyncAgencyComm.h"
#include <Basics/application-exit.h>
#include "Basics/NumberOfCores.h"
#include "Basics/PhysicalMemory.h"
#include "Basics/process-utils.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "FeaturePhases/ServerFeaturePhase.h"
#include "Logger/LoggerFeature.h"
#include "Logger/LogTimeFormat.h"
#include "Metrics/Metric.h"
#include "Metrics/MetricsFeature.h"
#include "Network/NetworkFeature.h"
#include "Replication/ReplicationFeature.h"
#include "Rest/Version.h"
#include "RestServer/EnvironmentFeature.h"
#include "RestServer/ServerFeature.h"
#include "RestServer/CpuUsageFeature.h"
#include "RestServer/InitDatabaseFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "Statistics/StatisticsFeature.h"
#include "Statistics/ServerStatistics.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/ClusterUtils.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

#include "Logger/LogMacros.h"

using namespace arangodb;

namespace arangodb::metrics {

TelemetricsFeature::TelemetricsFeature(Server& server)
    : ArangodFeature{server, *this},
      _enable{false},
      _interval{24},
      _infoHandler(server) {
  setOptional();
  startsAfter<LoggerFeature>();
  // startsAfter<InitDatabaseFeature>();
  startsAfter<DatabaseFeature>();
  startsAfter<ClusterFeature>();
  startsAfter<ServerFeaturePhase>();
  // startsBefore<application_features::GreetingsFeaturePhase>();
}

void TelemetricsFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  LOG_DEVEL << "collect options";
  options
      ->addOption("--server.send-telemetrics",
                  "Whether to enable the phone home API.",
                  new options::BooleanParameter(&_enable),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(31100);

  options
      ->addOption(
          "--server.telemetrics-interval",
          "Interval for telemetrics requests to be sent (unit: seconds. "
          "Default: 24h, 86400s)",
          new options::NumericParameter<long>(&_interval),
          arangodb::options::makeDefaultFlags(
              arangodb::options::Flags::Uncommon))
      .setIntroducedIn(31100);
}

void TelemetricsFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> /*options*/) {}

void TelemetricsFeature::sendTelemetrics() {
  LOG_DEVEL << "SEND TELEMETRICS";
  VPackBuilder result;
  _infoHandler.buildInfoMessage(result);

  LOG_DEVEL << result.slice().toJson();
  LOG_TOPIC("affd3", DEBUG, Logger::STATISTICS) << result.slice().toJson();
}

bool TelemetricsFeature::storeLastUpdate(bool isInsert) {
  ServerState::RoleEnum role = ServerState::instance()->getRole();
  bool isCoordinator = ServerState::instance()->isCoordinator(role);
  DatabaseFeature& databaseFeature = server().getFeature<DatabaseFeature>();
  TRI_vocbase_t* vocbase =
      databaseFeature.useDatabase(StaticStrings::SystemDatabase);
  auto ctx = transaction::StandaloneContext::Create(*vocbase);

  // options.overwriteMode = OperationOptions::OverwriteMode::Conflict;
  auto rightNow = std::chrono::high_resolution_clock::now();
  auto rightNowSecs =
      std::chrono::time_point_cast<std::chrono::seconds>(rightNow);
  auto rightNowAbs = rightNowSecs.time_since_epoch().count();
  SingleCollectionTransaction trx(ctx, kCollName, AccessMode::Type::WRITE);
  LOG_DEVEL << "created transaction";
  Result res = trx.begin();
  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  if (isInsert) {
    OperationOptions options;
    options.ignoreRevs = true;
    LOG_DEVEL << "last update is empty";
    velocypack::Builder body;
    {
      ObjectBuilder b(&body);
      body.add("_key", kKeyValue);
      body.add("lastUpdate", Value(rightNowAbs));
    }
    try {
      OperationResult result = trx.insert(kCollName, body.slice(), options);
      Result res = trx.finish(result.result);
      _latestUpdate = rightNowAbs;
    } catch (std::exception const& e) {
      LOG_TOPIC("56650", FATAL, Logger::STATISTICS)
          << "Failed to insert doc: " << e.what() << ". Bailing out.";
      FATAL_ERROR_EXIT();
    }
    return true;
  } else {
    LOG_DEVEL << "last update is empty";
    velocypack::Builder body;
    {
      ObjectBuilder b(&body);
      body.add("_key", kKeyValue);
      body.add("lastUpdate", Value(rightNowAbs));
    }
    try {
      OperationOptions options;
      if (isCoordinator) {
        options.ignoreRevs = false;
      }
      OperationResult result = trx.update(kCollName, body.slice(), options);
      if (isCoordinator && result.errorNumber() == TRI_ERROR_ARANGO_CONFLICT) {
        std::ignore = trx.abort();
        // revs don't match, coordinator must reschedule
        return false;
      }
      Result res = trx.finish(result.result);
      if (!res.ok()) {
        THROW_ARANGO_EXCEPTION(res);
      }
      _latestUpdate = rightNowAbs;
    } catch (std::exception const& e) {
      LOG_TOPIC("affd3", FATAL, Logger::STATISTICS)
          << "Failed to update doc: " << e.what() << ". Bailing out.";
      FATAL_ERROR_EXIT();
    }
    return true;
  }
  // never reached
  return true;
}

void TelemetricsFeature::beginShutdown() {
  std::lock_guard<std::mutex> guard(_workItemMutex);
  _workItem.reset();
}
void TelemetricsFeature::start() {
  if (!this->isEnabled()) {
    return;
  }
  LOG_DEVEL << "start";
  auto& serverFeaturePhase = server().getFeature<ServerFeaturePhase>();
  TRI_ASSERT(
      serverFeaturePhase.state() ==
      arangodb::application_features::ApplicationFeature::State::STARTED);

  DatabaseFeature& databaseFeature = server().getFeature<DatabaseFeature>();
  TRI_vocbase_t* vocbase =
      databaseFeature.useDatabase(StaticStrings::SystemDatabase);
  OperationOptions options;
  options.ignoreRevs = true;
  options.overwriteMode = OperationOptions::OverwriteMode::Ignore;
  auto ctx = transaction::StandaloneContext::Create(*vocbase);

  SingleCollectionTransaction trx(ctx, kCollName, AccessMode::Type::READ);
  Result res = trx.begin();

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  auto buffer = std::make_shared<VPackBuffer<uint8_t>>();
  VPackBuilder builder(buffer);
  {
    VPackObjectBuilder guard(&builder);
    builder.add(StaticStrings::KeyString, VPackValue(kKeyValue));
  }
  VPackBuilder telemetricsDoc;
  trx.documentFastPath(kCollName, builder.slice(), options, telemetricsDoc);
  bool isInsert = false;
  if (!telemetricsDoc.slice().isNone()) {
    _latestUpdate = telemetricsDoc.slice().get(kAttrName).getUInt();

  } else {
    isInsert = true;
  }
  auto rightNow = std::chrono::high_resolution_clock::now();
  auto rightNowSecs =
      std::chrono::time_point_cast<std::chrono::seconds>(rightNow);
  auto rightNowAbs = rightNowSecs.time_since_epoch().count();
  if (isInsert) {
    _latestUpdate = static_cast<uint64_t>(rightNowAbs);
  }
  auto timeDiff = std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::duration<double, std::milli>(rightNowAbs - _latestUpdate));
  if (timeDiff.count() >= _interval) {
    _telemetricsEnqueue(false);
    LOG_DEVEL << "it's passed " << timeDiff.count() << " secs";
  } else {
    LOG_DEVEL << "diff is " << timeDiff.count();
    auto workItem = SchedulerFeature::SCHEDULER->queueDelayed(
        arangodb::RequestLane::INTERNAL_LOW, timeDiff, _telemetricsEnqueue);
    std::lock_guard<std::mutex> guard(_workItemMutex);
    _workItem = std::move(workItem);
  }

  _telemetricsEnqueue = [this, ctx, isInsert, timeDiff](bool cancelled) {
    if (cancelled) {
      return;
    }
    LOG_DEVEL << "telemetrics enqueue";
    Scheduler::WorkHandle workItem;
    if (storeLastUpdate(isInsert)) {
      sendTelemetrics();
      workItem = SchedulerFeature::SCHEDULER->queueDelayed(
          arangodb::RequestLane::INTERNAL_LOW,
          std::chrono::seconds(this->_interval), _telemetricsEnqueue);
    } else {
      workItem = SchedulerFeature::SCHEDULER->queueDelayed(
          arangodb::RequestLane::INTERNAL_LOW, timeDiff, _telemetricsEnqueue);
    }
    std::lock_guard<std::mutex> guard(_workItemMutex);
    _workItem = std::move(workItem);
  };
}

}  // namespace arangodb::metrics
