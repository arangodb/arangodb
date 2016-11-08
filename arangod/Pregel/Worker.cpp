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
#include "Aggregator.h"
#include "GraphStore.h"
#include "IncomingCache.h"
#include "OutgoingCache.h"
#include "Utils.h"
#include "VertexComputation.h"
#include "WorkerState.h"

#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include "Algos/SSSP.h"

using namespace arangodb;
using namespace arangodb::pregel;

// Algorithm<V, E, M> const& alg, VPackSlice s

IWorker* IWorker::createWorker(TRI_vocbase_t* vocbase, VPackSlice params) {
  VPackSlice algorithm = params.get(Utils::algorithmKey);
  if (!algorithm.isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Supplied bad parameters to worker");
  }

  IWorker* worker = nullptr;
  if (algorithm.compareString("sssp") == 0) {
    // TODO transform to shared_ptr all the way
    auto algo = new algos::SSSPAlgorithm();
    auto ptr = algo->inputFormat();
    auto ctx = std::make_shared<WorkerState<int64_t, int64_t, int64_t>>(
        algo, vocbase->name(), params);
    auto graph = std::make_shared<GraphStore<int64_t, int64_t>>(
        ctx->localVertexShardIDs(), ctx->localEdgeShardIDs(), vocbase, ptr);
    worker = new Worker<int64_t, int64_t, int64_t>(graph, ctx);
  } /*if (algorithm.compareString("scc") == 0) {
      // TODO transform to shared_ptr all the way
      auto algo = new SCCAlgorithm();
      auto ptr = algo->inputFormat();
      auto ctx = std::make_shared<WorkerState<int64_t, int64_t, int64_t>>(
                                                                            algo, vocbase->name(), params);
      auto graph = std::make_shared<GraphStore<int64_t, int64_t>>(
                                                                  ctx->vertexCollectionName(), ctx->localVertexShardIDs(),
                                                                  ctx->localEdgeShardIDs(), vocbase, ptr);
      worker = new Worker<int64_t, int64_t, int64_t>(graph, ctx);
  }*/ else {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Unsupported Algorithm");
  }
  return worker;
};

template <typename V, typename E, typename M>
Worker<V, E, M>::Worker(std::shared_ptr<GraphStore<V, E>> graphStore,
                        std::shared_ptr<WorkerState<V, E, M>> context)
    : _running(true), _ctx(context), _graphStore(graphStore) {
  
  const size_t threadNum = 1;
  _workerPool.reset(new ThreadPool(static_cast<size_t>(threadNum), "Pregel Worker"));
  
  std::vector<std::unique_ptr<Aggregator>> aggrgs;
  _ctx->algorithm()->aggregators(aggrgs);
  for (std::unique_ptr<Aggregator>& a : aggrgs) {
    _aggregators[a->name()] = std::move(a);
  }
}

template <typename V, typename E, typename M>
Worker<V, E, M>::~Worker() {
  LOG(INFO) << "Called ~Worker()";
}

/// @brief Setup next superstep
template <typename V, typename E, typename M>
void Worker<V, E, M>::startGlobalStep(VPackSlice data) {
  LOG(INFO) << "Called next global step: " << data.toJson();

  // TODO do some work?
  VPackSlice gssSlice = data.get(Utils::globalSuperstepKey);
  if (!gssSlice.isInteger()) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_BAD_PARAMETER,
                                  "Invalid gss in %s:%d", __FILE__, __LINE__);
  }
  unsigned int gss = (unsigned int)gssSlice.getUInt();
  if (_ctx->_expectedGSS != gss) {
    THROW_ARANGO_EXCEPTION_FORMAT(
        TRI_ERROR_BAD_PARAMETER,
        "Seems like this worker missed a gss, expected %u. Data = %s ",
        _ctx->_expectedGSS, data.toJson().c_str());
  }
  _ctx->_globalSuperstep = gss;
  _ctx->_expectedGSS = gss + 1;

  // parse aggregated values
  VPackSlice aggregatedValues = data.get(Utils::aggregatorValuesKey);
  if (aggregatedValues.isObject()) {
    for (auto const& pair : _aggregators) {
      VPackSlice val = aggregatedValues.get(pair.second->name());
      if (!val.isNone()) {
        pair.second->setAggregatedValue(val);
      }
    }
  }

  // incoming caches are already switched
  _workerPool->enqueue([this, gss] {

    OutgoingCache<V, E, M> outCache(_ctx);
    auto vertexComputation = _ctx->algorithm()->createComputation();
    vertexComputation->_gss = gss;
    vertexComputation->_outgoing = &outCache;
    vertexComputation->_graphStore = _graphStore;

    size_t activeCount = 0;
    auto incoming = this->_ctx->readableIncomingCache();
    std::vector<VertexEntry>& vertexIterator =
        this->_graphStore->vertexIterator();

    for (auto& vertexEntry : vertexIterator) {
      std::string vertexID = vertexEntry.vertexID();
      MessageIterator<M> messages = incoming->getMessages(vertexID);

      if (gss == 0 || messages.size() > 0 || vertexEntry.active()) {
        vertexComputation->_vertexEntry = &vertexEntry;
        vertexComputation->compute(vertexID, messages);
        if (vertexEntry.active()) {
          activeCount++;
        } else {
          LOG(INFO) << vertexID << " vertex has halted";
        }
      }
      if (!_running) {
        LOG(INFO) << "Execution aborted prematurely.";
        return;
      }
    }
    LOG(INFO) << "Finished executing vertex programs.";

    // ==================== send messages to other shards ====================
    outCache.sendMessages();
    size_t sendCount = outCache.sendMessageCount();
    size_t receivedCount =
        _ctx->writeableIncomingCache()->receivedMessageCount();
    if (activeCount == 0 && sendCount == 0 && receivedCount == 0) {
      LOG(INFO) << "Worker seems to be done";
      workerJobIsDone(true);
    } else {
      LOG(INFO) << "Worker send " << sendCount << " messages";
      workerJobIsDone(false);
    }
  });
  LOG(INFO) << "Worker started new gss: " << gss;
}

template <typename V, typename E, typename M>
void Worker<V, E, M>::receivedMessages(VPackSlice data) {
  LOG(INFO) << "Worker received some messages: " << data.toJson();

  VPackSlice gssSlice = data.get(Utils::globalSuperstepKey);
  VPackSlice messageSlice = data.get(Utils::messagesKey);
  if (!gssSlice.isInteger() || !messageSlice.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Bad parameters in body");
  }
  int64_t gss = gssSlice.getInt();
  if (gss == _ctx->_globalSuperstep) {
    _ctx->writeableIncomingCache()->parseMessages(messageSlice);
  } else if (gss == _ctx->_globalSuperstep - 1) {
    LOG(ERR) << "Should not receive messages from last global superstep, "
                "during computation phase";
    //_ctx->_readCache->addMessages(messageSlice);
  } else {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Superstep out of sync");
  }

  LOG(INFO) << "Worker combined / stored incoming messages";
}

template <typename V, typename E, typename M>
void Worker<V, E, M>::workerJobIsDone(bool allDone) {
  _ctx->readableIncomingCache()->clear();
  _ctx->swapIncomingCaches();  // write cache becomes the readable cache
  _ctx->_expectedGSS = _ctx->_globalSuperstep + 1;

  // notify the conductor that we are done.
  VPackBuilder package;
  package.openObject();
  package.add(Utils::senderKey, VPackValue(ServerState::instance()->getId()));
  package.add(Utils::executionNumberKey, VPackValue(_ctx->executionNumber()));
  package.add(Utils::globalSuperstepKey, VPackValue(_ctx->globalSuperstep()));
  package.add(Utils::doneKey, VPackValue(allDone));
  package.close();

  ClusterComm* cc = ClusterComm::instance();
  std::string baseUrl = Utils::baseUrl(_ctx->database());
  CoordTransactionID coordinatorTransactionID = TRI_NewTickServer();
  auto headers =
      std::make_unique<std::unordered_map<std::string, std::string>>();
  auto body = std::make_shared<std::string const>(package.toJson());
  cc->asyncRequest("", coordinatorTransactionID,
                   "server:" + _ctx->coordinatorId(), rest::RequestType::POST,
                   baseUrl + Utils::finishedGSSPath, body, headers, nullptr,
                   90.0);
  LOG(INFO) << "Sending finishedGSS to coordinator: " << _ctx->coordinatorId();
  if (allDone)
    LOG(INFO) << "WE have no active vertices, and did not send messages";
}

template <typename V, typename E, typename M>
void Worker<V, E, M>::finalizeExecution(VPackSlice data) {
  _running = false;
  _workerPool.reset();

  VPackBuilder b;
  b.openArray();
  auto it = _graphStore->vertexIterator();
  for (const VertexEntry& vertexEntry : it) {
    V data = _graphStore->copyVertexData(&vertexEntry);
    VPackBuilder v;
    v.openObject();
    v.add("key", VPackValue(vertexEntry.vertexID()));
    v.add("result", VPackValue((int64_t)data));
    v.close();
    b.add(v.slice());
  }
  b.close();
  LOG(INFO) << "Results. " << b.toJson();
}

// template types to create
template class arangodb::pregel::Worker<int64_t, int64_t, int64_t>;
template class arangodb::pregel::Worker<float, float, float>;
