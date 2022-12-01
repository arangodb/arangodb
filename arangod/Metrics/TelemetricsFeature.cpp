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
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ServerState.h"
#include "FeaturePhases/ServerFeaturePhase.h"
#include "FeaturePhases/ClusterFeaturePhase.h"
#include "Logger/LoggerFeature.h"
#include "RestServer/ServerFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/OperationResult.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/SupportInfoBuilder.h"
#include "VocBase/vocbase.h"

using namespace std::chrono;
using namespace std::chrono_literals;
using namespace arangodb::velocypack;

namespace {
constexpr std::string_view kCollName = "_statistics";
constexpr std::string_view kKeyValue = "telemetrics";
constexpr std::string_view kAttrName = "lastUpdate";
}  // namespace

using namespace arangodb;

namespace arangodb::metrics {

TelemetricsFeature::TelemetricsFeature(Server& server)
    : ArangodFeature{server, *this}, _enable{true}, _interval{86400} {
  setOptional();
  startsAfter<SystemDatabaseFeature>();
  startsAfter<ClusterFeature>();
  startsAfter<ClusterFeaturePhase>();
  startsAfter<ServerFeaturePhase>();
}

void TelemetricsFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options
      ->addOption("--server.send-telemetrics",
                  "Whether to enable the telemetrics API.",
                  new options::BooleanParameter(&_enable),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(31100);

  options
      ->addOption("--server.telemetrics-interval",
                  "Interval for telemetrics requests to be sent (in seconds)",
                  new options::UInt64Parameter(&_interval),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(31100);
}

void TelemetricsFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> /*options*/) {
  // make it not less than an hour

  if (_interval < 3600) {
    _interval = 3600;
  }
}

void TelemetricsFeature::sendTelemetrics() {
  VPackBuilder result;
  SupportInfoBuilder::buildInfoMessage(result, StaticStrings::SystemDatabase,
                                       server());
  LOG_TOPIC("affd3", DEBUG, Logger::STATISTICS) << result.slice().toJson();
}

bool TelemetricsFeature::storeLastUpdate(bool isCoordinator) {
  auto& sysDbFeature = server().getFeature<SystemDatabaseFeature>();

  auto vocbase = sysDbFeature.use();

  OperationOptions options;

  auto ctx = transaction::StandaloneContext::Create(*vocbase);

  std::unique_ptr<SingleCollectionTransaction> trx;

  try {
    trx = std::make_unique<SingleCollectionTransaction>(
        ctx, std::string{::kCollName}, AccessMode::Type::WRITE);
  } catch (...) {
    // it throws if can't add the collection, which would mean the collection is
    // not ready yet but we don't care about it because we will reschedule and
    // try to create the transaction again, until the collection is ready
    return false;
  }
  TRI_ASSERT(trx);
  Result res = trx->begin();

  if (!res.ok()) {
    LOG_TOPIC("12c70", WARN, Logger::STATISTICS)
        << "Failed to begin transaction: " << res.errorMessage();
    return false;
  }

  VPackBuilder docRead;
  velocypack::Builder docInfo;
  {
    ObjectBuilder b(&docInfo);
    docInfo.add("_key", ::kKeyValue);
  }
  VPackSlice docInfoSlice = docInfo.slice();

  res = trx->documentFastPath(std::string{::kCollName}, docInfoSlice, options,
                              docRead);

  using cast = std::chrono::duration<std::uint64_t>;
  std::uint64_t rightNowSecs =
      std::chrono::duration_cast<cast>(steady_clock::now().time_since_epoch())
          .count();

  if (res.ok()) {
    VPackSlice docReadSlice = docRead.slice();
    TRI_ASSERT(!docReadSlice.isNone());
    uint64_t lastUpdateRead = docReadSlice.get(::kAttrName).getUInt();
    VPackValueLength length;
    char const* revValue = docReadSlice.get("_rev").getStringUnchecked(length);

    _lastUpdate = seconds{lastUpdateRead};
    auto diffInSecs = rightNowSecs - _lastUpdate.count();
    if (diffInSecs >= _interval) {
      velocypack::Builder docInfo;
      {
        ObjectBuilder b(&docInfo);
        docInfo.add("_key", ::kKeyValue);
        docInfo.add("lastUpdate", Value(rightNowSecs));
        docInfo.add("_rev", Value(revValue));
      }
      VPackSlice docInfoSlice = docInfo.slice();
      OperationOptions options;
      if (isCoordinator) {
        options.ignoreRevs = false;
      }
      OperationResult result =
          trx->update(std::string{::kCollName}, docInfoSlice, options);
      if (isCoordinator && result.errorNumber() == TRI_ERROR_ARANGO_CONFLICT) {
        std::ignore = trx->abort();
        // revs don't match, coordinator must reschedule
        return false;
      } else if (!result.ok()) {
        LOG_TOPIC("8cb6f", WARN, Logger::STATISTICS)
            << "Failed to update doc: " << res.errorMessage();
        std::ignore = trx->abort();
        return false;
      }
      Result res = trx->finish(result.result);
      if (!res.ok()) {
        LOG_TOPIC("e9bad", WARN, Logger::STATISTICS)
            << "Failed to finish transaction: " << res.errorMessage();
        std::ignore = trx->abort();
        return false;
      }
      return true;
    }
  } else if (res.is(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {  // must insert the
                                                             // doc for updating
                                                             // afterwards
    _lastUpdate = seconds{rightNowSecs};
    velocypack::Builder docInfo;
    {
      ObjectBuilder b(&docInfo);
      docInfo.add("_key", ::kKeyValue);
      docInfo.add("lastUpdate", Value(rightNowSecs));
    }
    VPackSlice docInfoSlice = docInfo.slice();
    OperationResult result =
        trx->insert(std::string{::kCollName}, docInfoSlice, options);

    if (!result.ok()) {
      LOG_TOPIC("56650", WARN, Logger::STATISTICS)
          << "Failed to insert doc: " << result.errorMessage();
      std::ignore = trx->abort();
      return false;
    }
    Result res = trx->finish(result.result);
    if (!res.ok()) {
      LOG_TOPIC("a7de1", WARN, Logger::STATISTICS)
          << "Failed to insert doc: " << res.errorMessage();
    }
  } else {
    LOG_TOPIC("26231", WARN, Logger::STATISTICS)
        << "Failed to read document: " << res.errorMessage();
  }
  std::ignore = trx->abort();
  return false;
}

void TelemetricsFeature::stop() {
  std::lock_guard<std::mutex> guard(_workItemMutex);
  _workItem.reset();
}

void TelemetricsFeature::beginShutdown() {
  std::lock_guard<std::mutex> guard(_workItemMutex);
  _workItem.reset();
}
void TelemetricsFeature::start() {
  ServerState::RoleEnum role = ServerState::instance()->getRole();
  bool isCoordinator = ServerState::instance()->isCoordinator(role);
  if (!this->isEnabled() ||
      (!ServerState::instance()->isSingleServer() && !isCoordinator)) {
    return;
  }

  _telemetricsEnqueue = [this, isCoordinator](bool cancelled) {
    if (cancelled) {
      return;
    }
    Scheduler::WorkHandle workItem;
    if (storeLastUpdate(isCoordinator)) {
      // has reached the interval to send telemetrics again
      sendTelemetrics();
    }
    workItem = SchedulerFeature::SCHEDULER->queueDelayed(
        arangodb::RequestLane::INTERNAL_LOW, 1800s,
        _telemetricsEnqueue);  // check every 30 min
    std::lock_guard<std::mutex> guard(_workItemMutex);
    _workItem = std::move(workItem);
  };
  _telemetricsEnqueue(false);
}

}  // namespace arangodb::metrics
