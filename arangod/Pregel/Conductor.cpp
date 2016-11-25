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

#include "Conductor.h"
#include "Utils.h"

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

#include "Pregel/Algorithm.h"
#include "Pregel/Algos/PageRank.h"
#include "Pregel/Algos/SSSP.h"

#include <algorithm>

using namespace arangodb;
using namespace arangodb::pregel;

static IAggregatorCreator* resolveAlgorithm(std::string name,
                                            VPackSlice userParams) {
  if ("sssp" == name) {
    return new algos::SSSPAlgorithm(userParams);
  } else if ("pagerank" == name) {
    return new algos::PageRankAlgorithm(userParams);
  }
  return nullptr;
}

Conductor::Conductor(
    uint64_t executionNumber, TRI_vocbase_t* vocbase,
    std::vector<std::shared_ptr<LogicalCollection>> const& vertexCollections,
    std::vector<std::shared_ptr<LogicalCollection>> const& edgeCollections,
    std::string const& algorithm)
    : _vocbaseGuard(vocbase),
      _executionNumber(executionNumber),
      _algorithm(algorithm),
      _recoveryManager(this),
      _vertexCollections(vertexCollections),
      _edgeCollections(edgeCollections) {
  bool isCoordinator = ServerState::instance()->isCoordinator();
  TRI_ASSERT(isCoordinator);
  LOG(INFO) << "constructed conductor";
  std::vector<std::string> algos{"sssp", "pagerank"};
  if (std::find(algos.begin(), algos.end(), _algorithm) == algos.end()) {
    LOG(ERR) << "Unsupported algorithm";
  }
}

Conductor::~Conductor() {}

static void printResults(std::vector<ClusterCommRequest> const& requests) {
  for (auto const& req : requests) {
    auto& res = req.result;
    if (res.status == CL_COMM_RECEIVED
        && res.answer_code != rest::ResponseCode::OK) {
      LOG(ERR) << req.destination << ": " << res.answer->payload().toJson();
    }
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

void Conductor::start(VPackSlice userConfig) {
  if (!userConfig.isObject()) {
    userConfig = VPackSlice::emptyObjectSlice();
  }
  _agregatorCreator.reset(resolveAlgorithm(_algorithm, userConfig));
  if (!_agregatorCreator) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Algorithm not found");
  }

  _aggregatorUsage.reset(new AggregatorUsage(_agregatorCreator.get()));
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
  
  _startTimeSecs = TRI_microtime();
  _globalSuperstep = 0;
  _state = ExecutionState::RUNNING;
  _responseCount = 0;
  _doneCount = 0;
  if (vertexMap.size() != edgeMap.size()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "Vertex and edge collections are not sharded correctly");
  }

  std::string const baseUrl = Utils::baseUrl(_vocbaseGuard.vocbase()->name());
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
    b.add(Utils::globalSuperstepKey, VPackValue(0));
    b.add(Utils::algorithmKey, VPackValue(_algorithm));
    b.add(Utils::userParametersKey, userConfig);
    b.add(Utils::coordinatorIdKey, VPackValue(coordinatorId));
    b.add(Utils::totalVertexCount, VPackValue(vertexCount));
    b.add(Utils::totalEdgeCount, VPackValue(edgeCount));
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
    b.close();


    auto body = std::make_shared<std::string const>(b.toJson());
    requests.emplace_back("server:" + server, rest::RequestType::POST,
                          baseUrl + Utils::startExecutionPath, body);
  }

  ClusterComm* cc = ClusterComm::instance();
  size_t nrDone = 0;
  cc->performRequests(requests, 5.0 * 60.0, nrDone, LogTopic("Pregel Conductor"));
  LOG(INFO) << "Send messages to " << nrDone << " shards of "
            << _vertexCollections[0]->name();
  // look at results
  printResults(requests);

  if (nrDone == requests.size()) {
    if (_startGlobalStep()) {
      _recoveryManager.monitorDBServers(_dbServers);
    }
  } else {
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

  std::string baseUrl = Utils::baseUrl(_vocbaseGuard.vocbase()->name());
  // first allow all workers to run worker level operations
  int r = _sendToAllDBServers(baseUrl + Utils::prepareGSSPath, b.slice());

  if (r == TRI_ERROR_NO_ERROR) {
    // start vertex level operations, does not get a response
    _sendToAllDBServers(baseUrl + Utils::startGSSPath, b.slice());// call me maybe
    LOG(INFO) << "Conductor started new gss " << _globalSuperstep;
    return true;
  } else {
    LOG(INFO) << "Seems there is at least one worker out of order";
    // TODO, in case a worker needs more than 5 minutes to do calculations
    // this will be triggered as well
    // TODO handle cluster failures
    return false;
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

  VPackSlice isDone = data.get(Utils::doneKey);
  if (isDone.isBool() && isDone.getBool()) {
    _doneCount++;
  }

  if (_responseCount == _dbServers.size()) {
    LOG(INFO) << "Finished gss " << _globalSuperstep;
    _globalSuperstep++;

    if (_state != ExecutionState::RUNNING
        || _doneCount == _dbServers.size()
        || _globalSuperstep == 100) {
      
      _endTimeSecs = TRI_microtime();
      bool storeResults = _state == ExecutionState::RUNNING;
      if (_state == ExecutionState::CANCELED) {
        LOG(WARN) << "Execution was canceled, results will be discarded.";
      } else {
        _state = ExecutionState::DONE;
      }
      
      VPackBuilder b;
      b.openObject();
      b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
      b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
      b.add(Utils::storeResultsKey, VPackValue(storeResults));
      b.close();
      std::string baseUrl = Utils::baseUrl(_vocbaseGuard.vocbase()->name());
      _sendToAllDBServers(baseUrl + Utils::finalizeExecutionPath, b.slice());
      
      LOG(INFO) << "Done. We did " << _globalSuperstep << " rounds";
      LOG(INFO) << "Send: " << _workerStats.sendCount
      << " Received: " << _workerStats.receivedCount;
      LOG(INFO) << "Worker Runtime: " << _workerStats.superstepRuntimeSecs << "s";
      LOG(INFO) << "Total Runtime: " << totalRuntimeSecs() << "s";
      
    } else {  // trigger next superstep
      _startGlobalStep();
    }
  }
}

void Conductor::cancel() {_state = ExecutionState::CANCELED; }

int Conductor::_sendToAllDBServers(std::string path, VPackSlice const& config) {
  ClusterComm* cc = ClusterComm::instance();
  _responseCount = 0;
  _doneCount = 0;
  if (_dbServers.size() == 0) {
    LOG(WARN) << "No servers registered";
    return TRI_ERROR_FAILED;
  }

  auto body = std::make_shared<std::string const>(config.toJson());
  std::vector<ClusterCommRequest> requests;
  for (auto const& server : _dbServers) {
    requests.emplace_back("server:" + server, rest::RequestType::POST, path,
                          body);
  }

  size_t nrDone = 0;
  size_t nrGood = cc->performRequests(requests, 5.0 * 60.0, nrDone,
                                      LogTopic("Pregel Conductor"));
  LOG(INFO) << "Send messages to " << nrDone << " servers";
  printResults(requests);

  return nrGood == requests.size() ? TRI_ERROR_NO_ERROR : TRI_ERROR_FAILED;
}
