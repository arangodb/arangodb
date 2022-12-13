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
#include "Replication/ReplicationFeature.h"
#include "RestServer/ServerFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/OperationResult.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/SupportInfoBuilder.h"
#include "VocBase/vocbase.h"

#include "Logger/LogMacros.h"

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
    : ArangodFeature{server, *this},
      _enable{true},
      _interval{86400},           // 24h
      _rescheduleInterval(1800),  // 30 min
      _updateHandler(std::make_unique<LastUpdateHandler>(server)) {
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
  // make it no les than an hour for sending telemetrics
  if (_interval < 3600) {
    _interval = 3600;
  }
}

void LastUpdateHandler::sendTelemetrics() {
  VPackBuilder result;

  SupportInfoBuilder::buildInfoMessage(result, StaticStrings::SystemDatabase,
                                       _server, false, true);
  _sender->send(result.slice());
}

void LastUpdateHandler::doLastUpdate(std::string_view oldRev,
                                     uint64_t lastUpdate) {
  auto& sysDbFeature = _server.getFeature<SystemDatabaseFeature>();
  auto vocbase = sysDbFeature.use();
  OperationOptions options;

  auto ctx = transaction::StandaloneContext::Create(*vocbase);

  SingleCollectionTransaction trx(ctx, std::string{::kCollName},
                                  AccessMode::Type::WRITE);

  Result res = trx.begin();

  if (!res.ok()) {
    LOG_TOPIC("7b1a7", WARN, Logger::STATISTICS)
        << "Failed to begin transaction: " << res.errorMessage();
  }

  velocypack::Builder docInfo;
  {
    ObjectBuilder b(&docInfo);
    docInfo.add(StaticStrings::KeyString, ::kKeyValue);
    docInfo.add("lastUpdate", Value(lastUpdate));
    docInfo.add("prepareTimestamp", VPackValueType::Null);
    docInfo.add("serverId", VPackValueType::Null);
    docInfo.add(StaticStrings::RevString, Value(oldRev));
  }
  VPackSlice docInfoSlice = docInfo.slice();
  options.ignoreRevs = false;

  OperationResult result =
      trx.update(std::string{::kCollName}, docInfoSlice, options);
  if (result.fail()) {
    LOG_TOPIC("0d011", WARN, Logger::STATISTICS)
        << "Failed to update doc: " << result.errorMessage();
  }
}

bool LastUpdateHandler::handleLastUpdatePersistance(bool isCoordinator,
                                                    std::string& oldRev,
                                                    uint64_t& lastUpdate,
                                                    uint64_t interval) {
  auto& sysDbFeature = _server.getFeature<SystemDatabaseFeature>();
  auto vocbase = sysDbFeature.use();

  OperationOptions options;

  auto ctx = transaction::StandaloneContext::Create(*vocbase);

  SingleCollectionTransaction trx(ctx, std::string{::kCollName},
                                  AccessMode::Type::WRITE);

  Result res = trx.begin();

  if (!res.ok()) {
    LOG_TOPIC("12c70", WARN, Logger::STATISTICS)
        << "Failed to begin transaction: " << res.errorMessage();
    return false;
  }

  VPackBuilder docRead;
  velocypack::Builder docInfo;
  {
    VPackObjectBuilder b(&docInfo);
    docInfo.add(StaticStrings::KeyString, ::kKeyValue);
  }
  VPackSlice docInfoSlice = docInfo.slice();

  res = trx.documentFastPath(std::string{::kCollName}, docInfoSlice, options,
                             docRead);

  using cast = std::chrono::duration<std::uint64_t>;
  std::uint64_t rightNowSecs =
      std::chrono::duration_cast<cast>(steady_clock::now().time_since_epoch())
          .count();

  if (res.fail() && res.isNot(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
    LOG_TOPIC("26231", WARN, Logger::STATISTICS)
        << "Failed to read document: " << res.errorMessage();
  } else if (res.ok()) {  // we found the document
    VPackSlice docReadSlice = docRead.slice();
    TRI_ASSERT(!docReadSlice.isNone());
    uint64_t lastUpdateRead = docReadSlice.get(::kAttrName).getUInt();

    // this must be there
    std::string_view revValue =
        docReadSlice.get(StaticStrings::RevString).stringView();

    if (isCoordinator) {
      auto serverIdSlice = docReadSlice.get("serverId");
      auto timestampSlice = docReadSlice.get("prepareTimestamp");
      std::string_view serverId;
      uint64_t prepareTimestamp = 0;
      if (serverIdSlice.isString()) {
        serverId = docReadSlice.get("serverId").stringView();
      }
      if (timestampSlice.isUInt()) {
        prepareTimestamp = timestampSlice.getUInt();
      }

      if (!serverId.empty() &&
          prepareTimestamp != 0) {  // must send as the former server might have
                                    // gone down before sending telemetrics
        bool isSameCoordinator =
            serverId.compare(ServerState::instance()->getId()) == 0 ? true
                                                                    : false;
        if (isSameCoordinator ||
            rightNowSecs - prepareTimestamp >= _prepareDeadline) {
          sendTelemetrics();
          doLastUpdate(revValue, rightNowSecs);
        }
        return false;
      }
    }

    auto diffInSecs = rightNowSecs - seconds{lastUpdateRead}.count();
    if (diffInSecs >= interval) {
      velocypack::Builder docInfo;
      {
        VPackObjectBuilder b(&docInfo);
        docInfo.add(StaticStrings::KeyString, ::kKeyValue);
        docInfo.add("lastUpdate", Value(rightNowSecs));
        docInfo.add("prepareTimestamp", Value(rightNowSecs));
        docInfo.add("serverId",
                    velocypack::Value(ServerState::instance()->getId()));
        docInfo.add(StaticStrings::RevString, Value(revValue));
      }
      VPackSlice docInfoSlice = docInfo.slice();
      OperationOptions options;
      options.ignoreRevs = false;
      options.returnNew = true;

      OperationResult result =
          trx.update(std::string{::kCollName}, docInfoSlice, options);
      if (result.errorNumber() == TRI_ERROR_ARANGO_CONFLICT) {
        // revs don't match, coordinator must reschedule
        return false;
      } else if (!result.ok()) {
        LOG_TOPIC("8cb6f", WARN, Logger::STATISTICS)
            << "Failed to update doc: " << result.errorMessage();
        return false;
      }
      Result res = trx.finish(result.result);
      if (!res.ok()) {
        LOG_TOPIC("e9bad", WARN, Logger::STATISTICS)
            << "Failed to finish transaction: " << res.errorMessage();
        return false;
      }
      oldRev = result.slice().get(StaticStrings::RevString).stringView();
      lastUpdate = rightNowSecs;
      return true;
    }
    return false;
  } else {  // must insert the doc for updating afterwards
    velocypack::Builder docInfo;
    {
      VPackObjectBuilder b(&docInfo);
      docInfo.add("_key", ::kKeyValue);
      docInfo.add(kAttrName, Value(rightNowSecs));
    }
    VPackSlice docInfoSlice = docInfo.slice();
    OperationResult result =
        trx.insert(std::string{::kCollName}, docInfoSlice, options);

    if (!result.ok()) {
      LOG_TOPIC("56650", WARN, Logger::STATISTICS)
          << "Failed to insert doc: " << result.errorMessage();
      return false;
    }

    Result res = trx.finish(result.result);

    if (!res.ok()) {
      LOG_TOPIC("a7de1", WARN, Logger::STATISTICS)
          << "Failed to insert doc: " << res.errorMessage();
    }
  }
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
  if (!this->isEnabled() || ((!ServerState::instance()->isSingleServer() ||
                              !_updateHandler->server()
                                   .getFeature<ReplicationFeature>()
                                   .isActiveFailoverEnabled()) &&
                             !isCoordinator)) {
    return;
  }
  // server id prepare with timestamp, then remove it
  // last update after sending telemetrics
  _telemetricsEnqueue = [this, isCoordinator](bool cancelled) {
    if (cancelled) {
      return;
    }
    Scheduler::WorkHandle workItem;
    std::string oldRev;
    std::uint64_t lastUpdate = 0;
    try {
      if (_updateHandler->handleLastUpdatePersistance(isCoordinator, oldRev,
                                                      lastUpdate, _interval)) {
        TRI_ASSERT(_updateHandler != nullptr);
        // has reached the interval to send telemetrics again
        _updateHandler->sendTelemetrics();
        if (isCoordinator) {
          _updateHandler->doLastUpdate(oldRev, lastUpdate);
        }
      }
    } catch (...) {
      // it throws if can't add the collection, which would mean
      // the collection is
      // not ready yet but we don't care about it because we will reschedule
      // and try to create the transaction again, until the collection is
      // ready
    }

    workItem = SchedulerFeature::SCHEDULER->queueDelayed(
        arangodb::RequestLane::INTERNAL_LOW,
        std::chrono::seconds(_rescheduleInterval), _telemetricsEnqueue);
    std::lock_guard<std::mutex> guard(_workItemMutex);
    _workItem = std::move(workItem);
  };
  _telemetricsEnqueue(false);
}

}  // namespace arangodb::metrics
