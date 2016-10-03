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

#include "Execution.h"
#include "WorkerThread.h"

#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterComm.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/StandaloneTransactionContext.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::pregel;

Execution::Execution(int executionNumber,
                     TRI_vocbase_t *vocbase,
                     CollectionID const& vertexCollection,
                     CollectionID const& edgeCollection,
                     std::string const& algorithm) : _executionNumber(executionNumber), _vocbase(vocbase) {
  
  _isCoordinator = ServerState::instance()->isCoordinator();
  if (_isCoordinator) {
    LOG(DEBUG) << "start execution as coordinator";
    _coordinatorId = ServerState::instance()->getId();
    
    VPackBuilder b;
    b(VPackValue(VPackValueType::Object));
    b.add("en", VPackValue(executionNumber));
    b.add("coordinator", VPackValue(_coordinatorId));
    b.add("vertex", VPackValue(vertexCollection));
    b.add("edge", VPackValue(edgeCollection));
    b.add("gss", VPackValue(0));
    b.add("algo", VPackValue(algorithm));
    b.close();
    
    sendToAllDBServers("/_api/pregel/nextGSS", b.slice());
  } else {
    LOG(DEBUG) << "start execution as worker";
    
    SingleCollectionTransaction trx(StandaloneTransactionContext::Create(vocbase),
                                    vertexCollection, TRI_TRANSACTION_READ);
    int res = trx.begin();
    
    if (res != TRI_ERROR_NO_ERROR) {
      LOG(ERR) << "cannot start transaction to load authentication";
      return;
    }
    
    OperationResult result = trx.all(vertexCollection, 0, UINT64_MAX, OperationOptions());
    // Commit or abort.
    res = trx.finish(result.code);
    
    if (!result.successful()) {
      THROW_ARANGO_EXCEPTION_FORMAT(result.code, "while looking up graph '%s'",
                                    vertexCollection.c_str());
    }
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION_FORMAT(res, "while looking up graph '%s'",
                                    vertexCollection.c_str());
    }
    VPackSlice vertices = result.slice();
    if (vertices.isExternal()) {
      vertices = vertices.resolveExternal();
    }
    
    // TODO actually parse vertices?
    _worker = new WorkerThread();
    
  }
}

void Execution::finishedGlobalStep(VPackSlice &data) {
  if (_isCoordinator) {
    mtx.lock();
    _responseCount++;
    if (_responseCount >= _dbServerCount) {
      
      _globalSuperstep++;
      
      VPackBuilder b;
      b.add("gss", VPackValue(_globalSuperstep));
      sendToAllDBServers("/_api/pregel/nextGSS", b.slice());
    }
    
    mtx.unlock();
  }
}

void Execution::nextGlobalStep(VPackSlice &data) {
  if (!_isCoordinator) {
    // TODO do some work?
    VPackSlice gss = data.get("gss");
    
    if (gss.isInt() && gss.getInt() == 0) {
      VPackSlice cid = data.get("coordinator");
      if (cid.isString())
        _coordinatorId = cid.copyString();
    }
    
    // TODO do some work
    
  }
}

void Execution::cancel() {
  _state = ExecutionState::ERROR;
}

int Execution::sendToAllDBServers(std::string url, VPackSlice const& config) {
  
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
