////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#include "Conductor.h"

#include "Pregel/Aggregator.h"
#include "Pregel/AlgoRegistry.h"
#include "Pregel/Algorithm.h"
#include "Pregel/MasterContext.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/Recovery.h"
#include "Pregel/Utils.h"

#include "Basics/FunctionUtils.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Futures/Utilities.h"
#include "Network/NetworkFeature.h"
#include "Network/Methods.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::basics;

const char* arangodb::pregel::ExecutionStateNames[7] = {
    "none", "running", "storing", "done", "canceled", "in error", "recovering"};

Conductor::Conductor(uint64_t executionNumber, TRI_vocbase_t& vocbase,
                     std::vector<CollectionID> const& vertexCollections,
                     std::vector<CollectionID> const& edgeCollections,
                     std::unordered_map<std::string, std::vector<std::string>> const& edgeCollectionRestrictions,
                     std::string const& algoName, VPackSlice const& config)
    : _vocbaseGuard(vocbase),
      _executionNumber(executionNumber),
      _algorithm(AlgoRegistry::createAlgorithm(vocbase.server(), algoName, config)),
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
  _aggregators.reset(new AggregatorHandler(_algorithm.get()));

  _maxSuperstep = VelocyPackHelper::getNumericValue(config, "maxGSS", _maxSuperstep);
  // configure the async mode as off by default
  VPackSlice async = _userParams.slice().get("async");
  _asyncMode = _algorithm->supportsAsyncMode() && async.isBool() && async.getBoolean();
  if (_asyncMode) {
    LOG_TOPIC("1b1c2", DEBUG, Logger::PREGEL) << "Running in async mode";
  }
  VPackSlice lazy = _userParams.slice().get(Utils::lazyLoadingKey);
  _lazyLoading = _algorithm->supportsLazyLoading();
  _lazyLoading = _lazyLoading && (lazy.isNone() || lazy.getBoolean());
  if (_lazyLoading) {
    LOG_TOPIC("464dd", DEBUG, Logger::PREGEL) << "Enabled lazy loading";
  }
  _useMemoryMaps = VelocyPackHelper::getBooleanValue(_userParams.slice(),
                                                      Utils::useMemoryMapsKey, _useMemoryMaps);
  VPackSlice storeSlice = config.get("store");
  _storeResults = !storeSlice.isBool() || storeSlice.getBool();
  if (!_storeResults) {
    LOG_TOPIC("f3817", DEBUG, Logger::PREGEL) << "Will keep results in-memory";
  }
}

Conductor::~Conductor() {
  if (_state != ExecutionState::CANCELED && _state != ExecutionState::DEFAULT) {
    try {
      this->cancel();
    } catch (...) {
      // must not throw exception from here
    }
  }
}

void Conductor::start() {
  MUTEX_LOCKER(guard, _callbackMutex);
  _callbackMutex.assertLockedByCurrentThread();
  _startTimeSecs = TRI_microtime();

  _computationStartTimeSecs = _startTimeSecs;
  _finalizationStartTimeSecs = _startTimeSecs;
  _endTimeSecs = _startTimeSecs;

  _globalSuperstep = 0;
  _state = ExecutionState::RUNNING;

  LOG_TOPIC("3a255", DEBUG, Logger::PREGEL)
      << "Telling workers to load the data";
  int res = _initializeWorkers(Utils::startExecutionPath, VPackSlice());
  if (res != TRI_ERROR_NO_ERROR) {
    _state = ExecutionState::CANCELED;
    LOG_TOPIC("30171", ERR, Logger::PREGEL)
        << "Not all DBServers started the execution";
  }
}

// only called by the conductor, is protected by the
// mutex locked in finishedGlobalStep
bool Conductor::_startGlobalStep() {
  _callbackMutex.assertLockedByCurrentThread();
  // send prepare GSS notice
  VPackBuilder b;
  b.openObject();
  b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
  b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
  b.add(Utils::vertexCountKey, VPackValue(_totalVerticesCount));
  b.add(Utils::edgeCountKey, VPackValue(_totalEdgesCount));
  b.close();

  /// collect the aggregators
  _aggregators->resetValues();
  _statistics.resetActiveCount();
  _totalVerticesCount = 0;  // might change during execution
  _totalEdgesCount = 0;
  // we are explicitly expecting an response containing the aggregated
  // values as well as the count of active vertices
  int res = _sendToAllDBServers(Utils::prepareGSSPath, b, [&](VPackSlice const& payload) {
    _aggregators->aggregateValues(payload);
    _statistics.accumulateActiveCounts(payload);
    _totalVerticesCount += payload.get(Utils::vertexCountKey).getUInt();
    _totalEdgesCount += payload.get(Utils::edgeCountKey).getUInt();
  });
  if (res != TRI_ERROR_NO_ERROR) {
    _state = ExecutionState::IN_ERROR;
    LOG_TOPIC("04189", ERR, Logger::PREGEL)
        << "Seems there is at least one worker out of order";
    // the recovery mechanisms should take care of this
    return false;
  }

  // workers are done if all messages were processed and no active vertices
  // are left to process
  bool proceed = true;
  if (_masterContext && _globalSuperstep > 0) {  // ask algorithm to evaluate aggregated values
    _masterContext->_globalSuperstep = _globalSuperstep - 1;
    _masterContext->_enterNextGSS = false;
    proceed = _masterContext->postGlobalSuperstep();
    if (!proceed) {
      LOG_TOPIC("0aa8e", DEBUG, Logger::PREGEL)
          << "Master context ended execution";
    }
  }

  // TODO make maximum configurable
  bool done = _globalSuperstep > 0 && _statistics.noActiveVertices() &&
              _statistics.allMessagesProcessed();
  if (!proceed || done || _globalSuperstep >= _maxSuperstep) {
    // tells workers to store / discard results
    if (_storeResults) {
      _state = ExecutionState::STORING;
      _finalizeWorkers();
    } else {  // just stop the timer
      _state = ExecutionState::DONE;
      _endTimeSecs = TRI_microtime();
      LOG_TOPIC("9e82c", INFO, Logger::PREGEL)
          << "Done execution took" << totalRuntimeSecs() << " s";
    }
    return false;
  }
  if (_masterContext) {
    _masterContext->_globalSuperstep = _globalSuperstep;
    _masterContext->_vertexCount = _totalVerticesCount;
    _masterContext->_edgeCount = _totalEdgesCount;
    _masterContext->preGlobalSuperstep();
  }

  b.clear();
  b.openObject();
  b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
  b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
  b.add(Utils::vertexCountKey, VPackValue(_totalVerticesCount));
  b.add(Utils::edgeCountKey, VPackValue(_totalEdgesCount));
  _aggregators->serializeValues(b);
  b.close();
  LOG_TOPIC("d98de", DEBUG, Logger::PREGEL) << b.toString();

  _stepStartTimeSecs = TRI_microtime();

  // start vertex level operations, does not get a response
  res = _sendToAllDBServers(Utils::startGSSPath, b);  // call me maybe
  if (res != TRI_ERROR_NO_ERROR) {
    _state = ExecutionState::IN_ERROR;
    LOG_TOPIC("f34bb", ERR, Logger::PREGEL)
        << "Conductor could not start GSS " << _globalSuperstep;
    // the recovery mechanisms should take care od this
  } else {
    LOG_TOPIC("411a5", DEBUG, Logger::PREGEL) << "Conductor started new gss " << _globalSuperstep;
  }
  return res == TRI_ERROR_NO_ERROR;
}

// ============ Conductor callbacks ===============
void Conductor::finishedWorkerStartup(VPackSlice const& data) {
  MUTEX_LOCKER(guard, _callbackMutex);
  _ensureUniqueResponse(data);
  if (_state != ExecutionState::RUNNING) {
    LOG_TOPIC("10f48", WARN, Logger::PREGEL)
        << "We are not in a state where we expect a response";
    return;
  }

  _totalVerticesCount += data.get(Utils::vertexCountKey).getUInt();
  _totalEdgesCount += data.get(Utils::edgeCountKey).getUInt();
  if (_respondedServers.size() != _dbServers.size()) {
    return;
  }

  LOG_TOPIC("76631", INFO, Logger::PREGEL)
      << "Running pregel with " << _totalVerticesCount << " vertices, "
      << _totalEdgesCount << " edges";
  if (_masterContext) {
    _masterContext->_globalSuperstep = 0;
    _masterContext->_vertexCount = _totalVerticesCount;
    _masterContext->_edgeCount = _totalEdgesCount;
    _masterContext->_aggregators = _aggregators.get();
    _masterContext->preApplication();
  }

  _computationStartTimeSecs = TRI_microtime();
  _startGlobalStep();
}

/// Will optionally send a response, to notify the worker of converging
/// aggregator
/// values which can be coninually updated (in async mode)
VPackBuilder Conductor::finishedWorkerStep(VPackSlice const& data) {
  MUTEX_LOCKER(guard, _callbackMutex);
  // this method can be called multiple times in a superstep depending on
  // whether we are in the async mode
  uint64_t gss = data.get(Utils::globalSuperstepKey).getUInt();
  if (gss != _globalSuperstep ||
      !(_state == ExecutionState::RUNNING || _state == ExecutionState::CANCELED)) {
    LOG_TOPIC("dc904", WARN, Logger::PREGEL)
        << "Conductor did received a callback from the wrong superstep";
    return VPackBuilder();
  }

  // track message counts to decide when to halt or add global barriers.
  // In normal mode this will wait for a response from each worker,
  // in async mode this will wait until all messages were processed
  _statistics.accumulateMessageStats(data);
  if (_asyncMode == false) {  // in async mode we wait for all responded
    _ensureUniqueResponse(data);
    // wait for the last worker to respond
    if (_respondedServers.size() != _dbServers.size()) {
      return VPackBuilder();
    }
  } else if (_statistics.clientCount() < _dbServers.size() ||  // no messages
             !_statistics.allMessagesProcessed()) {  // haven't received msgs
    VPackBuilder response;
    _aggregators->aggregateValues(data);
    if (_masterContext) {
      _masterContext->postLocalSuperstep();
    }
    response.openObject();
    _aggregators->serializeValues(response);
    if (_masterContext && _masterContext->_enterNextGSS) {
      response.add(Utils::enterNextGSSKey, VPackValue(true));
    }
    response.close();
    return response;
  }

  LOG_TOPIC("39385", DEBUG, Logger::PREGEL)
      << "Finished gss " << _globalSuperstep << " in "
      << (TRI_microtime() - _stepStartTimeSecs) << "s";
  //_statistics.debugOutput();
  _globalSuperstep++;

  TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);
  Scheduler* scheduler = SchedulerFeature::SCHEDULER;
  // don't block the response for workers waiting on this callback
  // this should allow workers to go into the IDLE state
  bool queued = scheduler->queue(RequestLane::INTERNAL_LOW, [this] {
    MUTEX_LOCKER(guard, _callbackMutex);

    if (_state == ExecutionState::RUNNING) {
      _startGlobalStep();  // trigger next superstep
    } else if (_state == ExecutionState::CANCELED) {
      LOG_TOPIC("dd721", WARN, Logger::PREGEL)
          << "Execution was canceled, results will be discarded.";
      _finalizeWorkers();  // tells workers to store / discard results
    } else {  // this prop shouldn't occur unless we are recovering or in error
      LOG_TOPIC("923db", WARN, Logger::PREGEL)
          << "No further action taken after receiving all responses";
    }
  });
  if (!queued) {
    LOG_TOPIC("038db", ERR, Logger::PREGEL)
        << "No thread available to queue response, canceling execution";
    cancel();
  }
  return VPackBuilder();
}

void Conductor::finishedRecoveryStep(VPackSlice const& data) {
  MUTEX_LOCKER(guard, _callbackMutex);
  _ensureUniqueResponse(data);
  if (_state != ExecutionState::RECOVERING) {
    LOG_TOPIC("23d8b", WARN, Logger::PREGEL)
        << "We are not in a state where we expect a recovery response";
    return;
  }

  // the recovery mechanism might be gathering state information
  _aggregators->aggregateValues(data);
  if (_respondedServers.size() != _dbServers.size()) {
    return;
  }

  // only compensations supported
  bool proceed = false;
  if (_masterContext) {
    proceed = proceed || _masterContext->postCompensation();
  }

  int res = TRI_ERROR_NO_ERROR;
  if (proceed) {
    // reset values which are calculated during the superstep
    _aggregators->resetValues();
    if (_masterContext) {
      _masterContext->preCompensation();
    }

    VPackBuilder b;
    b.openObject();
    b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
    _aggregators->serializeValues(b);
    b.close();
    // first allow all workers to run worker level operations
    res = _sendToAllDBServers(Utils::continueRecoveryPath, b);

  } else {
    LOG_TOPIC("6ecf2", INFO, Logger::PREGEL)
        << "Recovery finished. Proceeding normally";

    // build the message, works for all cases
    VPackBuilder b;
    b.openObject();
    b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
    b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
    b.close();
    res = _sendToAllDBServers(Utils::finalizeRecoveryPath, b);
    if (res == TRI_ERROR_NO_ERROR) {
      _state = ExecutionState::RUNNING;
      _startGlobalStep();
    }
  }
  if (res != TRI_ERROR_NO_ERROR) {
    cancelNoLock();
    LOG_TOPIC("7f97e", INFO, Logger::PREGEL) << "Recovery failed";
  }
}

void Conductor::cancel() {
  MUTEX_LOCKER(guard, _callbackMutex);
  cancelNoLock();
}

void Conductor::cancelNoLock() {
  _callbackMutex.assertLockedByCurrentThread();
  _state = ExecutionState::CANCELED;
  bool ok = basics::function_utils::retryUntilTimeout(
      [this]() -> bool { 
        return (_finalizeWorkers() != TRI_ERROR_QUEUE_FULL); 
      }, Logger::PREGEL, "cancel worker execution");
  if (!ok) {
    LOG_TOPIC("f8b3c", ERR, Logger::PREGEL)
        << "Failed to cancel worker execution for five minutes, giving up.";
  }
  _workHandle.reset();
}

void Conductor::startRecovery() {
  MUTEX_LOCKER(guard, _callbackMutex);
  if (_state != ExecutionState::RUNNING && _state != ExecutionState::IN_ERROR) {
    return;  // maybe we are already in recovery mode
  } else if (_algorithm->supportsCompensation() == false) {
    LOG_TOPIC("12e0e", ERR, Logger::PREGEL)
        << "Algorithm does not support recovery";
    cancelNoLock();
    return;
  }

  // we lost a DBServer, we need to reconfigure all remainging servers
  // so they load the data for the lost machine
  _state = ExecutionState::RECOVERING;
  _statistics.reset();

  TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);

  // let's wait for a final state in the cluster
  bool queued = false;
  std::tie(queued, _workHandle) = SchedulerFeature::SCHEDULER->queueDelay(
      RequestLane::CLUSTER_AQL, std::chrono::seconds(2), [this](bool cancelled) {
        if (cancelled || _state != ExecutionState::RECOVERING) {
          return;  // seems like we are canceled
        }
        std::vector<ServerID> goodServers;
        int res = PregelFeature::instance()->recoveryManager()->filterGoodServers(_dbServers, goodServers);
        if (res != TRI_ERROR_NO_ERROR) {
          LOG_TOPIC("3d08b", ERR, Logger::PREGEL)
              << "Recovery proceedings failed";
          cancelNoLock();
          return;
        }
        _dbServers = goodServers;

        VPackBuilder b;
        b.openObject();
        b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
        b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
        b.close();
        _sendToAllDBServers(Utils::cancelGSSPath, b);
        if (_state != ExecutionState::RECOVERING) {
          return;  // seems like we are canceled
        }

        // Let's try recovery
        if (_masterContext) {
          bool proceed = _masterContext->preCompensation();
          if (!proceed) {
            cancelNoLock();
          }
        }

        VPackBuilder additionalKeys;
        additionalKeys.openObject();
        additionalKeys.add(Utils::recoveryMethodKey, VPackValue(Utils::compensate));
        _aggregators->serializeValues(b);
        additionalKeys.close();
        _aggregators->resetValues();

        // initialize workers will reconfigure the workers and set the
        // _dbServers list to the new primary DBServers
        res = _initializeWorkers(Utils::startRecoveryPath, additionalKeys.slice());
        if (res != TRI_ERROR_NO_ERROR) {
          cancelNoLock();
          LOG_TOPIC("fefc6", ERR, Logger::PREGEL) << "Compensation failed";
        }
      });
  if (!queued) {
    LOG_TOPIC("92a8d", ERR, Logger::PREGEL)
        << "No thread available to queue recovery, may be in dirty state.";
  }
}

// resolves into an ordered list of shards for each collection on each server
static void resolveInfo(TRI_vocbase_t* vocbase, CollectionID const& collectionID,
                        std::map<CollectionID, std::string>& collectionPlanIdMap,
                        std::map<ServerID, std::map<CollectionID, std::vector<ShardID>>>& serverMap,
                        std::vector<ShardID>& allShards) {
  ServerState* ss = ServerState::instance();
  if (!ss->isRunningInCluster()) {  // single server mode
    auto lc = vocbase->lookupCollection(collectionID);

    if (lc == nullptr || lc->deleted()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, collectionID);
    }

    collectionPlanIdMap.try_emplace(collectionID, std::to_string(lc->planId()));
    allShards.push_back(collectionID);
    serverMap[ss->getId()][collectionID].push_back(collectionID);

  } else if (ss->isCoordinator()) {  // we are in the cluster

    ClusterInfo& ci = vocbase->server().getFeature<ClusterFeature>().clusterInfo();
    std::shared_ptr<LogicalCollection> lc = ci.getCollection(vocbase->name(), collectionID);
    if (lc->deleted()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, collectionID);
    }
    collectionPlanIdMap.try_emplace(collectionID, std::to_string(lc->planId()));

    std::shared_ptr<std::vector<ShardID>> shardIDs =
        ci.getShardList(std::to_string(lc->id()));
    allShards.insert(allShards.end(), shardIDs->begin(), shardIDs->end());

    for (auto const& shard : *shardIDs) {
      std::shared_ptr<std::vector<ServerID>> servers = ci.getResponsibleServer(shard);
      if (servers->size() > 0) {
        serverMap[(*servers)[0]][lc->name()].push_back(shard);
      }
    }
  } else {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_ONLY_ON_COORDINATOR);
  }
}

/// should cause workers to start a new execution or begin with recovery
/// proceedings
int Conductor::_initializeWorkers(std::string const& suffix, VPackSlice additional) {
  _callbackMutex.assertLockedByCurrentThread();

  std::string const path = Utils::baseUrl(Utils::workerPrefix) + suffix;

  // int64_t vertexCount = 0, edgeCount = 0;
  std::map<CollectionID, std::string> collectionPlanIdMap;
  std::map<ServerID, std::map<CollectionID, std::vector<ShardID>>> vertexMap, edgeMap;
  std::vector<ShardID> shardList;

  // resolve plan id's and shards on the servers
  for (CollectionID const& collectionID : _vertexCollections) {
    resolveInfo(&(_vocbaseGuard.database()), collectionID, collectionPlanIdMap, vertexMap,
                shardList);  // store or
  }
  for (CollectionID const& collectionID : _edgeCollections) {
    resolveInfo(&(_vocbaseGuard.database()), collectionID, collectionPlanIdMap, edgeMap,
                shardList);  // store or
  }

  _dbServers.clear();
  for (auto const& pair : vertexMap) {
    _dbServers.push_back(pair.first);
  }
  // do not reload all shard id's, this list must stay in the same order
  if (_allShards.size() == 0) {
    _allShards = shardList;
  }

  std::string coordinatorId = ServerState::instance()->getId();
  auto const& nf = _vocbaseGuard.database().server().getFeature<NetworkFeature>();
  network::ConnectionPool* pool = nf.pool();
  std::vector<futures::Future<network::Response>> responses;

  for (auto const& it : vertexMap) {
    ServerID const& server = it.first;
    std::map<CollectionID, std::vector<ShardID>> const& vertexShardMap = it.second;
    std::map<CollectionID, std::vector<ShardID>> const& edgeShardMap = edgeMap[it.first];

    VPackBuffer<uint8_t> buffer;
    VPackBuilder b(buffer);
    b.openObject();
    b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
    b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
    b.add(Utils::algorithmKey, VPackValue(_algorithm->name()));
    b.add(Utils::userParametersKey, _userParams.slice());
    b.add(Utils::coordinatorIdKey, VPackValue(coordinatorId));
    b.add(Utils::asyncModeKey, VPackValue(_asyncMode));
    b.add(Utils::lazyLoadingKey, VPackValue(_lazyLoading));
    b.add(Utils::useMemoryMapsKey, VPackValue(_useMemoryMaps));
    if (additional.isObject()) {
      for (auto pair : VPackObjectIterator(additional)) {
        b.add(pair.key.copyString(), pair.value);
      }
    }
    
    // edge collection restrictions
    b.add(Utils::edgeCollectionRestrictionsKey, VPackValue(VPackValueType::Object));
    for (auto const& pair : _edgeCollectionRestrictions) {
      b.add(pair.first, VPackValue(VPackValueType::Array));
      for (ShardID const& shard: pair.second) {
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
  
    // hack for single server
    if (ServerState::instance()->getRole() == ServerState::ROLE_SINGLE) {
      TRI_ASSERT(vertexMap.size() == 1);
      std::shared_ptr<PregelFeature> feature = PregelFeature::instance();
      if (!feature) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
      }
      std::shared_ptr<IWorker> worker = feature->worker(_executionNumber);

      if (worker) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL,
            "a worker with this execution number already exists.");
      }

      auto created = AlgoRegistry::createWorker(_vocbaseGuard.database(), b.slice());

      TRI_ASSERT(created.get() != nullptr);
      feature->addWorker(std::move(created), _executionNumber);
      worker = feature->worker(_executionNumber);
      TRI_ASSERT(worker);
      worker->setupWorker();

      return TRI_ERROR_NO_ERROR;
    } else {
      
      network::RequestOptions reqOpts;
      reqOpts.timeout = network::Timeout(5.0 * 60.0);
      reqOpts.database = _vocbaseGuard.database().name();
      
      responses.emplace_back(network::sendRequest(pool, "server:" + server, fuerte::RestVerb::Post,
                                                  path, std::move(buffer), reqOpts));
      
      LOG_TOPIC("6ae66", DEBUG, Logger::PREGEL) << "Initializing Server " << server;
    }
  }
  
  size_t nrGood = 0;
  futures::collectAll(responses).thenValue([&nrGood](auto const& results) {
    for (auto const& tryRes : results) {
      network::Response const& r = tryRes.get();  // throws exceptions upwards
      if (r.ok() && r.response->statusCode() < 400) {
        nrGood++;
      } else {
        LOG_TOPIC("6ae67", ERR, Logger::PREGEL) << "received error from worker: '"
          << (r.ok() ? r.slice().toJson() : fuerte::to_string(r.error)) << "'";
      }
    }
  }).wait();
  
  return nrGood == responses.size() ? TRI_ERROR_NO_ERROR : TRI_ERROR_FAILED;
}

int Conductor::_finalizeWorkers() {
  _callbackMutex.assertLockedByCurrentThread();
  _finalizationStartTimeSecs = TRI_microtime();

  bool store = _state == ExecutionState::STORING;
  if (_masterContext) {
    _masterContext->postApplication();
  }

  std::shared_ptr<PregelFeature> feature = PregelFeature::instance();
  if (!feature) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }
  // stop monitoring shards
  RecoveryManager* mngr = feature->recoveryManager();
  if (mngr) {
    mngr->stopMonitoring(this);
  }

  LOG_TOPIC("fc187", DEBUG, Logger::PREGEL) << "Finalizing workers";
  VPackBuilder b;
  b.openObject();
  b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
  b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
  b.add(Utils::storeResultsKey, VPackValue(store));
  b.close();
  return _sendToAllDBServers(Utils::finalizeExecutionPath, b);
}

void Conductor::finishedWorkerFinalize(VPackSlice data) {
  MUTEX_LOCKER(guard, _callbackMutex);
  _ensureUniqueResponse(data);
  if (_respondedServers.size() != _dbServers.size()) {
    return;
  }

  // do not swap an error state to done
  bool didStore = false;
  if (_state == ExecutionState::STORING) {
    _state = ExecutionState::DONE;
    didStore = true;
  }
  _endTimeSecs = TRI_microtime();  // offically done

  VPackBuilder debugOut;
  debugOut.openObject();
  debugOut.add("stats", VPackValue(VPackValueType::Object));
  _statistics.serializeValues(debugOut);
  debugOut.close();
  _aggregators->serializeValues(debugOut);
  debugOut.close();

  double compTime = _finalizationStartTimeSecs - _computationStartTimeSecs;
  TRI_ASSERT(compTime >= 0);
  if (didStore) {
    _storeTimeSecs = TRI_microtime() - _finalizationStartTimeSecs;
  }

  LOG_TOPIC("063b5", INFO, Logger::PREGEL) << "Done. We did " << _globalSuperstep << " rounds";
  LOG_TOPIC("3cfa8", INFO, Logger::PREGEL)
      << "Startup Time: " << _computationStartTimeSecs - _startTimeSecs << "s";
  LOG_TOPIC("d43cb", INFO, Logger::PREGEL) << "Computation time: " << compTime << "s";
  LOG_TOPIC_IF("74e05", INFO, Logger::PREGEL, didStore) << "Storage time: " << _storeTimeSecs << "s";
  LOG_TOPIC("06f03", INFO, Logger::PREGEL) << "Overall: " << totalRuntimeSecs() << "s";
  LOG_TOPIC("03f2e", DEBUG, Logger::PREGEL) << "Stats: " << debugOut.toString();

  // always try to cleanup
  if (_state == ExecutionState::CANCELED) {
    auto* scheduler = SchedulerFeature::SCHEDULER;
    if (scheduler) {
      uint64_t exe = _executionNumber;
      bool queued = scheduler->queue(RequestLane::CLUSTER_AQL, [exe] {
        auto pf = PregelFeature::instance();
        if (pf) {
          pf->cleanupConductor(exe);
        }
      });
      if (!queued) {
        LOG_TOPIC("038da", ERR, Logger::PREGEL)
            << "No thread available to queue cleanup, canceling execution";
        cancel();
      }
    }
  }
}

void Conductor::collectAQLResults(VPackBuilder& outBuilder, bool withId) {
  MUTEX_LOCKER(guard, _callbackMutex);

  if (_state != ExecutionState::DONE) {
    return;
  }

  VPackBuilder b;
  b.openObject();
  b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
  b.add("withId", VPackValue(withId));
  b.close();

  // merge results from DBServers
  outBuilder.openArray();
  int res = _sendToAllDBServers(Utils::aqlResultsPath, b, [&](VPackSlice const& payload) {
    if (payload.isArray()) {
      outBuilder.add(VPackArrayIterator(payload));
    }
  });
  outBuilder.close();
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

VPackBuilder Conductor::toVelocyPack() const {
  MUTEX_LOCKER(guard, _callbackMutex);

  VPackBuilder result;
  result.openObject();
  result.add("state", VPackValue(pregel::ExecutionStateNames[_state]));
  result.add("gss", VPackValue(_globalSuperstep));
  result.add("totalRuntime", VPackValue(totalRuntimeSecs()));
  result.add("startupTime", VPackValue(_computationStartTimeSecs - _startTimeSecs));
  result.add("computationTime", VPackValue(_finalizationStartTimeSecs - _computationStartTimeSecs));
  if (_storeTimeSecs > 0.0) {
    result.add("storageTime", VPackValue(_storeTimeSecs));
  }
  _aggregators->serializeValues(result);
  _statistics.serializeValues(result);
  if (_state != ExecutionState::RUNNING) {
    result.add("vertexCount", VPackValue(_totalVerticesCount));
    result.add("edgeCount", VPackValue(_totalEdgesCount));
  }
  result.close();
  return result;
}

int Conductor::_sendToAllDBServers(std::string const& path, VPackBuilder const& message) {
  return _sendToAllDBServers(path, message, std::function<void(VPackSlice)>());
}

int Conductor::_sendToAllDBServers(std::string const& path, VPackBuilder const& message,
                                   std::function<void(VPackSlice)> handle) {
  _callbackMutex.assertLockedByCurrentThread();
  _respondedServers.clear();

  // to support the single server case, we handle it without optimizing it
  if (ServerState::instance()->isRunningInCluster() == false) {
    if (handle) {
      VPackBuilder response;

      PregelFeature::handleWorkerRequest(_vocbaseGuard.database(), path,
                                         message.slice(), response);
      handle(response.slice());
    } else {
      TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);
      uint64_t exe = _executionNumber;
      Scheduler* scheduler = SchedulerFeature::SCHEDULER;
      bool queued = scheduler->queue(RequestLane::INTERNAL_LOW, [path, message, exe] {
        auto pf = PregelFeature::instance();
        if (!pf) {
          return;
        }
        auto conductor = pf->conductor(exe);
        if (conductor) {
          TRI_vocbase_t& vocbase = conductor->_vocbaseGuard.database();
          VPackBuilder response;
          PregelFeature::handleWorkerRequest(vocbase, path, message.slice(), response);
        }
      });
      if (!queued) {
        return TRI_ERROR_QUEUE_FULL;
      }
    }
    return TRI_ERROR_NO_ERROR;
  }
  
  if (_dbServers.size() == 0) {
    LOG_TOPIC("a14fa", WARN, Logger::PREGEL) << "No servers registered";
    return TRI_ERROR_FAILED;
  }

  std::string base = Utils::baseUrl(Utils::workerPrefix);
  
  VPackBuffer<uint8_t> buffer;
  buffer.append(message.slice().begin(), message.slice().byteSize());

  network::RequestOptions reqOpts;
  reqOpts.database = _vocbaseGuard.database().name();
  reqOpts.timeout = network::Timeout(5.0 * 60.0);
  reqOpts.skipScheduler = true;
  
  auto const& nf = _vocbaseGuard.database().server().getFeature<NetworkFeature>();
  network::ConnectionPool* pool = nf.pool();
  std::vector<futures::Future<network::Response>> responses;
  
  for (auto const& server : _dbServers) {
    responses.emplace_back(network::sendRequest(pool, "server:" + server, fuerte::RestVerb::Post,
                                                base + path, buffer, reqOpts));
  }
  
  size_t nrGood = 0;
  futures::collectAll(responses).thenValue([&](auto results) {
    for (auto const& tryRes : results) {
       network::Response const& res = tryRes.get();  // throws exceptions upwards
      if (res.ok() && res.response->statusCode() < 400) {
        nrGood++;
        if (handle) {
          handle(res.response->slice());
        }
      }
    }
  }).wait();
  
  return nrGood == responses.size() ? TRI_ERROR_NO_ERROR : TRI_ERROR_FAILED;
}

void Conductor::_ensureUniqueResponse(VPackSlice body) {
  _callbackMutex.assertLockedByCurrentThread();

  // check if this the only time we received this
  ServerID sender = body.get(Utils::senderKey).copyString();
  if (_respondedServers.find(sender) != _respondedServers.end()) {
    LOG_TOPIC("c38b8", ERR, Logger::PREGEL) << "Received response already from " << sender;
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_CONFLICT);
  }
  _respondedServers.insert(sender);
}

std::vector<ShardID> Conductor::getShardIds(ShardID const& collection) const {
  TRI_vocbase_t& vocbase = _vocbaseGuard.database();
  ClusterInfo& ci = vocbase.server().getFeature<ClusterFeature>().clusterInfo();

  std::vector<ShardID> result;
  try {
    std::shared_ptr<LogicalCollection> lc = ci.getCollection(vocbase.name(), collection);
    std::shared_ptr<std::vector<ShardID>> shardIDs = ci.getShardList(std::to_string(lc->id()));
    result.reserve(shardIDs->size());
    for (auto const& it : *shardIDs) {
      result.emplace_back(it);
    }
  } catch (...) {
    result.clear();
  }

  return result;
}
