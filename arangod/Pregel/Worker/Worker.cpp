////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include "Worker.h"

#include "Basics/MutexLocker.h"
#include "Basics/voc-errors.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/RequestLane.h"
#include "Pregel/Aggregator.h"
#include "Pregel/CommonFormats.h"
#include "Pregel/Connection/DirectConnection.h"
#include "Pregel/Connection/NetworkConnection.h"
#include "Pregel/Messaging/Message.h"
#include "Pregel/IncomingCache.h"
#include "Pregel/OutgoingCache.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/Status/Status.h"
#include "Pregel/VertexComputation.h"
#include "Pregel/Worker/ConductorApi.h"
#include "Pregel/WorkerInterface.h"
#include "Pregel/Worker/GraphStore.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/WriteLocker.h"
#include "Metrics/Counter.h"
#include "Metrics/Gauge.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Scheduler/SchedulerFeature.h"

#include "Inspection/VPack.h"
#include "velocypack/Builder.h"

#include "fmt/core.h"
#include <chrono>
#include <variant>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::pregel;

#define LOG_PREGEL(logId, level)          \
  LOG_TOPIC(logId, level, Logger::PREGEL) \
      << "[job " << _config.executionNumber() << "] "

#define MY_READ_LOCKER(obj, lock)                                              \
  ReadLocker<ReadWriteLock> obj(&lock, arangodb::basics::LockerType::BLOCKING, \
                                true, __FILE__, __LINE__)

#define MY_WRITE_LOCKER(obj, lock) \
  WriteLocker<ReadWriteLock> obj(  \
      &lock, arangodb::basics::LockerType::BLOCKING, true, __FILE__, __LINE__)

template<typename V, typename E, typename M>
std::function<void()> Worker<V, E, M>::_makeStatusCallback() {
  return [self = shared_from_this(), this] {
    auto statusUpdatedEvent =
        StatusUpdated{.senderId = ServerState::instance()->getId(),
                      .status = _observeStatus()};
    _callConductor(ModernMessage{.executionNumber = _config.executionNumber(),
                                 .payload = statusUpdatedEvent});
  };
}

template<typename V, typename E, typename M>
Worker<V, E, M>::Worker(TRI_vocbase_t& vocbase, Algorithm<V, E, M>* algo,
                        CreateWorker const& initConfig, PregelFeature& feature)
    : _feature(feature),
      _state(WorkerState::IDLE),
      _config(&vocbase),
      _algorithm(algo) {
  _config.updateConfig(_feature, initConfig);
  if (ServerState::instance()->getRole() == ServerState::ROLE_SINGLE) {
    _conductor = worker::ConductorApi{
        _config.coordinatorId(), _config.executionNumber(),
        std::make_unique<DirectConnection>(_feature, vocbase)};
  } else {
    network::RequestOptions reqOpts;
    reqOpts.database = vocbase.name();
    _conductor = worker::ConductorApi{
        _config.coordinatorId(), _config.executionNumber(),
        std::make_unique<NetworkConnection>(Utils::apiPrefix,
                                            std::move(reqOpts), vocbase)};
  }

  MUTEX_LOCKER(guard, _commandMutex);

  _workerContext.reset(algo->workerContext(initConfig.userParameters.slice()));
  _messageFormat.reset(algo->messageFormat());
  _messageCombiner.reset(algo->messageCombiner());
  _conductorAggregators = std::make_unique<AggregatorHandler>(algo);
  _workerAggregators = std::make_unique<AggregatorHandler>(algo);

  auto shardResolver = ShardResolver::create(
      ServerState::instance()->isRunningInCluster(),
      vocbase.server().getFeature<ClusterFeature>().clusterInfo());

  _graphStore = std::make_unique<GraphStore<V, E>>(
      _feature, vocbase, _config.executionNumber(), _algorithm->inputFormat(),
      std::move(shardResolver));

  _feature.metrics()->pregelWorkersNumber->fetch_add(1);

  _messageBatchSize = 5000;

  _initializeMessageCaches();
}

template<typename V, typename E, typename M>
Worker<V, E, M>::~Worker() {
  _state = WorkerState::DONE;
  std::this_thread::sleep_for(
      std::chrono::milliseconds(50));  // wait for threads to die
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

  _feature.metrics()->pregelWorkersNumber->fetch_sub(1);
  _feature.metrics()->pregelMemoryUsedForGraph->fetch_sub(
      _graphStore->allocatedSize());
}

template<typename V, typename E, typename M>
void Worker<V, E, M>::_initializeMessageCaches() {
  const size_t p = _config.parallelism();
  if (_messageCombiner) {
    _readCache = new CombiningInCache<M>(&_config, _messageFormat.get(),
                                         _messageCombiner.get());
    _writeCache = new CombiningInCache<M>(&_config, _messageFormat.get(),
                                          _messageCombiner.get());
    for (size_t i = 0; i < p; i++) {
      auto incoming = std::make_unique<CombiningInCache<M>>(
          nullptr, _messageFormat.get(), _messageCombiner.get());
      _inCaches.push_back(incoming.get());
      _outCaches.push_back(new CombiningOutCache<M>(
          &_config, _messageFormat.get(), _messageCombiner.get()));
      incoming.release();
    }
  } else {
    _readCache = new ArrayInCache<M>(&_config, _messageFormat.get());
    _writeCache = new ArrayInCache<M>(&_config, _messageFormat.get());
    for (size_t i = 0; i < p; i++) {
      auto incoming =
          std::make_unique<ArrayInCache<M>>(nullptr, _messageFormat.get());
      _inCaches.push_back(incoming.get());
      _outCaches.push_back(
          new ArrayOutCache<M>(&_config, _messageFormat.get()));
      incoming.release();
    }
  }
}

template<typename V, typename E, typename M>
void Worker<V, E, M>::receivedMessages(PregelMessage const& message) {
  if (message.gss != _config._globalSuperstep &&
      message.gss != _config._globalSuperstep + 1) {
    LOG_PREGEL("ecd34", ERR)
        << "Expected: " << _config._globalSuperstep << " Got: " << message.gss;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Superstep out of sync");
  }

  // make sure the cache and gss are not changed while parsing messages
  MY_READ_LOCKER(guard, _cacheRWLock);

  // queue message if read and write cache are not swapped yet, otherwise you
  // will loose this message
  if (message.gss == _config._globalSuperstep + 1 ||
      // if config.gss == 0 and _computationStarted == false: read and write
      // cache are not swapped yet, config.gss is currently 0 only due to its
      // initialization
      (_config._globalSuperstep == 0 && !_computationStarted)) {
    _messagesForNextGss.doUnderLock(
        [&](auto& messages) { messages.push_back(message); });
    return;
  }

  _writeCache->parseMessages(message);
}

template<typename V, typename E, typename M>
auto Worker<V, E, M>::_preGlobalSuperStep(RunGlobalSuperStep const& message)
    -> Result {
  const uint64_t gss = message.gss;

  // wait until worker received all messages that were sent to it
  auto timeout = std::chrono::minutes(5);
  auto start = std::chrono::steady_clock::now();
  while (message.sendCount != _writeCache->containedMessageCount()) {
    if (std::chrono::steady_clock::now() - start >= timeout) {
      return Result{
          TRI_ERROR_INTERNAL,
          fmt::format("gss {}: Worker did not receive all the messages that "
                      "were send to it. Received count {} != send count {}.",
                      _config._globalSuperstep,
                      _writeCache->containedMessageCount(), message.sendCount)};
    }
  }

  // write cache becomes the readable cache
  {
    MY_WRITE_LOCKER(wguard, _cacheRWLock);
    TRI_ASSERT(_readCache->containedMessageCount() == 0);
    std::swap(_readCache, _writeCache);
    _config._globalSuperstep = gss;
    _config._localSuperstep = gss;
    if (gss == 0) {
      // now config.gss is set explicitely to 0 and the computation started
      _computationStarted = true;
    }
  }

  // process all queued messages
  _messagesForNextGss.doUnderLock([&](auto& messages) {
    for (auto const& message : messages) {
      receivedMessages(message);
    }
    messages.clear();
  });

  if (gss == 0 && _workerContext) {
    _workerContext->_readAggregators = _conductorAggregators.get();
    _workerContext->_writeAggregators = _workerAggregators.get();
    _workerContext->_vertexCount = message.vertexCount;
    _workerContext->_edgeCount = message.edgeCount;
    _workerContext->preApplication();
  }

  _workerAggregators->resetValues();
  _conductorAggregators->setAggregatedValues(message.aggregators.slice());
  if (_workerContext) {
    _workerContext->_vertexCount = message.vertexCount;
    _workerContext->_edgeCount = message.edgeCount;
    _workerContext->preGlobalSuperstep(gss);
  }
  return Result{};
}

template<typename V, typename E, typename M>
auto Worker<V, E, M>::runGlobalSuperStep(RunGlobalSuperStep const& message)
    -> futures::Future<ResultT<GlobalSuperStepFinished>> {
  MUTEX_LOCKER(guard, _commandMutex);

  if (_state != WorkerState::IDLE) {
    return Result{TRI_ERROR_INTERNAL,
                  "Cannot start a gss when the worker is not idle"};
  }
  VPackBuilder serializedMessage;
  serialize(serializedMessage, message);
  LOG_PREGEL("d5e44", DEBUG) << "Starting GSS: " << serializedMessage.toJson();

  auto preGss = _preGlobalSuperStep(message);
  if (preGss.fail()) {
    return preGss;
  }
  LOG_PREGEL("39e20", DEBUG) << "Worker starts new gss: " << message.gss;

  return _processVerticesInThreads().thenValue(
      [&](auto results) -> ResultT<GlobalSuperStepFinished> {
        if (_state != WorkerState::COMPUTING) {
          return Result{TRI_ERROR_INTERNAL,
                        "Worker execution aborted prematurely."};
        }
        std::unordered_map<ShardID, uint64_t> sendCountPerShard;
        for (auto& result : results) {
          if (result.get().fail()) {
            return Result{result.get().errorNumber(),
                          fmt::format("Vertices could not be processed: {}",
                                      result.get().errorMessage())};
          }
          auto verticesProcessed = result.get().get();
          _runningThreads--;
          _feature.metrics()->pregelNumberOfThreads->fetch_sub(1);
          _workerAggregators->aggregateValues(
              verticesProcessed.aggregator.slice());
          _messageStats.accumulate(verticesProcessed.stats);
          for (auto const& [shard, count] :
               verticesProcessed.sendCountPerShard) {
            sendCountPerShard[shard] += count;
          }
          _activeCount += verticesProcessed.activeCount;
        }
        return _finishProcessing(sendCountPerShard);
      });
}

/// WARNING only call this while holding the _commandMutex
template<typename V, typename E, typename M>
auto Worker<V, E, M>::_processVerticesInThreads() -> VerticesProcessedFuture {
  _state = WorkerState::COMPUTING;
  _feature.metrics()->pregelWorkersRunningNumber->fetch_add(1);
  _activeCount = 0;  // active count is only valid after the run

  size_t total = _graphStore->localVertexCount();
  size_t numSegments = _graphStore->numberVertexSegments();

  if (total > 100000) {
    _runningThreads = std::min<size_t>(_config.parallelism(), numSegments);
  } else {
    _runningThreads = 1;
  }
  _feature.metrics()->pregelNumberOfThreads->fetch_add(_runningThreads);
  TRI_ASSERT(_runningThreads >= 1);
  TRI_ASSERT(_runningThreads <= _config.parallelism());

  auto processedVertices =
      std::vector<futures::Future<ResultT<VerticesProcessed>>>{};
  for (size_t i = 0; i < _runningThreads; i++) {
    size_t dividend = numSegments / _runningThreads;
    size_t remainder = numSegments % _runningThreads;
    size_t startI = (i * dividend) + std::min(i, remainder);
    size_t endI = ((i + 1) * dividend) + std::min(i + 1, remainder);
    TRI_ASSERT(endI <= numSegments);

    auto vertices = _graphStore->vertexIterator(startI, endI);
    processedVertices.emplace_back(
        futures::makeFuture(_processVertices(i, vertices)));
  }

  LOG_PREGEL("425c3", DEBUG)
      << "Starting processing using " << _runningThreads << " threads";
  return futures::collectAll(processedVertices);
}

template<typename V, typename E, typename M>
void Worker<V, E, M>::_initializeVertexContext(VertexContext<V, E, M>* ctx) {
  ctx->_gss = _config.globalSuperstep();
  ctx->_lss = _config.localSuperstep();
  ctx->_context = _workerContext.get();
  ctx->_graphStore = _graphStore.get();
  ctx->_readAggregators = _conductorAggregators.get();
}

template<typename V, typename E, typename M>
auto Worker<V, E, M>::_processVertices(
    size_t threadId, RangeIterator<Vertex<V, E>>& vertexIterator)
    -> ResultT<VerticesProcessed> {
  if (_state != WorkerState::COMPUTING) {
    return Result{TRI_ERROR_INTERNAL, "Execution aborted prematurely"};
  }

  double start = TRI_microtime();

  // thread local caches
  InCache<M>* inCache = _inCaches[threadId];
  OutCache<M>* outCache = _outCaches[threadId];
  outCache->setBatchSize(_messageBatchSize);
  outCache->setLocalCache(inCache);
  TRI_ASSERT(outCache->sendCount() == 0);
  TRI_ASSERT(outCache->sendCountPerShard().empty());

  AggregatorHandler workerAggregator(_algorithm.get());
  // TODO look if we can avoid instantiating this
  std::unique_ptr<VertexComputation<V, E, M>> vertexComputation(
      _algorithm->createComputation(&_config));
  _initializeVertexContext(vertexComputation.get());
  vertexComputation->_writeAggregators = &workerAggregator;
  vertexComputation->_cache = outCache;

  size_t activeCount = 0;
  for (; vertexIterator.hasMore(); ++vertexIterator) {
    Vertex<V, E>* vertexEntry = *vertexIterator;
    MessageIterator<M> messages =
        _readCache->getMessages(vertexEntry->shard(), vertexEntry->key());
    _currentGssObservables.messagesReceived += messages.size();
    _currentGssObservables.memoryBytesUsedForMessages +=
        messages.size() * sizeof(M);

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

    ++_currentGssObservables.verticesProcessed;
    if (_currentGssObservables.verticesProcessed %
            Utils::batchOfVerticesProcessedBeforeUpdatingStatus ==
        0) {
      _makeStatusCallback()();
    }
  }
  // ==================== send messages to other shards ====================
  outCache->flushMessages();
  if (ADB_UNLIKELY(!_writeCache)) {  // ~Worker was called
    return Result{TRI_ERROR_INTERNAL, "Worker execution aborted prematurely."};
  }

  // double t = TRI_microtime();
  // merge thread local messages, _writeCache does locking
  _writeCache->mergeCache(_config, inCache);
  // TODO ask how to implement message sending without waiting for a response
  // t = TRI_microtime() - t;

  _feature.metrics()->pregelMessagesSent->count(outCache->sendCount());
  MessageStats stats;
  stats.sendCount = outCache->sendCount();
  _currentGssObservables.messagesSent += outCache->sendCount();
  _currentGssObservables.memoryBytesUsedForMessages +=
      outCache->sendCount() * sizeof(M);
  stats.superstepRuntimeSecs = TRI_microtime() - start;

  VPackBuilder aggregatorVPack;
  {
    VPackObjectBuilder ob(&aggregatorVPack);
    workerAggregator.serializeValues(aggregatorVPack);
  }
  auto out = VerticesProcessed{std::move(aggregatorVPack), std::move(stats),
                               outCache->sendCountPerShard(), activeCount};

  inCache->clear();
  outCache->clear();

  return out;
}

// called at the end of a worker thread, needs mutex
template<typename V, typename E, typename M>
auto Worker<V, E, M>::_finishProcessing(
    std::unordered_map<ShardID, uint64_t> sendCountPerShard)
    -> ResultT<GlobalSuperStepFinished> {
  {
    MUTEX_LOCKER(guard, _threadMutex);
    if (_runningThreads != 0) {
      return Result{TRI_ERROR_INTERNAL,
                    "only one thread should ever enter this region"};
    }
  }

  _feature.metrics()->pregelWorkersRunningNumber->fetch_sub(1);
  if (_state != WorkerState::COMPUTING) {
    return Result{TRI_ERROR_INTERNAL,
                  "Worker in wrong state"};  // probably canceled
  }

  if (_workerContext) {
    _workerContext->postGlobalSuperstep(_config._globalSuperstep);
  }

  // count all received messages
  _messageStats.receivedCount = _readCache->containedMessageCount();
  _feature.metrics()->pregelMessagesReceived->count(
      _readCache->containedMessageCount());

  _allGssStatus.doUnderLock([this](AllGssStatus& obj) {
    obj.push(this->_currentGssObservables.observe());
  });
  _currentGssObservables.zero();
  _makeStatusCallback()();

  _readCache->clear();  // no need to keep old messages around
  _config._localSuperstep++;
  // only set the state here, because _processVertices checks for it
  _state = WorkerState::IDLE;

  VPackBuilder aggregators;
  {
    VPackObjectBuilder ob(&aggregators);
    _workerAggregators->serializeValues(aggregators);
  }
  GlobalSuperStepFinished gssFinishedEvent =
      GlobalSuperStepFinished{_messageStats,
                              sendCountPerShard,
                              _activeCount,
                              _graphStore->localVertexCount(),
                              _graphStore->localEdgeCount(),
                              aggregators};
  VPackBuilder event;
  serialize(event, gssFinishedEvent);
  LOG_PREGEL("2de5b", DEBUG) << "Finished GSS: " << event.toJson();

  uint64_t tn = _config.parallelism();
  uint64_t s = _messageStats.sendCount / tn / 2UL;
  _messageBatchSize = s > 1000 ? (uint32_t)s : 1000;
  _messageStats.reset();
  LOG_PREGEL("13dbf", DEBUG) << "Message batch size: " << _messageBatchSize;

  return gssFinishedEvent;
}

template<typename V, typename E, typename M>
auto Worker<V, E, M>::store(Store const& message)
    -> futures::Future<ResultT<Stored>> {
  _feature.metrics()->pregelWorkersStoringNumber->fetch_add(1);
  LOG_PREGEL("91264", DEBUG) << "Storing results";
  // tell graphstore to remove read locks
  return _graphStore->storeResults(&_config, _makeStatusCallback())
      .thenValue([&](auto result) -> ResultT<Stored> {
        _feature.metrics()->pregelWorkersStoringNumber->fetch_sub(1);
        if (result.fail()) {
          return result;
        }

        _state = WorkerState::DONE;

        return Stored{};
      });
}

template<typename V, typename E, typename M>
void Worker<V, E, M>::cancelGlobalStep(VPackSlice const& data) {
  MUTEX_LOCKER(guard, _commandMutex);
  _state = WorkerState::DONE;
  _workHandle.reset();
}

template<typename V, typename E, typename M>
auto Worker<V, E, M>::cleanup(Cleanup const& command)
    -> futures::Future<ResultT<CleanupFinished>> {
  // Only expect serial calls from the conductor.
  // Lock to prevent malicious activity
  MUTEX_LOCKER(guard, _commandMutex);
  LOG_PREGEL("4067a", DEBUG) << "Removing worker";
  return _feature.cleanupWorker(_config.executionNumber())
      .thenValue([](auto _) -> ResultT<CleanupFinished> {
        return {CleanupFinished{}};
      });
}

template<typename V, typename E, typename M>
auto Worker<V, E, M>::results(CollectPregelResults const& message) const
    -> futures::Future<ResultT<PregelResults>> {
  return futures::makeFuture(_resultsFct(message));
}

template<typename V, typename E, typename M>
auto Worker<V, E, M>::_resultsFct(CollectPregelResults const& message) const
    -> ResultT<PregelResults> {
  MUTEX_LOCKER(guard, _commandMutex);

  std::string tmp;

  VPackBuilder result;
  result.openArray(/*unindexed*/ true);
  auto it = _graphStore->vertexIterator();
  for (; it.hasMore(); ++it) {
    Vertex<V, E> const* vertexEntry = *it;

    TRI_ASSERT(vertexEntry->shard() < _config.globalShardIDs().size());
    ShardID const& shardId = _config.globalShardIDs()[vertexEntry->shard()];

    result.openObject(/*unindexed*/ true);

    if (message.withId) {
      std::string const& cname = _config.shardIDToCollectionName(shardId);
      if (!cname.empty()) {
        tmp.clear();
        tmp.append(cname);
        tmp.push_back('/');
        tmp.append(vertexEntry->key().data(), vertexEntry->key().size());
        result.add(StaticStrings::IdString, VPackValue(tmp));
      }
    }

    result.add(
        StaticStrings::KeyString,
        VPackValuePair(vertexEntry->key().data(), vertexEntry->key().size(),
                       VPackValueType::String));

    V const& data = vertexEntry->data();
    if (auto res =
            _graphStore->graphFormat()->buildVertexDocument(result, &data);
        !res) {
      return Result{TRI_ERROR_INTERNAL, "Failed to build vertex document"};
    }
    result.close();
  }
  result.close();
  return PregelResults{result};
}

template<typename V, typename E, typename M>
void Worker<V, E, M>::_callConductor(ModernMessage message) {
  if (!ServerState::instance()->isRunningInCluster()) {
    TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);
    Scheduler* scheduler = SchedulerFeature::SCHEDULER;
    scheduler->queue(RequestLane::INTERNAL_LOW,
                     [this, self = shared_from_this(), message] {
                       _feature.process(message, *_config.vocbase());
                     });
  } else {
    VPackBuffer<uint8_t> buffer;
    VPackBuilder serializedMessage;
    serialize(serializedMessage, message);

    buffer.append(serializedMessage.data(), serializedMessage.size());
    auto const& nf = _config.vocbase()
                         ->server()
                         .template getFeature<arangodb::NetworkFeature>();
    network::ConnectionPool* pool = nf.pool();

    network::RequestOptions reqOpts;
    reqOpts.database = _config.database();

    network::sendRequestRetry(pool, "server:" + _config.coordinatorId(),
                              fuerte::RestVerb::Post, Utils::apiPrefix,
                              std::move(buffer), reqOpts);
  }
}

template<typename V, typename E, typename M>
auto Worker<V, E, M>::_observeStatus() -> Status const {
  auto currentGss = _currentGssObservables.observe();
  auto fullGssStatus = _allGssStatus.copy();

  if (!currentGss.isDefault()) {
    fullGssStatus.gss.emplace_back(currentGss);
  }
  return Status{.graphStoreStatus = _graphStore->status(),
                .allGssStatus = fullGssStatus.gss.size() > 0
                                    ? std::optional{fullGssStatus}
                                    : std::nullopt};
}

template<typename V, typename E, typename M>
auto Worker<V, E, M>::loadGraph(LoadGraph const& graph) -> void {
  _feature.metrics()->pregelWorkersLoadingNumber->fetch_add(1);

  LOG_PREGEL("52070", DEBUG) << fmt::format(
      "Worker for execution number {} is loading", _config.executionNumber());
  auto graphLoaded = _graphStore->loadShards(&_config, _makeStatusCallback());
  LOG_PREGEL("52062", DEBUG)
      << fmt::format("Worker for execution number {} has finished loading.",
                     _config.executionNumber());
  _makeStatusCallback()();
  _feature.metrics()->pregelWorkersLoadingNumber->fetch_sub(1);
  _conductor.graphLoaded(graphLoaded);
}

// template types to create
template class arangodb::pregel::Worker<int64_t, int64_t, int64_t>;
template class arangodb::pregel::Worker<uint64_t, uint8_t, uint64_t>;
template class arangodb::pregel::Worker<float, float, float>;
template class arangodb::pregel::Worker<double, float, double>;
template class arangodb::pregel::Worker<float, uint8_t, float>;

// custom algorithm types
template class arangodb::pregel::Worker<uint64_t, uint64_t,
                                        SenderMessage<uint64_t>>;
template class arangodb::pregel::Worker<WCCValue, uint64_t,
                                        SenderMessage<uint64_t>>;
template class arangodb::pregel::Worker<SCCValue, int8_t,
                                        SenderMessage<uint64_t>>;
template class arangodb::pregel::Worker<HITSValue, int8_t,
                                        SenderMessage<double>>;
template class arangodb::pregel::Worker<HITSKleinbergValue, int8_t,
                                        SenderMessage<double>>;
template class arangodb::pregel::Worker<ECValue, int8_t, HLLCounter>;
template class arangodb::pregel::Worker<DMIDValue, float, DMIDMessage>;
template class arangodb::pregel::Worker<LPValue, int8_t, uint64_t>;
template class arangodb::pregel::Worker<SLPAValue, int8_t, uint64_t>;
