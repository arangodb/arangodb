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
#include "Pregel/MasterContext.h"
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
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  LOG(INFO) << "Constructed conductor";
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
  _masterContext.reset(_algorithm->masterContext(userConfig));
  _aggregators.reset(new AggregatorHandler(_algorithm.get()));
  // configure the async mode as optional
  VPackSlice async = _userParams.slice().get("async");
  _asyncMode = _algorithm->supportsAsyncMode();
  _asyncMode = _asyncMode && (async.isNone() || async.getBoolean());
  if (_asyncMode) {
    LOG(INFO) << "Running in async mode";
  }
  VPackSlice lazy = _userParams.slice().get("lazyLoading");
  _lazyLoading = _algorithm->supportsLazyLoading();
  _lazyLoading = _lazyLoading && (lazy.isNone() || lazy.getBoolean());
  if (_lazyLoading) {
    LOG(INFO) << "Enabled lazy loading";
  }

  LOG(INFO) << "Telling workers to load the data";
  int res = _initializeWorkers(Utils::startExecutionPath, VPackSlice());
  if (res != TRI_ERROR_NO_ERROR) {
    _state = ExecutionState::CANCELED;
    LOG(ERR) << "Not all DBServers started the execution";
  }
}

// only called by the conductor, is protected by the
// mutex locked in finishedGlobalStep
bool Conductor::_startGlobalStep() {
  // send prepare GSS notice
  VPackBuilder b;
  b.openObject();
  b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
  b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
  b.add(Utils::vertexCount, VPackValue(_totalVerticesCount));
  b.add(Utils::edgeCount, VPackValue(_totalEdgesCount));
  b.close();

  // we are explicitly expecting an response containing the aggregated
  // values as well as the count of active vertices
  std::vector<ClusterCommRequest> requests;
  int res = _sendToAllDBServers(Utils::prepareGSSPath, b.slice(), requests);
  if (res != TRI_ERROR_NO_ERROR) {
    _state = ExecutionState::IN_ERROR;
    LOG(INFO) << "Seems there is at least one worker out of order";
    Utils::printResponses(requests);
    // the recovery mechanisms should take care of this
    return false;
  }
  /// collect the aggregators
  _aggregators->resetValues();
  _statistics.resetActiveCount();
  _totalVerticesCount = 0;  // might change during execution
  _totalEdgesCount = 0;
  for (auto const& req : requests) {
    VPackSlice payload = req.result.answer->payload();
    _aggregators->parseValues(payload);
    _statistics.accumulateActiveCounts(payload);
    _totalVerticesCount += payload.get(Utils::vertexCount).getUInt();
    _totalEdgesCount += payload.get(Utils::edgeCount).getUInt();
  }

  // workers are done if all messages were processed and no active vertices
  // are left to process
  bool proceed = true;
  if (_masterContext && _globalSuperstep > 0) {  // ask algorithm to evaluate aggregated values
    _masterContext->_globalSuperstep = _globalSuperstep - 1;
    proceed = _masterContext->postGlobalSuperstep(_globalSuperstep);
    if (!proceed) {
      LOG(INFO) << "Master context ended execution";
    }
  }

  // TODO make maximum configurable
  bool done = _globalSuperstep != 0 && _statistics.executionFinished();
  if (!proceed || done || _globalSuperstep >= 100) {
    _state = ExecutionState::DONE;
    // tells workers to store / discard results
    _finalizeWorkers();
    return false;
  }
  if (_masterContext) {
    _masterContext->_globalSuperstep = _globalSuperstep;
    _masterContext->_vertexCount = _totalVerticesCount;
    _masterContext->_edgeCount = _totalEdgesCount;
    _masterContext->preGlobalSuperstep(_globalSuperstep);
  }

  b.clear();
  b.openObject();
  b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
  b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
  b.add(Utils::vertexCount, VPackValue(_totalVerticesCount));
  b.add(Utils::edgeCount, VPackValue(_totalEdgesCount));
  _aggregators->serializeValues(b);
  b.close();
  LOG(INFO) << b.toString();

  // start vertex level operations, does not get a response
  res = _sendToAllDBServers(Utils::startGSSPath, b.slice());  // call me maybe
  if (res != TRI_ERROR_NO_ERROR) {
    _state = ExecutionState::IN_ERROR;
    LOG(INFO) << "Conductor could not start GSS " << _globalSuperstep;
    // the recovery mechanisms should take care od this
  } else {
    LOG(INFO) << "Conductor started new gss " << _globalSuperstep;
  }
  return res == TRI_ERROR_NO_ERROR;
}

// ============ Conductor callbacks ===============
void Conductor::finishedWorkerStartup(VPackSlice data) {
  MUTEX_LOCKER(guard, _callbackMutex);
  _ensureUniqueResponse(data);
  if (_state != ExecutionState::RUNNING) {
    LOG(WARN) << "We are not in a state where we expect a response";
    return;
  }
  
  _totalVerticesCount += data.get(Utils::vertexCount).getUInt();
  _totalEdgesCount += data.get(Utils::edgeCount).getUInt();
  if (_respondedServers.size() != _dbServers.size()) {
    return;
  }

  LOG(INFO) << _totalVerticesCount << " vertices, " << _totalEdgesCount
            << " edges";
  if (_masterContext) {
    _masterContext->_globalSuperstep = 0;
    _masterContext->_vertexCount = _totalVerticesCount;
    _masterContext->_edgeCount = _totalEdgesCount;
    _masterContext->_aggregators = _aggregators.get();
    _masterContext->preApplication();
  }

  _computationStartTimeSecs = TRI_microtime();
  if (_startGlobalStep()) {
    // listens for changing primary DBServers on each collection shard
    RecoveryManager* mngr = PregelFeature::instance()->recoveryManager();
    if (mngr) {
      mngr->monitorCollections(_vertexCollections, this);
    }
  }
}

/// Will optionally send a response, to notify the worker of converging aggregator
/// values which can be coninually updated (in async mode)
void Conductor::finishedWorkerStep(VPackSlice data, VPackBuilder &response) {
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

  // track message counts to decide when to halt or add global barriers.
  // In normal mode this will wait for a response from each worker,
  // in async mode this will wait until all messages were processed
  _statistics.accumulateMessageStats(data);
  if (_asyncMode == false) {// in async mode we wait for all responded
    _ensureUniqueResponse(data);
    // wait for the last worker to respond
    if (_respondedServers.size() != _dbServers.size()) {
      return;
    }
  } else if (_statistics.clientCount() < _dbServers.size() ||  // no messages
             !_statistics.allMessagesProcessed()) {  // haven't received msgs    
    if (_aggregators->parseValues(data)) {
      response.openObject();
      _aggregators->serializeValues(response);
      response.close();
    }
    return;
  }

  LOG(INFO) << "Finished gss " << _globalSuperstep << " in "
            << (TRI_microtime() - _computationStartTimeSecs) << "s";
  _globalSuperstep++;
  
  // don't block the response for workers waiting on this callback
  // this should allow workers to go into the IDLE state
  basics::ThreadPool* pool = PregelFeature::instance()->threadPool();
  pool->enqueue([this] {
    MUTEX_LOCKER(guard, _callbackMutex);

    if (_state == ExecutionState::RUNNING) {
        _startGlobalStep();  // trigger next superstep
    } else if (_state == ExecutionState::CANCELED) {
      LOG(WARN) << "Execution was canceled, results will be discarded.";
      _finalizeWorkers();// tells workers to store / discard results
    } else {  // this prop shouldn't occur unless we are recovering or in error
      LOG(WARN) << "No further action taken after receiving all responses";
    }
  });
}

void Conductor::finishedRecoveryStep(VPackSlice data) {
  MUTEX_LOCKER(guard, _callbackMutex);
  _ensureUniqueResponse(data);
  if (_state != ExecutionState::RECOVERING) {
    LOG(WARN) << "We are not in a state where we expect a recovery response";
    return;
  }
  
  // the recovery mechanism might be gathering state information
  _aggregators->parseValues(data);
  if (_respondedServers.size() != _dbServers.size()) {
    return;
  }
  
  // only compensations supported
  bool proceed = false;
  if (_masterContext) {
    proceed = proceed || _masterContext->postCompensation(_globalSuperstep);
  }
  
  int res = TRI_ERROR_NO_ERROR;
  if (proceed) {
    // reset values which are calculated during the superstep
    _aggregators->resetValues();
    if (_masterContext) {
      _masterContext->preCompensation(_globalSuperstep);
    }
    
    VPackBuilder b;
    b.openObject();
    b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
    _aggregators->serializeValues(b);
    b.close();
    // first allow all workers to run worker level operations
    res = _sendToAllDBServers(Utils::continueRecoveryPath, b.slice());
    
  } else {
    LOG(INFO) << "Recovery finished. Proceeding normally";
    
    // build the message, works for all cases
    VPackBuilder b;
    b.openObject();
    b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
    b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
    b.close();
    res = _sendToAllDBServers(Utils::finalizeRecoveryPath, b.slice());
    if (res == TRI_ERROR_NO_ERROR) {
      _state = ExecutionState::RUNNING;
      _startGlobalStep();
    }
  }
  if (res != TRI_ERROR_NO_ERROR) {
    cancel();
    LOG(INFO) << "Recovery failed";
  }
}

void Conductor::cancel() {
  if (_state == ExecutionState::RUNNING
      || _state == ExecutionState::RECOVERING
      || _state == ExecutionState::IN_ERROR) {
    _state = ExecutionState::CANCELED;

    LOG(INFO) << "Telling workers to discard results";
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
}

void Conductor::startRecovery() {
  MUTEX_LOCKER(guard, _callbackMutex);
  if (_state != ExecutionState::RUNNING && _state != ExecutionState::IN_ERROR) {
    return;// maybe we are already in recovery mode
  } else if (_algorithm->supportsCompensation() == false) {
    LOG(ERR) << "Execution is not recoverable";
    cancel();
    return;
  }
  
  // we lost a DBServer, we need to reconfigure all remainging servers
  // so they load the data for the lost machine
  _state = ExecutionState::RECOVERING;
  _statistics.reset();

  basics::ThreadPool* pool = PregelFeature::instance()->threadPool();
  pool->enqueue([this] {
    // let's wait for a final state in the cluster
    // on some systems usleep does not
    // like arguments greater than 1000000
    usleep(1000000);
    usleep(1000000);
    if (_state != ExecutionState::RECOVERING) {
      return;// seems like we are canceled
    }
    
    std::vector<ServerID> goodServers;
    int res = PregelFeature::instance()->recoveryManager()->filterGoodServers(
        _dbServers, goodServers);
    if (res != TRI_ERROR_NO_ERROR) {
      LOG(ERR) << "Recovery proceedings failed";
      cancel();
      return;
    }
    _dbServers = goodServers;

    VPackBuilder b;
    b.openObject();
    b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
    b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
    b.close();
    _sendToAllDBServers(Utils::cancelGSSPath, b.slice());
    if (_state != ExecutionState::RECOVERING) {
      return;// seems like we are canceled
    }

    // Let's try recovery
    if (_masterContext) {
      bool proceed = _masterContext->preCompensation(_globalSuperstep);
      if (!proceed) {
        cancel();
      }
    }

    b.clear();// start a new message
    b.openObject();
    b.add(Utils::recoveryMethodKey, VPackValue(Utils::compensate));
    _aggregators->serializeValues(b);
    b.close();
    _aggregators->resetValues();

    // initialize workers will reconfigure the workers and set the
    // _dbServers list to the new primary DBServers
    res = _initializeWorkers(Utils::startRecoveryPath, b.slice());
    if (res != TRI_ERROR_NO_ERROR) {
      cancel();
      LOG(ERR) << "Compensation failed";
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
  // int64_t vertexCount = 0, edgeCount = 0;
  std::map<CollectionID, std::string> collectionPlanIdMap;
  std::map<ServerID, std::map<CollectionID, std::vector<ShardID>>> vertexMap,
      edgeMap;
  std::vector<ShardID> shardList;

  // resolve plan id's and shards on the servers
  for (auto& collection : _vertexCollections) {
    collectionPlanIdMap.emplace(collection->name(),
                                collection->planId_as_string());
    resolveShards(collection.get(), vertexMap, shardList);
  }
  for (auto& collection : _edgeCollections) {
    collectionPlanIdMap.emplace(collection->name(),
                                collection->planId_as_string());
    resolveShards(collection.get(), edgeMap, shardList);
  }
  _dbServers.clear();
  for (auto const& pair : vertexMap) {
    _dbServers.push_back(pair.first);
  }
  // do not reload all shard id's, this list is must stay in the same order
  if (_allShards.size() == 0) {
    _allShards = shardList;
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
    b.add(Utils::asyncMode, VPackValue(_asyncMode));
    b.add(Utils::lazyLoading, VPackValue(_lazyLoading));
    if (additional.isObject()) {
      for (auto const& pair : VPackObjectIterator(additional)) {
        b.add(pair.key.copyString(), pair.value);
      }
    }
    
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
  double compEnd = TRI_microtime();
  
  bool storeResults = _state == ExecutionState::DONE;
  if (_masterContext) {
    _masterContext->postApplication();
  }
  // stop monitoring shards
  RecoveryManager* mngr = PregelFeature::instance()->recoveryManager();
  if (mngr) {
    mngr->stopMonitoring(this);
  }

  LOG(INFO) << "Finalizing workers";
  VPackBuilder b;
  b.openObject();
  b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
  b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
  b.add(Utils::storeResultsKey, VPackValue(storeResults));
  b.close();
  int res = _sendToAllDBServers(Utils::finalizeExecutionPath, b.slice());
  _endTimeSecs = TRI_microtime();
  b.clear();

  b.openObject();
  b.add("stats", VPackValue(VPackValueType::Object));
  _statistics.serializeValues(b);
  b.close();
  _aggregators->serializeValues(b);
  b.close();

  LOG(INFO) << "Done. We did " << _globalSuperstep << " rounds";
  LOG(INFO) << "Startup Time: " << _computationStartTimeSecs - _startTimeSecs
            << "s";
  LOG(INFO) << "Computation Time: " << compEnd - _computationStartTimeSecs << "s";
  LOG(INFO) << "Storage Time: " << TRI_microtime() - compEnd << "s";
  LOG(INFO) << "Overall: " << totalRuntimeSecs() << "s";
  LOG(INFO) << "Stats: " << b.toString();
  return res;
}

int Conductor::_sendToAllDBServers(std::string const& suffix,
                                   VPackSlice const& message) {
  std::vector<ClusterCommRequest> requests;
  return _sendToAllDBServers(suffix, message, requests);
}

int Conductor::_sendToAllDBServers(std::string const& suffix,
                                   VPackSlice const& message,
                                   std::vector<ClusterCommRequest>& requests) {
  _respondedServers.clear();

  ClusterComm* cc = ClusterComm::instance();
  if (_dbServers.size() == 0) {
    LOG(WARN) << "No servers registered";
    return TRI_ERROR_FAILED;
  }

  std::string base = Utils::baseUrl(_vocbaseGuard.vocbase()->name());
  auto body = std::make_shared<std::string const>(message.toJson());
  for (auto const& server : _dbServers) {
    requests.emplace_back("server:" + server, rest::RequestType::POST,
                          base + suffix, body);
  }

  size_t nrDone = 0;
  size_t nrGood = cc->performRequests(requests, 5.0 * 60.0, nrDone,
                                      LogTopic("Pregel Conductor"));
  LOG(INFO) << "Send " << suffix << " to " << nrDone << " servers";
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
