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
#include "Cluster/ClusterInfo.h"

#include "Algorithm.h"
#include "WorkerContext.h"

struct TRI_vocbase_t;
namespace arangodb {
namespace pregel {

class IWorker {
 public:
  virtual ~IWorker(){};
  virtual void nextGlobalStep(VPackSlice data) = 0;  // called by coordinator
  virtual void receivedMessages(VPackSlice data) = 0;
  virtual void writeResults() = 0;

  static IWorker* createWorker(TRI_vocbase_t* vocbase, VPackSlice parameters);
};

template <typename V, typename E, typename M>
class WorkerJob;
template <typename V, typename E>
class GraphStore;

template <typename V, typename E, typename M>
class Worker : public IWorker {
  friend class WorkerJob<V, E, M>;

 public:
  Worker(std::shared_ptr<GraphStore<V, E>> graphStore,
         std::shared_ptr<WorkerContext<V, E, M>> context);
  ~Worker();

  void nextGlobalStep(VPackSlice data) override;  // called by coordinator
  void receivedMessages(VPackSlice data) override;
  void writeResults() override;

 private:
  // Mutex _messagesMutex; TODO figure this out
  std::shared_ptr<WorkerContext<V, E, M>> _ctx;
  std::shared_ptr<GraphStore<V, E>> _graphStore;

  void workerJobIsDone(bool allVerticesHalted);
};
}
}
#endif
