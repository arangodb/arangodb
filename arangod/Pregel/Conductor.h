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

#include "AggregatorUsage.h"

namespace arangodb {
namespace pregel {

class Conductor {
 public:
  enum ExecutionState { RUNNING, DONE, CANCELED};

  Conductor(uint64_t executionNumber, TRI_vocbase_t* vocbase,
            std::vector<std::shared_ptr<LogicalCollection>> vertexCollections,
            std::shared_ptr<LogicalCollection> edgeCollection,
            std::string const& algorithm);
  ~Conductor();

  void start(VPackSlice params);
  void finishedGlobalStep(VPackSlice& data);  //
  void cancel();

  ExecutionState getState() { return _state; }

 private:
  Mutex _finishedGSSMutex;  // prevents concurrent calls to finishedGlobalStep
  
  VocbaseGuard _vocbaseGuard;
  const uint64_t _executionNumber;
  std::string _algorithm;
  ExecutionState _state = ExecutionState::RUNNING;

  std::unique_ptr<AggregatorUsage> _aggregatorUsage;
  std::vector<std::shared_ptr<LogicalCollection>> _vertexCollections;
  std::shared_ptr<LogicalCollection> _edgeCollection;
  std::map<ServerID, std::vector<ShardID>> _vertexServerMap;

  uint64_t _globalSuperstep;
  int32_t _dbServerCount = 0;
  int32_t _responseCount = 0;
  int32_t _doneCount = 0;

  void startGlobalStep();
  int sendToAllDBServers(std::string url, VPackSlice const& body);
};
}
}
#endif
