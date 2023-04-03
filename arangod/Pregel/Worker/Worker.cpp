////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/WriteLocker.h"
#include "Basics/voc-errors.h"
#include "Cluster/ServerState.h"
#include "Containers/Enumerate.h"
#include "GeneralServer/RequestLane.h"
#include "Inspection/VPack.h"
#include "Inspection/VPackWithErrorT.h"
#include "Metrics/Counter.h"
#include "Metrics/Gauge.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Pregel/Aggregator.h"
#include "Pregel/AggregatorHandler.h"
#include "Pregel/Conductor/Messages.h"
#include "Pregel/GraphStore/Quiver.h"
#include "Pregel/GraphStore/GraphLoader.h"
#include "Pregel/GraphStore/GraphStorer.h"
#include "Pregel/GraphStore/GraphVPackBuilderStorer.h"
#include "Pregel/Worker/Messages.h"
#include "Pregel/Worker/Worker.h"
#include "Pregel/IncomingCache.h"
#include "Pregel/OutgoingCache.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/Status/Status.h"
#include "Pregel/VertexComputation.h"
#include "Scheduler/SchedulerFeature.h"
#include "VocBase/vocbase.h"

#include "Pregel/Algos/ColorPropagation/ColorPropagationValue.h"
#include "Pregel/Algos/DMID/DMIDValue.h"
#include "Pregel/Algos/DMID/DMIDMessage.h"
#include "Pregel/Algos/EffectiveCloseness/ECValue.h"
#include "Pregel/Algos/HITS/HITSValue.h"
#include "Pregel/Algos/HITSKleinberg/HITSKleinbergValue.h"
#include "Pregel/Algos/LabelPropagation/LPValue.h"
#include "Pregel/Algos/SCC/SCCValue.h"
#include "Pregel/Algos/SLPA/SLPAValue.h"
#include "Pregel/Algos/WCC/WCCValue.h"

#include <velocypack/Builder.h>

#include "fmt/core.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::pregel;

#define LOG_PREGEL(logId, level)          \
  LOG_TOPIC(logId, level, Logger::PREGEL) \
      << "[job " << _config->executionNumber() << "] "

#define MY_READ_LOCKER(obj, lock)                                              \
  ReadLocker<ReadWriteLock> obj(&lock, arangodb::basics::LockerType::BLOCKING, \
                                true, __FILE__, __LINE__)

#define MY_WRITE_LOCKER(obj, lock) \
  WriteLocker<ReadWriteLock> obj(  \
      &lock, arangodb::basics::LockerType::BLOCKING, true, __FILE__, __LINE__)

template<typename V, typename E, typename M>
Worker<V, E, M>::Worker(TRI_vocbase_t& vocbase, Algorithm<V, E, M>* algo,
                        worker::message::CreateWorker const& parameters,
                        PregelFeature& feature)
    : _feature(feature),
      _state(WorkerState::IDLE),
      _config(std::make_shared<WorkerConfig>(&vocbase)),
      _algorithm(algo),
      _magazine() {
  _config->updateConfig(parameters);

  std::lock_guard guard{_commandMutex};

  _workerContext.reset(
      algo->workerContext(std::make_unique<AggregatorHandler>(algo),
                          std::make_unique<AggregatorHandler>(algo),
                          parameters.userParameters.slice()));
  _messageFormat.reset(algo->messageFormat());
  _messageCombiner.reset(algo->messageCombiner());
  _conductorAggregators = std::make_unique<AggregatorHandler>(algo);
  _workerAggregators = std::make_unique<AggregatorHandler>(algo);

  _feature.metrics()->pregelWorkersNumber->fetch_add(1);

  _messageBatchSize = 5000;

  if (_messageCombiner) {
    _readCache =
        new CombiningInCache<M>(_config->localPregelShardIDs(),
                                _messageFormat.get(), _messageCombiner.get());
    _writeCache =
        new CombiningInCache<M>(_config->localPregelShardIDs(),
                                _messageFormat.get(), _messageCombiner.get());
  } else {
    _readCache = new ArrayInCache<M>(_config->localPregelShardIDs(),
                                     _messageFormat.get());
    _writeCache = new ArrayInCache<M>(_config->localPregelShardIDs(),
                                      _messageFormat.get());
  }

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
  _feature.metrics()->pregelMemoryUsedForGraph->fetch_sub(0);
}

template<typename V, typename E, typename M>
void Worker<V, E, M>::_initializeMessageCaches() {
  const size_t p = _magazine.size();
  if (_messageCombiner) {
    for (size_t i = 0; i < p; i++) {
      auto incoming = std::make_unique<CombiningInCache<M>>(
          std::set<PregelShard>{}, _messageFormat.get(),
          _messageCombiner.get());
      _inCaches.push_back(incoming.get());
      _outCaches.push_back(new CombiningOutCache<M>(
          _config, _messageFormat.get(), _messageCombiner.get()));
      incoming.release();
    }
  } else {
    for (size_t i = 0; i < p; i++) {
      auto incoming = std::make_unique<ArrayInCache<M>>(std::set<PregelShard>{},
                                                        _messageFormat.get());
      _inCaches.push_back(incoming.get());
      _outCaches.push_back(new ArrayOutCache<M>(_config, _messageFormat.get()));
      incoming.release();
    }
  }
}

// @brief load the initial worker data, call conductor eventually
template<typename V, typename E, typename M>
void Worker<V, E, M>::setupWorker() {
  std::function<void()> finishedCallback = [self = shared_from_this(), this] {
    LOG_PREGEL("52062", WARN)
        << fmt::format("Worker for execution number {} has finished loading.",
                       _config->executionNumber());
    auto graphLoaded = GraphLoaded{.executionNumber = _config->_executionNumber,
                                   .sender = ServerState::instance()->getId(),
                                   .vertexCount = _magazine.numberOfVertices(),
                                   .edgeCount = _magazine.numberOfEdges()};
    auto serialized = inspection::serializeWithErrorT(graphLoaded);
    if (!serialized.ok()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_FAILED,
          fmt::format("Cannot serialize GraphLoaded message: {}",
                      serialized.error().error()));
    }
    _callConductor(Utils::finishedStartupPath,
                   VPackBuilder(serialized.get().slice()));
    _feature.metrics()->pregelWorkersLoadingNumber->fetch_sub(1);
  };

  // initialization of the graphstore might take an undefined amount
  // of time. Therefore this is performed asynchronously
  TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);
  LOG_PREGEL("52070", WARN) << fmt::format(
      "Worker for execution number {} is loading", _config->executionNumber());
  _feature.metrics()->pregelWorkersLoadingNumber->fetch_add(1);
  Scheduler* scheduler = SchedulerFeature::SCHEDULER;
  scheduler->queue(RequestLane::INTERNAL_LOW,
                   [this, self = shared_from_this(),
                    statusUpdateCallback = std::move(_makeStatusCallback()),
                    finishedCallback = std::move(finishedCallback)] {
                     try {
                       auto loader = GraphLoader<V, E>(
                           _config, _algorithm->inputFormat(),
                           OldLoadingUpdate{.fn = statusUpdateCallback});
                       _magazine = std::move(loader.load());
                     } catch (std::exception const& ex) {
                       LOG_PREGEL("a47c4", WARN)
                           << "caught exception in loadShards: " << ex.what();
                       throw;
                     } catch (...) {
                       LOG_PREGEL("e932d", WARN)
                           << "caught unknown exception in loadShards";
                       throw;
                     }
                     finishedCallback();
                   });
}

template<typename V, typename E, typename M>
GlobalSuperStepPrepared Worker<V, E, M>::prepareGlobalStep(
    PrepareGlobalSuperStep const& data) {
  // Only expect serial calls from the conductor.
  // Lock to prevent malicous activity
  std::lock_guard guard{_commandMutex};
  if (_state != WorkerState::IDLE) {
    LOG_PREGEL("b8506", ERR)
        << "Cannot prepare a gss when the worker is not idle";
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "Cannot prepare a gss when the worker is not idle");
  }
  _state = WorkerState::PREPARING;  // stop any running step
  LOG_PREGEL("f16f2", DEBUG) << fmt::format("Received prepare GSS: {}", data);
  const uint64_t gss = data.gss;
  if (_expectedGSS != gss) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        fmt::format(
            "Seems like this worker missed a gss, expected {}. Data = {} ",
            _expectedGSS, data));
  }

  // initialize worker context
  if (_workerContext && gss == 0 && _config->localSuperstep() == 0) {
    _workerContext->_vertexCount = data.vertexCount;
    _workerContext->_edgeCount = data.edgeCount;
    _workerContext->preApplication();
  }

  // make us ready to receive messages
  _config->_globalSuperstep = gss;
  // write cache becomes the readable cache
  MY_WRITE_LOCKER(wguard, _cacheRWLock);
  TRI_ASSERT(_readCache->containedMessageCount() == 0);
  std::swap(_readCache, _writeCache);
  _config->_localSuperstep = gss;

  // only place where is makes sense to call this, since startGlobalSuperstep
  // might not be called again
  if (_workerContext && gss > 0) {
    _workerContext->postGlobalSuperstep(gss - 1);
  }

  // responds with info which allows the conductor to decide whether
  // to start the next GSS or end the execution
  VPackBuilder aggregators;
  {
    VPackObjectBuilder ob(&aggregators);
    _workerContext->_writeAggregators->serializeValues(aggregators);
  }
  return GlobalSuperStepPrepared{.executionNumber = _config->_executionNumber,
                                 .sender = ServerState::instance()->getId(),
                                 .activeCount = _activeCount,
                                 .vertexCount = _magazine.numberOfVertices(),
                                 .edgeCount = _magazine.numberOfEdges(),
                                 .aggregators = aggregators};
}

template<typename V, typename E, typename M>
void Worker<V, E, M>::receivedMessages(
    worker::message::PregelMessage const& data) {
  if (data.gss == _config->_globalSuperstep) {
    {  // make sure the pointer is not changed while
      // parsing messages
      MY_READ_LOCKER(guard, _cacheRWLock);
      // handles locking for us
      _writeCache->parseMessages(data);
    }

  } else {
    // Trigger the processing of vertices
    LOG_PREGEL("ecd34", ERR) << fmt::format(
        "Expected: {}, Got: {}", _config->_globalSuperstep, data.gss);
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Superstep out of sync");
  }
}

/// @brief Setup next superstep
template<typename V, typename E, typename M>
void Worker<V, E, M>::startGlobalStep(RunGlobalSuperStep const& data) {
  // Only expect serial calls from the conductor.
  // Lock to prevent malicous activity
  std::lock_guard guard{_commandMutex};
  if (_state != WorkerState::PREPARING) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "Cannot start a gss when the worker is not prepared");
  }
  LOG_PREGEL("d5e44", DEBUG) << fmt::format("Starting GSS: {}", data);

  _workerContext->_writeAggregators->resetValues();
  _workerContext->_readAggregators->setAggregatedValues(
      data.aggregators.slice());
  // execute context
  if (_workerContext) {
    _workerContext->_vertexCount = data.vertexCount;
    _workerContext->_edgeCount = data.edgeCount;
    _workerContext->preGlobalSuperstep(data.gss);
  }

  LOG_PREGEL("39e20", DEBUG) << "Worker starts new gss: " << data.gss;
  _startProcessing();  // sets _state = COMPUTING;
}

template<typename V, typename E, typename M>
void Worker<V, E, M>::cancelGlobalStep(VPackSlice const& data) {
  std::lock_guard guard{_commandMutex};
  _state = WorkerState::DONE;
  _workHandle.reset();
}

/// WARNING only call this while holding the _commandMutex
template<typename V, typename E, typename M>
void Worker<V, E, M>::_startProcessing() {
  _state = WorkerState::COMPUTING;
  _feature.metrics()->pregelWorkersRunningNumber->fetch_add(1);
  _activeCount = 0;  // active count is only valid after the run
  TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);
  Scheduler* scheduler = SchedulerFeature::SCHEDULER;

  _feature.metrics()->pregelNumberOfThreads->fetch_add(1);
  _initializeMessageCaches();

  auto self = shared_from_this();
  scheduler->queue(RequestLane::INTERNAL_LOW, [self, this] {
    if (_state != WorkerState::COMPUTING) {
      LOG_PREGEL("f0e3d", WARN) << "Execution aborted prematurely.";
      return;
    }
    for (auto [idx, quiver] : enumerate(_magazine)) {
      auto res = _processVertices(_inCaches[idx], _outCaches[idx], quiver);
      if (res.ok()) {
        auto& processRes = res.get();
        _workerContext->_writeAggregators->aggregateValues(
            processRes.workerAggregator);
        _messageStats.accumulate(processRes.stats);
      } else {
        // TODO: ?
      }
    }
    if (_state == WorkerState::COMPUTING) {
      _finishedProcessing();  // last thread turns the lights out
    }
  });

  LOG_PREGEL("425c3", DEBUG) << "Starting processing using " << 1 << " threads";
}

template<typename V, typename E, typename M>
void Worker<V, E, M>::_initializeVertexContext(VertexContext<V, E, M>* ctx) {
  ctx->_gss = _config->globalSuperstep();
  ctx->_lss = _config->localSuperstep();
  ctx->_context = _workerContext.get();
  ctx->_readAggregators = _workerContext->_readAggregators.get();
}

// internally called in a WORKER THREAD!!
template<typename V, typename E, typename M>
ResultT<ProcessVerticesResult> Worker<V, E, M>::_processVertices(
    InCache<M>* inCache, OutCache<M>* outCache,
    std::shared_ptr<Quiver<V, E>> quiver) {
  double start = TRI_microtime();

  // thread local caches
  outCache->setBatchSize(_messageBatchSize);
  outCache->setLocalCache(inCache);
  TRI_ASSERT(outCache->sendCount() == 0);

  ProcessVerticesResult verticesResult{
      .workerAggregator = AggregatorHandler(_algorithm.get()), .stats = {}};

  std::unique_ptr<VertexComputation<V, E, M>> vertexComputation(
      _algorithm->createComputation(_config));
  _initializeVertexContext(vertexComputation.get());
  vertexComputation->_writeAggregators = &verticesResult.workerAggregator;
  vertexComputation->_cache = outCache;

  size_t activeCount = 0;
  for (auto& vertexEntry : *quiver) {
    MessageIterator<M> messages =
        _readCache->getMessages(vertexEntry.shard(), vertexEntry.key());
    _currentGssObservables.messagesReceived += messages.size();
    _currentGssObservables.memoryBytesUsedForMessages +=
        messages.size() * sizeof(M);

    if (messages.size() > 0 || vertexEntry.active()) {
      vertexComputation->_vertexEntry = &vertexEntry;
      vertexComputation->compute(messages);
      if (vertexEntry.active()) {
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
    LOG_PREGEL("ee2ab", WARN) << "Execution aborted prematurely.";
    return {TRI_ERROR_INTERNAL};
  }

  // merge thread local messages, _writeCache does locking
  _writeCache->mergeCache(inCache);
  _feature.metrics()->pregelMessagesSent->count(outCache->sendCount());

  verticesResult.stats.sendCount = outCache->sendCount();
  _currentGssObservables.messagesSent += outCache->sendCount();
  _currentGssObservables.memoryBytesUsedForMessages +=
      outCache->sendCount() * sizeof(M);
  verticesResult.stats.superstepRuntimeSecs = TRI_microtime() - start;
  inCache->clear();
  outCache->clear();

  _activeCount += activeCount;
  _feature.metrics()->pregelNumberOfThreads->fetch_sub(1);
  return {std::move(verticesResult)};
}

// called at the end of a worker thread, needs mutex
template<typename V, typename E, typename M>
void Worker<V, E, M>::_finishedProcessing() {
  // only lock after there are no more processing threads
  std::lock_guard guard{_commandMutex};
  _feature.metrics()->pregelWorkersRunningNumber->fetch_sub(1);
  if (_state != WorkerState::COMPUTING) {
    return;  // probably canceled
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
  _expectedGSS = _config->_globalSuperstep + 1;
  _config->_localSuperstep++;
  // only set the state here, because _processVertices checks for it
  _state = WorkerState::IDLE;

  auto gssFinished =
      GlobalSuperStepFinished{.executionNumber = _config->executionNumber(),
                              .sender = ServerState::instance()->getId(),
                              .gss = _config->globalSuperstep(),
                              .messageStats = _messageStats};
  auto serialized = inspection::serializeWithErrorT(gssFinished);
  if (!serialized.ok()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        fmt::format("Cannot serialize GlobalSuperStepFinished message: {}",
                    serialized.error().error()));
  }
  _callConductor(Utils::finishedWorkerStepPath,
                 VPackBuilder(serialized.get().slice()));
  LOG_PREGEL("2de5b", DEBUG) << fmt::format("Finished GSS: {}", gssFinished);

  uint64_t tn = _config->parallelism();
  uint64_t s = _messageStats.sendCount / tn / 2UL;
  _messageBatchSize = s > 1000 ? (uint32_t)s : 1000;
  _messageStats.reset();
  LOG_PREGEL("13dbf", DEBUG) << "Message batch size: " << _messageBatchSize;
}

template<typename V, typename E, typename M>
void Worker<V, E, M>::finalizeExecution(FinalizeExecution const& msg,
                                        std::function<void()> cb) {
  // Only expect serial calls from the conductor.
  // Lock to prevent malicious activity
  std::lock_guard guard{_commandMutex};
  if (_state == WorkerState::DONE) {
    LOG_PREGEL("4067a", DEBUG) << "removing worker";
    cb();
    return;
  }

  auto cleanup = [self = shared_from_this(), this, msg, cb] {
    if (msg.store) {
      _feature.metrics()->pregelWorkersStoringNumber->fetch_sub(1);
    }

    auto finished = Finished{.executionNumber = _config->executionNumber(),
                             .sender = ServerState::instance()->getId()};
    auto serialized = inspection::serializeWithErrorT(finished);
    if (!serialized.ok()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          fmt::format("Cannot serialize Finished message: {}",
                      serialized.error().error()));
    }
    _callConductor(Utils::finishedWorkerFinalizationPath,
                   VPackBuilder(serialized.get().slice()));
    cb();
  };

  _state = WorkerState::DONE;
  if (msg.store) {
    LOG_PREGEL("91264", DEBUG) << "Storing results";
    _feature.metrics()->pregelWorkersStoringNumber->fetch_add(1);
    Scheduler* scheduler = SchedulerFeature::SCHEDULER;
    scheduler->queue(
        RequestLane::INTERNAL_LOW,
        [this, self = shared_from_this(),
         statusUpdateCallback = std::move(_makeStatusCallback()),
         cleanup = std::move(cleanup)] {
          try {
            auto storer = GraphStorer<V, E>(
                _config, _algorithm->inputFormat(), _config->globalShardIDs(),
                OldStoringUpdate{.fn = std::move(statusUpdateCallback)});
            _feature.metrics()->pregelWorkersStoringNumber->fetch_add(1);
            for (auto& quiver : _magazine) {
              storer.store(quiver);
            }
          } catch (std::exception const& ex) {
            LOG_PREGEL("a4774", WARN)
                << "caught exception in store: " << ex.what();
            throw;
          } catch (...) {
            LOG_PREGEL("e932e", WARN) << "caught unknown exception in store";
            throw;
          }
          cleanup();
        });
  } else {
    LOG_PREGEL("b3f35", WARN) << "Discarding results";
    cleanup();
  }
}

template<typename V, typename E, typename M>
auto Worker<V, E, M>::aqlResult(bool withId) const -> PregelResults {
  auto storer =
      GraphVPackBuilderStorer<V, E>(withId, _config, _algorithm->inputFormat(),
                                    std::move(_makeStatusCallback()));

  for (auto& quiver : _magazine) {
    storer.store(quiver);
  }
  return PregelResults{.results =
                           *storer.stealResult()};  // Yes, this is a copy rn.
}

template<typename V, typename E, typename M>
void Worker<V, E, M>::_callConductor(std::string const& path,
                                     VPackBuilder const& message) const {
  if (!ServerState::instance()->isRunningInCluster()) {
    TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);
    Scheduler* scheduler = SchedulerFeature::SCHEDULER;
    scheduler->queue(RequestLane::INTERNAL_LOW,
                     [this, self = shared_from_this(), path, message] {
                       VPackBuilder response;
                       _feature.handleConductorRequest(*_config->vocbase(),
                                                       path, message.slice(),
                                                       response);
                     });
  } else {
    std::string baseUrl = Utils::baseUrl(Utils::conductorPrefix);

    VPackBuffer<uint8_t> buffer;
    buffer.append(message.data(), message.size());
    auto const& nf = _config->vocbase()
                         ->server()
                         .template getFeature<arangodb::NetworkFeature>();
    network::ConnectionPool* pool = nf.pool();

    network::RequestOptions reqOpts;
    reqOpts.database = _config->database();

    network::sendRequestRetry(pool, "server:" + _config->coordinatorId(),
                              fuerte::RestVerb::Post, baseUrl + path,
                              std::move(buffer), reqOpts);
  }
}

template<typename V, typename E, typename M>
auto Worker<V, E, M>::_observeStatus() const -> Status const {
  auto currentGss = _currentGssObservables.observe();
  auto fullGssStatus = _allGssStatus.copy();

  if (!currentGss.isDefault()) {
    fullGssStatus.gss.emplace_back(currentGss);
  }
  return Status{.graphStoreStatus = {},  // TODO: GORDO-1583
                .allGssStatus = fullGssStatus.gss.size() > 0
                                    ? std::optional{fullGssStatus}
                                    : std::nullopt};
}

template<typename V, typename E, typename M>
auto Worker<V, E, M>::_makeStatusCallback() const -> std::function<void()> {
  return [self = shared_from_this(), this] {
    auto update = StatusUpdated{.executionNumber = _config->_executionNumber,
                                .sender = ServerState::instance()->getId(),
                                .status = _observeStatus()};
    auto serialized = inspection::serializeWithErrorT(update);
    if (!serialized.ok()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_FAILED,
          fmt::format("Cannot serialize StatusUpdated message: {}",
                      serialized.error().error()));
    }
    _callConductor(Utils::statusUpdatePath,
                   VPackBuilder(serialized.get().slice()));
  };
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
template class arangodb::pregel::Worker<algos::WCCValue, uint64_t,
                                        SenderMessage<uint64_t>>;
template class arangodb::pregel::Worker<algos::SCCValue, int8_t,
                                        SenderMessage<uint64_t>>;
template class arangodb::pregel::Worker<algos::HITSValue, int8_t,
                                        SenderMessage<double>>;
template class arangodb::pregel::Worker<algos::HITSKleinbergValue, int8_t,
                                        SenderMessage<double>>;
template class arangodb::pregel::Worker<algos::ECValue, int8_t, HLLCounter>;
template class arangodb::pregel::Worker<algos::DMIDValue, float, DMIDMessage>;
template class arangodb::pregel::Worker<algos::LPValue, int8_t, uint64_t>;
template class arangodb::pregel::Worker<algos::SLPAValue, int8_t, uint64_t>;
template class arangodb::pregel::Worker<algos::ColorPropagationValue, int8_t,
                                        algos::ColorPropagationMessageValue>;
