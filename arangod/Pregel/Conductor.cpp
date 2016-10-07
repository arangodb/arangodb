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
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/StandaloneTransactionContext.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterComm.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace std;
using namespace arangodb;
using namespace arangodb::pregel;

Conductor::Conductor(int executionNumber,
                     //TRI_vocbase_t *vocbase,
                     std::string const&vertexCollection,
                     CollectionID vertexCollectionID,
                     std::string const& edgeCollection,
                     std::string const& algorithm) :
_executionNumber(executionNumber),
//_vocbase(vocbase),
_vertexCollection(vertexCollection),
_edgeCollection(edgeCollection),
_vertexCollectionID(vertexCollectionID),
_algorithm(algorithm) {
  
  bool isCoordinator = ServerState::instance()->isCoordinator();
  assert(isCoordinator);
  LOG(INFO) << "constructed conductor";
}

void Conductor::start() {
  _globalSuperstep = 0;
  _state = ExecutionState::RUNNING;
  
  string coordinatorId = ServerState::instance()->getId();
  LOG(INFO) << "My id: " << coordinatorId;
  
  VPackBuilder b;
  b.openObject();
  b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
  b.add(Utils::coordinatorIdKey, VPackValue(coordinatorId));
  b.add(Utils::vertexCollectionKey, VPackValue(_vertexCollection));
  b.add(Utils::edgeCollectionKey, VPackValue(_edgeCollection));
  b.add(Utils::globalSuperstepKey, VPackValue(0));
  b.add(Utils::algorithmKey, VPackValue(_algorithm));
  b.close();
  
  sendToAllShards(Utils::nextGSSPath, b.slice());
}

void Conductor::finishedGlobalStep(VPackSlice &data) {
  MUTEX_LOCKER(locker, writeMutex);

  LOG(INFO) << "Conductor received finished callback";
  if (_state != ExecutionState::RUNNING) {
    LOG(WARN) << "Conductor did not expect another finishedGlobalStep()";
    return;
  }
  
  _responseCount++;
  if (_responseCount >= _dbServerCount) {
    _globalSuperstep++;
    
    if (_globalSuperstep >= 25) {
      LOG(INFO) << "We did 25 rounds";
      VPackBuilder b;
      b.openObject();
      b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
      b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
      b.close();
      sendToAllShards(Utils::writeResultsPath, b.slice());
      _state = ExecutionState::FINISHED;
    } else {
      VPackBuilder b;
      b.openObject();
      b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
      b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
      b.close();
      sendToAllShards(Utils::nextGSSPath, b.slice());
      LOG(INFO) << "Conductor started new gss\n";
    }
  }
}

void Conductor::cancel() {
  _state = ExecutionState::ERROR;
}

int Conductor::sendToAllShards(std::string path, VPackSlice const& config) {
  
  ClusterInfo* ci = ClusterInfo::instance();
  ClusterComm* cc = ClusterComm::instance();
  //CoordTransactionID coordTransactionID = TRI_NewTickServer();
  
  
  
  shared_ptr<vector<ShardID>> shardIDs = ci->getShardList(_vertexCollectionID);// ->getCurrentDBServers();
  _dbServerCount = shardIDs->size();
  _responseCount = 0;
  
  if (shardIDs->size() == 0) {
    LOG(WARN) << "No shards registered for " << _vertexCollection;
    return TRI_ERROR_FAILED;
  }
  
  auto body = std::make_shared<std::string const>(config.toJson());
  vector<ClusterCommRequest> requests;
  for (auto it = shardIDs->begin(); it != shardIDs->end(); ++it) {
    requests.emplace_back("shard:" + *it, rest::RequestType::POST, path, body);
  }
  
  LOG(INFO) << "Constructed requests";

  size_t nrDone = 0;
  cc->performRequests(requests, 120.0, nrDone, LogTopic("Pregel Conductor"));
  LOG(INFO) << "Send messages to " << nrDone << " shards of " << _vertexCollection;

  return TRI_ERROR_NO_ERROR;
}
