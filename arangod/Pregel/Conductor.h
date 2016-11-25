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
  enum OperationMode {
    NORMAL,
    RECOVERY
  };

  ExecutionState _state = ExecutionState::DEFAULT;
  OperationMode _operationMode = OperationMode::NORMAL;
  const VocbaseGuard _vocbaseGuard;
  const uint64_t _executionNumber;
  const std::string _algorithm;
  Mutex _finishedGSSMutex;  // prevents concurrent calls to finishedGlobalStep
  
  std::vector<std::shared_ptr<LogicalCollection>> _vertexCollections;
  std::vector<std::shared_ptr<LogicalCollection>> _edgeCollections;
  std::vector<ServerID> _dbServers;

  // initialized on startup
  std::unique_ptr<IAggregatorCreator> _agregatorCreator;
  std::unique_ptr<AggregatorUsage> _aggregatorUsage;

  double _startTimeSecs = 0, _endTimeSecs = 0;
  uint64_t _globalSuperstep = 0;
  uint32_t _responseCount = 0, _doneCount = 0;
  WorkerStats _workerStats;

  bool _startGlobalStep();
  int _sendToAllDBServers(std::string url, VPackSlice const& body);

  // === REST callbacks ===
  void finishedGlobalStep(VPackSlice& data);

 public:
  Conductor(uint64_t executionNumber, TRI_vocbase_t* vocbase,
            std::vector<std::shared_ptr<LogicalCollection>> const& vertexCollections,
            std::vector<std::shared_ptr<LogicalCollection>> const& edgeCollections,
            std::string const& algorithm);
  ~Conductor();

  void start(VPackSlice params);
  void cancel();
  void checkForWorkerOutage();

  ExecutionState getState() const { return _state; }
  WorkerStats workerStats() const {return _workerStats;}
  uint64_t globalSuperstep() const {return _globalSuperstep;}
  double totalRuntimeSecs() {
    return _endTimeSecs == 0 ? TRI_microtime() - _startTimeSecs : _endTimeSecs - _startTimeSecs;
  }
};
}
}
#endif
