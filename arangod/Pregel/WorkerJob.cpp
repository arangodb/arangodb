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
#include "WorkerContext.h"
#include "Worker.h"
#include "Vertex.h"
#include "Message.h"
#include "Utils.h"

#include "InMessageCache.h"
#include "OutMessageCache.h"

#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterComm.h"
#include "Dispatcher/DispatcherQueue.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::pregel;

WorkerJob::WorkerJob(Worker *worker,
                     std::shared_ptr<WorkerContext> ctx) : Job("Pregel Job"), _canceled(false), _worker(worker), _ctx(ctx) {
}

void WorkerJob::work() {
  LOG(INFO) << "Worker job started";
  if (_canceled) {
      LOG(INFO) << "Job was canceled before work started";
    return;
  }
  // TODO cache this
  OutMessageCache outCache(_ctx);

  unsigned int gss = _ctx->globalSuperstep();
  bool isDone = true;

  if (gss == 0) {
    isDone = false;
    
    for (auto const &it : _worker->_vertices) {
      Vertex *v = it.second;
      //std::string key = v->_data.get(StaticStrings::KeyString).copyString();
      //VPackSlice messages = _ctx->readableIncomingCache()->getMessages(key);
      v->compute(gss, MessageIterator(), &outCache);
      bool active = v->state() == VertexActivationState::ACTIVE;
      if (!active) LOG(INFO) << "vertex has halted";
      _worker->_activationMap[it.first] = active;
    }
  } else {
    for (auto &it : _worker->_activationMap) {
        
      //std::string key = _ctx->vertexCollectionName() + "/" + it.first;
      MessageIterator messages = _ctx->readableIncomingCache()->getMessages(it.first);
        
      if (messages.size() > 0 || it.second) {
        isDone = false;
        
        Vertex *v = _worker->_vertices[it.first];
        v->compute(gss, messages, &outCache);
        bool active = v->state() == VertexActivationState::ACTIVE;
        it.second = active;
        if (!active) LOG(INFO) << "vertex has halted";
      }
    }
  }
  LOG(INFO) << "Finished executing vertex programs.";

  if (_canceled) {
    return;
  }
  
  // ==================== send messages to other shards ====================
  
  if (!isDone) {
      outCache.sendMessages();
  } else {
    LOG(INFO) << "Worker job has nothing more to process";
  }
  _worker->workerJobIsDone(this, isDone);
}

bool WorkerJob::cancel() {
    LOG(INFO) << "Canceling worker job";
  _canceled = true;
  return true;
}

void WorkerJob::cleanup(rest::DispatcherQueue* queue) {
  queue->removeJob(this);
  delete this;
}

void WorkerJob::handleError(basics::Exception const& ex) {}
