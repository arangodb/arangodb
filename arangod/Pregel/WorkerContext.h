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

#ifndef ARANGODB_PREGEL_WORKER_CONTEXT_H
#define ARANGODB_PREGEL_WORKER_CONTEXT_H 1

#include "Basics/Common.h"
#include "Cluster/CLusterInfo.h"

namespace arangodb {
    class SingleCollectionTransaction;
namespace pregel {
  
  class InMessageCache;
    
////////////////////////////////////////////////////////////////////////////////
/// @brief carry common parameters
////////////////////////////////////////////////////////////////////////////////
  class WorkerContext {
    friend class Worker;
  public:
      WorkerContext(unsigned int exeNum);
      ~WorkerContext();
      
      inline unsigned int executionNumber() {
          return _executionNumber;
      }
      
      inline unsigned int globalSuperstep() {
          return _globalSuperstep;
      }
      
      inline std::string const& coordinatorId() const {
          return _coordinatorId;
      }
      
      inline std::string const& database() const {
          return _database;
      }
      
      inline std::string const& vertexCollectionName() const {
          return _vertexCollectionName;
      }
      
      inline std::string  const& vertexCollectionPlanId() const {
          return _vertexCollectionPlanId;
      }
      
      inline std::vector<ShardID> const& localVertexShardIDs() const {
          return _localVertexShardIDs;
      }
      
      //inline ShardID const& edgeShardId() const {
      //    return _edgeShardID;
      //}
      
      inline InMessageCache* readableIncomingCache() {
          return _readCache;
      }
      
      inline InMessageCache* writeableIncomingCache() {
          return _writeCache;
      }
    
  private:
    /// @brief guard to make sure the database is not dropped while used by us
      const unsigned int _executionNumber;
      unsigned int _globalSuperstep = 0;
      unsigned int _expectedGSS = 0;
      std::string _coordinatorId;
      std::string _database;
      std::string _vertexCollectionName, _vertexCollectionPlanId;
      std::vector<ShardID> _localVertexShardIDs;
      //ShardID _vertexShardID, _edgeShardID;
      
      InMessageCache *_readCache, *_writeCache;
      void swapIncomingCaches();// only call when message receiving is locked
  };
}
}
#endif
