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
#include "Pregel/WorkerConfig.h"

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
    : _config(vocbase->name(), initConfig), _algorithm(algo) {
  
  VPackSlice userParams = initConfig.get(Utils::userParametersKey);
  _workerContext.reset(algo->workerContext(userParams));
  _messageFormat.reset(algo->messageFormat());
  _messageCombiner.reset(algo->messageCombiner());
  _conductorAggregators.reset(new AggregatorHandler(algo));
  _workerAggregators.reset(new AggregatorHandler(algo));
  _graphStore.reset(new GraphStore<V, E>(vocbase, _algorithm->inputFormat()));
  _nextGSSSendMessageCount = 0;
  if (_messageCombiner) {
    _readCache = new CombiningInCache<M>(_messageFormat.get(), _messageCombiner.get());
    _writeCache =
        new CombiningInCache<M>(_messageFormat.get(), _messageCombiner.get());
    if (_config.asynchronousMode()) {
      _writeCacheNextGSS = new CombiningInCache<M>(_messageFormat.get(),
                                                   _messageCombiner.get());
    }
  } else {
    _readCache = new ArrayInCache<M>(_messageFormat.get());
    _writeCache = new ArrayInCache<M>(_messageFormat.get());
    if (_config.asynchronousMode()) {
      _writeCacheNextGSS = new ArrayInCache<M>(_messageFormat.get());
    }
  }

  uint64_t vc = initConfig.get(Utils::totalVertexCount).getUInt(),
           ec = initConfig.get(Utils::totalEdgeCount).getUInt();

  // initialization of the graphstore might take an undefined amount
  // of time. Therefore this is performed asynchronous
  ThreadPool* pool = PregelFeature::instance()->threadPool();
  pool->enqueue([this, vocbase, vc, ec] {
    _graphStore->loadShards(this->_config);
    _state = WorkerState::IDLE;

    // execute the user defined startup code
    if (_workerContext) {
      _workerContext->_conductorAggregators = _conductorAggregators.get();
      _workerContext->_workerAggregators = _workerAggregators.get();
      _workerContext->_vertexCount = vc;
      _workerContext->_edgeCount = ec;
      _workerContext->preApplication();
    }

    VPackBuilder package;
    package.openObject();
    package.add(Utils::senderKey, VPackValue(ServerState::instance()->getId()));
    package.add(Utils::executionNumberKey,
                VPackValue(_config.executionNumber()));
    package.close();
    _callConductor(Utils::finishedStartupPath, package.slice());
  });
}

/*template <typename M>
GSSContext::~GSSContext() {}*/

template <typename V, typename E, typename M>
Worker<V, E, M>::~Worker() {
  LOG(INFO) << "Called ~Worker()";
  _state = WorkerState::DONE;
  delete _readCache;
  delete _writeCache;
  delete _writeCacheNextGSS;
}

template <typename V, typename E, typename M>
void Worker<V, E, M>::prepareGlobalStep(VPackSlice data, VPackBuilder &response) {
  // Only expect serial calls from the conductor.
  // Lock to prevent malicous activity
  MUTEX_LOCKER(guard, _commandMutex);
  if (_state != WorkerState::IDLE) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "Cannot start a gss when the worker is not idle");
  }
  _state = WorkerState::PREPARING;// stop any running step
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
  
  // respons with info which allows the conductor to decide whether 
  // to start the next GSS or end the execution
  response.openObject();
  response.add(Utils::activeCountKey, VPackValue(_superstepStats.activeCount));
  response.add(Utils::aggregatorValuesKey, VPackValue(VPackValueType::Object));
  _workerAggregators->serializeValues(response);
  response.close();
  response.close();

  // make us ready to receive messages
  _config._globalSuperstep = gss;
  // write cache becomes the readable cache
  if (_config.asynchronousMode()) {
    TRI_ASSERT(_readCache->receivedMessageCount() == 0);
    TRI_ASSERT(_writeCache->receivedMessageCount() == 0);
    std::swap(_readCache, _writeCacheNextGSS);
    _writeCache->clear();
    _requestedNextGSS = false;// only relevant for async
    _superstepStats.sendCount = _nextGSSSendMessageCount;
    _nextGSSSendMessageCount = 0;
  } else {
    TRI_ASSERT(_readCache->receivedMessageCount() == 0);
    std::swap(_readCache, _writeCache);
    _config._localSuperstep = gss;
  }
}

template <typename V, typename E, typename M>
void Worker<V, E, M>::receivedMessages(VPackSlice data) {

  VPackSlice gssSlice = data.get(Utils::globalSuperstepKey);
  VPackSlice messageSlice = data.get(Utils::messagesKey);
  uint64_t gss = gssSlice.getUInt();
  if (gss == _config._globalSuperstep) {
    // handles locking for us
    _writeCache->parseMessages(messageSlice);
    
    // Trigger the processing of vertices
    if (_config.asynchronousMode() && _state == WorkerState::IDLE) {
      MUTEX_LOCKER(guard, _commandMutex);
      _continueAsync();
    }
  } else if (_config.asynchronousMode() && gss == _config._globalSuperstep+1) {
    _writeCacheNextGSS->parseMessages(messageSlice);
  } else {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Superstep out of sync");
    LOG(ERR) << "Expected: " << _config._globalSuperstep << "Got: " << gss;
  }
}

/// @brief Setup next superstep
template <typename V, typename E, typename M>
void Worker<V, E, M>::startGlobalStep(VPackSlice data) {
  // Only expect serial calls from the conductor.
  // Lock to prevent malicous activity
  MUTEX_LOCKER(guard, _commandMutex);
  if (_state != WorkerState::PREPARING) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "Cannot start a gss when the worker is not prepared");
  }
  LOG(INFO) << "Starting GSS: " << data.toJson();
  VPackSlice gssSlice = data.get(Utils::globalSuperstepKey);
  const uint64_t gss = (uint64_t)gssSlice.getUInt();
  if (gss != _config.globalSuperstep()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "Wrong GSS");
  }

  _workerAggregators->resetValues();
  _conductorAggregators->resetValues();
  // parse aggregated values from conductor
  VPackSlice aggValues = data.get(Utils::aggregatorValuesKey);
  if (aggValues.isObject()) {
    _conductorAggregators->aggregateValues(aggValues);
  }
  // execute context
  if (_workerContext != nullptr) {
    _workerContext->preGlobalSuperstep(gss);
  }

  LOG(INFO) << "Worker starts new gss: " << gss;
  _startProcessing();// sets _state = COMPUTING;
}

template <typename V, typename E, typename M>
void Worker<V, E, M>::cancelGlobalStep(VPackSlice data) {
  MUTEX_LOCKER(guard, _commandMutex);
  _state = WorkerState::DONE;
}

/// WARNING only call this while holding the _commandMutex
template <typename V, typename E, typename M>
void Worker<V, E, M>::_startProcessing() {
  _state = WorkerState::COMPUTING;
  // reset outdated values
  _superstepStats.reset();
  
  ThreadPool* pool = PregelFeature::instance()->threadPool();
  size_t total = _graphStore->vertexCount();
  size_t delta = total / pool->numThreads();
  size_t start = 0, end = delta;
  _runningThreads = total / delta;  // rounds-up unsigned integers
  unsigned i = 0;
  do {
    pool->enqueue([this, start, end] {
      if (_state != WorkerState::COMPUTING) {
        LOG(INFO) << "Execution aborted prematurely.";
        return;
      }
      auto vertexIterator = _graphStore->vertexIterator(start, end);
      _processVertices(vertexIterator);
    });
    start = end;
    end = end + delta;
    if (total < delta + end) {  // swallow the rest
      end = total;
    }
    i++;
  } while (start != total);
  if (i != total / delta) {
    LOG(ERR) << "FFFFFUUUUU";
  }
}

template <typename V, typename E, typename M>
void Worker<V, E, M>::_initializeVertexContext(VertexContext<V, E, M>* ctx) {
  ctx->_gss = _config.globalSuperstep();
  ctx->_lss = _config.localSuperstep();
  ctx->_context = _workerContext.get();
  ctx->_graphStore = _graphStore.get();
  ctx->_conductorAggregators = _conductorAggregators.get();
}

// internally called in a WORKER THREAD!!
template <typename V, typename E, typename M>
void Worker<V, E, M>::_processVertices(
    RangeIterator<VertexEntry>& vertexIterator) {
  double start = TRI_microtime();

  // thread local caches
  std::unique_ptr<InCache<M>> inCache;
  std::unique_ptr<OutCache<M>> outCache;
  if (_messageCombiner) {
    inCache.reset(
        new CombiningInCache<M>(_messageFormat.get(), _messageCombiner.get()));
    if (_config.asynchronousMode()) {
      outCache.reset(new CombiningOutCache<M>(&_config,
                                              (CombiningInCache<M>*)inCache.get(),
                                              _writeCacheNextGSS));
    } else {
      outCache.reset(new CombiningOutCache<M>(&_config,
                                              (CombiningInCache<M>*)inCache.get()));
    }
  } else {
    inCache.reset(new ArrayInCache<M>(_messageFormat.get()));
    if (_config.asynchronousMode()) {
      outCache.reset(new ArrayOutCache<M>(&_config, inCache.get(), _writeCacheNextGSS));
    } else {
       outCache.reset(new ArrayOutCache<M>(&_config, inCache.get()));
    }
  }

  AggregatorHandler workerAggregator(_algorithm.get());

  // TODO look if we can avoid instantiating this
  std::unique_ptr<VertexComputation<V, E, M>> vertexComputation(
      _algorithm->createComputation(_config.globalSuperstep()));
  _initializeVertexContext(vertexComputation.get());
  vertexComputation->_workerAggregators = &workerAggregator;
  vertexComputation->_cache = outCache.get();
  if (_config.asynchronousMode()) {
    outCache->sendToNextGSS(_requestedNextGSS);
  }

  size_t activeCount = 0;
  for (VertexEntry* vertexEntry : vertexIterator) {
    MessageIterator<M> messages =
        _readCache->getMessages(vertexEntry->shard(), vertexEntry->key());

    if (messages.size() > 0 || vertexEntry->active()) {
      vertexComputation->_vertexEntry = vertexEntry;
      vertexComputation->compute(messages);
      if (vertexEntry->active()) {
        activeCount++;
      } /* else {
         LOG(INFO) << vertexEntry->key() << " vertex has halted";
       }*/
    }
    // TODO delete read messages immediatly
    // technically messages to non-existing vertices trigger
    // their creation

    if (_state != WorkerState::COMPUTING) {
      LOG(INFO) << "Execution aborted prematurely.";
      break;
    }
  }
  // ==================== send messages to other shards ====================
  outCache->flushMessages();
  if (!_requestedNextGSS && vertexComputation->_nextPhase) {
    _requestedNextGSS = true;
    _nextGSSSendMessageCount += outCache->sendCountNextGSS();
  }
  
  // merge thread local messages, _writeCache does locking
  _writeCache->mergeCache(inCache.get());
  // TODO ask how to implement message sending without waiting for a response

  LOG(INFO) << "Finished executing vertex programs.";

  WorkerStats stats;
  stats.activeCount = activeCount;
  stats.sendCount = outCache->sendCount();
  stats.superstepRuntimeSecs = TRI_microtime() - start;
  _finishedProcessing(vertexComputation->_workerAggregators, stats);
}

// called at the end of a worker thread, needs mutex
template <typename V, typename E, typename M>
void Worker<V, E, M>::_finishedProcessing(AggregatorHandler* threadAggregators,
                                          WorkerStats const& threadStats) {
  MUTEX_LOCKER(guard, _threadMutex);  // only one thread at a time
  
  // merge the thread local stats and aggregators
  _workerAggregators->aggregateValues(*threadAggregators);
  _superstepStats.accumulate(threadStats);
  _runningThreads--;
  if (_runningThreads > 0) {// should work like a join operation
    return;// there are still threads running
  }
  // only locak this after there are no more processing threads
  MUTEX_LOCKER(guard2, _commandMutex);
  _state = WorkerState::IDLE;
  
  // ==================== Track statistics =================================
  // the stats we want to keep should be final. At this point we can only be
  // sure of the
  // messages we have received in total from the last superstep, and the
  // messages we have send in
  // the current superstep. Other workers are likely not finished yet, and might
  // still send stuff
  _superstepStats.receivedCount = _readCache->receivedMessageCount();
  _readCache->clear();  // no need to keep old messages around
  _expectedGSS = _config._globalSuperstep + 1;
  _config._localSuperstep++;
  // don't forget to reset before the superstep
  if (_config.asynchronousMode()) {
    _continueAsync();
  }

  // notify the conductor that we are done.
  VPackBuilder package;
  package.openObject();
  package.add(Utils::senderKey, VPackValue(ServerState::instance()->getId()));
  package.add(Utils::executionNumberKey, VPackValue(_config.executionNumber()));
  package.add(Utils::globalSuperstepKey, VPackValue(_config.globalSuperstep()));
  _superstepStats.serializeValues(package);  // add stats
  package.close();
  
  // TODO ask how to implement message sending without waiting for a response
  // ============ Call Coordinator ============
  _callConductor(Utils::finishedWorkerStepPath, package.slice());
  
}

template <typename V, typename E, typename M>
void Worker<V, E, M>::finalizeExecution(VPackSlice body) {
  // Only expect serial calls from the conductor.
  // Lock to prevent malicous activity
  MUTEX_LOCKER(guard, _commandMutex);
  _state = WorkerState::DONE;

  VPackSlice store = body.get(Utils::storeResultsKey);
  if (store.isBool() && store.getBool() == true) {
    _graphStore->storeResults(_config);
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
  MUTEX_LOCKER(guard, _commandMutex);

  _state = WorkerState::RECOVERING;
  VPackSlice method = data.get(Utils::recoveryMethodKey);
  if (method.compareString(Utils::compensate) == 0) {
    _preRecoveryTotal = _graphStore->vertexCount();
    WorkerConfig nextState(_config.database(), data);
    _graphStore->loadShards(nextState);
    _config = nextState;
    compensateStep(data);
  } else if (method.compareString(Utils::rollback) == 0) {
  }
}

template <typename V, typename E, typename M>
void Worker<V, E, M>::compensateStep(VPackSlice data) {
  MUTEX_LOCKER(guard, _commandMutex);

  _conductorAggregators->resetValues();
  VPackSlice aggValues = data.get(Utils::aggregatorValuesKey);
  if (aggValues.isObject()) {
    _conductorAggregators->aggregateValues(aggValues);
  }
  _workerAggregators->resetValues();

  ThreadPool* pool = PregelFeature::instance()->threadPool();
  pool->enqueue([this] {
    if (_state != WorkerState::RECOVERING) {
      LOG(INFO) << "Compensation aborted prematurely.";
      return;
    }

    auto vertexIterator = _graphStore->vertexIterator();

    // TODO look if we can avoid instantiating this
    std::unique_ptr<VertexCompensation<V, E, M>> vCompensate(
        _algorithm->createCompensation(_config.globalSuperstep()));
    _initializeVertexContext(vCompensate.get());
    vCompensate->_workerAggregators = _workerAggregators.get();

    size_t i = 0;
    for (VertexEntry* vertexEntry : vertexIterator) {
      vCompensate->_vertexEntry = vertexEntry;
      vCompensate->compensate(i < _preRecoveryTotal);
      i++;
      if (_state != WorkerState::RECOVERING) {
        LOG(INFO) << "Execution aborted prematurely.";
        break;
      }
    }
    VPackBuilder package;
    package.openObject();
    package.add(Utils::senderKey, VPackValue(ServerState::instance()->getId()));
    package.add(Utils::executionNumberKey,
                VPackValue(_config.executionNumber()));
    package.add(Utils::globalSuperstepKey,
                VPackValue(_config.globalSuperstep()));
    if (_workerAggregators->size() > 0) {  // add aggregators
      package.add(Utils::aggregatorValuesKey,
                  VPackValue(VPackValueType::Object));
      _workerAggregators->serializeValues(package);
      package.close();
    }
    package.close();
    _callConductor(Utils::finishedRecoveryPath, package.slice());
  });
}

/// WARNING only call this while holding the _commandMutex
/// in async mode checks if there are messages to process
template <typename V, typename E, typename M>
void Worker<V, E, M>::_continueAsync() {
  if (_state == WorkerState::IDLE && _writeCache->receivedMessageCount() > 0) {
    std::swap(_readCache, _writeCache);
    // overwrite conductor values with local values
    _conductorAggregators->resetValues();
    _conductorAggregators->aggregateValues(*_workerAggregators.get());
    _workerAggregators->resetValues();
    _startProcessing();
  }
}

template <typename V, typename E, typename M>
void Worker<V, E, M>::_callConductor(std::string path, VPackSlice message) {
  ClusterComm* cc = ClusterComm::instance();
  std::string baseUrl = Utils::baseUrl(_config.database());
  CoordTransactionID coordinatorTransactionID = TRI_NewTickServer();
  auto headers =
      std::make_unique<std::unordered_map<std::string, std::string>>();
  auto body = std::make_shared<std::string const>(message.toJson());
  cc->asyncRequest("", coordinatorTransactionID,
                   "server:" + _config.coordinatorId(), rest::RequestType::POST,
                   baseUrl + path, body, headers, nullptr,
                   90.0,  // timeout + single request
                   true);
}

// template types to create
template class arangodb::pregel::Worker<int64_t, int64_t, int64_t>;
template class arangodb::pregel::Worker<float, float, float>;
