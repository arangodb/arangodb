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

#include "Worker.h"
#include "Vertex.h"

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

Worker::Worker(int executionNumber,
               TRI_vocbase_t *vocbase,
               VPackSlice &s) {

  LOG(DEBUG) << "starting worker";
  //VPackSlice algo = s.get("algo");
  string vertexCollection = s.get("vertex").copyString();
  string edgeCollection = s.get("edge").copyString();
  
  
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
  
  
  SingleCollectionTransaction trx2(StandaloneTransactionContext::Create(vocbase),
                                  edgeCollection, TRI_TRANSACTION_READ);
  res = trx2.begin();
  
  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "cannot start transaction to load authentication";
    return;
  }
  
  OperationResult result2 = trx2.all(edgeCollection, 0, UINT64_MAX, OperationOptions());
  // Commit or abort.
  res = trx.finish(result.code);
  
  if (!result2.successful() || res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_FORMAT(result2.code, "while looking up graph '%s'",
                                  edgeCollection.c_str());
  }
  VPackSlice edges = result.slice();
  if (edges.isExternal()) {
    edges = vertices.resolveExternal();
  }

  /*
  
  for (auto const &it : velocypack::ArrayIterator(vertices)) {
    
    Vertex v(it);

    
    for (auto const& it : edges) {
      Edge e;
      e.edgeId = it.get(StaticStrings::IdString).copyString();
      e.toId = it.get(StaticStrings::ToString).copyString();
      e.value = it.get("value").getInt();
      v.edges.push_back(e);
    }
    
  }*/
}

void Worker::nextGlobalStep(VPackSlice &data) {
    // TODO do some work?
    VPackSlice gss = data.get("gss");
    
    if (gss.isInt() && gss.getInt() == 0) {
      VPackSlice cid = data.get("coordinator");
      if (cid.isString())
        _coordinatorId = cid.copyString();
    }
    
    // TODO queue tasks
  
}
