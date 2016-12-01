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

#include "Pregel/Algorithm.h"
#include "Pregel/Aggregator.h"
#include "Pregel/AlgoRegistry.h"
#include "Pregel/Conductor.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/Recovery.h"
#include "Pregel/Utils.h"

#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
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

Conductor::~Conductor() {}

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
  _responseCount = 0;
  _doneCount = 0;
  _algorithm.reset(AlgoRegistry::createAlgorithm(algoName, userConfig));
  if (!_algorithm) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Algorithm not found");
  }
  _aggregatorUsage.reset(new AggregatorUsage(_algorithm.get()));
  
  int res = _initializeWorkers(Utils::startExecutionPath, VPackSlice());
  if (res == TRI_ERROR_NO_ERROR) {
    if (_startGlobalStep()) {
      RecoveryManager *mngr = PregelFeature::instance()->recoveryManager();
      mngr->monitorCollections(_vertexCollections, this);
    }
  } else {
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
  if (_aggregatorUsage->size() > 0) {
    b.add(Utils::aggregatorValuesKey, VPackValue(VPackValueType::Object));
    _aggregatorUsage->serializeValues(b);
    b.close();
  }
  b.close();

  // reset aggregatores ahead of gss
  _aggregatorUsage->resetValues();
  _workerStats.activeCount = 0;// reset currently active vertices

  // first allow all workers to run worker level operations
  int res = _sendToAllDBServers(Utils::prepareGSSPath, b.slice());
  if (res == TRI_ERROR_NO_ERROR) {
    // start vertex level operations, does not get a response
    _sendToAllDBServers(Utils::startGSSPath, b.slice());// call me maybe
    LOG(INFO) << "Conductor started new gss " << _globalSuperstep;
    return true;
  } else {
    LOG(INFO) << "Seems there is at least one worker out of order";
    // TODO, in case a worker needs more than 5 minutes to do calculations
    // this will be triggered as well
    return false;
  }
}


static void printResults(std::vector<ClusterCommRequest> const& requests) {
  for (auto const& req : requests) {
    auto& res = req.result;
    if (res.status == CL_COMM_RECEIVED
        && res.answer_code != rest::ResponseCode::OK) {
      LOG(ERR) << req.destination << ": " << res.answer->payload().toJson();
    }
  }
}

void Conductor::finishedGlobalStep(VPackSlice& data) {
  MUTEX_LOCKER(mutexLocker, _finishedGSSMutex);

  uint64_t gss = data.get(Utils::globalSuperstepKey).getUInt();
  if (gss != _globalSuperstep) {
    LOG(WARN) << "Conductor did received a callback from the wrong superstep";
    return;
  }
  
  _responseCount++;
  VPackSlice workerValues = data.get(Utils::aggregatorValuesKey);
  if (workerValues.isObject()) {
    _aggregatorUsage->aggregateValues(workerValues);
  }
  _workerStats.accumulate(data);
/*
  VPackSlice isDone = data.get(Utils::doneKey);
  if (isDone.isBool() && isDone.getBool()) {
    _doneCount++;
  }*/

  if (_responseCount == _dbServers.size()) {
    LOG(INFO) << "Finished gss " << _globalSuperstep;
    _globalSuperstep++;

    bool done = _workerStats.sendCount == _workerStats.receivedCount
                && _workerStats.activeCount == 0;
    if (done) {
      LOG(INFO) << "1337: we are done";
    }
    
    if (_state != ExecutionState::RUNNING
        || _doneCount == _dbServers.size()
        || _globalSuperstep >= 100) {
      
      _endTimeSecs = TRI_microtime();
      bool storeResults = _state == ExecutionState::RUNNING;
      if (_state == ExecutionState::CANCELED) {
        LOG(WARN) << "Execution was canceled, results will be discarded.";
      } else {
        _state = ExecutionState::DONE;
      }
      if (_masterContext) {
        _masterContext->postApplication();
      }
      
      VPackBuilder b;
      b.openObject();
      b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
      b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
      b.add(Utils::storeResultsKey, VPackValue(storeResults));
      b.close();
      _sendToAllDBServers(Utils::finalizeExecutionPath, b.slice());
      
      LOG(INFO) << "Done. We did " << _globalSuperstep << " rounds";
      LOG(INFO) << "Send: " << _workerStats.sendCount
      << " Received: " << _workerStats.receivedCount;
      LOG(INFO) << "Worker Runtime: " << _workerStats.superstepRuntimeSecs << "s";
      LOG(INFO) << "Total Runtime: " << totalRuntimeSecs() << "s";
      
    } else {  // trigger next superstep
      if (_masterContext) {
        _masterContext->betweenGlobalSuperstep(_globalSuperstep);
      }
      _startGlobalStep();
    }
  }
}

void Conductor::cancel() {
  _state = ExecutionState::CANCELED;
  // TODO tell servers to discard results
}

void Conductor::startRecovery() {
  // we lost a DBServer, we need to reconfigure all remainging servers
  // so they load the data for the lost machine
  
  VPackBuilder b;
  b.openObject();
  b.add(Utils::recoveryMethodKey, VPackValue("optimistic"));
  b.close();
  
  int res = _initializeWorkers(Utils::recoveryPath, b.slice());
  if (res != TRI_ERROR_NO_ERROR) {
    cancel();
    LOG(ERR) << "Recovery proceedings failed";
  }
}

static void resolveShards(LogicalCollection const* collection,
                          std::map<ServerID, std::map<CollectionID,
                          std::vector<ShardID>>> &serverMap) {
  
  ClusterInfo* ci = ClusterInfo::instance();
  std::shared_ptr<std::vector<ShardID>> shardIDs =
  ci->getShardList(collection->cid_as_string());
  
  for (auto const& shard : *shardIDs) {
    std::shared_ptr<std::vector<ServerID>> servers =
    ci->getResponsibleServer(shard);
    if (servers->size() > 0) {
      serverMap[(*servers)[0]][collection->name()].push_back(shard);
    }
  }
}

/// should cause workers to start a new execution or begin with recovery proceedings
int Conductor::_initializeWorkers(std::string const& suffix, VPackSlice additional) {
  int64_t vertexCount = 0, edgeCount = 0;
  std::map<CollectionID, std::string> collectionPlanIdMap;
  std::map<ServerID, std::map<CollectionID, std::vector<ShardID>>> vertexMap, edgeMap;
  // resolve plan id's and shards on the servers
  for (auto &collection : _vertexCollections) {
    collectionPlanIdMap.emplace(collection->name(), collection->planId_as_string());
    int64_t cc =
    Utils::countDocuments(_vocbaseGuard.vocbase(), collection->name());
    if (cc > 0) {
      vertexCount += cc;
      resolveShards(collection.get(), vertexMap);
    }
  }
  for (auto &collection : _edgeCollections) {
    collectionPlanIdMap.emplace(collection->name(), collection->planId_as_string());
    int64_t cc =
    Utils::countDocuments(_vocbaseGuard.vocbase(), collection->name());
    if (cc > 0) {
      edgeCount += cc;
      resolveShards(collection.get(), edgeMap);
    }
  }
  for (auto const& pair : vertexMap) {
    _dbServers.push_back(pair.first);
  }
  
  LOG(INFO) << vertexCount << " vertices, " << edgeCount << " edges";
  if (vertexMap.size() != edgeMap.size()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
                                   TRI_ERROR_BAD_PARAMETER,
                                   "Vertex and edge collections are not sharded correctly");
  }
  
  // Need to do this here, because we need the counts
  if (_masterContext && _masterContext->_vertexCount == 0 ) {
    _masterContext->_vertexCount = vertexCount;
    _masterContext->_edgeCount = edgeCount;
    _masterContext->_aggregators = _aggregatorUsage.get();
    _masterContext->preApplication();
  }
  
  
  std::string const path = Utils::baseUrl(_vocbaseGuard.vocbase()->name()) + suffix;
  std::string coordinatorId = ServerState::instance()->getId();
  LOG(INFO) << "My id: " << coordinatorId;
  std::vector<ClusterCommRequest> requests;
  for (auto const& it : vertexMap) {
    ServerID const& server = it.first;
    std::map<CollectionID, std::vector<ShardID>> const& vertexShardMap = it.second;
    std::map<CollectionID, std::vector<ShardID>> const& edgeShardMap = edgeMap[it.first];
    
    VPackBuilder b;
    b.openObject();
    b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
    b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
    b.add(Utils::algorithmKey, VPackValue(_algorithm->name()));
    b.add(Utils::userParametersKey, _userParams.slice());
    b.add(Utils::coordinatorIdKey, VPackValue(coordinatorId));
    b.add(Utils::totalVertexCount, VPackValue(vertexCount));
    b.add(Utils::totalEdgeCount, VPackValue(edgeCount));
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
    b.close();
    
    auto body = std::make_shared<std::string const>(b.toJson());
    requests.emplace_back("server:" + server, rest::RequestType::POST, path, body);
  }
  
  ClusterComm* cc = ClusterComm::instance();
  size_t nrDone = 0;
  size_t nrGood = cc->performRequests(requests, 5.0 * 60.0, nrDone, LogTopic("Pregel Conductor"));
  printResults(requests);
  return nrGood == requests.size() ? TRI_ERROR_NO_ERROR : TRI_ERROR_FAILED;
}

int Conductor::_sendToAllDBServers(std::string const& suffix, VPackSlice const& message) {
  ClusterComm* cc = ClusterComm::instance();
  _responseCount = 0;
  _doneCount = 0;
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
  printResults(requests);

  return nrGood == requests.size() ? TRI_ERROR_NO_ERROR : TRI_ERROR_FAILED;
}
