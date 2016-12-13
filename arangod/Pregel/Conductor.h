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
#include "Pregel/Statistics.h"
#include "VocBase/vocbase.h"

namespace arangodb {
class RestPregelHandler;
namespace pregel {

enum ExecutionState {
  DEFAULT,    // before calling start
  RUNNING,    // during normal operation
  DONE,       // after everyting is done
  CANCELED,   // after an error or manual canceling
  RECOVERING  // during recovery
};

class MasterContext;
class AggregatorHandler;
struct IAlgorithm;

class Conductor {
  friend class arangodb::RestPregelHandler;

  ExecutionState _state = ExecutionState::DEFAULT;
  const VocbaseGuard _vocbaseGuard;
  const uint64_t _executionNumber;
  std::unique_ptr<IAlgorithm> _algorithm;
  VPackBuilder _userParams;
  Mutex _callbackMutex;  // prevents concurrent calls to finishedGlobalStep

  std::vector<std::shared_ptr<LogicalCollection>> _vertexCollections;
  std::vector<std::shared_ptr<LogicalCollection>> _edgeCollections;
  std::vector<ServerID> _dbServers;

  // initialized on startup
  std::unique_ptr<AggregatorHandler> _aggregators;
  std::unique_ptr<MasterContext> _masterContext;
  /// some tracking info
  double _startTimeSecs = 0, _endTimeSecs = 0;
  uint64_t _globalSuperstep = 0;
  /// tracks the servers which responded, only used for stages where we expect an
  /// unique response, not necessarily during the async mode
  std::set<ServerID> _respondedServers;
  bool _asyncMode = false;
  /// persistent tracking of active vertices, send messages, runtimes
  WorkerStats _workerStats;
  
  bool _startGlobalStep();
  int _initializeWorkers(std::string const& suffix, VPackSlice additional);
  int _finalizeWorkers();
  int _sendToAllDBServers(std::string const& suffix, VPackSlice const& message);
  void _ensureUniqueResponse(VPackSlice body);

  // === REST callbacks ===
  void finishedWorkerStartup(VPackSlice& data);
  void finishedWorkerStep(VPackSlice& data);
  void finishedRecovery(VPackSlice& data);

 public:
  Conductor(
      uint64_t executionNumber, TRI_vocbase_t* vocbase,
      std::vector<std::shared_ptr<LogicalCollection>> const& vertexCollections,
      std::vector<std::shared_ptr<LogicalCollection>> const& edgeCollections);
  ~Conductor();

  void start(std::string const& algoName, VPackSlice userConfig);
  void cancel();
  void startRecovery();

  ExecutionState getState() const { return _state; }
  WorkerStats workerStats() const { return _workerStats; }
  uint64_t globalSuperstep() const { return _globalSuperstep; }
  double totalRuntimeSecs() {
    return _endTimeSecs == 0 ? TRI_microtime() - _startTimeSecs
                             : _endTimeSecs - _startTimeSecs;
  }
};
}
}
#endif
