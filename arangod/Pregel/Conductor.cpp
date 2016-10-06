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

#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterComm.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/StandaloneTransactionContext.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace std;
using namespace arangodb;
using namespace arangodb::pregel;

Conductor::Conductor(int executionNumber,
                     TRI_vocbase_t *vocbase,
                     CollectionID const& vertexCollection,
                     CollectionID const& edgeCollection,
                     std::string const& algorithm) : _executionNumber(executionNumber), _vocbase(vocbase) {
  
  bool isCoordinator = ServerState::instance()->isCoordinator();
  assert(isCoordinator);
  
  LOG(DEBUG) << "start execution as coordinator";
  string coordinatorId = ServerState::instance()->getId();
  
  VPackBuilder b;
  b(VPackValue(VPackValueType::Object));
  b.add(Utils::executionNumberKey, VPackValue(executionNumber));
  b.add(Utils::coordinatorIdKey, VPackValue(coordinatorId));
  b.add(Utils::vertexCollectionKey, VPackValue(vertexCollection));
  b.add(Utils::edgeCollectionKey, VPackValue(edgeCollection));
  b.add(Utils::globalSuperstepKey, VPackValue(0));
  b.add(Utils::algorithmKey, VPackValue(algorithm));
  b.close();
  
  sendToAllDBServers(Utils::nextGSSPath, b.slice());

}

void Conductor::finishedGlobalStep(VPackSlice &data) {
    mtx.lock();
    _responseCount++;
    if (_responseCount >= _dbServerCount) {
      
      _globalSuperstep++;
      
      VPackBuilder b;
      b.add("gss", VPackValue(_globalSuperstep));
      sendToAllDBServers(Utils::nextGSSPath, b.slice());
    }
    
    mtx.unlock();
}

void Conductor::cancel() {
  _state = ExecutionState::ERROR;
}

int Conductor::sendToAllDBServers(std::string url, VPackSlice const& config) {
  
  ClusterInfo* ci = ClusterInfo::instance();
  ClusterComm* cc = ClusterComm::instance();
  CoordTransactionID coordTransactionID = TRI_NewTickServer();
  
  std::vector<ServerID> DBservers = ci->getCurrentDBServers();
  _dbServerCount = DBservers.size();
  _responseCount = 0;
  
  auto body = std::make_shared<std::string const>(config.toString());
  
  for (auto it = DBservers.begin(); it != DBservers.end(); ++it) {
    auto headers =
    std::make_unique<std::unordered_map<std::string, std::string>>();
    // set collection name (shard id)
    cc->asyncRequest("", coordTransactionID, "server:" + *it,
                     arangodb::rest::RequestType::PUT, url, body,
                     headers, nullptr, 120.0);
  }
  
  // Now listen to the results:
  int count;
  int nrok = 0;
  for (count = (int)DBservers.size(); count > 0; count--) {
    auto res = cc->wait("", coordTransactionID, 0, "", 0.0);
    if (res.status == CL_COMM_RECEIVED) {
      if (res.answer_code == arangodb::rest::ResponseCode::OK) {
        nrok++;
      }
    }
  }
  
  if (nrok != (int)DBservers.size()) {
    return TRI_ERROR_INTERNAL;
  }
  
  return TRI_ERROR_NO_ERROR;
}
