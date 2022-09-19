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
#include "Pregel/WorkerConductorMessages.h"
#include "Pregel/Worker/WorkerConfig.h"
#include "Pregel/Worker/WorkerContext.h"
#include "Pregel/WorkerInterface.h"
#include "Pregel/Reports.h"
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
  [[nodiscard]] virtual auto process(MessagePayload const& message)
      -> futures::Future<ResultT<ModernMessage>> = 0;
  [[nodiscard]] virtual auto loadGraph(LoadGraph const& graph)
      -> futures::Future<ResultT<GraphLoaded>> = 0;
  [[nodiscard]] virtual auto prepareGlobalSuperStep(
      PrepareGlobalSuperStep const& data)
      -> futures::Future<ResultT<GlobalSuperStepPrepared>> = 0;
  [[nodiscard]] virtual auto runGlobalSuperStep(RunGlobalSuperStep const& data)
      -> futures::Future<ResultT<GlobalSuperStepFinished>> = 0;
  virtual void cancelGlobalStep(
      VPackSlice const& data) = 0;  // called by coordinator
  virtual void receivedMessages(PregelMessage const& data) = 0;
  virtual auto finalizeExecution(StartCleanup const& data)
      -> CleanupStarted = 0;
  virtual auto aqlResult(bool withId) const -> PregelResults = 0;
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
  size_t activeCount;
  ReportManager reports;
};

template<typename V, typename E, typename M>
class Worker : public IWorker {
  // friend class arangodb::RestPregelHandler;

  enum WorkerState {
    DEFAULT,    // only initial
    IDLE,       // do nothing
    PREPARING,  // before starting GSS
    COMPUTING,  // during a superstep
    DONE        // after calling finished
  };

  PregelFeature& _feature;
  std::atomic<WorkerState> _state = WorkerState::DEFAULT;
  WorkerConfig _config;
  uint64_t _expectedGSS = 0;
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

  GssObservables _currentGssObservables;
  Guarded<AllGssStatus> _allGssStatus;

  /// Stats about the CURRENT gss
  MessageStats _messageStats;
  ReportManager _reports;
  /// valid after _finishedProcessing was called
  uint64_t _activeCount = 0;
  /// current number of running threads
  size_t _runningThreads = 0;
  /// if the worker has started sendng messages to the next GSS
  std::atomic<bool> _requestedNextGSS;
  Scheduler::WorkHandle _workHandle;

  using VerticesProcessedFuture =
      futures::Future<std::vector<futures::Try<ResultT<VerticesProcessed>>>>;
  void _initializeMessageCaches();
  void _initializeVertexContext(VertexContext<V, E, M>* ctx);
  auto _preGlobalSuperStep(RunGlobalSuperStep const& message) -> Result;
  auto _processVerticesInThreads() -> VerticesProcessedFuture;
  [[nodiscard]] auto _processVertices(
      size_t threadId, RangeIterator<Vertex<V, E>>& vertexIterator)
      -> ResultT<VerticesProcessed>;
  auto _finishProcessing() -> ResultT<GlobalSuperStepFinished>;
  void _callConductor(VPackBuilder const& message);
  void _callConductorWithResponse(std::string const& path,
                                  VPackBuilder const& message,
                                  std::function<void(VPackSlice slice)> handle);
  [[nodiscard]] auto _observeStatus() -> Status const;
  [[nodiscard]] auto _makeStatusCallback() -> std::function<void()>;

  auto _gssFinishedEvent() const -> GlobalSuperStepFinished;
  auto _cleanupFinishedEvent() const -> CleanupFinished;

 public:
  Worker(TRI_vocbase_t& vocbase, Algorithm<V, E, M>* algorithm,
         VPackSlice params, PregelFeature& feature);
  ~Worker();

  // futures helper function
  [[nodiscard]] auto prepareGlobalSuperStepFct(
      PrepareGlobalSuperStep const& data) -> ResultT<GlobalSuperStepPrepared>;

  // ====== called by rest handler =====
  auto process(MessagePayload const& message)
      -> futures::Future<ResultT<ModernMessage>> override;
  auto loadGraph(LoadGraph const& graph)
      -> futures::Future<ResultT<GraphLoaded>> override;
  auto prepareGlobalSuperStep(PrepareGlobalSuperStep const& data)
      -> futures::Future<ResultT<GlobalSuperStepPrepared>> override;
  auto runGlobalSuperStep(RunGlobalSuperStep const& data)
      -> futures::Future<ResultT<GlobalSuperStepFinished>> override;
  void cancelGlobalStep(VPackSlice const& data) override;
  void receivedMessages(PregelMessage const& data) override;
  auto finalizeExecution(StartCleanup const& data) -> CleanupStarted override;

  auto aqlResult(bool withId) const -> PregelResults override;
};

}  // namespace arangodb::pregel
