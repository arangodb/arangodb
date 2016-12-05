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
#include "Basics/Mutex.h"
#include "Pregel/AggregatorUsage.h"
#include "Pregel/Algorithm.h"
#include "Pregel/WorkerContext.h"
#include "Pregel/WorkerState.h"
#include "Pregel/Statistics.h"

struct TRI_vocbase_t;
namespace arangodb {
class RestPregelHandler;
namespace pregel {

class IWorker {
 public:
  virtual ~IWorker(){};
  virtual void prepareGlobalStep(VPackSlice data) = 0;
  virtual void startGlobalStep(VPackSlice data) = 0;  // called by coordinator
  virtual void receivedMessages(VPackSlice data) = 0;
  virtual void finalizeExecution(VPackSlice data) = 0;
  virtual void startRecovery(VPackSlice data) = 0;
  virtual void compensateStep(VPackSlice data) = 0;
};

template <typename V, typename E>
class GraphStore;

template <typename M>
class IncomingCache;
  
template <typename T>
class RangeIterator;
class VertexEntry;
  
template <typename V, typename E, typename M>
class VertexContext;
  
  

template <typename V, typename E, typename M>
class Worker : public IWorker {
  //friend class arangodb::RestPregelHandler;
  
  bool _running = true;
  WorkerState _state;
  WorkerStats _workerStats;
  uint64_t _expectedGSS = 0;
  std::unique_ptr<Algorithm<V, E, M>> _algorithm;
  std::unique_ptr<WorkerContext> _workerContext;
  Mutex _conductorMutex;// locks callbak methods
  mutable Mutex _threadMutex;// locks _workerThreadDone
  
  // only valid while recovering to determine the offset
  // where new vertices were inserted
  size_t _preRecoveryTotal;
 
  std::unique_ptr<GraphStore<V, E>> _graphStore;
  std::unique_ptr<IncomingCache<M>> _readCache, _writeCache;
  std::unique_ptr<AggregatorUsage> _conductorAggregators;
  std::unique_ptr<AggregatorUsage> _workerAggregators;
  std::unique_ptr<MessageFormat<M>> _messageFormat;
  std::unique_ptr<MessageCombiner<M>> _messageCombiner;
  WorkerStats _superstepStats;
  
  size_t _runningThreads;
  
  void _swapIncomingCaches() {
    _readCache.swap(_writeCache);
    _writeCache->clear();
  }
  
  void _initializeVertexContext(VertexContext<V, E, M> *ctx);
  void _executeGlobalStep(RangeIterator<VertexEntry> &vertexIterator);
  void _workerThreadDone(AggregatorUsage *threadAggregators,
                         WorkerStats const& threadStats);
  void _callConductor(std::string path, VPackSlice message);
 public:
  Worker(TRI_vocbase_t* vocbase, Algorithm<V, E, M>* algorithm,
         VPackSlice params);
  ~Worker();
  
  // ====== called by rest handler =====
  void prepareGlobalStep(VPackSlice data) override;
  void startGlobalStep(VPackSlice data) override;
  void receivedMessages(VPackSlice data) override;
  void finalizeExecution(VPackSlice data) override;
  void startRecovery(VPackSlice data) override;
  void compensateStep(VPackSlice data) override;
};
}
}
#endif
