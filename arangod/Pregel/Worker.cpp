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
#include "Utils.h"
#include "WorkerJob.h"
#include "WorkerContext.h"
#include "InMessageCache.h"
#include "OutMessageCache.h"

#include "Basics/MutexLocker.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterComm.h"
#include "VocBase/vocbase.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/EdgeCollectionInfo.h"

#include "Dispatcher/DispatcherQueue.h"
#include "Dispatcher/DispatcherFeature.h"
#include "Utils/Transaction.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/StandaloneTransactionContext.h"
#include "Utils/OperationCursor.h"

#include "Indexes/EdgeIndex.h"
#include "Indexes/Index.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include "OutMessageCache.h"

using namespace arangodb;
using namespace arangodb::pregel;

Worker::Worker(unsigned int executionNumber,
               TRI_vocbase_t *vocbase,
               VPackSlice s) : _vocbase(vocbase), _ctx(new WorkerContext(executionNumber)) {

  
  VPackSlice coordID = s.get(Utils::coordinatorIdKey);
    VPackSlice vertexCollName = s.get(Utils::vertexCollectionNameKey);
    VPackSlice vertexCollPlanId = s.get(Utils::vertexCollectionPlanIdKey);
  VPackSlice vertexShardIDs = s.get(Utils::vertexShardsListKey);
  VPackSlice edgeShardIDs = s.get(Utils::edgeShardsListKey);
  //if (!(coordID.isString() && vertexShardIDs.length() == 1 && edgeShardIDs.length() == 1)) {
  //  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "Only one shard per collection supported");
  //}
  if (!coordID.isString()
      || !vertexCollName.isString()
      || !vertexCollPlanId.isString()
      || !vertexShardIDs.isArray()
      || !edgeShardIDs.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "Supplied bad parameters to worker");
  }
  _ctx->_coordinatorId = coordID.copyString();
  _ctx->_database = vocbase->name();
  _ctx->_vertexCollectionName = vertexCollName.copyString();// readable name of collection
  _ctx->_vertexCollectionPlanId = vertexCollPlanId.copyString();
    
   VPackArrayIterator vertices(vertexShardIDs);
    for (VPackSlice shardSlice : vertices) {
        ShardID name = shardSlice.copyString();
        _ctx->_localVertexShardIDs.push_back(name);
        lookupVertices(name);
    }
    VPackArrayIterator edges(edgeShardIDs);
    for (VPackSlice shardSlice : edges) {
        ShardID name = shardSlice.copyString();
        lookupEdges(name);
    }
}

Worker::~Worker() {
  LOG(INFO) << "worker deconstructor";
  for (auto const &it : _vertices) {
    delete(it.second);
  }
  _vertices.clear();
    cleanupReadTransactions();
}

/// @brief Setup next superstep
void Worker::nextGlobalStep(VPackSlice data) {
  LOG(INFO) << "Called next global step: " << data.toJson();
  
  // TODO do some work?
  VPackSlice gssSlice = data.get(Utils::globalSuperstepKey);
  if (!gssSlice.isInteger()) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_BAD_PARAMETER,
                                  "Invalid gss in %s:%d", __FILE__, __LINE__);
  }
    unsigned int gss = (unsigned int) gssSlice.getUInt();
    if (_ctx->_expectedGSS != gss) {
        THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_BAD_PARAMETER, "Seems like this worker missed a gss, expected %u. Data = %s ",
                                      _ctx->_expectedGSS, data.toJson().c_str());
    }
    
  _ctx->_globalSuperstep = gss;
   _ctx->_expectedGSS = gss + 1;
  _ctx->readableIncomingCache()->clear();
  _ctx->swapIncomingCaches();// write cache becomes the readable cache
  
  std::unique_ptr<rest::Job> job(new WorkerJob(this, _ctx));
  int res = DispatcherFeature::DISPATCHER->addJob(job, true);
    if (res != TRI_ERROR_NO_ERROR) {
        LOG(ERR) << "Could not start worker job";
    }
  LOG(INFO) << "Worker started new gss: " << gss;
}

void Worker::receivedMessages(VPackSlice data) {
  LOG(INFO) << "Received message";
  
  VPackSlice gssSlice = data.get(Utils::globalSuperstepKey);
  VPackSlice messageSlice = data.get(Utils::messagesKey);
  if (!gssSlice.isInt() || !messageSlice.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "Bad parameters in body");
  }
  int64_t gss = gssSlice.getInt();
    if (gss == _ctx->_globalSuperstep) {
        _ctx->writeableIncomingCache()->addMessages(VPackArrayIterator(messageSlice));
    } else if (gss == _ctx->_globalSuperstep - 1) {
        LOG(WARN) << "Should not receive messages from last global superstep, during computation phase";
        _ctx->_readCache->addMessages(VPackArrayIterator(messageSlice));
    } else {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "Superstep out of sync");
    }
    
  LOG(INFO) << "Worker received messages";
}

void Worker::writeResults() {
  
  /*SingleCollectionTransaction trx(StandaloneTransactionContext::Create(_vocbaseGuard.vocbase()),
                                  _vertexCollection, TRI_TRANSACTION_WRITE);
  int res = trx.begin();
  
  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "cannot start transaction to load authentication";
    return;
  }*/

  OperationResult result;
  OperationOptions options;
  options.waitForSync = false;
  options.mergeObjects = true;
  for (auto const &pair : _vertices) {
    //TransactionBuilderLeaser b(&trx);
    VPackBuilder b;
    b.openObject();
    b.add(StaticStrings::KeyString, pair.second->_data.get(StaticStrings::KeyString));
    b.add("value", VPackValue(pair.second->_vertexState));
    b.close();
    LOG(INFO) << b.toJson();
    /*result = trx.update(_vertexCollection, b->slice(), options);
    if (!result.successful()) {
      THROW_ARANGO_EXCEPTION_FORMAT(result.code, "while looking up graph '%s'",
                                    _vertexCollection.c_str());
    }*/
  }
  // Commit or abort.
  /*res = trx.finish(result.code);
  
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_FORMAT(res, "while looking up graph '%s'",
                                  _vertexCollection.c_str());
  }*/
}

void Worker::cleanupReadTransactions() {
    for (auto const &it : _readTrxList) {// clean transactions
        if (it->getStatus() == TRI_TRANSACTION_RUNNING) {
            if (it->commit() != TRI_ERROR_NO_ERROR) {
                LOG(WARN) << "Pregel worker: Failed to commit on a read transaction";
            }
        }
        delete(it);
    }
    _readTrxList.clear();
}

void Worker::lookupVertices(ShardID const &vertexShard) {
    
    SingleCollectionTransaction *trx = new SingleCollectionTransaction(StandaloneTransactionContext::Create(_vocbase),
                                                                       vertexShard, TRI_TRANSACTION_READ);
    int res = trx->begin();
    if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION_FORMAT(res, "while looking up vertices '%s'", vertexShard.c_str());
    }
    
    OperationResult result = trx->all(vertexShard, 0, UINT64_MAX, OperationOptions());
    // Commit or abort.
    res = trx->finish(result.code);
    if (!result.successful()) {
        THROW_ARANGO_EXCEPTION_FORMAT(result.code, "while looking up shard '%s'", vertexShard.c_str());
    }
    if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION_FORMAT(res, "while looking up shard '%s'", vertexShard.c_str());
    }
    VPackSlice vertices = result.slice();
    if (vertices.isExternal()) {
        vertices = vertices.resolveExternal();
    }
    _readTrxList.push_back(trx);// store transactions, otherwise VPackSlices become invalid

    VPackArrayIterator arr = VPackArrayIterator(vertices);
    LOG(INFO) << "Found vertices: " << arr.size();
    for (auto it : arr) {
        LOG(INFO) << it.toJson();
        if (it.isExternal()) {
            it = it.resolveExternal();
        }
        
        std::string vertexId = it.get(StaticStrings::KeyString).copyString();
        std::unique_ptr<Vertex> v (new Vertex(it));
        _vertices[vertexId] = v.get();
        v.release();
    }
}

void Worker::lookupEdges(ShardID const &edgeShardID) {
    std::unique_ptr<SingleCollectionTransaction> trx(new SingleCollectionTransaction(StandaloneTransactionContext::Create(_vocbase),
                                                                                     edgeShardID, TRI_TRANSACTION_READ));
    int res = trx->begin();
    if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION_FORMAT(res, "while looking up edges '%s'", edgeShardID.c_str());
    }
    
    auto info = std::make_unique<arangodb::traverser::EdgeCollectionInfo>(trx.get(), edgeShardID, TRI_EDGE_OUT,
                                                                          StaticStrings::FromString, 0);
    
    size_t edgeCount = 0;
    for (auto const &it : _vertices) {
        Vertex *v = it.second;
        
        std::string _from = _ctx->_vertexCollectionName+"/"+it.first;// TODO geht das schneller
        LOG(INFO) << "Retrieving edge _from: " << _from;
        
        auto cursor = info->getEdges(_from);
        if (cursor->failed()) {
            THROW_ARANGO_EXCEPTION_FORMAT(cursor->code, "while looking up edges '%s' from %s",
                                          _from.c_str(), edgeShardID.c_str());
        }
        
        std::vector<TRI_doc_mptr_t*> result;
        result.reserve(1000);
        while (cursor->hasMore()) {
            cursor->getMoreMptr(result, 1000);
            for (auto const& mptr : result) {
                
                VPackSlice s(mptr->vpack());
                if (s.isExternal()) {
                    s = s.resolveExternal();
                }
                LOG(INFO) << s.toJson();
                
                v->_edges.emplace_back(s);
                edgeCount++;
                
            }
        }
    }
    
    _readTrxList.push_back(trx.get());
    trx.release();
}

