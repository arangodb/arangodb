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
#include "WorkerContext.h"
#include "InMessageCache.h"
#include "OutMessageCache.h"

#include "Basics/MutexLocker.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterComm.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/EdgeCollectionInfo.h"

#include "Indexes/Index.h"
#include "Dispatcher/DispatcherQueue.h"
#include "Dispatcher/DispatcherFeature.h"
#include "Utils/Transaction.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/StandaloneTransactionContext.h"
#include "Utils/OperationCursor.h"
#include "Indexes/EdgeIndex.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include "OutMessageCache.h"

using namespace arangodb;
using namespace arangodb::pregel;

Worker::Worker(unsigned int executionNumber,
               TRI_vocbase_t *vocbase,
               VPackSlice s) : _vocbase(vocbase), _ctx(new WorkerContext) {

  //VPackSlice algo = s.get("algo");
  
  VPackSlice coordID = s.get(Utils::coordinatorIdKey);
  VPackSlice vertexShardIDs = s.get(Utils::vertexShardsListKey);
  VPackSlice edgeShardIDs = s.get(Utils::edgeShardsListKey);
  // TODO support more shards
  if (!(coordID.isString() && vertexShardIDs.length() == 1 && edgeShardIDs.length() == 1)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "Only one shard per collection supported");
  }
    _ctx->_executionNumber = executionNumber;
    _ctx->_coordinatorId = coordID.copyString();
    _ctx->_database = vocbase->name();
  _ctx->_vertexCollectionName = s.get(Utils::vertexCollectionKey).copyString();// readable name of collection
  _ctx->_vertexShardID = vertexShardIDs.at(0).copyString();
  _ctx->_edgeShardID = edgeShardIDs.at(0).copyString();
  LOG(INFO) << "Received collection " << _ctx->_vertexCollectionName;
  LOG(INFO) << "starting worker with (" << _ctx->_vertexShardID << ", " << _ctx->_edgeShardID << ")";
  
  SingleCollectionTransaction *trx = new SingleCollectionTransaction(StandaloneTransactionContext::Create(_vocbase),
                                  _ctx->_vertexShardID, TRI_TRANSACTION_READ);
  int res = trx->begin();
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_FORMAT(res, "while looking up vertices '%s'",
                                  _ctx->_vertexShardID.c_str());
    return;
  }
  // resolve planId
   _ctx->_vertexCollectionPlanId = trx->documentCollection()->planId_as_string();
  
  OperationResult result = trx->all( _ctx->_vertexShardID, 0, UINT64_MAX, OperationOptions());
  // Commit or abort.
  res = trx->finish(result.code);
  if (!result.successful()) {
    THROW_ARANGO_EXCEPTION_FORMAT(result.code, "while looking up graph '%s'",  _ctx->_vertexCollectionName.c_str());
  }
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_FORMAT(res, "while looking up graph '%s'",  _ctx->_vertexCollectionName.c_str());
  }
  VPackSlice vertices = result.slice();
  if (vertices.isExternal()) {
    vertices = vertices.resolveExternal();
  }
  _transactions.push_back(trx);// store transactions, otherwise VPackSlices become invalid
  
  // ======= Look up edges
  
  trx = new SingleCollectionTransaction(StandaloneTransactionContext::Create(_vocbase),
                                         _ctx->_edgeShardID, TRI_TRANSACTION_READ);
  res = trx->begin();
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_FORMAT(res, "while looking up edges '%s'",  _ctx->_edgeShardID.c_str());
  }
  _transactions.push_back(trx);
  
  auto info = std::make_unique<arangodb::traverser::EdgeCollectionInfo>(trx,  _ctx->_edgeShardID, TRI_EDGE_OUT,
                                                                        StaticStrings::FromString, 0);
  
  size_t edgeCount = 0;
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
    
    std::string key =  _ctx->_vertexCollectionName+"/"+vertexId;// TODO geht das schneller
    LOG(INFO) << "Retrieving edge " << key;
    
    auto cursor = info->getEdges(key);
    if (cursor->failed()) {
      THROW_ARANGO_EXCEPTION_FORMAT(cursor->code, "while looking up edges '%s' from %s",
                                    key.c_str(),  _ctx->_edgeShardID.c_str());
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
        
        VPackSlice i = s.get("value");
        v->_edges.emplace_back(s);
        v->_edges.end()->_value = i.isInteger() ? i.getInt() : 1;
        edgeCount++;
        
      }
    }
    LOG(INFO) << "done retrieving edge";
    
    v.release();
  }
  trx->finish(res);
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_FORMAT(res, "after looking up edges '%s'",  _ctx->_edgeShardID.c_str());
  }
  
  LOG(INFO) << "Resolved " << _vertices.size() << " vertices";
  LOG(INFO) << "Resolved " << edgeCount << " edges";
}

Worker::~Worker() {
  LOG(INFO) << "worker deconstructor";
  for (auto const &it : _vertices) {
    delete(it.second);
  }
  _vertices.clear();
  for (auto const &it : _transactions) {// clean transactions
    delete(it);
  }
  _transactions.clear();
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
    unsigned int expected = _ctx->_globalSuperstep + 1;
    if (gss != 0 && expected != gss) {
        THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_BAD_PARAMETER, "Seems like this worker missed a gss, expected %u. Data = %s ",
                                      expected, data.toJson().c_str());
    }
  _ctx->_globalSuperstep = gss;
  _ctx->readableIncomingCache()->clear();
  _ctx->swapIncomingCaches();// write cache becomes the readable cache
  
  std::unique_ptr<rest::Job> job(new WorkerJob(this, _ctx));
  DispatcherFeature::DISPATCHER->addJob(job, false);
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
    
  LOG(INFO) << "Worker received messages\n";
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
    LOG(INFO) << b.toString();
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


// ========== WorkerJob ==========

WorkerJob::WorkerJob(Worker *worker, std::shared_ptr<WorkerContext> ctx) : Job("Pregel Job"), _worker(worker), _ctx(ctx) {
}

void WorkerJob::work() {
  if (_canceled) {
    return;
  }
  LOG(INFO) << "Worker job started\n";
  // TODO cache this
  OutMessageCache outCache(_ctx);

  unsigned int gss = _ctx->globalSuperstep();
  bool isDone = true;

  if (gss == 0) {
    isDone = false;
    
    for (auto const &it : _worker->_vertices) {
      Vertex *v = it.second;
      std::string key = v->_data.get(StaticStrings::KeyString).copyString();
      
      VPackSlice messages = _ctx->readableIncomingCache()->getMessages(key);
      v->compute(gss, MessageIterator(messages), &outCache);
      bool active = v->state() == VertexActivationState::ACTIVE;
      if (!active) LOG(INFO) << "vertex has halted";
      _worker->_activationMap[it.first] = active;
    }
  } else {
    for (auto &it : _worker->_activationMap) {
      VPackSlice messages = _ctx->readableIncomingCache()->getMessages(it.first);
      MessageIterator iterator(messages);
      if (iterator.size() > 0 || it.second) {
        isDone = false;
        
        Vertex *v = _worker->_vertices[it.first];
        v->compute(gss, iterator, &outCache);
        it.second = v->state() == VertexActivationState::ACTIVE;
      }
    }
  }
  LOG(INFO) << "Finished executing vertex programs.";

  if (_canceled) {
    return;
  }
  
  // ==================== send messages to other shards ====================
  
  if (!isDone) {
      outCache.sendMessages();
  } else {
    LOG(INFO) << "Worker job has nothing more to process\n";
  }
  
  // notify the conductor that we are done.
  VPackBuilder package;
  package.openObject();
  package.add(Utils::senderKey, VPackValue(ServerState::instance()->getId()));
  package.add(Utils::executionNumberKey, VPackValue(_ctx->executionNumber()));
  package.add(Utils::globalSuperstepKey, VPackValue(gss));
  if (!isDone) package.add(Utils::doneKey, VPackValue(isDone));
  package.close();
  
  LOG(INFO) << "Sending finishedGSS to coordinator: " << _ctx->coordinatorId();
  // TODO handle communication failures?
    
    ClusterComm *cc =  ClusterComm::instance();
  std::string baseUrl = Utils::baseUrl(_worker->_vocbase->name());
  CoordTransactionID coordinatorTransactionID = TRI_NewTickServer();
  auto headers =
  std::make_unique<std::unordered_map<std::string, std::string>>();
  auto body = std::make_shared<std::string const>(package.toJson());
  cc->asyncRequest("", coordinatorTransactionID,
                   "server:" + _ctx->coordinatorId(),
                   rest::RequestType::POST,
                   baseUrl + Utils::finishedGSSPath,
                   body, headers, nullptr, 90.0);
  
  LOG(INFO) << "Worker job finished sending stuff";
}

bool WorkerJob::cancel() {
  _canceled = true;
  return true;
}

void WorkerJob::cleanup(rest::DispatcherQueue* queue) {
  queue->removeJob(this);
  delete this;
}

void WorkerJob::handleError(basics::Exception const& ex) {}
