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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include <chrono>
#include <memory>
#include <string_view>
#include <thread>
#include <variant>

#include <fmt/core.h>
#include <fmt/ostream.h>

#include "Basics/voc-errors.h"
#include "Conductor.h"

#include "Pregel/Aggregator.h"
#include "Pregel/AggregatorHandler.h"
#include "Pregel/AlgoRegistry.h"
#include "Pregel/Algorithm.h"
#include "Pregel/PregelOptions.h"
#include "Pregel/Conductor/States/CanceledState.h"
#include "Pregel/Conductor/States/ComputingState.h"
#include "Pregel/Conductor/States/DoneState.h"
#include "Pregel/Conductor/States/FatalErrorState.h"
#include "Pregel/Conductor/States/LoadingState.h"
#include "Pregel/Conductor/States/State.h"
#include "Pregel/Conductor/States/StoringState.h"
#include "Pregel/Connection/DirectConnection.h"
#include "Pregel/MasterContext.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/Status/ConductorStatus.h"
#include "Pregel/Status/Status.h"
#include "Pregel/Utils.h"
#include "Pregel/Messaging/Message.h"
#include "Pregel/Conductor/GraphSource.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FunctionUtils.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/TimeString.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Futures/Utilities.h"
#include "Metrics/Counter.h"
#include "Metrics/Gauge.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"
#include "velocypack/Builder.h"
#include "Pregel/Connection/NetworkConnection.h"
#include "velocypack/Value.h"

#include <Inspection/VPack.h>
#include <velocypack/Iterator.h>
#include <fmt/core.h>

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::basics;

#define LOG_PREGEL(logId, level)          \
  LOG_TOPIC(logId, level, Logger::PREGEL) \
      << fmt::format("[job {}]", _executionNumber)

Conductor::Conductor(ExecutionNumber executionNumber, TRI_vocbase_t& vocbase,
                     conductor::PregelGraphSource graphSource,
                     std::string const& algoName, VPackSlice const& config,
                     PregelFeature& feature)
    : _feature(feature),
      _created(std::chrono::system_clock::now()),
      _vocbaseGuard(vocbase),
      _executionNumber(executionNumber),
      _algorithm(
          AlgoRegistry::createAlgorithm(vocbase.server(), algoName, config)),
      _graphSource{std::move(graphSource)} {
  if (!config.isObject()) {
    _userParams.add(VPackSlice::emptyObjectSlice());
  } else {
    _userParams.add(config);
  }

  if (!_algorithm) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Algorithm not found");
  }
  _masterContext.reset(_algorithm->masterContext(config));
  _aggregators = std::make_unique<AggregatorHandler>(_algorithm.get());

  _maxSuperstep =
      VelocyPackHelper::getNumericValue(config, "maxGSS", _maxSuperstep);
  _useMemoryMaps = VelocyPackHelper::getBooleanValue(
      _userParams.slice(), Utils::useMemoryMapsKey, _feature.useMemoryMaps());

  VPackSlice storeSlice = config.get("store");
  _storeResults = !storeSlice.isBool() || storeSlice.getBool();

  // time-to-live for finished/failed Pregel jobs before garbage collection.
  // default timeout is 10 minutes for each conductor
  uint64_t ttl = 600;
  _ttl = std::chrono::seconds(
      VelocyPackHelper::getNumericValue(config, "ttl", ttl));

  _feature.metrics()->pregelConductorsNumber->fetch_add(1);

  auto connection = std::unique_ptr<Connection>();
  if (ServerState::instance()->getRole() == ServerState::ROLE_SINGLE) {
    connection =
        std::make_unique<DirectConnection>(_feature, _vocbaseGuard.database());
  } else {
    network::RequestOptions reqOpts;
    reqOpts.timeout = network::Timeout(5.0 * 60.0);
    reqOpts.database = _vocbaseGuard.database().name();
    connection = std::make_unique<NetworkConnection>(
        Utils::apiPrefix, std::move(reqOpts), _vocbaseGuard.database());
  }
  _state = std::make_unique<conductor::Initial>(
      *this, conductor::WorkerApi<WorkerCreated>::create(
                 executionNumber, std::move(connection)));

  LOG_PREGEL("00f5f", INFO)
      << "Starting " << _algorithm->name() << " in database '" << vocbase.name()
      << "', ttl: " << _ttl.count() << "s"
      << ", parallelism: "
      << WorkerConfig::parallelism(_feature, _userParams.slice())
      << ", memory mapping: " << (_useMemoryMaps ? "yes" : "no")
      << ", store: " << (_storeResults ? "yes" : "no")
      << ", config: " << _userParams.slice().toJson();
}

Conductor::~Conductor() {
  try {
    this->cancel();
  } catch (...) {
    // must not throw exception from here
  }
  _feature.metrics()->pregelConductorsNumber->fetch_sub(1);
}

void Conductor::start() { _run(); }

auto Conductor ::_postGlobalSuperStep() -> PostGlobalSuperStepResult {
  // workers are done if all messages were processed and no active vertices
  // are left to process
  bool done =
      _statistics.noActiveVertices() && _statistics.allMessagesProcessed();
  bool proceed = true;
  if (_masterContext) {  // ask algorithm to evaluate aggregated values
    proceed = _masterContext->postGlobalSuperstep();
    if (!proceed) {
      LOG_PREGEL("0aa8e", DEBUG) << "Master context ended execution";
    }
  }
  return PostGlobalSuperStepResult{
      .finished = !proceed || done || _globalSuperstep >= _maxSuperstep};
}

auto Conductor::_preGlobalSuperStep() -> void {
  if (_masterContext) {
    _masterContext->_globalSuperstep = _globalSuperstep;
    _masterContext->_vertexCount = _totalVerticesCount;
    _masterContext->_edgeCount = _totalEdgesCount;
    _masterContext->preGlobalSuperstep();
  }
}

// ============ Conductor callbacks ===============

// The worker can (and should) periodically call back
// to update its status
void Conductor::workerStatusUpdated(StatusUpdated const& data) {
  MUTEX_LOCKER(guard, _callbackMutex);
  // TODO: for these updates we do not care about uniqueness of responses
  // _ensureUniqueResponse(data);

  VPackBuilder event;
  serialize(event, data);
  LOG_PREGEL("76632", TRACE)
      << fmt::format("Update received {}", event.toJson());

  _status.updateWorkerStatus(data.senderId, data.status);
}

bool Conductor::canBeGarbageCollected() const {
  // we don't want to block other operations for longer, so if we can't
  // immediately acuqire the mutex here, we assume a conductor cannot be
  // garbage-collected. the same conductor will be probed later anyway, so
  // we should be fine
  TRY_MUTEX_LOCKER(guard, _callbackMutex);
  if (!guard.isLocked()) {
    return false;
  }

  auto expiration = _state->getExpiration();
  return expiration.has_value() &&
         expiration.value() <= std::chrono::system_clock::now();
}

auto Conductor::collectAQLResults(bool withId) -> ResultT<PregelResults> {
  MUTEX_LOCKER(guard, _callbackMutex);
  if (_storeResults) {
    VPackBuilder results;
    { VPackArrayBuilder ab(&results); }
    return PregelResults{results};
  }
  return _state->getResults(withId);
}

void Conductor::toVelocyPack(VPackBuilder& result) const {
  MUTEX_LOCKER(guard, _callbackMutex);

  result.openObject();
  result.add("id", VPackValue(std::to_string(_executionNumber.value)));
  result.add("database", VPackValue(_vocbaseGuard.database().name()));
  if (_algorithm != nullptr) {
    result.add("algorithm", VPackValue(_algorithm->name()));
  }
  result.add("created", VPackValue(timepointToString(_created)));
  if (auto expiration = _state->getExpiration(); expiration.has_value()) {
    result.add("expires", VPackValue(timepointToString(expiration.value())));
  }
  result.add("ttl", VPackValue(_ttl.count()));
  result.add("state", VPackValue(_state->name()));
  result.add("gss", VPackValue(_globalSuperstep));

  if (_timing.total.hasStarted()) {
    result.add("totalRuntime",
               VPackValue(_timing.total.elapsedSeconds().count()));
  }
  if (_timing.loading.hasStarted()) {
    result.add("startupTime",
               VPackValue(_timing.loading.elapsedSeconds().count()));
  }
  if (_timing.computation.hasStarted()) {
    result.add("computationTime",
               VPackValue(_timing.computation.elapsedSeconds().count()));
  }
  if (_timing.storing.hasStarted()) {
    result.add("storageTime",
               VPackValue(_timing.storing.elapsedSeconds().count()));
  }
  {
    result.add(VPackValue("gssTimes"));
    VPackArrayBuilder array(&result);
    for (auto const& gssTime : _timing.gss) {
      result.add(VPackValue(gssTime.elapsedSeconds().count()));
    }
  }
  _aggregators->serializeValues(result);
  auto statistics = velocypack::serialize(_statistics);
  result.add(VPackObjectIterator(statistics.slice()));
  result.add("vertexCount", VPackValue(_totalVerticesCount));
  result.add("edgeCount", VPackValue(_totalEdgesCount));
  VPackSlice p = _userParams.slice().get(Utils::parallelismKey);
  if (!p.isNone()) {
    result.add("parallelism", p);
  }
  if (_masterContext) {
    VPackObjectBuilder ob(&result, "masterContext");
    _masterContext->serializeValues(result);
  }
  result.add("useMemoryMaps", VPackValue(_useMemoryMaps));

  result.add(VPackValue("detail"));
  auto conductorStatus = _status.accumulate();
  serialize(result, conductorStatus);

  result.close();
}

auto Conductor::receive(MessagePayload message) -> void {
  _state->receive(message);
}

auto Conductor::_run() -> void {
  auto nextState = _state->run();
  if (nextState.has_value()) {
    _changeState(std::move(nextState.value()));
  }
}

void Conductor::cancel() {
  auto newState = _state->cancel();
  if (newState.has_value()) {
    _changeState(std::move(newState.value()));
  }
}

auto Conductor::_changeState(std::unique_ptr<conductor::State> state) -> void {
  _state = std::move(state);
  _run();
}
