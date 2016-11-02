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

#include "WorkerJob.h"
#include "GraphStore.h"
#include "IncomingCache.h"
#include "OutgoingCache.h"
#include "Utils.h"
#include "VertexComputation.h"
#include "Worker.h"
#include "WorkerState.h"

#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Dispatcher/DispatcherQueue.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::pregel;

template <typename V, typename E, typename M>
WorkerJob<V, E, M>::WorkerJob(Worker<V, E, M>* worker,
                              std::shared_ptr<WorkerState<V, E, M>> ctx,
                              std::shared_ptr<GraphStore<V, E>> graphStore)
    : Job("Pregel Job"),
      _worker(worker),
      _canceled(false),
      _ctx(ctx),
      _graphStore(graphStore) {}

template <typename V, typename E, typename M>
void WorkerJob<V, E, M>::work() {
  LOG(INFO) << "Worker job started";
  if (_canceled) {
    LOG(INFO) << "Job was canceled before work started";
    return;
  }
  // TODO cache this
  OutgoingCache<V, E, M> outCache(_ctx);

  unsigned int gss = _ctx->globalSuperstep();
  std::vector<VertexEntry>& vertexIterator = _graphStore->vertexIterator();
  auto vertexComputation = _ctx->algorithm()->createComputation();
  vertexComputation->_gss = gss;
  vertexComputation->_outgoing = &outCache;
  vertexComputation->_graphStore = _graphStore;

  size_t activeCount = 0;
  auto incoming = _ctx->readableIncomingCache();

  for (auto& vertexEntry : vertexIterator) {
    std::string vertexID = vertexEntry.vertexID();
    MessageIterator<M> messages = incoming->getMessages(vertexID);

    if (gss == 0 || messages.size() > 0 || vertexEntry.active()) {
      vertexComputation->_vertexEntry = &vertexEntry;
      vertexComputation->compute(vertexID, messages);
      if (vertexEntry.active()) {
        activeCount++;
      } else {
        LOG(INFO) << vertexEntry.vertexID() << " vertex has halted";
      }
    }
  }
  LOG(INFO) << "Finished executing vertex programs.";

  if (_canceled) {
    return;
  }

  // ==================== send messages to other shards ====================
  outCache.sendMessages();
  size_t sendCount = outCache.sendMessageCount();
  size_t receivedCount = _ctx->writeableIncomingCache()->receivedMessageCount();
  if (activeCount == 0 && sendCount == 0 && receivedCount == 0) {
    LOG(INFO) << "Worker seems to be done";
    _worker->workerJobIsDone(true);
  } else {
    LOG(INFO) << "Worker send " << sendCount << " messages";
    _worker->workerJobIsDone(false);
  }
}

template <typename V, typename E, typename M>
bool WorkerJob<V, E, M>::cancel() {
  LOG(INFO) << "Canceling worker job";
  _canceled = true;
  return true;
}

template <typename V, typename E, typename M>
void WorkerJob<V, E, M>::cleanup(rest::DispatcherQueue* queue) {
  queue->removeJob(this);
  delete this;
}

template <typename V, typename E, typename M>
void WorkerJob<V, E, M>::handleError(basics::Exception const& ex) {}

// template types to create
template class arangodb::pregel::WorkerJob<int64_t, int64_t, int64_t>;
