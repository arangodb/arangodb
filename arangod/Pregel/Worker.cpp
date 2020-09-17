////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
#include "Pregel/CommonFormats.h"
#include "Pregel/GraphStore.h"
#include "Pregel/IncomingCache.h"
#include "Pregel/OutgoingCache.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/Utils.h"
#include "Pregel/VertexComputation.h"
#include "Pregel/WorkerConfig.h"

#include "Basics/MutexLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Basics/system-compiler.h"
#include "Cluster/ServerState.h"
#include "Futures/Utilities.h"
#include "Network/NetworkFeature.h"
#include "Network/Methods.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::pregel;

#define MY_READ_LOCKER(obj, lock)                                              \
  ReadLocker<ReadWriteLock> obj(&lock, arangodb::basics::LockerType::BLOCKING, \
                                true, __FILE__, __LINE__)

#define MY_WRITE_LOCKER(obj, lock)                                              \
  WriteLocker<ReadWriteLock> obj(&lock, arangodb::basics::LockerType::BLOCKING, \
                                 true, __FILE__, __LINE__)

template <typename V, typename E, typename M>
Worker<V, E, M>::Worker(TRI_vocbase_t& vocbase, Algorithm<V, E, M>* algo, VPackSlice initConfig)
    : _state(WorkerState::IDLE),
      _config(&vocbase, initConfig),
      _algorithm(algo),
      _nextGSSSendMessageCount(0),
      _requestedNextGSS(false) {
  MUTEX_LOCKER(guard, _commandMutex);

  VPackSlice userParams = initConfig.get(Utils::userParametersKey);

  _workerContext.reset(algo->workerContext(userParams));
  _messageFormat.reset(algo->messageFormat());
  _messageCombiner.reset(algo->messageCombiner());
  _conductorAggregators.reset(new AggregatorHandler(algo));
  _workerAggregators.reset(new AggregatorHandler(algo));
  _graphStore.reset(new GraphStore<V, E>(vocbase, _algorithm->inputFormat()));

  if (_config.asynchronousMode()) {
    _messageBatchSize = _algorithm->messageBatchSize(_config, _messageStats);
  } else {
    _messageBatchSize = 5000;
  }

  _initializeMessageCaches();
}

template <typename V, typename E, typename M>
Worker<V, E, M>::~Worker() {
  _state = WorkerState::DONE;
  std::this_thread::sleep_for(std::chrono::milliseconds(50));  // wait for threads to die
  delete _readCache;
  delete _writeCache;
  delete _writeCacheNextGSS;
  for (InCache<M>* cache : _inCaches) {
    delete cache;
  }
  for (OutCache<M>* cache : _outCaches) {
    delete cache;
  }
  _writeCache = nullptr;
}

template <typename V, typename E, typename M>
void Worker<V, E, M>::_initializeMessageCaches() {
  const size_t p = _config.parallelism();
  if (_messageCombiner) {
    _readCache = new CombiningInCache<M>(&_config, _messageFormat.get(),
                                         _messageCombiner.get());
    _writeCache = new CombiningInCache<M>(&_config, _messageFormat.get(),
                                          _messageCombiner.get());
    if (_config.asynchronousMode()) {
      _writeCacheNextGSS = new CombiningInCache<M>(&_config, _messageFormat.get(),
                                                   _messageCombiner.get());
    }
    for (size_t i = 0; i < p; i++) {
      auto incoming =
          std::make_unique<CombiningInCache<M>>(nullptr, _messageFormat.get(),
                                                _messageCombiner.get());
      _inCaches.push_back(incoming.get());
      _outCaches.push_back(new CombiningOutCache<M>(&_config, _messageFormat.get(),
                                                    _messageCombiner.get()));
      incoming.release();
    }
  } else {
    _readCache = new ArrayInCache<M>(&_config, _messageFormat.get());
    _writeCache = new ArrayInCache<M>(&_config, _messageFormat.get());
    if (_config.asynchronousMode()) {
      _writeCacheNextGSS = new ArrayInCache<M>(&_config, _messageFormat.get());
    }
    for (size_t i = 0; i < p; i++) {
      auto incoming = std::make_unique<ArrayInCache<M>>(nullptr, _messageFormat.get());
      _inCaches.push_back(incoming.get());
      _outCaches.push_back(new ArrayOutCache<M>(&_config, _messageFormat.get()));
      incoming.release();
    }
  }
}

// @brief load the initial worker data, call conductor eventually
template <typename V, typename E, typename M>
void Worker<V, E, M>::setupWorker() {
  std::function<void()> cb = [this] {
    VPackBuilder package;
    package.openObject();
    package.add(Utils::senderKey, VPackValue(ServerState::instance()->getId()));
    package.add(Utils::executionNumberKey, VPackValue(_config.executionNumber()));
    package.add(Utils::vertexCountKey, VPackValue(_graphStore->localVertexCount()));
    package.add(Utils::edgeCountKey, VPackValue(_graphStore->localEdgeCount()));
    package.close();
    _callConductor(Utils::finishedStartupPath, package);
  };

  // initialization of the graphstore might take an undefined amount
  // of time. Therefore this is performed asynchronous
  TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);
  Scheduler* scheduler = SchedulerFeature::SCHEDULER;
  auto self = shared_from_this();
  bool queued = scheduler->queue(RequestLane::INTERNAL_LOW, [self, this, cb] {
                                                              _graphStore->loadShards(&_config, cb);
                                                            });
  if (!queued) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUEUE_FULL,
                                   "No available thread to load shards");
  }
}

template <typename V, typename E, typename M>
void Worker<V, E, M>::prepareGlobalStep(VPackSlice const& data, VPackBuilder& response) {
  // Only expect serial calls from the conductor.
  // Lock to prevent malicous activity
  MUTEX_LOCKER(guard, _commandMutex);
  if (_state != WorkerState::IDLE) {
    LOG_TOPIC("b8506", ERR, Logger::PREGEL)
        << "Cannot prepare a gss when the worker is not idle";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "Cannot prepare a gss when the worker is not idle");
  }
  _state = WorkerState::PREPARING;  // stop any running step
  LOG_TOPIC("f16f2", DEBUG, Logger::PREGEL) << "Received prepare GSS: " << data.toJson();
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

  // initialize worker context
  if (_workerContext && gss == 0 && _config.localSuperstep() == 0) {
    _workerContext->_readAggregators = _conductorAggregators.get();
    _workerContext->_writeAggregators = _workerAggregators.get();
    _workerContext->_vertexCount = data.get(Utils::vertexCountKey).getUInt();
    _workerContext->_edgeCount = data.get(Utils::edgeCountKey).getUInt();
    _workerContext->preApplication();
  }

  // make us ready to receive messages
  _config._globalSuperstep = gss;
  // write cache becomes the readable cache
  if (_config.asynchronousMode()) {
    MY_WRITE_LOCKER(wguard, _cacheRWLock);  // by design shouldn't be necessary
    TRI_ASSERT(_readCache->containedMessageCount() == 0);
    TRI_ASSERT(_writeCache->containedMessageCount() == 0);
    std::swap(_readCache, _writeCacheNextGSS);
    _writeCache->clear();
    _requestedNextGSS = false;  // only relevant for async
    _messageStats.sendCount = _nextGSSSendMessageCount;
    _nextGSSSendMessageCount = 0;
  } else {
    MY_WRITE_LOCKER(wguard, _cacheRWLock);
    TRI_ASSERT(_readCache->containedMessageCount() == 0);
    std::swap(_readCache, _writeCache);
    _config._localSuperstep = gss;
  }

  // only place where is makes sense to call this, since startGlobalSuperstep
  // might not be called again
  if (_workerContext && gss > 0) {
    _workerContext->postGlobalSuperstep(gss - 1);
  }

  // responds with info which allows the conductor to decide whether
  // to start the next GSS or end the execution
  response.openObject();
  response.add(Utils::senderKey, VPackValue(ServerState::instance()->getId()));
  response.add(Utils::activeCountKey, VPackValue(_activeCount));
  response.add(Utils::vertexCountKey, VPackValue(_graphStore->localVertexCount()));
  response.add(Utils::edgeCountKey, VPackValue(_graphStore->localEdgeCount()));
  _workerAggregators->serializeValues(response);
  response.close();
}

template <typename V, typename E, typename M>
void Worker<V, E, M>::receivedMessages(VPackSlice const& data) {
  VPackSlice gssSlice = data.get(Utils::globalSuperstepKey);
  uint64_t gss = gssSlice.getUInt();
  if (gss == _config._globalSuperstep) {
    {  // make sure the pointer is not changed while
       // parsing messages
      MY_READ_LOCKER(guard, _cacheRWLock);
      // handles locking for us
      _writeCache->parseMessages(data);
    }

    // Trigger the processing of vertices
    if (_config.asynchronousMode() && _state == WorkerState::IDLE) {
      _continueAsync();
    }
  } else if (_config.asynchronousMode() && gss == _config._globalSuperstep + 1) {
    MY_READ_LOCKER(guard, _cacheRWLock);
    _writeCacheNextGSS->parseMessages(data);
  } else {
    LOG_TOPIC("ecd34", ERR, Logger::PREGEL)
        << "Expected: " << _config._globalSuperstep << "Got: " << gss;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Superstep out of sync");
  }
}

/// @brief Setup next superstep
template <typename V, typename E, typename M>
void Worker<V, E, M>::startGlobalStep(VPackSlice const& data) {
  // Only expect serial calls from the conductor.
  // Lock to prevent malicous activity
  MUTEX_LOCKER(guard, _commandMutex);
  if (_state != WorkerState::PREPARING) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "Cannot start a gss when the worker is not prepared");
  }
  LOG_TOPIC("d5e44", DEBUG, Logger::PREGEL) << "Starting GSS: " << data.toJson();
  VPackSlice gssSlice = data.get(Utils::globalSuperstepKey);
  const uint64_t gss = (uint64_t)gssSlice.getUInt();
  if (gss != _config.globalSuperstep()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "Wrong GSS");
  }

  _workerAggregators->resetValues();
  _conductorAggregators->setAggregatedValues(data);
  // execute context
  if (_workerContext) {
    _workerContext->_vertexCount = data.get(Utils::vertexCountKey).getUInt();
    _workerContext->_edgeCount = data.get(Utils::edgeCountKey).getUInt();
    _workerContext->preGlobalSuperstep(gss);
  }

  LOG_TOPIC("39e20", DEBUG, Logger::PREGEL) << "Worker starts new gss: " << gss;
  _startProcessing();  // sets _state = COMPUTING;
}

template <typename V, typename E, typename M>
void Worker<V, E, M>::cancelGlobalStep(VPackSlice const& data) {
  MUTEX_LOCKER(guard, _commandMutex);
  _state = WorkerState::DONE;
  _workHandle.reset();
}

/// WARNING only call this while holding the _commandMutex
template <typename V, typename E, typename M>
void Worker<V, E, M>::_startProcessing() {
  _state = WorkerState::COMPUTING;
  _activeCount = 0;  // active count is only valid after the run
  TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);
  Scheduler* scheduler = SchedulerFeature::SCHEDULER;

  size_t total = _graphStore->localVertexCount();
  size_t numSegments = _graphStore->numberVertexSegments();

  if (total > 100000) {
    _runningThreads = std::min<size_t>(_config.parallelism(), numSegments);
  } else {
    _runningThreads = 1;
  }
  TRI_ASSERT(_runningThreads >= 1);
  TRI_ASSERT(_runningThreads <= _config.parallelism());
  size_t numT = _runningThreads;
  
  auto self = shared_from_this();
  for (size_t i = 0; i < numT; i++) {
    bool queued = scheduler->queue(RequestLane::INTERNAL_LOW, [self, this, i, numT, numSegments] {
      if (_state != WorkerState::COMPUTING) {
        LOG_TOPIC("f0e3d", WARN, Logger::PREGEL) << "Execution aborted prematurely.";
        return;
      }
      size_t startI = i * (numSegments / numT);
      size_t endI = (i+1) * (numSegments / numT);
      TRI_ASSERT(endI <= numSegments);
      
      auto vertices = _graphStore->vertexIterator(startI, endI);
      // should work like a join operation
      if (_processVertices(i, vertices) && _state == WorkerState::COMPUTING) {
        _finishedProcessing();  // last thread turns the lights out
      }
    });
    if (!queued) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUEUE_FULL,
                                     "No thread available to start processing");
    }
  }
  
  // TRI_ASSERT(_runningThreads == i);
  LOG_TOPIC("425c3", DEBUG, Logger::PREGEL) << "Using " << numT << " Threads";
}

template <typename V, typename E, typename M>
void Worker<V, E, M>::_initializeVertexContext(VertexContext<V, E, M>* ctx) {
  ctx->_gss = _config.globalSuperstep();
  ctx->_lss = _config.localSuperstep();
  ctx->_context = _workerContext.get();
  ctx->_graphStore = _graphStore.get();
  ctx->_readAggregators = _conductorAggregators.get();
}

// internally called in a WORKER THREAD!!
template <typename V, typename E, typename M>
bool Worker<V, E, M>::_processVertices(size_t threadId,
                                       RangeIterator<Vertex<V,E>>& vertexIterator) {
  double start = TRI_microtime();

  // thread local caches
  InCache<M>* inCache = _inCaches[threadId];
  OutCache<M>* outCache = _outCaches[threadId];
  outCache->setBatchSize(_messageBatchSize);
  outCache->setLocalCache(inCache);
  if (_config.asynchronousMode()) {
    outCache->sendToNextGSS(_requestedNextGSS);
    outCache->setLocalCacheNextGSS(_writeCacheNextGSS);
    TRI_ASSERT(outCache->sendCountNextGSS() == 0);
  }
  TRI_ASSERT(outCache->sendCount() == 0);

  AggregatorHandler workerAggregator(_algorithm.get());
  // TODO look if we can avoid instantiating this
  std::unique_ptr<VertexComputation<V, E, M>> vertexComputation(
      _algorithm->createComputation(&_config));
  _initializeVertexContext(vertexComputation.get());
  vertexComputation->_writeAggregators = &workerAggregator;
  vertexComputation->_cache = outCache;
  if (!_config.asynchronousMode()) {
    // Should cause enterNextGlobalSuperstep to do nothing
    vertexComputation->_enterNextGSS = true;
  }

  size_t activeCount = 0;
  for (; vertexIterator.hasMore(); ++vertexIterator) {
    Vertex<V,E>* vertexEntry = *vertexIterator;
    MessageIterator<M> messages =
        _readCache->getMessages(vertexEntry->shard(), vertexEntry->key());
    
    if (messages.size() > 0 || vertexEntry->active()) {
      vertexComputation->_vertexEntry = vertexEntry;
      vertexComputation->compute(messages);
      if (vertexEntry->active()) {
        activeCount++;
      }
    }
    if (_state != WorkerState::COMPUTING) {
      break;
    }
  }
  // ==================== send messages to other shards ====================
  outCache->flushMessages();
  if (ADB_UNLIKELY(!_writeCache)) {  // ~Worker was called
    LOG_TOPIC("ee2ab", WARN, Logger::PREGEL) << "Execution aborted prematurely.";
    return false;
  }
  if (vertexComputation->_enterNextGSS) {
    _requestedNextGSS = true;
    _nextGSSSendMessageCount += outCache->sendCountNextGSS();
  }

  // double t = TRI_microtime();
  // merge thread local messages, _writeCache does locking
  _writeCache->mergeCache(_config, inCache);
  // TODO ask how to implement message sending without waiting for a response
  // t = TRI_microtime() - t;

  MessageStats stats;
  stats.sendCount = outCache->sendCount();
  stats.superstepRuntimeSecs = TRI_microtime() - start;
  inCache->clear();
  outCache->clear();

  bool lastThread = false;
  {  // only one thread at a time
    MUTEX_LOCKER(guard, _threadMutex);

    // merge the thread local stats and aggregators
    _workerAggregators->aggregateValues(workerAggregator);
    _messageStats.accumulate(stats);
    _activeCount += activeCount;
    _runningThreads--;
    lastThread = _runningThreads == 0;  // should work like a join operation    
  }
  return lastThread;
}

// called at the end of a worker thread, needs mutex
template <typename V, typename E, typename M>
void Worker<V, E, M>::_finishedProcessing() {
  {
    MUTEX_LOCKER(guard, _threadMutex);
    if (_runningThreads != 0) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL, "only one thread should ever enter this region");
    }
  }

  VPackBuilder package;
  {  // only lock after there are no more processing threads
    MUTEX_LOCKER(guard, _commandMutex);
    if (_state != WorkerState::COMPUTING) {
      return;  // probably canceled
    }

    // count all received messages
    _messageStats.receivedCount = _readCache->containedMessageCount();

    _readCache->clear();  // no need to keep old messages around
    _expectedGSS = _config._globalSuperstep + 1;
    _config._localSuperstep++;
    // only set the state here, because _processVertices checks for it
    _state = WorkerState::IDLE;

    package.openObject();
    package.add(Utils::senderKey, VPackValue(ServerState::instance()->getId()));
    package.add(Utils::executionNumberKey, VPackValue(_config.executionNumber()));
    package.add(Utils::globalSuperstepKey, VPackValue(_config.globalSuperstep()));
    _messageStats.serializeValues(package);
    if (_config.asynchronousMode()) {
      _workerAggregators->serializeValues(package, true);
    }
    package.close();

    if (_config.asynchronousMode()) {
      // async adaptive message buffering
      _messageBatchSize = _algorithm->messageBatchSize(_config, _messageStats);
    } else {
      uint64_t tn = _config.parallelism();
      uint64_t s = _messageStats.sendCount / tn / 2UL;
      _messageBatchSize = s > 1000 ? (uint32_t)s : 1000;
    }
    _messageStats.resetTracking();
    LOG_TOPIC("13dbf", DEBUG, Logger::PREGEL) << "Batch size: " << _messageBatchSize;
  }

  if (_config.asynchronousMode()) {
    LOG_TOPIC("56a27", DEBUG, Logger::PREGEL) << "Finished LSS: " << package.toJson();

    // if the conductor is unreachable or has send data (try to) proceed
    _callConductorWithResponse(Utils::finishedWorkerStepPath, package, [this](VPackSlice response) {
      if (response.isObject()) {
        _conductorAggregators->aggregateValues(response);  // only aggregate values
        VPackSlice nextGSS = response.get(Utils::enterNextGSSKey);
        if (nextGSS.isBool()) {
          _requestedNextGSS = _requestedNextGSS || nextGSS.getBool();
        }
        _continueAsync();
      }
    });

  } else {  // no answer expected
    _callConductor(Utils::finishedWorkerStepPath, package);
    LOG_TOPIC("2de5b", DEBUG, Logger::PREGEL) << "Finished GSS: " << package.toJson();
  }
}

/// WARNING only call this while holding the _commandMutex
/// in async mode checks if there are messages to process
template <typename V, typename E, typename M>
void Worker<V, E, M>::_continueAsync() {
  {
    MUTEX_LOCKER(guard, _commandMutex);
    if (_state != WorkerState::IDLE || _writeCache->containedMessageCount() == 0) {
      return;
    }
    // avoid calling this method accidentially
    _state = WorkerState::COMPUTING;
  }

  TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);

  // wait for new messages before beginning to process
  int64_t milli = _writeCache->containedMessageCount() < _messageBatchSize ? 50 : 5;
  // start next iteration in $milli mseconds.
  bool queued = false;
  std::tie(queued, _workHandle) = SchedulerFeature::SCHEDULER->queueDelay(
      RequestLane::INTERNAL_LOW, std::chrono::milliseconds(milli), [this](bool cancelled) {
        if (!cancelled) {
          {  // swap these pointers atomically
            MY_WRITE_LOCKER(guard, _cacheRWLock);
            std::swap(_readCache, _writeCache);
            if (_writeCacheNextGSS->containedMessageCount() > 0) {
              _requestedNextGSS = true;
            }
          }
          MUTEX_LOCKER(guard, _commandMutex);
          // overwrite conductor values with local values
          _conductorAggregators->resetValues();
          _conductorAggregators->aggregateValues(*_workerAggregators.get());
          _workerAggregators->resetValues();
          _startProcessing();
        }
      });
  if (!queued) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_QUEUE_FULL, "No thread available to continue execution.");
  }
}

template <typename V, typename E, typename M>
void Worker<V, E, M>::finalizeExecution(VPackSlice const& body,
                                        std::function<void()> cb) {
  // Only expect serial calls from the conductor.
  // Lock to prevent malicous activity
  MUTEX_LOCKER(guard, _commandMutex);
  if (_state == WorkerState::DONE) {
    LOG_TOPIC("4067a", DEBUG, Logger::PREGEL) << "removing worker";
    cb();
    return;
  }
  
  auto self = shared_from_this();
  auto cleanup = [self, this, cb] {
    VPackBuilder body;
    body.openObject();
    body.add(Utils::senderKey, VPackValue(ServerState::instance()->getId()));
    body.add(Utils::executionNumberKey, VPackValue(_config.executionNumber()));
    body.close();
    _callConductor(Utils::finishedWorkerFinalizationPath, body);
    cb();
  };
  
  _state = WorkerState::DONE;
  VPackSlice store = body.get(Utils::storeResultsKey);
  if (store.isBool() && store.getBool() == true) {
    LOG_TOPIC("91264", DEBUG, Logger::PREGEL) << "Storing results";
    // tell graphstore to remove read locks
    _graphStore->storeResults(&_config, std::move(cleanup));
  } else {
    LOG_TOPIC("b3f35", WARN, Logger::PREGEL) << "Discarding results";
    cleanup();
  }
}

template <typename V, typename E, typename M>
void Worker<V, E, M>::aqlResult(VPackBuilder& b, bool withId) const {
  MUTEX_LOCKER(guard, _commandMutex);
  TRI_ASSERT(b.isEmpty());
  
//  std::vector<ShardID> const& shards = _config.globalShardIDs();
  std::string tmp;

  b.openArray(/*unindexed*/true);
  auto it = _graphStore->vertexIterator();
  for (; it.hasMore(); ++it) {
    Vertex<V,E> const* vertexEntry = *it;
    
    TRI_ASSERT(vertexEntry->shard() < _config.globalShardIDs().size());
    ShardID const& shardId = _config.globalShardIDs()[vertexEntry->shard()];
    
    b.openObject(/*unindexed*/true);
    
    if (withId) {
      std::string const& cname = _config.shardIDToCollectionName(shardId);
      if (!cname.empty()) {
        tmp.clear();
        tmp.append(cname);
        tmp.push_back('/');
        tmp.append(vertexEntry->key().data(), vertexEntry->key().size());
        b.add(StaticStrings::IdString, VPackValue(tmp));
      }
    }
    
    b.add(StaticStrings::KeyString, VPackValuePair(vertexEntry->key().data(),
                                                   vertexEntry->key().size(),
                                                   VPackValueType::String));
    
    V const& data = vertexEntry->data();
    // bool store =
    _graphStore->graphFormat()->buildVertexDocument(b, &data, sizeof(V));
    b.close();
  }
  b.close();
}

template <typename V, typename E, typename M>
void Worker<V, E, M>::startRecovery(VPackSlice const& data) {
  // other methods might lock _commandMutex
  MUTEX_LOCKER(guard, _commandMutex);
  VPackSlice method = data.get(Utils::recoveryMethodKey);
  if (method.compareString(Utils::compensate) != 0) {
    LOG_TOPIC("742c5", ERR, Logger::PREGEL) << "Unsupported operation";
    return;
  }
  // else if (method.compareString(Utils::rollback) == 0)

  _state = WorkerState::RECOVERING;
  {
    MY_WRITE_LOCKER(guard, _cacheRWLock);
    _writeCache->clear();
    _readCache->clear();
    if (_writeCacheNextGSS) {
      _writeCacheNextGSS->clear();
    }
  }

  VPackBuilder copy(data);
  // hack to determine newly added vertices
  _preRecoveryTotal = _graphStore->localVertexCount();
  WorkerConfig nextState(_config);
  nextState.updateConfig(data);
  _graphStore->loadShards(&nextState, [this, nextState, copy] {
    _config = nextState;
    compensateStep(copy.slice());
  });
}

template <typename V, typename E, typename M>
void Worker<V, E, M>::compensateStep(VPackSlice const& data) {
  MUTEX_LOCKER(guard, _commandMutex);

  _workerAggregators->resetValues();
  _conductorAggregators->setAggregatedValues(data);

  TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);
  Scheduler* scheduler = SchedulerFeature::SCHEDULER;
  auto self = shared_from_this();
  bool queued = scheduler->queue(RequestLane::INTERNAL_LOW, [self, this] {
    if (_state != WorkerState::RECOVERING) {
      LOG_TOPIC("554e2", WARN, Logger::PREGEL) << "Compensation aborted prematurely.";
      return;
    }

    auto vertexIterator = _graphStore->vertexIterator();
    std::unique_ptr<VertexCompensation<V, E, M>> vCompensate(
        _algorithm->createCompensation(&_config));
    _initializeVertexContext(vCompensate.get());
    if (!vCompensate) {
      _state = WorkerState::DONE;
      LOG_TOPIC("938d2", WARN, Logger::PREGEL) << "Compensation aborted prematurely.";
      return;
    }
    vCompensate->_writeAggregators = _workerAggregators.get();

    size_t i = 0;
    for (; vertexIterator.hasMore(); ++vertexIterator) {
      Vertex<V,E>* vertexEntry = *vertexIterator;
      vCompensate->_vertexEntry = vertexEntry;
      vCompensate->compensate(i > _preRecoveryTotal);
      i++;
      if (_state != WorkerState::RECOVERING) {
        LOG_TOPIC("e9011", WARN, Logger::PREGEL) << "Execution aborted prematurely.";
        break;
      }
    }
    VPackBuilder package;
    package.openObject();
    package.add(Utils::senderKey, VPackValue(ServerState::instance()->getId()));
    package.add(Utils::executionNumberKey, VPackValue(_config.executionNumber()));
    package.add(Utils::globalSuperstepKey, VPackValue(_config.globalSuperstep()));
    _workerAggregators->serializeValues(package);
    package.close();
    _callConductor(Utils::finishedRecoveryPath, package);
  });
  if (!queued) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_QUEUE_FULL, "No thread available to queue compensation.");
  }
}

template <typename V, typename E, typename M>
void Worker<V, E, M>::finalizeRecovery(VPackSlice const& data) {
  MUTEX_LOCKER(guard, _commandMutex);
  if (_state != WorkerState::RECOVERING) {
    LOG_TOPIC("22e42", WARN, Logger::PREGEL) << "Compensation aborted prematurely.";
    return;
  }

  _expectedGSS = data.get(Utils::globalSuperstepKey).getUInt();
  _messageStats.resetTracking();
  _state = WorkerState::IDLE;
  LOG_TOPIC("17f3c", INFO, Logger::PREGEL) << "Recovery finished";
}

template <typename V, typename E, typename M>
void Worker<V, E, M>::_callConductor(std::string const& path, VPackBuilder const& message) {
  if (ServerState::instance()->isRunningInCluster() == false) {
    TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);
    Scheduler* scheduler = SchedulerFeature::SCHEDULER;
    auto self = shared_from_this();
    bool queued = scheduler->queue(RequestLane::INTERNAL_LOW, [this, self, path, message] {
      VPackBuilder response;
      PregelFeature::handleConductorRequest(*_config.vocbase(), path,
                                            message.slice(), response);
    });
    if (!queued) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUEUE_FULL,
                                     "No thread available to call conductor");
    }
  } else {

    std::string baseUrl = Utils::baseUrl(Utils::conductorPrefix);
    
    VPackBuffer<uint8_t> buffer;
    buffer.append(message.data(), message.size());
    
    application_features::ApplicationServer& server = _config.vocbase()->server();
    auto const& nf = server.getFeature<arangodb::NetworkFeature>();
    network::ConnectionPool* pool = nf.pool();
    
    network::RequestOptions reqOpts;
    reqOpts.database = _config.database();
    
    network::sendRequest(pool, "server:" + _config.coordinatorId(),
                         fuerte::RestVerb::Post, baseUrl + path, std::move(buffer), reqOpts);
    
  }
}

template <typename V, typename E, typename M>
void Worker<V, E, M>::_callConductorWithResponse(std::string const& path,
                                                 VPackBuilder const& message,
                                                 std::function<void(VPackSlice slice)> handle) {
  LOG_TOPIC("6d349", TRACE, Logger::PREGEL) << "Calling the conductor";
  if (ServerState::instance()->isRunningInCluster() == false) {
    VPackBuilder response;
    PregelFeature::handleConductorRequest(*_config.vocbase(), path, message.slice(), response);
    handle(response.slice());
  } else {
    std::string baseUrl = Utils::baseUrl(Utils::conductorPrefix);
    
    application_features::ApplicationServer& server = _config.vocbase()->server();
    auto const& nf = server.getFeature<arangodb::NetworkFeature>();
    network::ConnectionPool* pool = nf.pool();
    
    VPackBuffer<uint8_t> buffer;
    buffer.append(message.data(), message.size());
    
    
    network::RequestOptions reqOpts;
    reqOpts.database = _config.database();
    reqOpts.skipScheduler = true;

    network::Response r = network::sendRequest(pool, "server:" + _config.coordinatorId(),
                                               fuerte::RestVerb::Post,
                                               baseUrl + path, std::move(buffer), reqOpts).get();
    
    if (handle) {
      handle(r.slice());
    }
  }
}
