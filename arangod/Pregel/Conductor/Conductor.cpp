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
#include "Pregel/Conductor/States/CanceledState.h"
#include "Pregel/Conductor/States/ComputingState.h"
#include "Pregel/Conductor/States/DoneState.h"
#include "Pregel/Conductor/States/FatalErrorState.h"
#include "Pregel/Conductor/States/InErrorState.h"
#include "Pregel/Conductor/States/LoadingState.h"
#include "Pregel/Conductor/States/State.h"
#include "Pregel/Conductor/States/StoringState.h"
#include "Pregel/Conductor/ClusterWorkerApi.h"
#include "Pregel/Conductor/SingleServerWorkerApi.h"
#include "Pregel/MasterContext.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/Status/ConductorStatus.h"
#include "Pregel/Status/Status.h"
#include "Pregel/Utils.h"
#include "Pregel/WorkerConductorMessages.h"
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

#include <Inspection/VPack.h>
#include <velocypack/Iterator.h>
#include <fmt/core.h>

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::basics;

namespace {
template<class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;
}  // namespace

#define LOG_PREGEL(logId, level)          \
  LOG_TOPIC(logId, level, Logger::PREGEL) \
      << fmt::format("[job {}]", _executionNumber)

Conductor::Conductor(
    ExecutionNumber executionNumber, TRI_vocbase_t& vocbase,
    std::vector<CollectionID> const& vertexCollections,
    std::vector<CollectionID> const& edgeCollections,
    std::unordered_map<std::string, std::vector<std::string>> const&
        edgeCollectionRestrictions,
    std::string const& algoName, VPackSlice const& config,
    PregelFeature& feature)
    : _feature(feature),
      _created(std::chrono::system_clock::now()),
      _vocbaseGuard(vocbase),
      _executionNumber(executionNumber),
      _algorithm(
          AlgoRegistry::createAlgorithm(vocbase.server(), algoName, config)),
      _vertexCollections(vertexCollections),
      _edgeCollections(edgeCollections) {
  if (!config.isObject()) {
    _userParams.add(VPackSlice::emptyObjectSlice());
  } else {
    _userParams.add(config);
  }

  // handle edge collection restrictions
  if (ServerState::instance()->isCoordinator()) {
    for (auto const& it : edgeCollectionRestrictions) {
      for (auto const& shardId : getShardIds(it.first)) {
        // intentionally create key in map
        auto& restrictions = _edgeCollectionRestrictions[shardId];
        for (auto const& cn : it.second) {
          for (auto const& edgeShardId : getShardIds(cn)) {
            restrictions.push_back(edgeShardId);
          }
        }
      }
    }
  } else {
    _edgeCollectionRestrictions = edgeCollectionRestrictions;
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
  if (_state != ExecutionState::CANCELED && _state != ExecutionState::DEFAULT) {
    try {
      this->cancel();
    } catch (...) {
      // must not throw exception from here
    }
  }
  _feature.metrics()->pregelConductorsNumber->fetch_sub(1);
}

void Conductor::start() {
  MUTEX_LOCKER(guard, _callbackMutex);
  _timing.total.start();
  state->run();
}

auto Conductor::process(MessagePayload const& message) -> Result {
  return std::visit(
      overloaded{[&](CleanupFinished const& x) -> Result {
                   finishedWorkerFinalize(x);
                   return {};
                 },
                 [&](StatusUpdated const& x) -> Result {
                   workerStatusUpdate(x);
                   return {};
                 },
                 [](auto const& x) -> Result {
                   return Result{TRI_ERROR_INTERNAL,
                                 "Conductor: Cannot handle received message"};
                 }},
      message);
}

auto Conductor ::_postGlobalSuperStep(VPackBuilder messagesFromWorkers)
    -> PostGlobalSuperStepResult {
  // workers are done if all messages were processed and no active vertices
  // are left to process
  bool activateAll = false;
  bool done = _globalSuperstep > 0 && _statistics.noActiveVertices() &&
              _statistics.allMessagesProcessed();
  bool proceed = true;
  if (_masterContext &&
      _globalSuperstep > 0) {  // ask algorithm to evaluate aggregated values
    _masterContext->_globalSuperstep = _globalSuperstep - 1;
    _masterContext->_enterNextGSS = false;
    _masterContext->_reports = &_reports;
    _masterContext->postGlobalSuperstepMessage(messagesFromWorkers.slice());
    proceed = _masterContext->postGlobalSuperstep();
    if (!proceed) {
      LOG_PREGEL("0aa8e", DEBUG) << "Master context ended execution";
    }
    if (proceed) {
      switch (_masterContext->postGlobalSuperstep(done)) {
        case MasterContext::ContinuationResult::ACTIVATE_ALL:
          activateAll = true;
          [[fallthrough]];
        case MasterContext::ContinuationResult::CONTINUE:
          done = false;
          break;
        case MasterContext::ContinuationResult::ERROR_ABORT:
          _inErrorAbort = true;
          [[fallthrough]];
        case MasterContext::ContinuationResult::ABORT:
          proceed = false;
          break;
        case MasterContext::ContinuationResult::DONT_CARE:
          break;
      }
    }
  }
  return PostGlobalSuperStepResult{
      .activateAll = activateAll,
      .finished = !proceed || done || _globalSuperstep >= _maxSuperstep};
}

auto Conductor::_preGlobalSuperStep() -> bool {
  if (_masterContext) {
    _masterContext->_globalSuperstep = _globalSuperstep;
    _masterContext->_vertexCount = _totalVerticesCount;
    _masterContext->_edgeCount = _totalEdgesCount;
    _masterContext->_reports = &_reports;
    return _masterContext->preGlobalSuperstepWithResult();
  }
  return true;
}

// ============ Conductor callbacks ===============

// The worker can (and should) periodically call back
// to update its status
void Conductor::workerStatusUpdate(StatusUpdated const& data) {
  MUTEX_LOCKER(guard, _callbackMutex);
  // TODO: for these updates we do not care about uniqueness of responses
  // _ensureUniqueResponse(data);

  VPackBuilder event;
  serialize(event, data);
  LOG_PREGEL("76632", DEBUG)
      << fmt::format("Update received {}", event.toJson());

  _status.updateWorkerStatus(data.senderId, data.status);
}

void Conductor::cancel() {
  MUTEX_LOCKER(guard, _callbackMutex);
  changeState(conductor::StateType::Canceled);
}

// resolves into an ordered list of shards for each collection on each
// server
static void resolveInfo(
    TRI_vocbase_t* vocbase, CollectionID const& collectionID,
    std::map<CollectionID, std::string>& collectionPlanIdMap,
    std::map<ServerID, std::map<CollectionID, std::vector<ShardID>>>& serverMap,
    std::vector<ShardID>& allShards) {
  ServerState* ss = ServerState::instance();
  if (!ss->isRunningInCluster()) {  // single server mode
    auto lc = vocbase->lookupCollection(collectionID);

    if (lc == nullptr || lc->deleted()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                                     collectionID);
    }

    collectionPlanIdMap.try_emplace(collectionID,
                                    std::to_string(lc->planId().id()));
    allShards.push_back(collectionID);
    serverMap[ss->getId()][collectionID].push_back(collectionID);

  } else if (ss->isCoordinator()) {  // we are in the cluster

    ClusterInfo& ci =
        vocbase->server().getFeature<ClusterFeature>().clusterInfo();
    std::shared_ptr<LogicalCollection> lc =
        ci.getCollection(vocbase->name(), collectionID);
    if (lc->deleted()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                                     collectionID);
    }
    collectionPlanIdMap.try_emplace(collectionID,
                                    std::to_string(lc->planId().id()));

    std::shared_ptr<std::vector<ShardID>> shardIDs =
        ci.getShardList(std::to_string(lc->id().id()));
    allShards.insert(allShards.end(), shardIDs->begin(), shardIDs->end());

    for (auto const& shard : *shardIDs) {
      std::shared_ptr<std::vector<ServerID> const> servers =
          ci.getResponsibleServer(shard);
      if (servers->size() > 0) {
        serverMap[(*servers)[0]][lc->name()].push_back(shard);
      }
    }
  } else {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_ONLY_ON_COORDINATOR);
  }
}

/// should cause workers to start a new execution
auto Conductor::_initializeWorkers(VPackSlice additional) -> GraphLoadedFuture {
  _callbackMutex.assertLockedByCurrentThread();

  // int64_t vertexCount = 0, edgeCount = 0;
  std::map<CollectionID, std::string> collectionPlanIdMap;
  std::map<ServerID, std::map<CollectionID, std::vector<ShardID>>> vertexMap,
      edgeMap;
  std::vector<ShardID> shardList;

  // resolve plan id's and shards on the servers
  for (CollectionID const& collectionID : _vertexCollections) {
    resolveInfo(&(_vocbaseGuard.database()), collectionID, collectionPlanIdMap,
                vertexMap,
                shardList);  // store or
  }
  for (CollectionID const& collectionID : _edgeCollections) {
    resolveInfo(&(_vocbaseGuard.database()), collectionID, collectionPlanIdMap,
                edgeMap,
                shardList);  // store or
  }

  if (ServerState::instance()->getRole() == ServerState::ROLE_SINGLE) {
    TRI_ASSERT(vertexMap.size() == 1);
    auto [server, _] = *vertexMap.begin();
    _dbServers.push_back(server);
    workers.emplace(server,
                    std::make_unique<conductor::SingleServerWorkerApi>(
                        _executionNumber, _feature, _vocbaseGuard.database()));
  } else {
    for (auto const& [server, _] : vertexMap) {
      _dbServers.push_back(server);
      network::RequestOptions reqOpts;
      reqOpts.timeout = network::Timeout(5.0 * 60.0);
      reqOpts.database = _vocbaseGuard.database().name();
      workers.emplace(
          server, std::make_unique<conductor::ClusterWorkerApi>(
                      server, _executionNumber,
                      Connection::create(Utils::baseUrl(Utils::workerPrefix),
                                         std::move(reqOpts),
                                         _vocbaseGuard.database())));
    }
  }
  _status = ConductorStatus::forWorkers(_dbServers);
  // do not reload all shard id's, this list must stay in the same order
  if (_allShards.size() == 0) {
    _allShards = shardList;
  }

  std::string coordinatorId = ServerState::instance()->getId();
  auto results = std::vector<futures::Future<ResultT<GraphLoaded>>>{};

  for (auto const& it : vertexMap) {
    ServerID const& server = it.first;
    std::map<CollectionID, std::vector<ShardID>> const& vertexShardMap =
        it.second;
    std::map<CollectionID, std::vector<ShardID>> const& edgeShardMap =
        edgeMap[it.first];

    VPackBuffer<uint8_t> buffer;
    VPackBuilder b(buffer);
    b.openObject();
    b.add(Utils::executionNumberKey, VPackValue(_executionNumber.value));
    b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
    b.add(Utils::algorithmKey, VPackValue(_algorithm->name()));
    b.add(Utils::userParametersKey, _userParams.slice());
    b.add(Utils::coordinatorIdKey, VPackValue(coordinatorId));
    b.add(Utils::useMemoryMapsKey, VPackValue(_useMemoryMaps));
    if (additional.isObject()) {
      for (auto pair : VPackObjectIterator(additional)) {
        b.add(pair.key.copyString(), pair.value);
      }
    }

    // edge collection restrictions
    b.add(Utils::edgeCollectionRestrictionsKey,
          VPackValue(VPackValueType::Object));
    for (auto const& pair : _edgeCollectionRestrictions) {
      b.add(pair.first, VPackValue(VPackValueType::Array));
      for (ShardID const& shard : pair.second) {
        b.add(VPackValue(shard));
      }
      b.close();
    }
    b.close();

    b.add(Utils::vertexShardsKey, VPackValue(VPackValueType::Object));
    for (auto const& pair : vertexShardMap) {
      b.add(pair.first, VPackValue(VPackValueType::Array));
      for (ShardID const& shard : pair.second) {
        b.add(VPackValue(shard));
      }
      b.close();
    }
    b.close();
    b.add(Utils::edgeShardsKey, VPackValue(VPackValueType::Object));
    for (auto const& pair : edgeShardMap) {
      b.add(pair.first, VPackValue(VPackValueType::Array));
      for (ShardID const& shard : pair.second) {
        b.add(VPackValue(shard));
      }
      b.close();
    }

    b.close();
    b.add(Utils::collectionPlanIdMapKey, VPackValue(VPackValueType::Object));
    for (auto const& pair : collectionPlanIdMap) {
      b.add(pair.first, VPackValue(pair.second));
    }
    b.close();
    b.add(Utils::globalShardListKey, VPackValue(VPackValueType::Array));
    for (std::string const& shard : _allShards) {
      b.add(VPackValue(shard));
    }
    b.close();
    b.close();

    auto graph = LoadGraph{.details = b};
    results.emplace_back(workers[server]->loadGraph(graph));
  }
  return futures::collectAll(results);
}

void Conductor::cleanup() {
  if (_masterContext) {
    _masterContext->postApplication();
  }
}

void Conductor::finishedWorkerFinalize(CleanupFinished const& data) {
  MUTEX_LOCKER(guard, _callbackMutex);

  LOG_PREGEL("08142", WARN) << fmt::format(
      "finishedWorkerFinalize, got response from {}.", data.senderId);

  state->receive(data);
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

  auto expiration = state->getExpiration();
  return expiration.has_value() &&
         expiration.value() <= std::chrono::system_clock::now();
}

auto Conductor::collectAQLResults(bool withId) -> PregelResults {
  MUTEX_LOCKER(guard, _callbackMutex);
  if (_storeResults) {
    VPackBuilder results;
    { VPackArrayBuilder ab(&results); }
    return PregelResults{.results = results};
  }
  return state->getResults(withId);
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
  if (auto expiration = state->getExpiration(); expiration.has_value()) {
    result.add("expires", VPackValue(timepointToString(expiration.value())));
  }
  result.add("ttl", VPackValue(_ttl.count()));
  result.add("state", VPackValue(state->name()));
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
  _statistics.serializeValues(result);
  result.add(VPackValue("reports"));
  _reports.intoBuilder(result);
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

// currently: returns immediate - blocking - messages
template<typename InType>
auto Conductor::_sendToAllDBServers(InType const& message)
    -> ResultT<std::vector<ModernMessage>> {
  _respondedServers.clear();

  auto modernMessage =
      ModernMessage{.executionNumber = _executionNumber, .payload = message};
  VPackBuilder in;
  serialize(in, modernMessage);

  // to support the single server case, we handle it without optimizing it
  if (!ServerState::instance()->isRunningInCluster()) {
    // blocking at the moment
    VPackBuilder out;
    _feature.handleWorkerRequest(_vocbaseGuard.database(),
                                 Utils::modernMessagingPath, in.slice(), out);
    auto result = deserialize<ModernMessage>(out.slice());
    return {{result}};
  }

  if (_dbServers.empty()) {
    LOG_PREGEL("a14fa", WARN) << "No servers registered";
    return Result{TRI_ERROR_FAILED, "No servers registered"};
  }

  std::string base = Utils::baseUrl(Utils::workerPrefix);

  VPackBuffer<uint8_t> buffer;
  buffer.append(in.slice().begin(), in.slice().byteSize());

  network::RequestOptions reqOpts;
  reqOpts.database = _vocbaseGuard.database().name();
  reqOpts.timeout = network::Timeout(5.0 * 60.0);
  reqOpts.skipScheduler = true;

  auto const& nf =
      _vocbaseGuard.database().server().getFeature<NetworkFeature>();
  network::ConnectionPool* pool = nf.pool();
  std::vector<futures::Future<network::Response>> responses;

  for (auto const& server : _dbServers) {
    responses.emplace_back(network::sendRequestRetry(
        pool, "server:" + server, fuerte::RestVerb::Post,
        base + Utils::modernMessagingPath, buffer, reqOpts));
  }

  std::vector<ModernMessage> workerResponses;
  futures::collectAll(responses)
      .thenValue([&](auto results) {
        for (auto const& tryRes : results) {
          network::Response const& res = tryRes.get();
          if (res.ok() && res.statusCode() < 400) {
            try {
              auto response = deserialize<ModernMessage>(res.slice());
              workerResponses.emplace_back(response);
            } catch (Exception& e) {
              LOG_PREGEL("56187", ERR)
                  << "Conductor received unknown message: " << e.message();
            }
          }
        }
        return workerResponses;
      })
      .wait();

  if (workerResponses.size() != responses.size()) {
    return Result{TRI_ERROR_FAILED, "Not all workers responded"};
  }

  return {workerResponses};
}

void Conductor::_ensureUniqueResponse(std::string const& sender) {
  // check if this the only time we received this
  if (_respondedServers.find(sender) != _respondedServers.end()) {
    LOG_PREGEL("c38b8", ERR) << "Received response already from " << sender;
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_CONFLICT);
  }
  _respondedServers.insert(sender);
}

std::vector<ShardID> Conductor::getShardIds(ShardID const& collection) const {
  TRI_vocbase_t& vocbase = _vocbaseGuard.database();
  ClusterInfo& ci = vocbase.server().getFeature<ClusterFeature>().clusterInfo();

  std::vector<ShardID> result;
  try {
    std::shared_ptr<LogicalCollection> lc =
        ci.getCollection(vocbase.name(), collection);
    std::shared_ptr<std::vector<ShardID>> shardIDs =
        ci.getShardList(std::to_string(lc->id().id()));
    result.reserve(shardIDs->size());
    for (auto const& it : *shardIDs) {
      result.emplace_back(it);
    }
  } catch (...) {
    result.clear();
  }

  return result;
}

void Conductor::updateState(ExecutionState state) { _state = state; }

auto Conductor::changeState(conductor::StateType name) -> void {
  switch (name) {
    case conductor::StateType::Loading:
      state = std::make_unique<conductor::Loading>(*this);
      break;
    case conductor::StateType::Computing:
      state = std::make_unique<conductor::Computing>(*this);
      break;
    case conductor::StateType::Storing:
      state = std::make_unique<conductor::Storing>(*this);
      break;
    case conductor::StateType::Canceled:
      state = std::make_unique<conductor::Canceled>(*this, _ttl);
      break;
    case conductor::StateType::Done:
      state = std::make_unique<conductor::Done>(*this, _ttl);
      break;
    case conductor::StateType::InError:
      state = std::make_unique<conductor::InError>(*this, _ttl);
      break;
    case conductor::StateType::FatalError:
      state = std::make_unique<conductor::FatalError>(*this, _ttl);
      break;
  };
  run();
}

template auto Conductor::_sendToAllDBServers(
    CollectPregelResults const& message) -> ResultT<std::vector<ModernMessage>>;

template auto Conductor::_sendToAllDBServers(StartCleanup const& message)
    -> ResultT<std::vector<ModernMessage>>;
