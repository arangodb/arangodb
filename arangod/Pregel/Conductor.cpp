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
#include <thread>

#include "Conductor.h"

#include "Pregel/Aggregator.h"
#include "Pregel/AlgoRegistry.h"
#include "Pregel/Algorithm.h"
#include "Pregel/MasterContext.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/Recovery.h"
#include "Pregel/Utils.h"

#include "Agency/TimeString.h"
#include "Basics/FunctionUtils.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Futures/Utilities.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
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

#define LOG_PREGEL(logId, level) \
  LOG_TOPIC(logId, level, Logger::PREGEL) << "[job " << _executionNumber << "] "

const char* arangodb::pregel::ExecutionStateNames[8] = {
    "none",     "running",  "storing",    "done",
    "canceled", "in error", "recovering", "fatal error"};

Conductor::Conductor(
    uint64_t executionNumber, TRI_vocbase_t& vocbase,
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
  // configure the async mode as off by default
  VPackSlice async = _userParams.slice().get("async");
  _asyncMode =
      _algorithm->supportsAsyncMode() && async.isBool() && async.getBoolean();
  _useMemoryMaps = VelocyPackHelper::getBooleanValue(
      _userParams.slice(), Utils::useMemoryMapsKey, _useMemoryMaps);
  VPackSlice storeSlice = config.get("store");
  _storeResults = !storeSlice.isBool() || storeSlice.getBool();

  // time-to-live for finished/failed Pregel jobs before garbage collection.
  // default timeout is 10 minutes for each conductor
  uint64_t ttl = 600;
  _ttl = std::chrono::seconds(
      VelocyPackHelper::getNumericValue(config, "ttl", ttl));

  LOG_PREGEL("00f5f", INFO)
      << "Starting " << _algorithm->name() << " in database '" << vocbase.name()
      << "', ttl: " << _ttl.count() << "s"
      << ", async: " << (_asyncMode ? "yes" : "no")
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
}

void Conductor::start() {
  MUTEX_LOCKER(guard, _callbackMutex);
  _startTimeSecs = TRI_microtime();
  _computationStartTimeSecs = _startTimeSecs;
  _finalizationStartTimeSecs = _startTimeSecs;
  _endTimeSecs = _startTimeSecs;

  _globalSuperstep = 0;
  updateState(ExecutionState::RUNNING);

  LOG_PREGEL("3a255", DEBUG) << "Telling workers to load the data";
  auto res = _initializeWorkers(Utils::startExecutionPath, VPackSlice());
  if (res != TRI_ERROR_NO_ERROR) {
    updateState(ExecutionState::CANCELED);
    LOG_PREGEL("30171", ERR) << "Not all DBServers started the execution";
  }
}

// only called by the conductor, is protected by the
// mutex locked in finishedGlobalStep
bool Conductor::_startGlobalStep() {
  if (_feature.isStopping()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }

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

  VPackBuilder messagesFromWorkers;

  {
    VPackArrayBuilder guard(&messagesFromWorkers);
    // we are explicitly expecting an response containing the aggregated
    // values as well as the count of active vertices
    auto res = _sendToAllDBServers(
        Utils::prepareGSSPath, b, [&](VPackSlice const& payload) {
          _aggregators->aggregateValues(payload);

          messagesFromWorkers.add(
              payload.get(Utils::workerToMasterMessagesKey));
          _statistics.accumulateActiveCounts(payload);
          _totalVerticesCount += payload.get(Utils::vertexCountKey).getUInt();
          _totalEdgesCount += payload.get(Utils::edgeCountKey).getUInt();
        });

    if (res != TRI_ERROR_NO_ERROR) {
      updateState(ExecutionState::IN_ERROR);
      LOG_PREGEL("04189", ERR)
          << "Seems there is at least one worker out of order";
      // the recovery mechanisms should take care of this
      return false;
    }
  }

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

  // TODO make maximum configurable
  if (!proceed || done || _globalSuperstep >= _maxSuperstep) {
    // tells workers to store / discard results
    if (_storeResults) {
      updateState(ExecutionState::STORING);
      _finalizeWorkers();
    } else {  // just stop the timer
      updateState(_inErrorAbort ? ExecutionState::FATAL_ERROR
                                : ExecutionState::DONE);
      _endTimeSecs = TRI_microtime();
      LOG_PREGEL("9e82c", INFO)
          << "Done, execution took: " << totalRuntimeSecs() << " s";
    }
    return false;
  }

  VPackBuilder toWorkerMessages;
  if (_masterContext) {
    _masterContext->_globalSuperstep = _globalSuperstep;
    _masterContext->_vertexCount = _totalVerticesCount;
    _masterContext->_edgeCount = _totalEdgesCount;
    _masterContext->_reports = &_reports;
    if (!_masterContext->preGlobalSuperstepWithResult()) {
      updateState(ExecutionState::FATAL_ERROR);
      _endTimeSecs = TRI_microtime();
      return false;
    }
    _masterContext->preGlobalSuperstepMessage(toWorkerMessages);
  }

  b.clear();
  b.openObject();
  b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
  b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
  b.add(Utils::vertexCountKey, VPackValue(_totalVerticesCount));
  b.add(Utils::edgeCountKey, VPackValue(_totalEdgesCount));
  b.add(Utils::activateAllKey, VPackValue(activateAll));

  if (!toWorkerMessages.slice().isNone()) {
    b.add(Utils::masterToWorkerMessagesKey, toWorkerMessages.slice());
  }
  _aggregators->serializeValues(b);

  b.close();

  LOG_PREGEL("d98de", DEBUG) << b.slice().toJson();

  _stepStartTimeSecs = TRI_microtime();

  // start vertex level operations, does not get a response
  auto res = _sendToAllDBServers(Utils::startGSSPath, b);  // call me maybe
  if (res != TRI_ERROR_NO_ERROR) {
    updateState(ExecutionState::IN_ERROR);
    LOG_PREGEL("f34bb", ERR)
        << "Conductor could not start GSS " << _globalSuperstep;
    // the recovery mechanisms should take care od this
  } else {
    LOG_PREGEL("411a5", DEBUG)
        << "Conductor started new gss " << _globalSuperstep;
  }
  return res == TRI_ERROR_NO_ERROR;
}

// ============ Conductor callbacks ===============
void Conductor::finishedWorkerStartup(VPackSlice const& data) {
  MUTEX_LOCKER(guard, _callbackMutex);
  _ensureUniqueResponse(data);
  if (_state != ExecutionState::RUNNING) {
    LOG_PREGEL("10f48", WARN)
        << "We are not in a state where we expect a response";
    return;
  }

  _totalVerticesCount += data.get(Utils::vertexCountKey).getUInt();
  _totalEdgesCount += data.get(Utils::edgeCountKey).getUInt();
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
  if (gss != _globalSuperstep || !(_state == ExecutionState::RUNNING ||
                                   _state == ExecutionState::CANCELED)) {
    LOG_PREGEL("dc904", WARN)
        << "Conductor did received a callback from the wrong superstep";
    return VPackBuilder();
  }

  if (auto reports = data.get("reports"); reports.isArray()) {
    _reports.appendFromSlice(reports);
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

  LOG_PREGEL("39385", DEBUG) << "Finished gss " << _globalSuperstep << " in "
                             << (TRI_microtime() - _stepStartTimeSecs) << "s";
  //_statistics.debugOutput();
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
  return VPackBuilder();
}

void Conductor::finishedRecoveryStep(VPackSlice const& data) {
  MUTEX_LOCKER(guard, _callbackMutex);
  _ensureUniqueResponse(data);
  if (_state != ExecutionState::RECOVERING) {
    LOG_PREGEL("23d8b", WARN)
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

  auto res = TRI_ERROR_NO_ERROR;
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
    LOG_PREGEL("6ecf2", INFO) << "Recovery finished. Proceeding normally";

    // build the message, works for all cases
    VPackBuilder b;
    b.openObject();
    b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
    b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
    b.close();
    res = _sendToAllDBServers(Utils::finalizeRecoveryPath, b);
    if (res == TRI_ERROR_NO_ERROR) {
      updateState(ExecutionState::RUNNING);
      _startGlobalStep();
    }
  }
  if (res != TRI_ERROR_NO_ERROR) {
    cancelNoLock();
    LOG_PREGEL("7f97e", INFO) << "Recovery failed";
  }
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

void Conductor::startRecovery() {
  MUTEX_LOCKER(guard, _callbackMutex);
  if (_state != ExecutionState::RUNNING && _state != ExecutionState::IN_ERROR) {
    return;  // maybe we are already in recovery mode
  } else if (_algorithm->supportsCompensation() == false) {
    LOG_PREGEL("12e0e", ERR) << "Algorithm does not support recovery";
    cancelNoLock();
    return;
  }

  // we lost a DBServer, we need to reconfigure all remainging servers
  // so they load the data for the lost machine
  updateState(ExecutionState::RECOVERING);
  _statistics.reset();

  TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);

  // let's wait for a final state in the cluster
  _workHandle = SchedulerFeature::SCHEDULER->queueDelayed(
      RequestLane::CLUSTER_AQL, std::chrono::seconds(2),
      [this, self = shared_from_this()](bool cancelled) {
        if (cancelled || _state != ExecutionState::RECOVERING) {
          return;  // seems like we are canceled
        }
        std::vector<ServerID> goodServers;
        auto res = _feature.recoveryManager()->filterGoodServers(_dbServers,
                                                                 goodServers);
        if (res != TRI_ERROR_NO_ERROR) {
          LOG_PREGEL("3d08b", ERR) << "Recovery proceedings failed";
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
        additionalKeys.add(Utils::recoveryMethodKey,
                           VPackValue(Utils::compensate));
        _aggregators->serializeValues(b);
        additionalKeys.close();
        _aggregators->resetValues();

        // initialize workers will reconfigure the workers and set the
        // _dbServers list to the new primary DBServers
        res = _initializeWorkers(Utils::startRecoveryPath,
                                 additionalKeys.slice());
        if (res != TRI_ERROR_NO_ERROR) {
          cancelNoLock();
          LOG_PREGEL("fefc6", ERR) << "Compensation failed";
        }
      });
}

// resolves into an ordered list of shards for each collection on each server
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

/// should cause workers to start a new execution or begin with recovery
/// proceedings
ErrorCode Conductor::_initializeWorkers(std::string const& suffix,
                                        VPackSlice additional) {
  _callbackMutex.assertLockedByCurrentThread();

  std::string const path = Utils::baseUrl(Utils::workerPrefix) + suffix;

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

  _dbServers.clear();
  for (auto const& pair : vertexMap) {
    _dbServers.push_back(pair.first);
  }
  // do not reload all shard id's, this list must stay in the same order
  if (_allShards.size() == 0) {
    _allShards = shardList;
  }

  std::string coordinatorId = ServerState::instance()->getId();
  auto const& nf =
      _vocbaseGuard.database().server().getFeature<NetworkFeature>();
  network::ConnectionPool* pool = nf.pool();
  std::vector<futures::Future<network::Response>> responses;

  for (auto const& it : vertexMap) {
    ServerID const& server = it.first;
    std::map<CollectionID, std::vector<ShardID>> const& vertexShardMap =
        it.second;
    std::map<CollectionID, std::vector<ShardID>> const& edgeShardMap =
        edgeMap[it.first];

    VPackBuffer<uint8_t> buffer;
    VPackBuilder b(buffer);
    b.openObject();
    b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
    b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
    b.add(Utils::algorithmKey, VPackValue(_algorithm->name()));
    b.add(Utils::userParametersKey, _userParams.slice());
    b.add(Utils::coordinatorIdKey, VPackValue(coordinatorId));
    b.add(Utils::asyncModeKey, VPackValue(_asyncMode));
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
                                                b.slice(), _feature);

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

      responses.emplace_back(network::sendRequestRetry(
          pool, "server:" + server, fuerte::RestVerb::Post, path,
          std::move(buffer), reqOpts));

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
  _finalizationStartTimeSecs = TRI_microtime();

  bool store = _state == ExecutionState::STORING;
  if (_masterContext) {
    _masterContext->postApplication();
  }

  // stop monitoring shards
  RecoveryManager* mngr = _feature.recoveryManager();
  if (mngr) {
    mngr->stopMonitoring(this);
  }

  LOG_PREGEL("fc187", DEBUG) << "Finalizing workers";
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
  {
    auto reports = data.get(Utils::reportsKey);
    if (reports.isArray()) {
      _reports.appendFromSlice(reports);
    }
  }

  _ensureUniqueResponse(data);
  if (_respondedServers.size() != _dbServers.size()) {
    return;
  }

  // do not swap an error state to done
  bool didStore = false;
  if (_state == ExecutionState::STORING) {
    updateState(_inErrorAbort ? ExecutionState::FATAL_ERROR
                              : ExecutionState::DONE);
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

  if (_finalizationStartTimeSecs < _computationStartTimeSecs) {
    // prevent negative computation times from being reported
    _finalizationStartTimeSecs = _computationStartTimeSecs;
  }

  double compTime = _finalizationStartTimeSecs - _computationStartTimeSecs;
  TRI_ASSERT(compTime >= 0);
  if (didStore) {
    _storeTimeSecs = TRI_microtime() - _finalizationStartTimeSecs;
  }

  LOG_PREGEL("063b5", INFO)
      << "Done. We did " << _globalSuperstep << " rounds"
      << ". Startup time: " << _computationStartTimeSecs - _startTimeSecs << "s"
      << ", computation time: " << compTime << "s"
      << (didStore ? (", storage time: " + std::to_string(_storeTimeSecs) + "s")
                   : "")
      << ", overall: " << totalRuntimeSecs() << "s"
      << ", stats: " << debugOut.slice().toJson();

  // always try to cleanup
  if (_state == ExecutionState::CANCELED) {
    auto* scheduler = SchedulerFeature::SCHEDULER;
    if (scheduler) {
      uint64_t exe = _executionNumber;
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
        _state == ExecutionState::IN_ERROR ||
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

  VPackBuilder b;
  b.openObject();
  b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
  b.add("withId", VPackValue(withId));
  b.close();

  // merge results from DBServers
  outBuilder.openArray();
  auto res = _sendToAllDBServers(
      Utils::aqlResultsPath, b, [&](VPackSlice const& payload) {
        if (payload.isArray()) {
          outBuilder.add(VPackArrayIterator(payload));
        }
      });
  outBuilder.close();
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

void Conductor::toVelocyPack(VPackBuilder& result) const {
  MUTEX_LOCKER(guard, _callbackMutex);

  result.openObject();
  result.add("id", VPackValue(std::to_string(_executionNumber)));
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
  result.add("totalRuntime", VPackValue(totalRuntimeSecs()));
  result.add("startupTime",
             VPackValue(_computationStartTimeSecs - _startTimeSecs));
  result.add("computationTime", VPackValue(_finalizationStartTimeSecs -
                                           _computationStartTimeSecs));
  if (_storeTimeSecs > 0.0) {
    result.add("storageTime", VPackValue(_storeTimeSecs));
  }
  _aggregators->serializeValues(result);
  _statistics.serializeValues(result);
  result.add(VPackValue("reports"));
  _reports.intoBuilder(result);
  if (_state != ExecutionState::RUNNING) {
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

void Conductor::_ensureUniqueResponse(VPackSlice body) {
  _callbackMutex.assertLockedByCurrentThread();

  // check if this the only time we received this
  ServerID sender = body.get(Utils::senderKey).copyString();
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

void Conductor::updateState(ExecutionState state) {
  _state = state;
  if (_state == ExecutionState::CANCELED || _state == ExecutionState::DONE ||
      _state == ExecutionState::IN_ERROR ||
      _state == ExecutionState::FATAL_ERROR) {
    _expires = std::chrono::system_clock::now() + _ttl;
  }
}
