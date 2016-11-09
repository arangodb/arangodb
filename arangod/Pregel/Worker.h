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
#include "Basics/ThreadPool.h"
#include "WorkerState.h"

struct TRI_vocbase_t;
namespace arangodb {
namespace pregel {

class IWorker {
 public:
  virtual ~IWorker(){};
  virtual void prepareGlobalStep(VPackSlice data) = 0;
  virtual void startGlobalStep(VPackSlice data) = 0;  // called by coordinator
  virtual void receivedMessages(VPackSlice data) = 0;
  virtual void finalizeExecution(VPackSlice data) = 0;

  static IWorker* createWorker(TRI_vocbase_t* vocbase, VPackSlice parameters);
};

template <typename V, typename E>
class GraphStore;
  
template <typename V, typename E, typename M>
class Worker : public IWorker {
  
 public:
  Worker(std::shared_ptr<GraphStore<V, E>> graphStore,
         std::shared_ptr<WorkerState<V, E, M>> context);
  ~Worker();

  void prepareGlobalStep(VPackSlice data) override;
  void startGlobalStep(VPackSlice data) override;  // called by coordinator
  void receivedMessages(VPackSlice data) override;
  void finalizeExecution(VPackSlice data) override;

 private:
  bool _running = true;
  std::shared_ptr<WorkerState<V, E, M>> _ctx;
  std::shared_ptr<GraphStore<V, E>> _graphStore;
  std::unique_ptr<basics::ThreadPool> _workerPool;
  std::map<std::string, std::unique_ptr<Aggregator>> _aggregators;

  void workerJobIsDone(bool allVerticesHalted);
};
}
}
#endif
