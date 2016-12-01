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
#include "Pregel/PregelFeature.h"
#include "Pregel/Utils.h"
#include "Pregel/VertexComputation.h"
#include "Pregel/WorkerState.h"

#include "Basics/MutexLocker.h"
#include "Basics/ThreadPool.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::pregel;

template <typename V, typename E, typename M>
Worker<V, E, M>::Worker(TRI_vocbase_t* vocbase, Algorithm<V, E, M>* algo,
                        VPackSlice initConfig)
    : _running(true), _state(vocbase->name(), initConfig), _algorithm(algo) {
  VPackSlice userParams = initConfig.get(Utils::userParametersKey);
  _workerContext.reset(algo->workerContext(userParams));
  _graphStore.reset(
      new GraphStore<V, E>(vocbase, _state, algo->inputFormat()));
  _messageFormat.reset(algo->messageFormat());
  _messageCombiner.reset(algo->messageCombiner());
  _readCache.reset(new IncomingCache<M>(_messageFormat.get(), _messageCombiner.get()));
  _writeCache.reset(new IncomingCache<M>(_messageFormat.get(), _messageCombiner.get()));
  _conductorAggregators.reset(new AggregatorUsage(algo));
  _workerAggregators.reset(new AggregatorUsage(algo));

  if (_workerContext) {
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
  // Only expect serial calls from the conductor. Lock to prevent malicous activity
  MUTEX_LOCKER(guard, _conductorMutex);
  
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
  _state._globalSuperstep = gss;
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
  if (gss == _state._globalSuperstep) {
    // handles locking for us
    _writeCache->parseMessages(messageSlice);
  } else {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Superstep out of sync");
    LOG(ERR) << "Expected: " << _state._globalSuperstep << "Got: " << gss;
  }
}

/// @brief Setup next superstep
template <typename V, typename E, typename M>
void Worker<V, E, M>::startGlobalStep(VPackSlice data) {
  // Only expect serial calls from the conductor. Lock to prevent malicous activity
  MUTEX_LOCKER(guard, _conductorMutex);
  
  LOG(INFO) << "Starting GSS: " << data.toJson();
  VPackSlice gssSlice = data.get(Utils::globalSuperstepKey);
  const uint64_t gss = (uint64_t)gssSlice.getUInt();
  if (gss != _state.globalSuperstep()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "Wrong GSS");
  }
  LOG(INFO) << "Worker starts new gss: " << gss;
  
  ThreadPool *pool = PregelFeature::instance()->threadPool();
  size_t total = _graphStore->vertexCount();
  size_t delta = total / pool->numThreads();
  size_t start = 0, end = delta;
  _runningThreads = total / delta;//rounds-up unsigned integers
  do {
    pool->enqueue([this, start, end] {
      if (!_running) {
        LOG(INFO) << "Execution aborted prematurely.";
        return;
      }
      auto vertexIterator = _graphStore->vertexIterator(start, end);
      _executeGlobalStep(vertexIterator);
    });
    start = end;
    end = end+delta;
    if (total < delta+end) {// swallow the rest
      end = total;
    }
  } while(start != total);
}

template <typename V, typename E, typename M>
void Worker<V, E, M>::_initializeVertexContext(VertexContext<V, E, M> *ctx) {
  ctx->_gss = _state.globalSuperstep();
  ctx->_context = _workerContext.get();
  ctx->_graphStore = _graphStore.get();
  ctx->_conductorAggregators = _conductorAggregators.get();
}

// internally called in a WORKER THREAD!!
template <typename V, typename E, typename M>
void Worker<V, E, M>::_executeGlobalStep(RangeIterator<VertexEntry> &vertexIterator) {
  double start = TRI_microtime();

  // thread local caches
  IncomingCache<M> localIncoming(_messageFormat.get(), _messageCombiner.get());
  OutgoingCache<M> outCache(&_state,
                            _messageFormat.get(),
                            _messageCombiner.get(),
                            &localIncoming);
  AggregatorUsage workerAggregator(_algorithm.get());
  
  // TODO look if we can avoid instantiating this
  std::unique_ptr<VertexComputation<V, E, M>>
  vertexComputation(_algorithm->createComputation(_state.globalSuperstep()));
  _initializeVertexContext(vertexComputation.get());
  vertexComputation->_outgoing = &outCache;
  vertexComputation->_workerAggregators = &workerAggregator;
  
  size_t activeCount = 0;
  for (VertexEntry *vertexEntry : vertexIterator) {
    std::string vertexID = vertexEntry->vertexID();
    MessageIterator<M> messages = _readCache->getMessages(vertexID);
    
    if (messages.size() > 0 || vertexEntry->active()) {
      vertexComputation->_vertexEntry = vertexEntry;
      vertexComputation->compute(vertexID, messages);
      if (vertexEntry->active()) {
        activeCount++;
      } else {
        LOG(INFO) << vertexID << " vertex has halted";
      }
    }
    // TODO delete read messages immediatly
    // technically messages to non-existing vertices trigger
    // their creation
    
    if (!_running) {
      LOG(INFO) << "Execution aborted prematurely.";
      break;
    }
  }
  // ==================== send messages to other shards ====================
  outCache.sendMessages();
  // merge thread local messages, _writeCache does locking
  _writeCache->mergeCache(localIncoming);
  // TODO ask how to implement message sending without waiting for a response
  
  LOG(INFO) << "Finished executing vertex programs.";
  
  WorkerStats stats;
  stats.activeCount = activeCount;
  stats.sendCount = outCache.sendMessageCount();;
  stats.superstepRuntimeSecs = TRI_microtime() - start;
  _workerThreadDone(vertexComputation->_workerAggregators, stats);
}

// called at the end of a worker thread, needs mutex
template <typename V, typename E, typename M>
void Worker<V, E, M>::_workerThreadDone(AggregatorUsage *threadAggregators,
                                        WorkerStats const& threadStats) {
  MUTEX_LOCKER(guard, _threadMutex);// only one thread at a time
  
  // merge the thread local stats and aggregators
  _workerAggregators->aggregateValues(*threadAggregators);
  _superstepStats.accumulate(threadStats);
  _runningThreads--;
  if (_runningThreads > 0) {// should work like a join operatopm
    return;// there are still threads running
  }
  
  // ==================== Track statistics =================================
  // the stats we want to keep should be final. At this point we can only be sure of the
  // messages we have received in total from the last superstep, and the messages we have send in
  // the current superstep. Other workers are likely not finished yet, and might still send stuff
  _superstepStats.receivedCount = _readCache->receivedMessageCount();
  _readCache->clear();//no need to keep old messages around
  _expectedGSS = _state._globalSuperstep + 1;

  // notify the conductor that we are done.
  VPackBuilder package;
  package.openObject();
  package.add(Utils::senderKey, VPackValue(ServerState::instance()->getId()));
  package.add(Utils::executionNumberKey, VPackValue(_state.executionNumber()));
  package.add(Utils::globalSuperstepKey, VPackValue(_state.globalSuperstep()));
  if (_workerAggregators->size() > 0) {// add aggregators
    package.add(Utils::aggregatorValuesKey, VPackValue(VPackValueType::Object));
    _workerAggregators->serializeValues(package);
    package.close();
  }
  _superstepStats.serializeValues(package);// add stats
  if (_superstepStats.activeCount == 0
      && _superstepStats.sendCount == 0
      && _superstepStats.receivedCount == 0) {
    LOG(INFO) << "WE have no active vertices, and did not send messages";
    package.add(Utils::doneKey, VPackValue(true));
  }
  package.close();
  _superstepStats.reset();// don't forget to reset at the end of the superstep

  // TODO ask how to implement message sending without waiting for a response
  // ============ Call Coordinator ============
  ClusterComm* cc = ClusterComm::instance();
  std::string baseUrl = Utils::baseUrl(_state.database());
  CoordTransactionID coordinatorTransactionID = TRI_NewTickServer();
  auto headers =
      std::make_unique<std::unordered_map<std::string, std::string>>();
  auto body = std::make_shared<std::string const>(package.toJson());
  cc->asyncRequest("", coordinatorTransactionID,
                   "server:" + _state.coordinatorId(), rest::RequestType::POST,
                   baseUrl + Utils::finishedGSSPath, body, headers, nullptr,
                   90.0,// timeout + single request
                   true);
}

template <typename V, typename E, typename M>
void Worker<V, E, M>::finalizeExecution(VPackSlice body) {
  // Only expect serial calls from the conductor. Lock to prevent malicous activity
  MUTEX_LOCKER(guard, _conductorMutex);
  _running = false;
  
  VPackSlice store = body.get(Utils::storeResultsKey);
  if (store.isBool() && store.getBool() == true) {
    _graphStore->storeResults(_state);
  } else {
    LOG(WARN) << "Discarding results";
  }
  _graphStore.reset();
  
  /*VPackBuilder b;
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
  LOG(INFO) << "Results. " << b.toJson();//*/
}

template <typename V, typename E, typename M>
void Worker<V, E, M>::startRecovery(VPackSlice data) {
  VPackSlice method = data.get(Utils::recoveryMethodKey);
  if (method.compareString(Utils::compensate) == 0) {
    WorkerState nextState(_state.database(), data);
    _graphStore->loadShards(nextState);
    _state = nextState;
    
    // TODO start compensate method
    /*
    ThreadPool *pool = PregelFeature::instance()->threadPool();
    pool->enqueue([this, start, end] {
      if (!_running) {
        LOG(INFO) << "Execution aborted prematurely.";
        return;
      }
      
      size_t total = _graphStore->vertexCount();
      size_t start = 0, end = delta;
      auto vertexIterator = _graphStore->vertexIterator(start, end);
      _executeGlobalStep(vertexIterator);
    });
    
    uint64_t gss = _state.globalSuperstep();
    std::unique_ptr<VertexComputation<V, E, M>>
    vertexComputation(_algorithm->createComputation(gss));
    vertexComputation->_gss = gss;
    vertexComputation->_context = _workerContext.get();
    vertexComputation->_graphStore = _graphStore.get();
    vertexComputation->_outgoing = &outCache;
    vertexComputation->_conductorAggregators = _conductorAggregators.get();
    vertexComputation->_workerAggregators = &workerAggregator;
    */
    
  } else if (method.compareString(Utils::rollback) == 0) {
    
  }
}

// template types to create
template class arangodb::pregel::Worker<int64_t, int64_t, int64_t>;
template class arangodb::pregel::Worker<float, float, float>;
