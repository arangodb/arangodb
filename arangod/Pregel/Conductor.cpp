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

#include "Pregel/Conductor.h"
#include "Pregel/Aggregator.h"
#include "Pregel/AlgoRegistry.h"
#include "Pregel/Algorithm.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/Recovery.h"
#include "Pregel/Utils.h"

#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/ThreadPool.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/StandaloneTransactionContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::pregel;

Conductor::Conductor(
    uint64_t executionNumber, TRI_vocbase_t* vocbase,
    std::vector<std::shared_ptr<LogicalCollection>> const& vertexCollections,
    std::vector<std::shared_ptr<LogicalCollection>> const& edgeCollections)
    : _vocbaseGuard(vocbase),
      _executionNumber(executionNumber),
      _vertexCollections(vertexCollections),
      _edgeCollections(edgeCollections) {
  bool isCoordinator = ServerState::instance()->isCoordinator();
  TRI_ASSERT(isCoordinator);
  LOG(INFO) << "constructed conductor";
}

Conductor::~Conductor() { this->cancel(); }

void Conductor::start(std::string const& algoName, VPackSlice userConfig) {
  if (!userConfig.isObject()) {
    _userParams.openObject();
    _userParams.close();
  } else {
    _userParams.add(userConfig);
  }
  
  _startTimeSecs = TRI_microtime();
  _globalSuperstep = 0;
  _state = ExecutionState::RUNNING;
  _algorithm.reset(AlgoRegistry::createAlgorithm(algoName, userConfig));
  if (!_algorithm) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Algorithm not found");
  }
  _aggregators.reset(new AggregatorHandler(_algorithm.get()));
  // configure the async mode as optional
  VPackSlice async = _userParams.slice().get("async");
  _asyncMode = _algorithm->supportsAsyncMode();
  _asyncMode = _asyncMode && (async.isNone() || async.getBoolean());

  int res = _initializeWorkers(Utils::startExecutionPath, VPackSlice());
  if (res != TRI_ERROR_NO_ERROR) {
    _state = ExecutionState::CANCELED;
    LOG(ERR) << "Not all DBServers started the execution";
  }
}

// only called by the conductor, is protected by the
// mutex locked in finishedGlobalStep
bool Conductor::_startGlobalStep() {
  VPackBuilder b;
  b.openObject();
  b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
  b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
  if (_aggregators->size() > 0) {
    b.add(Utils::aggregatorValuesKey, VPackValue(VPackValueType::Object));
    _aggregators->serializeValues(b);
    b.close();
  }
  b.close();

  // reset values which are calculated during the superstep
  _aggregators->resetValues();
  _workerStats.activeCount = 0;

  // first allow all workers to run worker level operations
  int res = _sendToAllDBServers(Utils::prepareGSSPath, b.slice());
  if (res == TRI_ERROR_NO_ERROR) {
    if (_masterContext) {
      _masterContext->preGlobalSuperstep(_globalSuperstep);
    }
    // start vertex level operations, does not get a response
    _sendToAllDBServers(Utils::startGSSPath, b.slice());  // call me maybe
    LOG(INFO) << "Conductor started new gss " << _globalSuperstep;
    return true;
  } else {
    LOG(INFO) << "Seems there is at least one worker out of order";
    // TODO, in case a worker needs more than 5 minutes to do calculations
    // this will be triggered as well
    return false;
  }
}

// ============ Conductor callbacks ===============
void Conductor::finishedWorkerStartup(VPackSlice& data) {
  MUTEX_LOCKER(guard, _callbackMutex);
  if (_state != ExecutionState::RUNNING) {
    LOG(WARN) << "We are not in a state where we expect a response";
    return;
  }
  _ensureUniqueResponse(data);
  if (_respondedServers.size() != _dbServers.size()) {
    return;
  }

  if (_startGlobalStep()) {
    // listens for changing primary DBServers on each collection shard
    RecoveryManager* mngr = PregelFeature::instance()->recoveryManager();
    if (mngr) {
      mngr->monitorCollections(_vertexCollections, this);
    }
  }
}

void Conductor::finishedWorkerStep(VPackSlice& data) {
  MUTEX_LOCKER(guard, _callbackMutex);
  // this method can be called multiple times in a superstep depending on
  // whether we are in the async mode
  uint64_t gss = data.get(Utils::globalSuperstepKey).getUInt();
  if (gss != _globalSuperstep ||
      !(_state == ExecutionState::RUNNING ||
        _state == ExecutionState::CANCELED)) {
    LOG(WARN) << "Conductor did received a callback from the wrong superstep";
    return;
  }
  
  WorkerStats stats(data);
  if (!_asyncMode || stats.allMessagesProcessed()) {
    _ensureUniqueResponse(data);
    
    // collect worker information
    VPackSlice slice = data.get(Utils::aggregatorValuesKey);
    if (slice.isObject()) {
      _aggregators->aggregateValues(slice);
    }
    _workerStats.accumulate(stats);
  }
  
  // wait for the last worker to respond
  if (_respondedServers.size() != _dbServers.size()) {
    return;
  }
  
  bool proceed = true;
  if (_masterContext) {  // ask algorithm to evaluate aggregated values
    proceed = _masterContext->postGlobalSuperstep(_globalSuperstep);
  }

  LOG(INFO) << "Finished gss " << _globalSuperstep;
  _globalSuperstep++;

  // workers are done if all messages were processed and no active vertices
  // are left to process
  bool workersDone = _workerStats.isDone();
  // TODO make maxumum configurable
  proceed = proceed && _globalSuperstep <= 100;

  if (proceed && !workersDone && _state == ExecutionState::RUNNING) {
    _startGlobalStep();  // trigger next superstep
  } else if (_state == ExecutionState::RUNNING ||
             _state == ExecutionState::CANCELED) {
    if (_state == ExecutionState::CANCELED) {
      LOG(WARN) << "Execution was canceled, results will be discarded.";
    } else {
      _state = ExecutionState::DONE;
    }

    // stop monitoring shards
    RecoveryManager* mngr = PregelFeature::instance()->recoveryManager();
    if (mngr) {
      mngr->stopMonitoring(this);
    }
    // tells workers to store / discard results
    _finalizeWorkers();

  } else {  // this prop shouldn't occur,
    LOG(WARN) << "No further action taken after receiving all responses";
  }
}

void Conductor::finishedRecovery(VPackSlice& data) {
  MUTEX_LOCKER(guard, _callbackMutex);
  if (_state != ExecutionState::RECOVERING) {
    LOG(WARN) << "We are not in a state where we expect a recovery response";
    return;
  }
  _ensureUniqueResponse(data);
  if (_respondedServers.size() != _dbServers.size()) {
    return;
  }

  if (_algorithm->supportsCompensation()) {
    bool proceed = false;
    if (_masterContext) {
      proceed = proceed || _masterContext->postCompensation(_globalSuperstep);
    }
    if (proceed) {
      VPackBuilder b;
      b.openObject();
      b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
      b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
      if (_aggregators->size() > 0) {
        b.add(Utils::aggregatorValuesKey, VPackValue(VPackValueType::Object));
        _aggregators->serializeValues(b);
        b.close();
      }
      b.close();

      // reset values which are calculated during the superstep
      _aggregators->resetValues();
      _workerStats.activeCount = 0;

      // first allow all workers to run worker level operations
      int res = _sendToAllDBServers(Utils::startRecoveryPath, b.slice());
      if (res != TRI_ERROR_NO_ERROR) {
        cancel();
        LOG(INFO) << "Recovery failed";
      }
    } else {
      _startGlobalStep();
    }
  } else {
    LOG(ERR) << "Recovery not supported";
  }
}

void Conductor::cancel() {
  if (_state == ExecutionState::RUNNING ||
      _state == ExecutionState::RECOVERING) {
    _state = ExecutionState::CANCELED;
    
    VPackBuilder b;
    b.openObject();
    b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
    b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
    b.close();
    _sendToAllDBServers(Utils::cancelGSSPath, b.slice());
  }
  _state = ExecutionState::CANCELED;
  // stop monitoring shards
  RecoveryManager* mngr = PregelFeature::instance()->recoveryManager();
  if (mngr) {
    mngr->stopMonitoring(this);
  }

  // tell workers to discard results
  /*int res = _finalizeWorkers();
  // we do not expect to reach all workers after a failed recovery
  if (res != TRI_ERROR_NO_ERROR && before != ExecutionState::RECOVERING) {
    LOG(ERR) << "Could not reach all workers to cancel execution";
  }*/
}

void Conductor::startRecovery() {
  MUTEX_LOCKER(guard, _callbackMutex);
  if (_state != ExecutionState::RUNNING) {
    LOG(INFO) << "Execution is already in the recovery state";
    return;
  }

  // we lost a DBServer, we need to reconfigure all remainging servers
  // so they load the data for the lost machine
  _state = ExecutionState::RECOVERING;

  basics::ThreadPool* pool = PregelFeature::instance()->threadPool();
  pool->enqueue([this] {
    // let's wait for a final state in the cluster
    usleep(2 * 15 * 1000000);
    std::vector<ServerID> goodServers;
    int res = PregelFeature::instance()->recoveryManager()->filterGoodServers(
        _dbServers, goodServers);
    if (res != TRI_ERROR_NO_ERROR) {
      LOG(ERR) << "Recovery proceedings failed";
      cancel();
      return;
    }

    VPackBuilder b;
    b.openObject();
    b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
    b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
    b.close();
    _dbServers = goodServers;
    _sendToAllDBServers(Utils::cancelGSSPath, b.slice());
    usleep(5 * 1000000);// workers may need a little bit

    // Let's try recovery
    if (_algorithm->supportsCompensation()) {
      if (_masterContext) {
        _masterContext->preCompensation(_globalSuperstep);
      }

      VPackBuilder b;
      b.openObject();
      b.add(Utils::recoveryMethodKey, VPackValue(Utils::compensate));
      if (_aggregators->size() > 0) {
        b.add(Utils::aggregatorValuesKey, VPackValue(VPackValueType::Object));
        _aggregators->serializeValues(b);
        b.close();
      }
      b.close();
      _aggregators->resetValues();
      _workerStats.activeCount = 0;

      // initialize workers will reconfigure the workers and set the
      // _dbServers list to the new primary DBServers
      int res = _initializeWorkers(Utils::startRecoveryPath, b.slice());
      if (res != TRI_ERROR_NO_ERROR) {
        cancel();
        LOG(ERR) << "Compensation failed";
      }
    } else {
      LOG(ERR) << "Recovery not supported";
      cancel();
    }
  });
}

static void resolveShards(
    LogicalCollection const* collection,
    std::map<ServerID, std::map<CollectionID, std::vector<ShardID>>>& serverMap,
    std::vector<ShardID>& allShards) {
  ClusterInfo* ci = ClusterInfo::instance();
  std::shared_ptr<std::vector<ShardID>> shardIDs =
      ci->getShardList(collection->cid_as_string());
  allShards.insert(allShards.end(), shardIDs->begin(), shardIDs->end());

  for (auto const& shard : *shardIDs) {
    std::shared_ptr<std::vector<ServerID>> servers =
        ci->getResponsibleServer(shard);
    if (servers->size() > 0) {
      serverMap[(*servers)[0]][collection->name()].push_back(shard);
    }
  }
}

/// should cause workers to start a new execution or begin with recovery
/// proceedings
int Conductor::_initializeWorkers(std::string const& suffix,
                                  VPackSlice additional) {
  int64_t vertexCount = 0, edgeCount = 0;
  std::map<CollectionID, std::string> collectionPlanIdMap;
  std::map<ServerID, std::map<CollectionID, std::vector<ShardID>>> vertexMap,
      edgeMap;
  std::vector<ShardID> allShardIDs;

  // resolve plan id's and shards on the servers
  for (auto& collection : _vertexCollections) {
    collectionPlanIdMap.emplace(collection->name(),
                                collection->planId_as_string());
    int64_t cc =
        Utils::countDocuments(_vocbaseGuard.vocbase(), collection->name());
    if (cc > 0) {
      vertexCount += cc;
      resolveShards(collection.get(), vertexMap, allShardIDs);
    }
  }
  for (auto& collection : _edgeCollections) {
    collectionPlanIdMap.emplace(collection->name(),
                                collection->planId_as_string());
    int64_t cc =
        Utils::countDocuments(_vocbaseGuard.vocbase(), collection->name());
    if (cc > 0) {
      edgeCount += cc;
      resolveShards(collection.get(), edgeMap, allShardIDs);
    }
  }
  _dbServers.clear();
  for (auto const& pair : vertexMap) {
    _dbServers.push_back(pair.first);
  }
  LOG(INFO) << vertexCount << " vertices, " << edgeCount << " edges";

  // Need to do this here, because we need the counts
  if (_masterContext && _masterContext->_vertexCount == 0) {
    _masterContext->_vertexCount = vertexCount;
    _masterContext->_edgeCount = edgeCount;
    _masterContext->_aggregators = _aggregators.get();
    _masterContext->preApplication();
  }

  std::string const path =
      Utils::baseUrl(_vocbaseGuard.vocbase()->name()) + suffix;
  std::string coordinatorId = ServerState::instance()->getId();
  LOG(INFO) << "My id: " << coordinatorId;
  std::vector<ClusterCommRequest> requests;
  for (auto const& it : vertexMap) {
    ServerID const& server = it.first;
    std::map<CollectionID, std::vector<ShardID>> const& vertexShardMap =
        it.second;
    std::map<CollectionID, std::vector<ShardID>> const& edgeShardMap =
        edgeMap[it.first];

    VPackBuilder b;
    b.openObject();
    b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
    b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
    b.add(Utils::algorithmKey, VPackValue(_algorithm->name()));
    b.add(Utils::userParametersKey, _userParams.slice());
    b.add(Utils::coordinatorIdKey, VPackValue(coordinatorId));
    b.add(Utils::totalVertexCount, VPackValue(vertexCount));
    b.add(Utils::totalEdgeCount, VPackValue(edgeCount));
    b.add(Utils::asyncMode, VPackValue(_asyncMode));
    b.add(Utils::vertexShardsKey, VPackValue(VPackValueType::Object));
    if (additional.isObject()) {
      b.add(additional);
    }
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
    for (std::string const& shard : allShardIDs) {
      b.add(VPackValue(shard));
    }
    b.close();
    b.close();

    auto body = std::make_shared<std::string const>(b.toJson());
    requests.emplace_back("server:" + server, rest::RequestType::POST, path,
                          body);
  }

  ClusterComm* cc = ClusterComm::instance();
  size_t nrDone = 0;
  size_t nrGood = cc->performRequests(requests, 5.0 * 60.0, nrDone,
                                      LogTopic("Pregel Conductor"));
  Utils::printResponses(requests);
  return nrGood == requests.size() ? TRI_ERROR_NO_ERROR : TRI_ERROR_FAILED;
}

int Conductor::_finalizeWorkers() {
  _endTimeSecs = TRI_microtime();
  bool storeResults = _state == ExecutionState::DONE;
  if (_masterContext) {
    _masterContext->postApplication();
  }

  VPackBuilder b;
  b.openObject();
  b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
  b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
  b.add(Utils::storeResultsKey, VPackValue(storeResults));
  b.close();
  int res = _sendToAllDBServers(Utils::finalizeExecutionPath, b.slice());

  LOG(INFO) << "Done. We did " << _globalSuperstep << " rounds";
  LOG(INFO) << "Send: " << _workerStats.sendCount
            << " Received: " << _workerStats.receivedCount;
  LOG(INFO) << "Worker Runtime: " << _workerStats.superstepRuntimeSecs << "s";
  LOG(INFO) << "Total Runtime: " << totalRuntimeSecs() << "s";
  return res;
}

int Conductor::_sendToAllDBServers(std::string const& suffix,
                                   VPackSlice const& message) {
  _respondedServers.clear();

  ClusterComm* cc = ClusterComm::instance();
  if (_dbServers.size() == 0) {
    LOG(WARN) << "No servers registered";
    return TRI_ERROR_FAILED;
  }

  std::string base = Utils::baseUrl(_vocbaseGuard.vocbase()->name());
  auto body = std::make_shared<std::string const>(message.toJson());
  std::vector<ClusterCommRequest> requests;
  for (auto const& server : _dbServers) {
    requests.emplace_back("server:" + server, rest::RequestType::POST,
                          base + suffix, body);
  }

  size_t nrDone = 0;
  size_t nrGood = cc->performRequests(requests, 5.0 * 60.0, nrDone,
                                      LogTopic("Pregel Conductor"));
  LOG(INFO) << "Send messages to " << nrDone << " servers";
  Utils::printResponses(requests);

  return nrGood == requests.size() ? TRI_ERROR_NO_ERROR : TRI_ERROR_FAILED;
}

void Conductor::_ensureUniqueResponse(VPackSlice body) {
  // check if this the only time we received this
  ServerID sender = body.get(Utils::senderKey).copyString();
  if (_respondedServers.find(sender) != _respondedServers.end()) {
    LOG(ERR) << "Received response already from " << sender;
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_CONFLICT);
  }
  _respondedServers.insert(sender);
}
