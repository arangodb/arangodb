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

using namespace arangodb;
using namespace arangodb::pregel;

Conductor::Conductor(
    unsigned int executionNumber, TRI_vocbase_t* vocbase,
    std::vector<std::shared_ptr<LogicalCollection>> vertexCollections,
    std::vector<std::shared_ptr<LogicalCollection>> edgeCollections,
    std::string const& algorithm)
    : _vocbaseGuard(vocbase),
      _executionNumber(executionNumber),
      _vertexCollections(vertexCollections),
      _edgeCollections(edgeCollections),
      _algorithm(algorithm) {
  bool isCoordinator = ServerState::instance()->isCoordinator();
  assert(isCoordinator);
  LOG(INFO) << "constructed conductor";
}

Conductor::~Conductor() {
  /*for (auto const &it : _aggregators) {
      delete(it.second);
  }
  _aggregators.clear();*/
}

static void printResults(std::vector<ClusterCommRequest> const& requests) {
  for (auto const& req : requests) {
    auto& res = req.result;
    if (res.status == CL_COMM_RECEIVED) {
      LOG(INFO) << res.answer->payload().toJson();
    }
  }
}

static void resolveShards(LogicalCollection const* collection,
                                 std::map<ServerID, std::vector<ShardID>>& serverMap,
                                 std::map<ServerID, std::map<ShardID, std::string>> &serverShardPlanIdMap) {
    
  ClusterInfo* ci = ClusterInfo::instance();
  std::shared_ptr<std::vector<ShardID>> shardIDs =
  ci->getShardList(collection->cid_as_string());

  for (auto const& shard : *shardIDs) {
      std::shared_ptr<std::vector<ServerID>> servers =
      ci->getResponsibleServer(shard);
      if (servers->size() > 0) {
          serverMap[(*servers)[0]].push_back(shard);
        serverShardPlanIdMap[(*servers)[0]][shard] = collection->planId_as_string();
      }
  }
}

void Conductor::start() {
  ClusterComm* cc = ClusterComm::instance();
  int64_t vertexCount = 0, edgeCount = 0;
  std::map<ServerID, std::map<ShardID, std::string>> serverShardPlanIdMap;
  std::map<ServerID, std::vector<ShardID>> edgeServerMap;
  
  for (auto collection : _vertexCollections) {
    size_t cc = Utils::countDocuments(_vocbaseGuard.vocbase(), collection->name());
    if (cc > 0) {
      vertexCount += cc;
      resolveShards(collection.get(), _vertexServerMap, serverShardPlanIdMap);
    } else {
      LOG(WARN) << "Collection does not contain vertices";
    }
  }
  for (auto collection : _edgeCollections) {
    size_t cc = Utils::countDocuments(_vocbaseGuard.vocbase(), collection->name());
    if (cc > 0) {
      edgeCount += cc;
      resolveShards(collection.get(), edgeServerMap, serverShardPlanIdMap);
    } else {
      LOG(WARN) << "Collection does not contain edges";
    }
  }
  
  std::string const baseUrl = Utils::baseUrl(_vocbaseGuard.vocbase()->name());
  _globalSuperstep = 0;
  _state = ExecutionState::RUNNING;
  _dbServerCount = _vertexServerMap.size();
  _responseCount = 0;
  _doneCount = 0;
  if (_vertexServerMap.size() != edgeServerMap.size()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "Vertex and edge collections are not shared correctly");
  }

  std::string coordinatorId = ServerState::instance()->getId();
  LOG(INFO) << "My id: " << coordinatorId;
  std::vector<ClusterCommRequest> requests;
  for (auto const& it : _vertexServerMap) {
    VPackBuilder b;
    b.openObject();
    b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
    b.add(Utils::globalSuperstepKey, VPackValue(0));
    b.add(Utils::algorithmKey, VPackValue(_algorithm));
    b.add(Utils::coordinatorIdKey, VPackValue(coordinatorId));
    b.add(Utils::totalVertexCount, VPackValue(vertexCount));
    b.add(Utils::totalEdgeCount, VPackValue(edgeCount));
    b.add(Utils::vertexShardsListKey, VPackValue(VPackValueType::Array));
    for (ShardID const& vit : it.second) {
      b.add(VPackValue(vit));
    }
    b.close();
    b.add(Utils::edgeShardsListKey, VPackValue(VPackValueType::Array));
    for (ShardID const& eit : edgeServerMap[it.first]) {
      b.add(VPackValue(eit));
    }
    b.close();
    b.add(Utils::shardPlanMapKey, VPackValue(VPackValueType::Object));
    for (auto const& shardPair : serverShardPlanIdMap[it.first]) {
      b.add(shardPair.first, VPackValue(shardPair.second));
    }
    b.close();
    b.close();

    auto body = std::make_shared<std::string const>(b.toJson());
    requests.emplace_back("server:" + it.first, rest::RequestType::POST,
                          baseUrl + Utils::nextGSSPath, body);
  }
  size_t nrDone = 0;
  cc->performRequests(requests, 120.0, nrDone, LogTopic("Pregel Conductor"));
  LOG(INFO) << "Send messages to " << nrDone << " shards of "
            << _vertexCollections[0]->name();
  // look at results
  printResults(requests);
}

void Conductor::finishedGlobalStep(VPackSlice& data) {
  MUTEX_LOCKER(mutexLocker, _finishedGSSMutex);

  LOG(INFO) << "Conductor received finishedGlobalStep callback";
  if (_state != ExecutionState::RUNNING) {
    LOG(WARN) << "Conductor did not expect another finishedGlobalStep call";
    return;
  }

  _responseCount++;
  VPackSlice isDone = data.get(Utils::doneKey);
  if (isDone.isBool() && isDone.getBool()) {
    _doneCount++;
  }

  if (_responseCount == _dbServerCount) {
    LOG(INFO) << "Finished gss " << _globalSuperstep;
    _globalSuperstep++;

    std::string baseUrl = Utils::baseUrl(_vocbaseGuard.vocbase()->name());
    if (_doneCount == _dbServerCount || _globalSuperstep == 101) {
      LOG(INFO) << "Done. We did " << _globalSuperstep - 1 << " rounds";
      VPackBuilder b;
      b.openObject();
      b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
      b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
      b.close();
      sendToAllDBServers(baseUrl + Utils::writeResultsPath, b.slice());
      _state = ExecutionState::FINISHED;

    } else {  // trigger next superstep
      VPackBuilder b;
      b.openObject();
      b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
      b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
      b.close();
      sendToAllDBServers(baseUrl + Utils::nextGSSPath, b.slice());
      LOG(INFO) << "Conductor started new gss " << _globalSuperstep;
    }
  }
}

void Conductor::cancel() { _state = ExecutionState::ERROR; }

int Conductor::sendToAllDBServers(std::string path, VPackSlice const& config) {
  ClusterComm* cc = ClusterComm::instance();
  _dbServerCount = _vertexServerMap.size();
  _responseCount = 0;
  _doneCount = 0;

  if (_dbServerCount == 0) {
    LOG(WARN) << "No shards registered for " << _vertexCollections[0]->name();
    return TRI_ERROR_FAILED;
  }

  auto body = std::make_shared<std::string const>(config.toJson());
  std::vector<ClusterCommRequest> requests;
  for (auto const& it : _vertexServerMap) {
    requests.emplace_back("server:" + it.first, rest::RequestType::POST, path,
                          body);
  }

  size_t nrDone = 0;
  cc->performRequests(requests, 120.0, nrDone, LogTopic("Pregel Conductor"));
  LOG(INFO) << "Send messages to " << nrDone << " shards of "
            << _vertexCollections[0]->name();
  printResults(requests);

  return TRI_ERROR_NO_ERROR;
}

