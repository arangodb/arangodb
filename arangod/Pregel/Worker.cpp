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

#include "Pregel/Worker.h"
#include "Pregel/Aggregator.h"
#include "Pregel/GraphStore.h"
#include "Pregel/IncomingCache.h"
#include "Pregel/OutgoingCache.h"
#include "Pregel/Utils.h"
#include "Pregel/VertexComputation.h"
#include "Pregel/WorkerState.h"

#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include "Algos/PageRank.h"
#include "Algos/SSSP.h"

using namespace arangodb;
using namespace arangodb::pregel;

template <typename V, typename E, typename M>
IWorker* IWorker::createWorker(TRI_vocbase_t* vocbase, Algorithm<V, E, M>* algo,
                               VPackSlice body) {
  return new Worker<V, E, M>(vocbase, algo, body);
}

IWorker* IWorker::createWorker(TRI_vocbase_t* vocbase, VPackSlice body) {
  VPackSlice algorithm = body.get(Utils::algorithmKey);
  if (!algorithm.isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Supplied bad parameters to worker");
  }

  VPackSlice userParams = body.get(Utils::userParametersKey);
  if (algorithm.compareString("sssp") == 0) {
    return createWorker(vocbase, new algos::SSSPAlgorithm(userParams), body);
  } else if (algorithm.compareString("pagerank") == 0) {
    return createWorker(vocbase, new algos::PageRankAlgorithm(userParams), body);
  } else {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Unsupported Algorithm");
  }
  return nullptr;
};

template <typename V, typename E, typename M>
Worker<V, E, M>::Worker(TRI_vocbase_t* vocbase, Algorithm<V, E, M>* algo,
                        VPackSlice initConfig)
    : _running(true), _algorithm(algo) {
  VPackSlice userParams = initConfig.get(Utils::userParametersKey);
  _state.reset(new WorkerState(vocbase->name(), initConfig));
  _workerContext.reset(algo->workerContext(userParams));

  const size_t threadNum = 1;
  _workerPool.reset(
      new ThreadPool(static_cast<size_t>(threadNum), "Pregel Worker"));
  _graphStore.reset(
      new GraphStore<V, E>(vocbase, _state.get(), algo->inputFormat()));
  std::shared_ptr<MessageFormat<M>> mFormat(algo->messageFormat());
  std::shared_ptr<MessageCombiner<M>> combiner(algo->messageCombiner());
  _readCache.reset(new IncomingCache<M>(mFormat, combiner));
  _writeCache.reset(new IncomingCache<M>(mFormat, combiner));
  _conductorAggregators.reset(new AggregatorUsage(algo));
  _workerAggregators.reset(new AggregatorUsage(algo));

  if (_workerContext != nullptr) {
    _workerContext->_vertexCount =
        initConfig.get(Utils::totalVertexCount).getUInt();
    _workerContext->_edgeCount =
        initConfig.get(Utils::totalEdgeCount).getUInt();
    _workerContext->_conductorAggregators = _conductorAggregators.get();
    _workerContext->_workerAggregators = _workerAggregators.get();
    _workerContext->preApplication();
  }
}

template <typename V, typename E, typename M>
Worker<V, E, M>::~Worker() {
  LOG(INFO) << "Called ~Worker()";
}

template <typename V, typename E, typename M>
void Worker<V, E, M>::prepareGlobalStep(VPackSlice data) {
  LOG(INFO) << "Prepare GSS: " << data.toJson();

  VPackSlice gssSlice = data.get(Utils::globalSuperstepKey);
  if (!gssSlice.isInteger()) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_BAD_PARAMETER,
                                  "Invalid gss in %s:%d", __FILE__, __LINE__);
  }
  const uint64_t gss = (uint64_t)gssSlice.getUInt();
  if (_expectedGSS != gss) {
    THROW_ARANGO_EXCEPTION_FORMAT(
        TRI_ERROR_BAD_PARAMETER,
        "Seems like this worker missed a gss, expected %u. Data = %s ",
        _expectedGSS, data.toJson().c_str());
  }
  
  // clean up message caches, intialize gss
  _state->_globalSuperstep = gss;
  _swapIncomingCaches();  // write cache becomes the readable cache
  // parse aggregated values from conductor
  _conductorAggregators->resetValues();
  VPackSlice aggValues = data.get(Utils::aggregatorValuesKey);
  if (aggValues.isObject()) {
    _conductorAggregators->aggregateValues(aggValues);
  }
  _workerAggregators->resetValues();
  // execute context
  if (_workerContext != nullptr) {
    _workerContext->preGlobalSuperstep(gss);
  }
}

/// @brief Setup next superstep
template <typename V, typename E, typename M>
void Worker<V, E, M>::startGlobalStep(VPackSlice data) {
  LOG(INFO) << "Starting GSS: " << data.toJson();
  VPackSlice gssSlice = data.get(Utils::globalSuperstepKey);
  const uint64_t gss = (uint64_t)gssSlice.getUInt();
  if (gss != _state->globalSuperstep()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "Wrong GSS");
  }
  LOG(INFO) << "Worker starts new gss: " << gss;
  
  //size_t numThreads = _workerPool->numThreads();
  // for (; numThreads > 0; numThreads--) {}
  // incoming caches are already switched
  _workerPool->enqueue([this, gss] {

    std::unique_ptr<MessageFormat<M>> format(_algorithm->messageFormat());
    std::unique_ptr<MessageCombiner<M>> combiner(_algorithm->messageCombiner());
    
    OutgoingCache<M> outCache(_state.get(), format.get(), combiner.get(),
                              this->_writeCache.get());
    AggregatorUsage workerAggregator(_algorithm.get());
    std::unique_ptr<VertexComputation<V, E, M>> vertexComputation(
        _algorithm->createComputation(gss));
    vertexComputation->_gss = gss;
    vertexComputation->_context = _workerContext.get();
    vertexComputation->_graphStore = _graphStore.get();
    vertexComputation->_outgoing = &outCache;
    vertexComputation->_conductorAggregators = _conductorAggregators.get();
    vertexComputation->_workerAggregators = &workerAggregator;

    size_t activeCount = 0;
    std::vector<VertexEntry>& vertexIterator =
        _graphStore->vertexIterator();

    for (auto& vertexEntry : vertexIterator) {
      std::string vertexID = vertexEntry.vertexID();
      MessageIterator<M> messages = this->_readCache->getMessages(vertexID);

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
    size_t receivedCount = this->_writeCache->receivedMessageCount();

    // ========================= merge aggregators ===========================
    // TODO locking
    _workerAggregators->aggregateValues(*vertexComputation->_workerAggregators);

    if (activeCount == 0 && sendCount == 0 && receivedCount == 0) {
      LOG(INFO) << "Worker seems to be done";
      _workerJobIsDone(true);
    } else {
      LOG(INFO) << "Worker send " << sendCount << " messages";
      _workerJobIsDone(false);
    }
  });
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
  uint64_t gss = gssSlice.getUInt();
  if (gss == _state->_globalSuperstep) {
    _writeCache->parseMessages(messageSlice);
  } else {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Superstep out of sync");
    LOG(ERR) << "Expected: " << _state->_globalSuperstep << "Got: " << gss;
  }
}

template <typename V, typename E, typename M>
void Worker<V, E, M>::_workerJobIsDone(bool allDone) {
  _expectedGSS = _state->_globalSuperstep + 1;

  // notify the conductor that we are done.
  VPackBuilder package;
  package.openObject();
  package.add(Utils::senderKey, VPackValue(ServerState::instance()->getId()));
  package.add(Utils::executionNumberKey, VPackValue(_state->executionNumber()));
  package.add(Utils::globalSuperstepKey, VPackValue(_state->globalSuperstep()));
  package.add(Utils::doneKey, VPackValue(allDone));
  if (_workerAggregators->size() > 0) {
    package.add(Utils::aggregatorValuesKey, VPackValue(VPackValueType::Object));
    _workerAggregators->serializeValues(package);
    package.close();
  }
  package.close();

  ClusterComm* cc = ClusterComm::instance();
  std::string baseUrl = Utils::baseUrl(_state->database());
  CoordTransactionID coordinatorTransactionID = TRI_NewTickServer();
  auto headers =
      std::make_unique<std::unordered_map<std::string, std::string>>();
  auto body = std::make_shared<std::string const>(package.toJson());
  cc->asyncRequest("", coordinatorTransactionID,
                   "server:" + _state->coordinatorId(), rest::RequestType::POST,
                   baseUrl + Utils::finishedGSSPath, body, headers, nullptr,
                   90.0);
  LOG(INFO) << "Sending finishedGSS to coordinator: "
            << _state->coordinatorId();
  if (allDone)
    LOG(INFO) << "WE have no active vertices, and did not send messages";
}

template <typename V, typename E, typename M>
void Worker<V, E, M>::finalizeExecution(VPackSlice body) {
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
    v.add("result", VPackValue(data));
    v.close();
    b.add(v.slice());
  }
  b.close();
  LOG(INFO) << "Results. " << b.toJson();
}

// template types to create
template class arangodb::pregel::Worker<int64_t, int64_t, int64_t>;
template class arangodb::pregel::Worker<float, float, float>;
