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
#include <Basics/application-exit.h>
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ServerState.h"
#include "FeaturePhases/ServerFeaturePhase.h"
#include "Logger/LoggerFeature.h"
#include "Metrics/Metric.h"
#include "RestServer/InitDatabaseFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "Statistics/StatisticsFeature.h"
#include "Transaction/ClusterUtils.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

#include "Logger/LogMacros.h"

namespace arangodb::metrics {

TelemetricsFeature::TelemetricsFeature(Server& server)
    : ArangodFeature{server, *this}, _enable{false}, _interval{24} {
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
      ->addOption("--server.telemetrics-interval",
                  "Interval for telemetrics requests to be sent (unit: hours. "
                  "Default: 24)",
                  new options::NumericParameter<long>(&_interval),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(31100);
}

void TelemetricsFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> /*options*/) {}

void TelemetricsFeature::storeLastUpdate(
    std::shared_ptr<transaction::Context> ctx, bool isInsert) {
  LOG_DEVEL << "SENDING TELEMETRICS";
  OperationOptions options;
  options.ignoreRevs = true;
  options.overwriteMode = OperationOptions::OverwriteMode::Update;
  auto rightNow = std::chrono::high_resolution_clock::now();
  auto rightNowSecs =
      std::chrono::time_point_cast<std::chrono::seconds>(rightNow);
  auto rightNowAbs = rightNowSecs.time_since_epoch().count();
  SingleCollectionTransaction trx(ctx, kCollName, AccessMode::Type::WRITE);
  Result res = trx.begin();
  if (isInsert) {
    LOG_DEVEL << "last update is empty";
    Builder body;
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
  } else {
    LOG_DEVEL << "last update is empty";
    Builder body;
    {
      ObjectBuilder b(&body);
      body.add("_key", kKeyValue);
      body.add("lastUpdate", Value(rightNowAbs));
    }
    try {
      OperationResult result = trx.update(kCollName, body.slice(), options);
      Result res = trx.finish(result.result);
      _latestUpdate = rightNowAbs;
    } catch (std::exception const& e) {
      LOG_TOPIC("affd3", FATAL, Logger::STATISTICS)
          << "Failed to update doc: " << e.what() << ". Bailing out.";
      FATAL_ERROR_EXIT();
    }
  }
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
  ServerState::RoleEnum role = ServerState::instance()->getRole();
  if (ServerState::instance()->isSingleServer(role)) {
    LOG_DEVEL << "is single server";
    auto& serverFeaturePhase = server().getFeature<ServerFeaturePhase>();
    TRI_ASSERT(
        serverFeaturePhase.state() ==
        arangodb::application_features::ApplicationFeature::State::STARTED);
    LOG_DEVEL << "will enqueue";
  }

  LOG_DEVEL << "telemetrics feature is enabled";
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
  // if (auto timeDiff = std::chrono::duration_cast<std::chrono::hours>(
  if (auto timeDiff = std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::duration<double, std::milli>(rightNowAbs -
                                                    _latestUpdate));
      timeDiff.count() >= _interval) {
    _telemetricsEnqueue(false);
    LOG_DEVEL << "it's passed " << timeDiff.count() << " hours";
  } else {
    LOG_DEVEL << "diff is " << timeDiff.count();
    auto workItem = SchedulerFeature::SCHEDULER->queueDelayed(
        arangodb::RequestLane::INTERNAL_LOW, timeDiff, _telemetricsEnqueue);
    std::lock_guard<std::mutex> guard(_workItemMutex);
    _workItem = std::move(workItem);
  }

  _telemetricsEnqueue = [this, ctx, isInsert](bool cancelled) {
    if (cancelled) {
      return;
    }
    LOG_DEVEL << "telemetrics enqueue";
    storeLastUpdate(ctx, isInsert);
    auto workItem = SchedulerFeature::SCHEDULER->queueDelayed(
        arangodb::RequestLane::INTERNAL_LOW,
        std::chrono::seconds(this->_interval), _telemetricsEnqueue);
    //    arangodb::RequestLane::INTERNAL_LOW,
    //    std::chrono::hours(this->_interval), _telemetricsEnqueue);
    std::lock_guard<std::mutex> guard(_workItemMutex);
    _workItem = std::move(workItem);
  };
}

}  // namespace arangodb::metrics
