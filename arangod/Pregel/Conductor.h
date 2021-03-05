////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#include "Basics/Common.h"

#include <boost/date_time/posix_time/posix_time.hpp>

#include "Basics/Mutex.h"
#include "Basics/asio_ns.h"
#include "Basics/system-functions.h"
#include "Cluster/ClusterInfo.h"
#include "Pregel/Statistics.h"
#include "Pregel/Reports.h"
#include "Scheduler/Scheduler.h"
#include "Utils/DatabaseGuard.h"

namespace arangodb {
namespace pregel {

enum ExecutionState {
  DEFAULT = 0,  // before calling start
  RUNNING,      // during normal operation
  STORING,      // store results
  DONE,         // after everyting is done
  CANCELED,     // after an terminal error or manual canceling
  IN_ERROR,     // after an error which should allow recovery
  RECOVERING,   // during recovery
  FATAL_ERROR,  // execution can not continue because of errors
};
extern const char* ExecutionStateNames[8];

class PregelFeature;
class MasterContext;
class AggregatorHandler;
struct IAlgorithm;

struct Error {
  std::string message;
};

class Conductor {
  friend class PregelFeature;

  ExecutionState _state = ExecutionState::DEFAULT;
  const DatabaseGuard _vocbaseGuard;
  const uint64_t _executionNumber;
  VPackBuilder _userParams;
  std::unique_ptr<IAlgorithm> _algorithm;
  mutable Mutex _callbackMutex;  // prevents concurrent calls to finishedGlobalStep

  std::vector<CollectionID> _vertexCollections;
  std::vector<CollectionID> _edgeCollections;
  std::vector<ServerID> _dbServers;
  std::vector<ShardID> _allShards;  // persistent shard list
  
  // maps from vertex collection name to a list of edge collections that this
  // vertex collection is restricted to. only use for a collection if there is at least
  // one entry for the collection!
  std::unordered_map<CollectionID, std::vector<CollectionID>> _edgeCollectionRestrictions;

  // initialized on startup
  std::unique_ptr<AggregatorHandler> _aggregators;
  std::unique_ptr<MasterContext> _masterContext;
  /// tracks the servers which responded, only used for stages where we expect
  /// an
  /// unique response, not necessarily during the async mode
  std::set<ServerID> _respondedServers;
  uint64_t _globalSuperstep = 0;
  /// adjustable maximum gss for some algorithms
  uint64_t _maxSuperstep = 500;
  /// determines whether we support async execution
  bool _asyncMode = false;
  bool _useMemoryMaps = false;
  bool _storeResults = false;
  bool _inErrorAbort = false;

  /// persistent tracking of active vertices, send messages, runtimes
  StatsManager _statistics;
  ReportManager _reports;
  /// Current number of vertices
  uint64_t _totalVerticesCount = 0;
  uint64_t _totalEdgesCount = 0;
  /// some tracking info
  double _startTimeSecs = 0;
  double _computationStartTimeSecs = 0.0;
  double _finalizationStartTimeSecs = 0.0;
  double _storeTimeSecs = 0.0;
  double _endTimeSecs = 0.0;
  double _stepStartTimeSecs = 0.0; // start time of current gss
  Scheduler::WorkHandle _workHandle;

  bool _startGlobalStep();
  ErrorCode _initializeWorkers(std::string const& suffix, VPackSlice additional);
  ErrorCode _finalizeWorkers();
  ErrorCode _sendToAllDBServers(std::string const& path, VPackBuilder const& message);
  ErrorCode _sendToAllDBServers(std::string const& path, VPackBuilder const& message,
                                std::function<void(VPackSlice)> handle);
  void _ensureUniqueResponse(VPackSlice body);

  // === REST callbacks ===
  void finishedWorkerStartup(VPackSlice const& data);
  VPackBuilder finishedWorkerStep(VPackSlice const& data);
  void finishedWorkerFinalize(VPackSlice data);
  void finishedRecoveryStep(VPackSlice const& data);

  std::vector<ShardID> getShardIds(ShardID const& collection) const;

 public:
  Conductor(uint64_t executionNumber, TRI_vocbase_t& vocbase,
            std::vector<CollectionID> const& vertexCollections,
            std::vector<CollectionID> const& edgeCollections,
            std::unordered_map<std::string, std::vector<std::string>> const& edgeCollectionRestrictions,
            std::string const& algoName, VPackSlice const& userConfig);

  ~Conductor();

  void start();
  void cancel();
  void startRecovery();
  void collectAQLResults(velocypack::Builder& outBuilder, bool withId);
  VPackBuilder toVelocyPack() const;

  double totalRuntimeSecs() const {
    return _endTimeSecs == 0 ? TRI_microtime() - _startTimeSecs : _endTimeSecs - _startTimeSecs;
  }

 private:
  void cancelNoLock();
};
}  // namespace pregel
}  // namespace arangodb
#endif
