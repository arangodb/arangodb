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

#pragma once

#include <mutex>

#include "Basics/Common.h"

#include "Basics/Guarded.h"
#include "Basics/ReadWriteLock.h"
#include "Pregel/AggregatorHandler.h"
#include "Pregel/Algorithm.h"
#include "Pregel/Conductor/Messages.h"
#include "Pregel/GraphStore/Magazine.h"
#include "Pregel/Statistics.h"
#include "Pregel/Status/Status.h"
#include "Pregel/Worker/Messages.h"
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
  virtual GlobalSuperStepPrepared prepareGlobalStep(
      PrepareGlobalSuperStep const& data) = 0;
  virtual void startGlobalStep(
      RunGlobalSuperStep const& data) = 0;  // called by coordinator
  virtual void cancelGlobalStep(
      VPackSlice const& data) = 0;  // called by coordinator
  virtual void receivedMessages(worker::message::PregelMessage const& data) = 0;
  virtual void finalizeExecution(FinalizeExecution const& data,
                                 std::function<void()> cb) = 0;
  virtual auto aqlResult(bool withId) const -> PregelResults = 0;
};

template<typename M>
class InCache;

template<typename M>
class OutCache;

template<typename T>
class RangeIterator;

template<typename V, typename E, typename M>
class VertexContext;

struct ProcessVerticesResult {
  AggregatorHandler workerAggregator;
  MessageStats stats;
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
  std::shared_ptr<WorkerConfig> _config;
  uint64_t _expectedGSS = 0;
  size_t _messageBatchSize = 500;
  std::unique_ptr<Algorithm<V, E, M>> _algorithm;
  std::unique_ptr<WorkerContext> _workerContext;
  // locks modifying member vars
  mutable std::mutex _commandMutex;
  // locks swapping
  mutable arangodb::basics::ReadWriteLock _cacheRWLock;

  std::unique_ptr<AggregatorHandler> _conductorAggregators;
  std::unique_ptr<AggregatorHandler> _workerAggregators;
  std::shared_ptr<Magazine<V, E>> _magazine;
  std::unique_ptr<MessageFormat<M>> _messageFormat;
  std::unique_ptr<MessageCombiner<M>> _messageCombiner;

  // from previous or current superstep
  InCache<M>* _readCache = nullptr;
  // for the current or next superstep
  InCache<M>* _writeCache = nullptr;

  GssObservables _currentGssObservables;
  Guarded<AllGssStatus> _allGssStatus;

  /// Stats about the CURRENT gss
  MessageStats _messageStats;
  /// valid after _finishedProcessing was called
  std::atomic<uint64_t> _activeCount = 0;
  /// current number of running threads
  size_t _runningThreads = 0;
  Scheduler::WorkHandle _workHandle;

  void _startProcessing();
  void _finishedProcessing();
  void _callConductor(std::string const& path,
                      VPackBuilder const& message) const;
  [[nodiscard]] auto _observeStatus() const -> Status const;
  [[nodiscard]] auto _makeStatusCallback() const -> std::function<void()>;

 public:
  Worker(TRI_vocbase_t& vocbase, Algorithm<V, E, M>* algorithm,
         worker::message::CreateWorker const& params, PregelFeature& feature);
  ~Worker();

  // ====== called by rest handler =====
  void setupWorker() override;
  GlobalSuperStepPrepared prepareGlobalStep(
      PrepareGlobalSuperStep const& data) override;
  void startGlobalStep(RunGlobalSuperStep const& data) override;
  void cancelGlobalStep(VPackSlice const& data) override;
  void receivedMessages(worker::message::PregelMessage const& data) override;
  void finalizeExecution(FinalizeExecution const& data,
                         std::function<void()> cb) override;

  auto aqlResult(bool withId) const -> PregelResults override;
};

}  // namespace arangodb::pregel
