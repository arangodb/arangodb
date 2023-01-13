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
#include "Pregel/Conductor/Messages.h"
#include "Pregel/Statistics.h"
#include "Pregel/Status/Status.h"
#include "Pregel/Worker/WorkerConfig.h"
#include "Pregel/WorkerContext.h"
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
  virtual void setupWorker() = 0;
  virtual void prepareGlobalStep(PrepareGlobalSuperStep const& data,
                                 VPackBuilder& result) = 0;
  virtual void startGlobalStep(
      VPackSlice const& data) = 0;  // called by coordinator
  virtual void cancelGlobalStep(
      VPackSlice const& data) = 0;  // called by coordinator
  virtual void receivedMessages(VPackSlice const& data) = 0;
  virtual void finalizeExecution(VPackSlice const& data,
                                 std::function<void()> cb) = 0;
  virtual void aqlResult(VPackBuilder&, bool withId) const = 0;
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
  /// valid after _finishedProcessing was called
  uint64_t _activeCount = 0;
  /// current number of running threads
  size_t _runningThreads = 0;
  Scheduler::WorkHandle _workHandle;

  void _initializeMessageCaches();
  void _initializeVertexContext(VertexContext<V, E, M>* ctx);
  void _startProcessing();
  bool _processVertices(size_t threadId,
                        RangeIterator<Vertex<V, E>>& vertexIterator);
  void _finishedProcessing();
  void _callConductor(std::string const& path, VPackBuilder const& message);
  [[nodiscard]] auto _observeStatus() -> Status const;
  [[nodiscard]] auto _makeStatusCallback() -> std::function<void()>;

 public:
  Worker(TRI_vocbase_t& vocbase, Algorithm<V, E, M>* algorithm,
         CreateWorker const& params, PregelFeature& feature);
  ~Worker();

  // ====== called by rest handler =====
  void setupWorker() override;
  void prepareGlobalStep(PrepareGlobalSuperStep const& data,
                         VPackBuilder& result) override;
  void startGlobalStep(VPackSlice const& data) override;
  void cancelGlobalStep(VPackSlice const& data) override;
  void receivedMessages(VPackSlice const& data) override;
  void finalizeExecution(VPackSlice const& data,
                         std::function<void()> cb) override;

  void aqlResult(VPackBuilder&, bool withId) const override;
};

}  // namespace arangodb::pregel
