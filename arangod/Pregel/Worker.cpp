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
               VPackSlice s) : _vocbase(vocbase), _executionNumber(executionNumber) {

  //VPackSlice algo = s.get("algo");
  
  VPackSlice coordID = s.get(Utils::coordinatorIdKey);
  VPackSlice vertexShardIDs = s.get(Utils::vertexShardsListKey);
  VPackSlice edgeShardIDs = s.get(Utils::edgeShardsListKey);
  // TODO support more shards
  if (!(coordID.isString() && vertexShardIDs.length() == 1 && edgeShardIDs.length() == 1)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "Only one shard per collection supported");
  }
  _coordinatorId = coordID.copyString();
  _vertexCollectionName = s.get(Utils::vertexCollectionKey).copyString();// readable name of collection
  _vertexShardID = vertexShardIDs.at(0).copyString();
  _edgeShardID = edgeShardIDs.at(0).copyString();
  LOG(INFO) << "Received collection " << _vertexCollectionName;
  LOG(INFO) << "starting worker with (" << _vertexShardID << ", " << _edgeShardID << ")";
  
  _cache1 = new InMessageCache();
  _cache2 = new InMessageCache();
  _currentCache = _cache1;
  
  SingleCollectionTransaction *trx = new SingleCollectionTransaction(StandaloneTransactionContext::Create(_vocbase),
                                  _vertexShardID, TRI_TRANSACTION_READ);
  int res = trx->begin();
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_FORMAT(res, "while looking up vertices '%s'",
                                  _vertexShardID.c_str());
    return;
  }
  // resolve planId
  _vertexCollectionPlanId = trx->documentCollection()->planId_as_string();
  
  OperationResult result = trx->all(_vertexShardID, 0, UINT64_MAX, OperationOptions());
  // Commit or abort.
  res = trx->finish(result.code);
  if (!result.successful()) {
    THROW_ARANGO_EXCEPTION_FORMAT(result.code, "while looking up graph '%s'", _vertexCollectionName.c_str());
  }
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_FORMAT(res, "while looking up graph '%s'", _vertexCollectionName.c_str());
  }
  VPackSlice vertices = result.slice();
  if (vertices.isExternal()) {
    vertices = vertices.resolveExternal();
  }
  _transactions.push_back(trx);// store transactions, otherwise VPackSlices become invalid
  
  // ======= Look up edges
  
  trx = new SingleCollectionTransaction(StandaloneTransactionContext::Create(_vocbase),
                                        _edgeShardID, TRI_TRANSACTION_READ);
  res = trx->begin();
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_FORMAT(res, "while looking up edges '%s'", _edgeShardID.c_str());
  }
  _transactions.push_back(trx);
  
  auto info = std::make_unique<arangodb::traverser::EdgeCollectionInfo>(trx, _edgeShardID, TRI_EDGE_OUT,
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
    
    std::string key = _vertexCollectionName+"/"+vertexId;// TODO geht das schneller
    LOG(INFO) << "Retrieving edge " << key;
    
    auto cursor = info->getEdges(key);
    if (cursor->failed()) {
      THROW_ARANGO_EXCEPTION_FORMAT(cursor->code, "while looking up edges '%s' from %s",
                                    key.c_str(), _edgeShardID.c_str());
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
    THROW_ARANGO_EXCEPTION_FORMAT(res, "after looking up edges '%s'", _edgeShardID.c_str());
  }
  
  LOG(INFO) << "Resolved " << _vertices.size() << " vertices";
  LOG(INFO) << "Resolved " << edgeCount << " edges";
}

Worker::~Worker() {
  for (auto const &it : _vertices) {
    delete(it.second);
  }
  _vertices.clear();
  for (auto const &it : _transactions) {// clean transactions
    delete(it);
  }
  _transactions.clear();
}

void Worker::nextGlobalStep(VPackSlice data) {
  LOG(INFO) << "Called next global step: " << data.toString();
  
  // TODO do some work?
  VPackSlice gssSlice = data.get(Utils::globalSuperstepKey);
  if (!gssSlice.isInteger()) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_BAD_PARAMETER,
                                  "Invalid gss in %s:%d", __FILE__, __LINE__);
  }
  uint64_t gss = gssSlice.getUInt();
  _globalSuperstep = gss;
  InMessageCache *inCache = _currentCache;
  if (_currentCache == _cache1) {
    _currentCache = _cache2;
  } else {
    _currentCache = _cache1;
  }
  
  std::unique_ptr<rest::Job> job(new WorkerJob(this, inCache));
  DispatcherFeature::DISPATCHER->addJob(job, false);
  LOG(INFO) << "Worker started new gss computation: " << gss;
}

void Worker::receivedMessages(VPackSlice data) {
  MUTEX_LOCKER(locker, _messagesMutex);
  
  VPackSlice gssSlice = data.get(Utils::globalSuperstepKey);
  VPackSlice messageSlice = data.get(Utils::messagesKey);
  if (!gssSlice.isInt() || !messageSlice.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "Bad parameters in body");
  }
  int64_t gss = gssSlice.getInt();
  if (gss != _globalSuperstep+1) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "Superstep out of sync");
  }
  
  _currentCache->addMessages(VPackArrayIterator(messageSlice));
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

static void waitForResults(std::vector<ClusterCommRequest> &requests) {
  for (auto const& req : requests) {
    auto& res = req.result;
    if (res.status == CL_COMM_RECEIVED) {
      LOG(INFO) << res.answer->payload().toJson();
    }
  }
}

WorkerJob::WorkerJob(Worker *worker, InMessageCache *inCache) : Job("Pregel Job"), _worker(worker), _inCache(inCache) {
}

void WorkerJob::work() {
  if (_canceled) {
    return;
  }
  LOG(INFO) << "Worker job started\n";
  // TODO cache this
  OutMessageCache outCache(_worker->_vertexCollectionPlanId);

  unsigned int gss = _worker->_globalSuperstep;
  bool isDone = true;

  if (gss == 0) {
    isDone = false;
    
    for (auto const &it : _worker->_vertices) {
      Vertex *v = it.second;
      std::string key = v->_data.get(StaticStrings::KeyString).copyString();
      
      VPackSlice messages = _inCache->getMessages(key);
      v->compute(gss, MessageIterator(messages), &outCache);
      bool active = v->state() == VertexActivationState::ACTIVE;
      if (!active) LOG(INFO) << "vertex has halted";
      _worker->_activationMap[it.first] = active;
    }
  } else {
    for (auto &it : _worker->_activationMap) {
      VPackSlice messages = _inCache->getMessages(it.first);
      MessageIterator iterator(messages);
      if (iterator.size() > 0 || it.second) {
        isDone = false;
        
        Vertex *v = _worker->_vertices[it.first];
        v->compute(gss, iterator, &outCache);
        it.second = v->state() == VertexActivationState::ACTIVE;
      }
    }
  }
  LOG(INFO) << "Worker job computations done";

  if (_canceled) {
    return;
  }
  
  // ==================== send messages to other shards ====================
  ClusterComm *cc =  ClusterComm::instance();
  ClusterInfo *ci = ClusterInfo::instance();
  std::string baseUrl = Utils::baseUrl(_worker->_vocbase);
  
  if (!isDone) {
    LOG(INFO) << "Sending messages to shards";
    std::shared_ptr<std::vector<ShardID>> shards = ci->getShardList(_worker->_vertexCollectionPlanId);
    LOG(INFO) << "Seeing shards: " << shards->size();

    std::vector<ClusterCommRequest> requests;
    for (auto const &it : *shards) {
      
      VPackBuilder messages;
      outCache.getMessages(it, messages);
      if (_worker->_vertexShardID == it) {
        LOG(INFO) << "Worker: Getting messages for myself";
        _worker->_currentCache->addMessages(VPackArrayIterator(messages.slice()));
      } else {
        LOG(INFO) << "Worker: Sending messages for shard " << it;

        VPackBuilder package;
        package.openObject();
        package.add(Utils::senderKey, VPackValue(ServerState::instance()->getId()));
        package.add(Utils::executionNumberKey, VPackValue(_worker->_executionNumber));
        package.add(Utils::globalSuperstepKey, VPackValue(gss+1));
        package.add(Utils::messagesKey, messages.slice());
        package.close();
        // add a request
        auto body = std::make_shared<std::string const>(package.toJson());
        requests.emplace_back("shard:" + it, rest::RequestType::POST, baseUrl + Utils::messagesPath, body);
      }
    }
    size_t nrDone = 0;
    cc->performRequests(requests, 120, nrDone, LogTopic("Pregel message transfer"));
    waitForResults(requests);
  } else {
    LOG(INFO) << "Worker job has nothing more to process\n";
  }
  
  // notify the conductor that we are done.
  VPackBuilder package;
  package.openObject();
  package.add(Utils::senderKey, VPackValue(ServerState::instance()->getId()));
  package.add(Utils::executionNumberKey, VPackValue(_worker->_executionNumber));
  if (!isDone) package.add(Utils::doneKey, VPackValue(isDone));
  package.close();
  
  LOG(INFO) << "Sending finishedGSS to coordinator: " << _worker->_coordinatorId;
  // TODO handle communication failures?
  CoordTransactionID coordinatorTransactionID = TRI_NewTickServer();
  auto headers =
  std::make_unique<std::unordered_map<std::string, std::string>>();
  auto body = std::make_shared<std::string const>(package.toJson());
  cc->asyncRequest("", coordinatorTransactionID,
                   "server:"+_worker->_coordinatorId,
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
