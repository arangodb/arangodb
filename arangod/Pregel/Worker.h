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

#ifndef ARANGODB_PREGEL_WORKER_H
#define ARANGODB_PREGEL_WORKER_H 1

#include <atomic>
#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Basics/ReadWriteLock.h"
#include "Pregel/AggregatorHandler.h"
#include "Pregel/Algorithm.h"
#include "Pregel/Statistics.h"
#include "Pregel/WorkerConfig.h"
#include "Pregel/WorkerContext.h"

struct TRI_vocbase_t;
namespace arangodb {
class RestPregelHandler;
namespace pregel {

class IWorker {
 public:
  virtual ~IWorker(){};
  virtual void prepareGlobalStep(VPackSlice data, VPackBuilder& response) = 0;
  virtual void startGlobalStep(VPackSlice data) = 0;   // called by coordinator
  virtual void cancelGlobalStep(VPackSlice data) = 0;  // called by coordinator
  virtual void receivedMessages(VPackSlice data) = 0;
  virtual void finalizeExecution(VPackSlice data) = 0;
  virtual void startRecovery(VPackSlice data) = 0;
  virtual void compensateStep(VPackSlice data) = 0;
  virtual void finalizeRecovery(VPackSlice data) = 0;
};

template <typename V, typename E>
class GraphStore;

template <typename M>
class InCache;

template <typename T>
class RangeIterator;
class VertexEntry;

template <typename V, typename E, typename M>
class VertexContext;

template <typename V, typename E, typename M>
class Worker : public IWorker {
  // friend class arangodb::RestPregelHandler;

  enum WorkerState {
    DEFAULT,     // only initial
    IDLE,        // do nothing
    PREPARING,   // before starting GSS
    COMPUTING,   // during a superstep
    RECOVERING,  // during recovery
    DONE         // after calling finished
  };

  WorkerState _state = WorkerState::DEFAULT;
  WorkerConfig _config;
  uint64_t _expectedGSS = 0;
  std::unique_ptr<Algorithm<V, E, M>> _algorithm;
  std::unique_ptr<WorkerContext> _workerContext;
  // locks modifying member vars
  mutable Mutex _commandMutex;
  // locks _workerThreadDone
  mutable Mutex _threadMutex;
  // locks swapping
  mutable arangodb::basics::ReadWriteLock _cacheRWLock;

  // only valid while recovering to determine the offset
  // where new vertices were inserted
  size_t _preRecoveryTotal;

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
  /// During async mode this should keep track of the send messages
  std::atomic<uint64_t> _nextGSSSendMessageCount;
  /// if the worker has started sendng messages to the next GSS
  std::atomic<bool> _requestedNextGSS;

  MessageStats _messageStats;
  uint64_t _activeCount = 0;
  size_t _runningThreads = 0;
  void _initializeVertexContext(VertexContext<V, E, M>* ctx);
  void _startProcessing();
  bool _processVertices(RangeIterator<VertexEntry>& vertexIterator);
  void _finishedProcessing();
  void _continueAsync();
  void _callConductor(std::string path, VPackSlice message);

 public:
  Worker(TRI_vocbase_t* vocbase, Algorithm<V, E, M>* algorithm,
         VPackSlice params);
  ~Worker();

  // ====== called by rest handler =====
  void prepareGlobalStep(VPackSlice data, VPackBuilder& response) override;
  void startGlobalStep(VPackSlice data) override;
  void cancelGlobalStep(VPackSlice data) override;
  void receivedMessages(VPackSlice data) override;
  void finalizeExecution(VPackSlice data) override;
  void startRecovery(VPackSlice data) override;
  void compensateStep(VPackSlice data) override;
  void finalizeRecovery(VPackSlice data) override;
};
}
}
#endif
