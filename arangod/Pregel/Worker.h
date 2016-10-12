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
#include "VocBase/vocbase.h"
#include "Scheduler/Task.h"
#include "Cluster/ClusterInfo.h"
#include "Dispatcher/Job.h"

namespace arangodb {
    class SingleCollectionTransaction;
namespace pregel {
  class Vertex;
  class InMessageCache;
  class OutMessageCache;
  
  class Worker {
    friend class WorkerJob;
  public:
    Worker(unsigned int executionNumber, TRI_vocbase_t *vocbase, VPackSlice s);
    ~Worker();
      
    void nextGlobalStep(VPackSlice data);// called by coordinator
    void receivedMessages(VPackSlice data);
    void writeResults();
    
  private:
    /// @brief guard to make sure the database is not dropped while used by us
    TRI_vocbase_t* _vocbase;
    Mutex _messagesMutex;
    const unsigned int _executionNumber;
      
    unsigned int _globalSuperstep;
    std::string _coordinatorId;
    std::string _vertexCollectionName, _vertexCollectionPlanId;
    ShardID _vertexShardID, _edgeShardID;
    
    std::unordered_map<std::string, Vertex*> _vertices;
    std::map<std::string, bool> _activationMap;
    
    InMessageCache *_cache1, *_cache2;
    InMessageCache *_currentCache;
    std::vector<SingleCollectionTransaction*> _transactions;
  };
  
  class WorkerJob : public rest::Job {
    WorkerJob(WorkerJob const&) = delete;
    WorkerJob& operator=(WorkerJob const&) = delete;
    
  public:
    WorkerJob(Worker *worker, InMessageCache *inCache);
    
    void work() override;
    bool cancel() override;
    void cleanup(rest::DispatcherQueue*) override;
    void handleError(basics::Exception const& ex) override;

  private:
    std::atomic<bool> _canceled;
    Worker *_worker;
    InMessageCache *_inCache;
  };
}
}
#endif
