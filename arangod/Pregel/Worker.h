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

#include "Basics/Common.h"

#include <boost/date_time/posix_time/posix_time.hpp>

#include "Basics/Mutex.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/asio_ns.h"
#include "Pregel/AggregatorHandler.h"
#include "Pregel/Algorithm.h"
#include "Pregel/Statistics.h"
#include "Pregel/WorkerConfig.h"
#include "Pregel/WorkerContext.h"
#include "Scheduler/Scheduler.h"

struct TRI_vocbase_t;

namespace arangodb {

class RestPregelHandler;

namespace pregel {

class IWorker : public std::enable_shared_from_this<IWorker> {
 public:
  virtual ~IWorker() = default;
  virtual void setupWorker() = 0;
  virtual void prepareGlobalStep(VPackSlice const& data, VPackBuilder& result) = 0;
  virtual void startGlobalStep(VPackSlice const& data) = 0;  // called by coordinator
  virtual void cancelGlobalStep(VPackSlice const& data) = 0;  // called by coordinator
  virtual void receivedMessages(VPackSlice const& data) = 0;
  virtual void finalizeExecution(VPackSlice const& data,
                                 std::function<void()> cb) = 0;
  virtual void startRecovery(VPackSlice const& data) = 0;
  virtual void compensateStep(VPackSlice const& data) = 0;
  virtual void finalizeRecovery(VPackSlice const& data) = 0;
  virtual void aqlResult(VPackBuilder&, bool withId) const = 0;
};

template <typename V, typename E>
class GraphStore;

template <typename M>
class InCache;

template <typename M>
class OutCache;

template <typename T>
class RangeIterator;

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
  uint32_t _messageBatchSize = 500;
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
  size_t _preRecoveryTotal = 0;

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

  /// Stats about the CURRENT gss
  MessageStats _messageStats;
  /// valid after _finishedProcessing was called
  uint64_t _activeCount = 0;
  /// current number of running threads
  size_t _runningThreads = 0;
  /// During async mode this should keep track of the send messages
  std::atomic<uint64_t> _nextGSSSendMessageCount;
  /// if the worker has started sendng messages to the next GSS
  std::atomic<bool> _requestedNextGSS;
  Scheduler::WorkHandle _workHandle;

  void _initializeMessageCaches();
  void _initializeVertexContext(VertexContext<V, E, M>* ctx);
  void _startProcessing();
  bool _processVertices(size_t threadId, RangeIterator<Vertex<V,E>>& vertexIterator);
  void _finishedProcessing();
  void _continueAsync();
  void _callConductor(std::string const& path, VPackBuilder const& message);
  void _callConductorWithResponse(std::string const& path, VPackBuilder const& message,
                                  std::function<void(VPackSlice slice)> handle);

 public:
  Worker(TRI_vocbase_t& vocbase, Algorithm<V, E, M>* algorithm, VPackSlice params);
  ~Worker();

  // ====== called by rest handler =====
  void setupWorker() override;
  void prepareGlobalStep(VPackSlice const& data, VPackBuilder& result) override;
  void startGlobalStep(VPackSlice const& data) override;
  void cancelGlobalStep(VPackSlice const& data) override;
  void receivedMessages(VPackSlice const& data) override;
  void finalizeExecution(VPackSlice const& data, std::function<void()> cb) override;
  void startRecovery(VPackSlice const& data) override;
  void compensateStep(VPackSlice const& data) override;
  void finalizeRecovery(VPackSlice const& data) override;

  void aqlResult(VPackBuilder&, bool withId) const override;
};

}  // namespace pregel
}  // namespace arangodb

#endif
