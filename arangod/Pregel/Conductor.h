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

#ifndef ARANGODB_PREGEL_CONDUCTOR_H
#define ARANGODB_PREGEL_CONDUCTOR_H 1

#include <string>

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Cluster/ClusterInfo.h"
#include "VocBase/vocbase.h"

#include "Aggregator.h"


namespace arangodb {
namespace pregel {
  
  class WorkerThread;
  enum ExecutionState {RUNNING, FINISHED, ERROR};
  
  class Conductor {
  public:
    
    Conductor(unsigned int executionNumber,
              TRI_vocbase_t *vocbase,
              std::shared_ptr<LogicalCollection> vertexCollection,
              std::shared_ptr<LogicalCollection> edgeCollection,
              std::string const& algorithm);
      ~Conductor();
    
    void start();
    void finishedGlobalStep(VPackSlice &data);//
    void cancel();
    
    ExecutionState getState() {return _state;}
    
  private:
    Mutex _finishedGSSMutex;// prevents concurrent calls to finishedGlobalStep
    VocbaseGuard _vocbaseGuard;
    const unsigned int _executionNumber;
      
    unsigned int _globalSuperstep;
    int32_t _dbServerCount = 0;
    int32_t _responseCount = 0;
    int32_t _doneCount = 0;
      
    std::shared_ptr<LogicalCollection> _vertexCollection, _edgeCollection;
    CollectionID _vertexCollectionID;
    std::string _algorithm;
    
    ExecutionState _state = ExecutionState::RUNNING;
      
    // convenience
      void resolveWorkerServers(std::unordered_map<ServerID, std::vector<ShardID>> &vertexServerMap,
                                std::unordered_map<ServerID, std::vector<ShardID>> &edgeServerMap);
    int sendToAllDBServers(std::string url, VPackSlice const& body);
  };
}
}
#endif
