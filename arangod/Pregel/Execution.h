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

#ifndef ARANGODB_PREGEL_EXECUTION_H
#define ARANGODB_PREGEL_EXECUTION_H 1

#include <mutex>
#include "Basics/Common.h"
#include "Cluster/ClusterInfo.h"
#include "VocBase/vocbase.h"


namespace arangodb {
namespace pregel {
  
  class WorkerThread;
  enum ExecutionState {RUNNING, FINISHED, ERROR};
  
  class Execution {
  public:
    
    Execution(int executionNumber,
              TRI_vocbase_t *vocbase,
              arangodb::CollectionID const& vertexCollection,
              arangodb::CollectionID const& edgeCollection,
              std::string const& algorithm);
    
    
    void finishedGlobalStep(VPackSlice &data);//
    void nextGlobalStep(VPackSlice &data);// called by coordinator
    void cancel();
    
    ExecutionState getState() {return _state;}
    
  private:
    std::mutex mtx;
    int _executionNumber;
    size_t _globalSuperstep;
    size_t _dbServerCount = 0;
    size_t _responseCount = 0;
    
    TRI_vocbase_t *_vocbase;
    ExecutionState _state = ExecutionState::RUNNING;
    WorkerThread *_worker;
    bool _isCoordinator;
    std::string _coordinatorId;
    
    int sendToAllDBServers(std::string url, VPackSlice const& body);
  };
}
}
#endif
