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

#pragma once

#include "Basics/Common.h"

#include "Basics/Mutex.h"
#include "Basics/Guarded.h"
#include "Basics/ReadWriteLock.h"
#include "Pregel/AggregatorHandler.h"
#include "Pregel/Algorithm.h"
#include "Pregel/Statistics.h"
#include "Pregel/Status/Status.h"
#include "Pregel/Messaging/Message.h"
#include "Pregel/Worker/ConductorApi.h"
#include "Pregel/Worker/WorkerConfig.h"
#include "Pregel/Worker/WorkerContext.h"
#include "Pregel/WorkerInterface.h"
#include "Scheduler/Scheduler.h"

struct TRI_vocbase_t;

namespace arangodb {
class RestPregelHandler;
}

namespace arangodb::pregel {

class PregelFeature;

class IWorker : public std::enable_shared_from_this<IWorker> {
 public:
  virtual ~IWorker() = default;
  virtual auto loadGraph(LoadGraph const& graph) -> void = 0;
  [[nodiscard]] virtual auto runGlobalSuperStep(RunGlobalSuperStep const& data)
      -> futures::Future<ResultT<GlobalSuperStepFinished>> = 0;
  [[nodiscard]] virtual auto store(Store const& message)
      -> futures::Future<ResultT<Stored>> = 0;
  [[nodiscard]] virtual auto cleanup(Cleanup const& message)
      -> futures::Future<ResultT<CleanupFinished>> = 0;
  [[nodiscard]] virtual auto results(CollectPregelResults const& message) const
      -> futures::Future<ResultT<PregelResults>> = 0;

  virtual void cancelGlobalStep(
      VPackSlice const& data) = 0;  // called by coordinator
  virtual void receivedMessages(PregelMessage const& data) = 0;
};

template<typename V, typename E>
class GraphStore;

template<typename M>
class InCache;

template<typename M>
class OutCache;

template<typename T>
class RangeIterator;

template<typename V, typename E, typename M>
class VertexContext;

struct VerticesProcessed {
  VPackBuilder aggregator;
  MessageStats stats;
  std::unordered_map<ShardID, uint64_t> sendCountPerShard;
  size_t activeCount;
};

template<typename V, typename E, typename M>
class Worker : public IWorker {
  // friend class arangodb::RestPregelHandler;

  enum WorkerState {
    DEFAULT,    // only initial
    IDLE,       // do nothing
    COMPUTING,  // during a superstep
    DONE        // after calling finished
  };

  worker::ConductorApi _conductor;
  PregelFeature& _feature;
  std::atomic<WorkerState> _state = WorkerState::DEFAULT;
  WorkerConfig _config;
  uint32_t _messageBatchSize = 500;
  std::unique_ptr<Algorithm<V, E, M>> _algorithm;
  std::unique_ptr<WorkerContext> _workerContext;
  // locks modifying member vars
  mutable Mutex _commandMutex;
  // locks _workerThreadDone
  mutable Mutex _threadMutex;
  // locks swapping
  mutable arangodb::basics::ReadWriteLock _cacheRWLock;

  std::unique_ptr<AggregatorHandler> _conductorAggregators;
  std::unique_ptr<AggregatorHandler> _workerAggregators;
  std::unique_ptr<GraphStore<V, E>> _graphStore;
  std::unique_ptr<MessageFormat<M>> _messageFormat;
  std::unique_ptr<MessageCombiner<M>> _messageCombiner;

  // from previous or current superstep
  InCache<M>* _readCache = nullptr;
  // for the current or next superstep
  InCache<M>* _writeCache = nullptr;
  // intended for the next superstep phase
  InCache<M>* _writeCacheNextGSS = nullptr;
  // preallocated incoming caches
  std::vector<InCache<M>*> _inCaches;
  // preallocated ootgoing caches
  std::vector<OutCache<M>*> _outCaches;
  Guarded<std::vector<PregelMessage>> _messagesForNextGss;

  GssObservables _currentGssObservables;
  Guarded<AllGssStatus> _allGssStatus;

  /// Stats about the CURRENT gss
  MessageStats _messageStats;
  /// valid after _finishedProcessing was called
  uint64_t _activeCount = 0;
  /// current number of running threads
  size_t _runningThreads = 0;
  Scheduler::WorkHandle _workHandle;
  // distinguishes config.globalSuperStep being initialized to 0 and
  // config.globalSuperStep being explicitely set to 0 when the first superstep
  // starts: needed to process incoming messages in its dedicated gss
  bool _computationStarted = false;

  using VerticesProcessedFuture =
      futures::Future<std::vector<futures::Try<ResultT<VerticesProcessed>>>>;
  void _initializeMessageCaches();
  void _initializeVertexContext(VertexContext<V, E, M>* ctx);
  auto _preGlobalSuperStep(RunGlobalSuperStep const& message) -> Result;
  auto _processVerticesInThreads() -> VerticesProcessedFuture;
  [[nodiscard]] auto _processVertices(
      size_t threadId, RangeIterator<Vertex<V, E>>& vertexIterator)
      -> ResultT<VerticesProcessed>;
  auto _finishProcessing(
      std::unordered_map<ShardID, uint64_t> sendCountPerShard)
      -> ResultT<GlobalSuperStepFinished>;
  void _callConductor(ModernMessage message);
  [[nodiscard]] auto _observeStatus() -> Status const;
  [[nodiscard]] auto _makeStatusCallback() -> std::function<void()>;

  [[nodiscard]] auto _prepareGlobalSuperStepFct(
      PrepareGlobalSuperStep const& data) -> ResultT<GlobalSuperStepPrepared>;
  [[nodiscard]] auto _resultsFct(CollectPregelResults const& message) const
      -> ResultT<PregelResults>;

 public:
  Worker(TRI_vocbase_t& vocbase, Algorithm<V, E, M>* algorithm,
         CreateWorker const& params, PregelFeature& feature);
  ~Worker();

  // ====== called by rest handler =====
  auto loadGraph(LoadGraph const& graph) -> void override;
  auto runGlobalSuperStep(RunGlobalSuperStep const& data)
      -> futures::Future<ResultT<GlobalSuperStepFinished>> override;
  auto store(Store const& message) -> futures::Future<ResultT<Stored>> override;
  auto cleanup(Cleanup const& message)
      -> futures::Future<ResultT<CleanupFinished>> override;
  auto results(CollectPregelResults const& message) const
      -> futures::Future<ResultT<PregelResults>> override;

  void cancelGlobalStep(VPackSlice const& data) override;
  void receivedMessages(PregelMessage const& data) override;
};

}  // namespace arangodb::pregel
