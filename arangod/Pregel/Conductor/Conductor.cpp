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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include <chrono>
#include <thread>

#include <fmt/core.h>

#include "Conductor.h"

#include "Inspection/VPackWithErrorT.h"
#include "Logger/LogMacros.h"
#include "Pregel/Aggregator.h"
#include "Pregel/AlgoRegistry.h"
#include "Pregel/Algorithm.h"
#include "Pregel/Conductor/Messages.h"
#include "Pregel/MasterContext.h"
#include "Pregel/SpawnMessages.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/Status/ConductorStatus.h"
#include "Pregel/Status/Status.h"
#include "Pregel/Utils.h"
#include "Pregel/Worker/Messages.h"

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
#include "Pregel/Worker/Messages.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"
#include "velocypack/Builder.h"

#include <Inspection/VPack.h>
#include <velocypack/Iterator.h>
#include <velocypack/SharedSlice.h>

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::basics;

#define LOG_PREGEL(logId, level)          \
  LOG_TOPIC(logId, level, Logger::PREGEL) \
      << "[job " << _executionNumber.value << "] "

const char* arangodb::pregel::ExecutionStateNames[9] = {
    "none", "loading", "running", "storing", "done", "canceled", "fatal error"};

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
      _edgeCollections(edgeCollections),
      _edgeCollectionRestrictions(edgeCollectionRestrictions) {
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
      VelocyPackHelper::getNumericValue(config, Utils::maxGSS, _maxSuperstep);
  if (config.hasKey(Utils::maxNumIterations)) {
    // set to "infinity"
    _maxSuperstep = std::numeric_limits<uint64_t>::max();
  }
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
  _timing.loading.start();

  _globalSuperstep = 0;

  updateState(ExecutionState::LOADING);
  _feature.metrics()->pregelConductorsLoadingNumber->fetch_add(1);

  LOG_PREGEL("3a255", DEBUG) << "Telling workers to load the data";
  auto res = _initializeWorkers();
  if (res != TRI_ERROR_NO_ERROR) {
    updateState(ExecutionState::CANCELED);
    _feature.metrics()->pregelConductorsRunningNumber->fetch_sub(1);
    LOG_PREGEL("30171", ERR) << "Not all DBServers started the execution";
  }
}

// only called by the conductor, is protected by the
// mutex locked in finishedGlobalStep
bool Conductor::_startGlobalStep() {
  updateState(ExecutionState::RUNNING);
  if (_feature.isStopping()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }

  _callbackMutex.assertLockedByCurrentThread();

  /// collect the aggregators
  _aggregators->resetValues();
  _statistics.resetActiveCount();
  _totalVerticesCount = 0;  // might change during execution
  _totalEdgesCount = 0;

  auto prepareGss = PrepareGlobalSuperStep{.executionNumber = _executionNumber,
                                           .gss = _globalSuperstep,
                                           .vertexCount = _totalVerticesCount,
                                           .edgeCount = _totalEdgesCount};
  auto serialized = inspection::serializeWithErrorT(prepareGss);
  if (!serialized.ok()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        fmt::format("Cannot serialize PrepareGlobalSuperStep message: {}",
                    serialized.error().error()));
  }

  // we are explicitly expecting an response containing the aggregated
  // values as well as the count of active vertices
  auto prepareRes = _sendToAllDBServers(
      Utils::prepareGSSPath, VPackBuilder(serialized.get().slice()),
      [&](VPackSlice const& payload) {
        auto prepared =
            inspection::deserializeWithErrorT<GlobalSuperStepPrepared>(
                velocypack::SharedSlice({}, payload));
        if (!prepared.ok()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_INTERNAL,
              fmt::format(
                  "Cannot deserialize GlobalSuperStepPrepared message: {}",
                  prepared.error().error()));
        }
        _aggregators->aggregateValues(prepared.get().aggregators.slice());
        _statistics.accumulateActiveCounts(prepared.get().sender,
                                           prepared.get().activeCount);
        _totalVerticesCount += prepared.get().vertexCount;
        _totalEdgesCount += prepared.get().edgeCount;
      });

  if (prepareRes != TRI_ERROR_NO_ERROR) {
    updateState(ExecutionState::FATAL_ERROR);
    LOG_PREGEL("04189", ERR)
        << "Seems there is at least one worker out of order";
    return false;
  }

  // workers are done if all messages were processed and no active vertices
  // are left to process
  bool done = _globalSuperstep > 0 && _statistics.noActiveVertices() &&
              _statistics.allMessagesProcessed();
  bool proceed = true;
  if (_masterContext &&
      _globalSuperstep > 0) {  // ask algorithm to evaluate aggregated values
    _masterContext->_globalSuperstep = _globalSuperstep - 1;
    proceed = _masterContext->postGlobalSuperstep();
    if (!proceed) {
      LOG_PREGEL("0aa8e", DEBUG) << "Master context ended execution";
    }
  }

  // TODO make maximum configurable
  if (!proceed || done || _globalSuperstep >= _maxSuperstep) {
    // tells workers to store / discard results
    _timing.computation.finish();
    _feature.metrics()->pregelConductorsRunningNumber->fetch_sub(1);
    if (_storeResults) {
      updateState(ExecutionState::STORING);
      _feature.metrics()->pregelConductorsStoringNumber->fetch_add(1);
      _timing.storing.start();
      _finalizeWorkers();
    } else {  // just stop the timer
      updateState(ExecutionState::DONE);
      _timing.total.finish();
      LOG_PREGEL("9e82c", INFO)
          << "Done, execution took: " << _timing.total.elapsedSeconds().count()
          << " s";
    }
    return false;
  }

  if (_masterContext) {
    _masterContext->_globalSuperstep = _globalSuperstep;
    _masterContext->_vertexCount = _totalVerticesCount;
    _masterContext->_edgeCount = _totalEdgesCount;
    if (!_masterContext->preGlobalSuperstepWithResult()) {
      updateState(ExecutionState::FATAL_ERROR);
      return false;
    }
  }

  VPackBuilder agg;
  {
    VPackObjectBuilder ob(&agg);
    _aggregators->serializeValues(agg);
  }
  auto runGss = RunGlobalSuperStep{.executionNumber = _executionNumber,
                                   .gss = _globalSuperstep,
                                   .vertexCount = _totalVerticesCount,
                                   .edgeCount = _totalEdgesCount,
                                   .aggregators = agg};

  LOG_PREGEL("d98de", DEBUG) << fmt::format("Start gss: {}", runGss);
  _timing.gss.emplace_back(Duration{._start = std::chrono::steady_clock::now(),
                                    ._finish = std::nullopt});

  // start vertex level operations, does not get a response
  auto serializedRun = inspection::serializeWithErrorT(runGss);
  if (!serializedRun.ok()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        fmt::format("Cannot serialize RunGlobalSuperStep: {}",
                    serializedRun.error().error()));
  }
  auto startRes = _sendToAllDBServers(
      Utils::startGSSPath,
      VPackBuilder(serializedRun.get().slice()));  // call me maybe
  if (startRes != TRI_ERROR_NO_ERROR) {
    updateState(ExecutionState::FATAL_ERROR);
    LOG_PREGEL("f34bb", ERR)
        << "Conductor could not start GSS " << _globalSuperstep;
  } else {
    LOG_PREGEL("411a5", DEBUG)
        << "Conductor started new gss " << _globalSuperstep;
  }
  return startRes == TRI_ERROR_NO_ERROR;
}

// ============ Conductor callbacks ===============

// The worker can (and should) periodically call back
// to update its status
void Conductor::workerStatusUpdate(StatusUpdated&& update) {
  MUTEX_LOCKER(guard, _callbackMutex);

  LOG_PREGEL("76632", TRACE) << fmt::format("Update received {}", update);

  _status.updateWorkerStatus(update.sender, std::move(update.status));
}

void Conductor::finishedWorkerStartup(GraphLoaded const& graphLoaded) {
  MUTEX_LOCKER(guard, _callbackMutex);

  _ensureUniqueResponse(graphLoaded.sender);

  if (_state != ExecutionState::LOADING) {
    LOG_PREGEL("10f48", WARN)
        << "We are not in a state where we expect a response";
    return;
  }
  LOG_PREGEL("08142", WARN) << fmt::format(
      "finishedWorkerStartup, got response from {}.", graphLoaded.sender);

  _totalVerticesCount += graphLoaded.vertexCount;
  _totalEdgesCount += graphLoaded.edgeCount;
  if (_respondedServers.size() != _dbServers.size()) {
    return;
  }

  LOG_PREGEL("76631", INFO)
      << "Running Pregel " << _algorithm->name() << " with "
      << _totalVerticesCount << " vertices, " << _totalEdgesCount << " edges";
  if (_masterContext) {
    _masterContext->_globalSuperstep = 0;
    _masterContext->_vertexCount = _totalVerticesCount;
    _masterContext->_edgeCount = _totalEdgesCount;
    _masterContext->_aggregators = _aggregators.get();
    _masterContext->preApplication();
  }

  _timing.loading.finish();
  _timing.computation.start();

  _feature.metrics()->pregelConductorsLoadingNumber->fetch_sub(1);
  _feature.metrics()->pregelConductorsRunningNumber->fetch_add(1);
  _startGlobalStep();
}

/// Will optionally send a response, to notify the worker of converging
/// aggregator values
void Conductor::finishedWorkerStep(GlobalSuperStepFinished const& data) {
  MUTEX_LOCKER(guard, _callbackMutex);
  if (data.gss != _globalSuperstep || !(_state == ExecutionState::RUNNING ||
                                        _state == ExecutionState::CANCELED)) {
    LOG_PREGEL("dc904", WARN)
        << "Conductor did received a callback from the wrong superstep";
    return;
  }

  // track message counts to decide when to halt or add global barriers.
  // this will wait for a response from each worker
  _statistics.accumulateMessageStats(data.sender, data.messageStats);
  _ensureUniqueResponse(data.sender);
  LOG_PREGEL("faeb0", WARN)
      << fmt::format("finishedWorkerStep, got response from {}.", data.sender);
  // wait for the last worker to respond
  if (_respondedServers.size() != _dbServers.size()) {
    return;
  }

  _timing.gss.back().finish();
  LOG_PREGEL("39385", DEBUG)
      << "Finished gss " << _globalSuperstep << " in "
      << _timing.gss.back().elapsedSeconds().count() << "s";
  _globalSuperstep++;

  TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);
  Scheduler* scheduler = SchedulerFeature::SCHEDULER;
  // don't block the response for workers waiting on this callback
  // this should allow workers to go into the IDLE state
  scheduler->queue(RequestLane::INTERNAL_LOW, [this,
                                               self = shared_from_this()] {
    MUTEX_LOCKER(guard, _callbackMutex);

    if (_state == ExecutionState::RUNNING) {
      _startGlobalStep();  // trigger next superstep
    } else if (_state == ExecutionState::CANCELED) {
      LOG_PREGEL("dd721", WARN)
          << "Execution was canceled, results will be discarded.";
      _finalizeWorkers();  // tells workers to store / discard results
    } else {  // this prop shouldn't occur unless we are recovering or in error
      LOG_PREGEL("923db", WARN)
          << "No further action taken after receiving all responses";
    }
  });
  return;
}

void Conductor::cancel() {
  MUTEX_LOCKER(guard, _callbackMutex);
  cancelNoLock();
}

void Conductor::cancelNoLock() {
  _callbackMutex.assertLockedByCurrentThread();
  updateState(ExecutionState::CANCELED);
  bool ok = basics::function_utils::retryUntilTimeout(
      [this]() -> bool { return (_finalizeWorkers() != TRI_ERROR_QUEUE_FULL); },
      Logger::PREGEL, "cancel worker execution");
  if (!ok) {
    LOG_PREGEL("f8b3c", ERR)
        << "Failed to cancel worker execution for five minutes, giving up.";
  }
  _workHandle.reset();
}

// resolves into an ordered list of shards for each collection on each server
static void resolveInfo(
    TRI_vocbase_t* vocbase, CollectionID const& collectionID,
    std::unordered_map<CollectionID, std::string>& collectionPlanIdMap,
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
ErrorCode Conductor::_initializeWorkers() {
  _callbackMutex.assertLockedByCurrentThread();

  std::unordered_map<CollectionID, std::string> collectionPlanIdMap;
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

  _dbServers.clear();
  for (auto const& pair : vertexMap) {
    _dbServers.push_back(pair.first);
  }
  _status = ConductorStatus::forWorkers(_dbServers);
  // do not reload all shard id's, this list must stay in the same order
  if (_allShards.size() == 0) {
    _allShards = shardList;
  }

  std::string coordinatorId = ServerState::instance()->getId();
  auto const& nf =
      _vocbaseGuard.database().server().getFeature<NetworkFeature>();
  network::ConnectionPool* pool = nf.pool();
  std::vector<futures::Future<network::Response>> responses;

  for (auto const& [server, vertexShardMap] : vertexMap) {
    auto const& edgeShardMap = edgeMap[server];

    auto createWorker =
        CreateWorker{.executionNumber = _executionNumber,
                     .algorithm = _algorithm->name(),
                     .userParameters = _userParams,
                     .coordinatorId = coordinatorId,
                     .useMemoryMaps = _useMemoryMaps,
                     .edgeCollectionRestrictions = _edgeCollectionRestrictions,
                     .vertexShards = vertexShardMap,
                     .edgeShards = edgeShardMap,
                     .collectionPlanIds = collectionPlanIdMap,
                     .allShards = _allShards};

    // TODO should be done inside conductor actor (this whole function will be
    // moved into the conductor actor state)
    _feature.spawnActor(
        server,
        // TODO will be the pid of the conductor actor
        actor::ActorPID{.server = _feature._actorRuntime->myServerID,
                        .database = _vocbaseGuard.database().name(),
                        .id = {0}},
        SpawnMessages{SpawnWorker{}});

    // hack for single server
    if (ServerState::instance()->getRole() == ServerState::ROLE_SINGLE) {
      TRI_ASSERT(vertexMap.size() == 1);
      if (_feature.isStopping()) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
      }
      std::shared_ptr<IWorker> worker = _feature.worker(_executionNumber);

      if (worker) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL,
            "a worker with this execution number already exists.");
      }

      auto created = AlgoRegistry::createWorker(_vocbaseGuard.database(),
                                                createWorker, _feature);

      TRI_ASSERT(created.get() != nullptr);
      _feature.addWorker(std::move(created), _executionNumber);
      worker = _feature.worker(_executionNumber);
      TRI_ASSERT(worker);
      worker->setupWorker();

      return TRI_ERROR_NO_ERROR;
    } else {
      network::RequestOptions reqOpts;
      reqOpts.timeout = network::Timeout(5.0 * 60.0);
      reqOpts.database = _vocbaseGuard.database().name();
      std::string const path =
          Utils::baseUrl(Utils::workerPrefix) + Utils::startExecutionPath;

      auto serialized = inspection::serializeWithErrorT(createWorker);
      if (!serialized.ok()) {
        return TRI_ERROR_FAILED;
      }
      VPackBuilder v;
      v.add(serialized.get());
      responses.emplace_back(network::sendRequestRetry(
          pool, "server:" + server, fuerte::RestVerb::Post, path,
          std::move(v.bufferRef()), reqOpts));

      LOG_PREGEL("6ae66", DEBUG) << "Initializing Server " << server;
    }
  }

  size_t nrGood = 0;
  futures::collectAll(responses)
      .thenValue([&nrGood, this](auto const& results) {
        for (auto const& tryRes : results) {
          network::Response const& r =
              tryRes.get();  // throws exceptions upwards
          if (r.ok() && r.statusCode() < 400) {
            nrGood++;
          } else {
            LOG_PREGEL("6ae67", ERR)
                << "received error from worker: '"
                << (r.ok() ? r.slice().toJson() : fuerte::to_string(r.error))
                << "'";
          }
        }
      })
      .wait();

  return nrGood == responses.size() ? TRI_ERROR_NO_ERROR : TRI_ERROR_FAILED;
}

ErrorCode Conductor::_finalizeWorkers() {
  _callbackMutex.assertLockedByCurrentThread();

  bool store = _state == ExecutionState::STORING;
  if (_masterContext) {
    _masterContext->postApplication();
  }

  LOG_PREGEL("fc187", DEBUG) << "Finalizing workers";
  auto finalize =
      FinalizeExecution{.executionNumber = _executionNumber, .store = store};
  auto serialized = inspection::serializeWithErrorT(finalize);
  if (!serialized.ok()) {
    return TRI_ERROR_FAILED;
  }
  return _sendToAllDBServers(Utils::finalizeExecutionPath,
                             VPackBuilder(serialized.get().slice()));
}

void Conductor::finishedWorkerFinalize(Finished const& data) {
  MUTEX_LOCKER(guard, _callbackMutex);

  LOG_PREGEL("60f0c", WARN) << fmt::format(
      "finishedWorkerFinalize, got response from {}.", data.sender);

  _ensureUniqueResponse(data.sender);

  if (_respondedServers.size() != _dbServers.size()) {
    return;
  }

  // do not swap an error state to done
  bool didStore = false;
  if (_state == ExecutionState::STORING) {
    updateState(ExecutionState::DONE);
    didStore = true;
    _timing.storing.finish();
    _feature.metrics()->pregelConductorsStoringNumber->fetch_sub(1);
    _timing.total.finish();
  }

  VPackBuilder debugOut;
  debugOut.openObject();
  debugOut.add("stats", VPackValue(VPackValueType::Object));
  _statistics.serializeValues(debugOut);
  debugOut.close();
  _aggregators->serializeValues(debugOut);
  debugOut.close();

  LOG_PREGEL("063b5", INFO)
      << "Done. We did " << _globalSuperstep << " rounds."
      << (_timing.loading.hasStarted()
              ? fmt::format("Startup time: {}s",
                            _timing.loading.elapsedSeconds().count())
              : "")
      << (_timing.computation.hasStarted()
              ? fmt::format(", computation time: {}s",
                            _timing.computation.elapsedSeconds().count())
              : "")
      << (didStore ? fmt::format(", storage time: {}s",
                                 _timing.storing.elapsedSeconds().count())
                   : "")
      << ", overall: " << _timing.total.elapsedSeconds().count() << "s"
      << ", stats: " << debugOut.slice().toJson();

  // always try to cleanup
  if (_state == ExecutionState::CANCELED) {
    auto* scheduler = SchedulerFeature::SCHEDULER;
    if (scheduler) {
      auto exe = _executionNumber;
      scheduler->queue(RequestLane::CLUSTER_AQL,
                       [this, exe, self = shared_from_this()] {
                         _feature.cleanupConductor(exe);
                       });
    }
  }
}

bool Conductor::canBeGarbageCollected() const {
  // we don't want to block other operations for longer, so if we can't
  // immediately acuqire the mutex here, we assume a conductor cannot be
  // garbage-collected. the same conductor will be probed later anyway, so we
  // should be fine
  TRY_MUTEX_LOCKER(guard, _callbackMutex);

  if (guard.isLocked()) {
    if (_state == ExecutionState::CANCELED || _state == ExecutionState::DONE ||
        _state == ExecutionState::FATAL_ERROR ||
        _state == ExecutionState::FATAL_ERROR) {
      return (_expires != std::chrono::system_clock::time_point{} &&
              _expires <= std::chrono::system_clock::now());
    }
  }

  return false;
}

void Conductor::collectAQLResults(VPackBuilder& outBuilder, bool withId) {
  MUTEX_LOCKER(guard, _callbackMutex);

  if (_state != ExecutionState::DONE && _state != ExecutionState::FATAL_ERROR) {
    return;
  }

  if (_storeResults) {
    return;
  }

  auto collectResults = CollectPregelResults{
      .executionNumber = _executionNumber, .withId = withId};
  auto serialized = inspection::serializeWithErrorT(collectResults);
  if (!serialized.ok()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_FAILED,
        fmt::format("Cannot serialize CollectPregelResults message: {}",
                    serialized.error().error()));
  }
  // merge results from DBServers
  outBuilder.openArray();
  auto res = _sendToAllDBServers(
      Utils::aqlResultsPath, VPackBuilder(serialized.get().slice()),
      [&](VPackSlice const& payload) {
        auto results = inspection::deserializeWithErrorT<PregelResults>(
            velocypack::SharedSlice({}, payload));
        if (!results.ok()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_FAILED,
              fmt::format("Cannot deserialize PregelResults message: {}",
                          results.error().error()));
        }
        outBuilder.add(VPackArrayIterator(results.get().results.slice()));
      });
  outBuilder.close();
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
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
  if (_expires != std::chrono::system_clock::time_point{}) {
    result.add("expires", VPackValue(timepointToString(_expires)));
  }
  result.add("ttl", VPackValue(_ttl.count()));
  result.add("state", VPackValue(pregel::ExecutionStateNames[_state]));
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
  if (_state != ExecutionState::RUNNING || ExecutionState::LOADING) {
    result.add("vertexCount", VPackValue(_totalVerticesCount));
    result.add("edgeCount", VPackValue(_totalEdgesCount));
  }
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

ErrorCode Conductor::_sendToAllDBServers(std::string const& path,
                                         VPackBuilder const& message) {
  return _sendToAllDBServers(path, message, std::function<void(VPackSlice)>());
}

ErrorCode Conductor::_sendToAllDBServers(
    std::string const& path, VPackBuilder const& message,
    std::function<void(VPackSlice)> handle) {
  _callbackMutex.assertLockedByCurrentThread();
  _respondedServers.clear();

  // to support the single server case, we handle it without optimizing it
  if (!ServerState::instance()->isRunningInCluster()) {
    if (handle) {
      VPackBuilder response;
      _feature.handleWorkerRequest(_vocbaseGuard.database(), path,
                                   message.slice(), response);
      handle(response.slice());
    } else {
      TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);
      Scheduler* scheduler = SchedulerFeature::SCHEDULER;
      scheduler->queue(RequestLane::INTERNAL_LOW, [this, path, message,
                                                   self = shared_from_this()] {
        TRI_vocbase_t& vocbase = _vocbaseGuard.database();
        VPackBuilder response;
        _feature.handleWorkerRequest(vocbase, path, message.slice(), response);
      });
    }
    return TRI_ERROR_NO_ERROR;
  }

  if (_dbServers.empty()) {
    LOG_PREGEL("a14fa", WARN) << "No servers registered";
    return TRI_ERROR_FAILED;
  }

  std::string base = Utils::baseUrl(Utils::workerPrefix);

  VPackBuffer<uint8_t> buffer;
  buffer.append(message.slice().begin(), message.slice().byteSize());

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
        pool, "server:" + server, fuerte::RestVerb::Post, base + path, buffer,
        reqOpts));
  }

  size_t nrGood = 0;

  futures::collectAll(responses)
      .thenValue([&](auto results) {
        for (auto const& tryRes : results) {
          network::Response const& res =
              tryRes.get();  // throws exceptions upwards
          if (res.ok() && res.statusCode() < 400) {
            nrGood++;
            if (handle) {
              handle(res.slice());
            }
          }
        }
      })
      .wait();

  return nrGood == responses.size() ? TRI_ERROR_NO_ERROR : TRI_ERROR_FAILED;
}

void Conductor::_ensureUniqueResponse(std::string const& sender) {
  _callbackMutex.assertLockedByCurrentThread();

  // check if this the only time we received this
  if (_respondedServers.find(sender) != _respondedServers.end()) {
    LOG_PREGEL("c38b8", ERR) << "Received response already from " << sender;
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_CONFLICT);
  }
  _respondedServers.insert(sender);
}

void Conductor::updateState(ExecutionState state) {
  _state = state;
  if (_state == ExecutionState::CANCELED || _state == ExecutionState::DONE ||
      _state == ExecutionState::FATAL_ERROR) {
    _expires = std::chrono::system_clock::now() + _ttl;
  }
}
