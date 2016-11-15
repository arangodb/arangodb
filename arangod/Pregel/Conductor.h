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
#include "Pregel/AggregatorUsage.h"
#include "Pregel/Statistics.h"
#include "VocBase/vocbase.h"

namespace arangodb {
class RestPregelHandler;
namespace pregel {

enum ExecutionState { DEFAULT, RUNNING, DONE, CANCELED };

class Conductor {
  friend class arangodb::RestPregelHandler;

  VocbaseGuard _vocbaseGuard;
  const uint64_t _executionNumber;
  const std::string _algorithm;
  ExecutionState _state;
  std::vector<std::shared_ptr<LogicalCollection>> _vertexCollections;
  std::shared_ptr<LogicalCollection> _edgeCollection;

  // initialized on startup
  std::unique_ptr<IAggregatorCreator> _agregatorCreator;
  std::unique_ptr<AggregatorUsage> _aggregatorUsage;
  std::map<ServerID, std::vector<ShardID>> _vertexServerMap;

  uint64_t _globalSuperstep = 0;
  int32_t _dbServerCount = 0;
  int32_t _responseCount = 0;
  int32_t _doneCount = 0;
  WorkerStats _workerStats;
  Mutex _finishedGSSMutex;  // prevents concurrent calls to finishedGlobalStep

  void startGlobalStep();
  int sendToAllDBServers(std::string url, VPackSlice const& body);

  // === REST callbacks ===
  void finishedGlobalStep(VPackSlice& data);

 public:
  Conductor(uint64_t executionNumber, TRI_vocbase_t* vocbase,
            std::vector<std::shared_ptr<LogicalCollection>> vertexCollections,
            std::shared_ptr<LogicalCollection> edgeCollection,
            std::string const& algorithm);
  ~Conductor();

  void start(VPackSlice params);
  void cancel();

  ExecutionState getState() const { return _state; }
  WorkerStats workerStats() const {return _workerStats;}
  uint64_t globalSuperstep() const {return _globalSuperstep;}
};
}
}
#endif
