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

#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterComm.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"
#include "Indexes/Index.h"
#include "Dispatcher/DispatcherQueue.h"
#include "Dispatcher/DispatcherFeature.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/StandaloneTransactionContext.h"
#include "Indexes/EdgeIndex.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include "OutMessageCache.h"

using namespace arangodb;
using namespace arangodb::pregel;

Worker::Worker(int executionNumber,
               TRI_vocbase_t *vocbase,
               VPackSlice s) : _vocbaseGuard(vocbase), _executionNumber(executionNumber) {

  LOG(DEBUG) << "starting worker";
  //VPackSlice algo = s.get("algo");
  _vertexCollection = s.get("vertex").copyString();
  _edgeCollection = s.get("edge").copyString();
  
  _cache1 = new InMessageCache();
  _cache2 = new InMessageCache();
  _currentCache = _cache1;
  
  
  SingleCollectionTransaction trx(StandaloneTransactionContext::Create(vocbase),
                                  _vertexCollection, TRI_TRANSACTION_READ);
  int res = trx.begin();
  
  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "cannot start transaction to load authentication";
    return;
  }
  
  OperationResult result = trx.all(_vertexCollection, 0, UINT64_MAX, OperationOptions());
  // Commit or abort.
  res = trx.finish(result.code);
  
  if (!result.successful()) {
    THROW_ARANGO_EXCEPTION_FORMAT(result.code, "while looking up graph '%s'",
                                  _vertexCollection.c_str());
  }
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_FORMAT(res, "while looking up graph '%s'",
                                  _vertexCollection.c_str());
  }
  VPackSlice vertices = result.slice();
  if (vertices.isExternal()) {
    vertices = vertices.resolveExternal();
  }
  
  
  SingleCollectionTransaction trx2(StandaloneTransactionContext::Create(vocbase),
                                  _edgeCollection, TRI_TRANSACTION_READ);
  res = trx2.begin();
  
  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "cannot start transaction to load authentication";
    return;
  }
  
  /*OperationResult result2 = trx2.all(edgeCollection, 0, UINT64_MAX, OperationOptions());
  // Commit or abort.
  res = trx.finish(result.code);
  
  if (!result2.successful() || res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_FORMAT(result2.code, "while looking up graph '%s'",
                                  edgeCollection.c_str());
  }
  VPackSlice edges = result.slice();
  if (edges.isExternal()) {
    edges = vertices.resolveExternal();
  }*/
  
  SingleCollectionTransaction::IndexHandle handle = trx2.edgeIndexHandle(_edgeCollection);
  if (handle.isEdgeIndex()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
  
  std::shared_ptr<Index> edgeIndexPtr = handle.getIndex();
  EdgeIndex* edgeIndex = static_cast<EdgeIndex*>(edgeIndexPtr.get());
  
  
  /*map<std::string, vector<Edge*>> sortBucket;// TODO hash_map ?
  for (auto const& it : velocypack::ArrayIterator(edges)) {
    Edge *e = new Edge();
    e->edgeId = it.get(StaticStrings::IdString).copyString();
    e->toId = it.get(StaticStrings::ToString).copyString();
    e->value = it.get("value").getInt();
    sortBucket[e->toId].push_back(e);
  }*/
  for (auto const &it : velocypack::ArrayIterator(vertices)) {
    Vertex *v = new Vertex(it);
    _vertices[v->documentId] = v;
    
    //v._edges = sortBucket[v.documentId];
    
    TransactionBuilderLeaser b(&trx2);
    b->openArray();
    b->add(it.get(StaticStrings::IdString));
    b->add(VPackValue(VPackValueType::Null));
    b->close();
    IndexIterator* vit = edgeIndex->iteratorForSlice(&trx2, nullptr, b->slice(), false);
    TRI_doc_mptr_t *edgePack;
    while((edgePack = vit->next()) != nullptr) {
      VPackSlice s(edgePack->vpack());
      
      std::unique_ptr<Edge> e(new Edge());
      e->edgeId = it.get(StaticStrings::IdString).copyString();
      e->toId = it.get(StaticStrings::ToString).copyString();
      e->value = it.get("value").getInt();
      v->_edges.push_back(e.get());
      e.release();
    }
  }
}

Worker::~Worker() {
  for (auto const &it : _vertices) {
    delete(it.second);
  }
  _vertices.clear();
}

void Worker::nextGlobalStep(VPackSlice data) {
  // TODO do some work?
  VPackSlice gssSlice = data.get(Utils::globalSuperstepKey);
  if (!gssSlice.isInt()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "Invalid gss in body");
  }
  
  int64_t gss = gssSlice.getInt();
  if (gss == 0) {
    VPackSlice cid = data.get(Utils::coordinatorIdKey);
    //VPackSlice algo = data.get(Utils::algorithmKey);
    if (cid.isString())
      _coordinatorId = cid.copyString();
  }
  _globalSuperstep = gss;
  InMessageCache *inCache = _currentCache;
  if (_currentCache == _cache1) _currentCache = _cache2;
  else _currentCache = _cache1;
  
  std::unique_ptr<rest::Job> job(new WorkerJob(this, inCache));
  DispatcherFeature::DISPATCHER->addJob(job, false);
  LOG(DEBUG) << "Worker started new gss computation\n";
}

void Worker::receivedMessages(VPackSlice data) {
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
  LOG(DEBUG) << "Worker received messages\n";
}

// ========== WorkerJob

/*
V8Job::~V8Job() {
  if (_task != nullptr) {
    V8PeriodicTask::jobDone(_task);
  }
}*/

WorkerJob::WorkerJob(Worker *worker, InMessageCache *inCache) : Job("Pregel Job"), _worker(worker), _inCache(inCache) {
}

void WorkerJob::work() {
  if (_canceled) {
    return;
  }
  LOG(DEBUG) << "Worker job started\n";
  
  OutMessageCache outCache(_worker->_vertexCollection);
  
  int64_t gss = _worker->_globalSuperstep;
  bool isDone = true;

  if (gss == 0) {
    isDone = false;
    
    for (auto const &it : _worker->_vertices) {
      Vertex *v = it.second;
      VPackArrayIterator messages = _worker->_currentCache->getMessages(v->documentId);
      v->compute(gss, messages, &outCache);
      _worker->_activationMap[it.first] = v->state() == VertexActivationState::ACTIVE;
    }
  } else {
    for (auto &it : _worker->_activationMap) {
      VPackArrayIterator messages = _worker->_currentCache->getMessages(it.first);
      if (messages.size() > 0 || it.second) {
        isDone = false;
        
        Vertex *v = _worker->_vertices[it.first];
        v->compute(gss, messages, &outCache);
        it.second = v->state() == VertexActivationState::ACTIVE;
      }
    }
  }
  LOG(DEBUG) << "Worker job computations done, now distributing results\n";

  if (_canceled) {
    return;
  }
  
  // send messages to other shards
  ClusterComm *cc =  ClusterComm::instance();
  ClusterInfo *ci = ClusterInfo::instance();
  
  if (!isDone) {
    std::shared_ptr<std::vector<ShardID>> shards = ci->getShardList(_worker->_vertexCollection);
    std::vector<ClusterCommRequest> requests;
    for (auto it = shards->begin(); it != shards->end(); ++it) {
      VPackBuilder package;
      package.add(Utils::senderKey, VPackValue(ServerState::instance()->getId()));
      package.add(Utils::executionNumberKey, VPackValue(_worker->_executionNumber));
      package.add(Utils::globalSuperstepKey, VPackValue(gss+1));
      VPackBuilder messages;
      outCache.getMessages(*it, messages);
      package.add(Utils::messagesKey, messages.slice());
      package.close();
      
      auto body = std::make_shared<std::string const>(package.toString());
      requests.emplace_back("shard:" + *it, rest::RequestType::POST, Utils::messagesPath, body);
    }
    size_t nrDone = 0;
    cc->performRequests(requests, 120, nrDone, LogTopic("Pregel message transfer"));
  } else {
    LOG(DEBUG) << "Worker job has nothing more to process\n";
  }
  
  // notify the conductor that we are done.
  VPackBuilder package;
  package.add(Utils::senderKey, VPackValue(ServerState::instance()->getId()));
  package.add(Utils::executionNumberKey, VPackValue(_worker->_executionNumber));
  package.add(Utils::doneKey, VPackValue(isDone));
  package.close();
  
  // TODO handle communication failure
  CoordTransactionID coordinatorTransactionID = TRI_NewTickServer();
  auto headers =
  std::make_unique<std::unordered_map<std::string, std::string>>();
  auto body = std::make_shared<std::string const>(package.toString());
  cc->asyncRequest("", coordinatorTransactionID,
                   "server:"+_worker->_coordinatorId,
                   rest::RequestType::POST,
                   Utils::finishedGSSPath,
                   body, headers, nullptr, 90.0);
  
  LOG(DEBUG) << "Worker job finished sending results\n";
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
