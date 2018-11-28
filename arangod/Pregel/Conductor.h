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

#include "Basics/Common.h"

#include <boost/date_time/posix_time/posix_time.hpp>

#include "Basics/Mutex.h"
#include "Basics/asio_ns.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Pregel/Statistics.h"
#include "Utils/DatabaseGuard.h"

namespace arangodb {
namespace pregel {

enum ExecutionState {
  DEFAULT = 0,  // before calling start
  RUNNING,      // during normal operation
  DONE,         // after everyting is done
  CANCELED,     // after an terminal error or manual canceling
  IN_ERROR,     // after an error which should allow recovery
  RECOVERING    // during recovery
};
extern const char* ExecutionStateNames[6];

class PregelFeature;
class MasterContext;
class AggregatorHandler;
struct IAlgorithm;

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
  bool _lazyLoading = false;
  bool _storeResults = false;

  /// persistent tracking of active vertices, send messages, runtimes
  StatsManager _statistics;
  /// Current number of vertices
  uint64_t _totalVerticesCount = 0;
  uint64_t _totalEdgesCount = 0;
  /// some tracking info
  double _startTimeSecs = 0;
  double _computationStartTimeSecs = 0;
  double _endTimeSecs = 0;
  std::unique_ptr<asio::steady_timer> _steady_timer;

  bool _startGlobalStep();
  int _initializeWorkers(std::string const& path, VPackSlice additional);
  int _finalizeWorkers();
  int _sendToAllDBServers(std::string const& path, VPackBuilder const& message);
  int _sendToAllDBServers(std::string const& path, VPackBuilder const& message,
                          std::function<void(VPackSlice)> handle);
  void _ensureUniqueResponse(VPackSlice body);

  // === REST callbacks ===
  void finishedWorkerStartup(VPackSlice const& data);
  VPackBuilder finishedWorkerStep(VPackSlice const& data);
  void finishedRecoveryStep(VPackSlice const& data);

 public:
  Conductor(
    uint64_t executionNumber,
    TRI_vocbase_t& vocbase,
    std::vector<CollectionID> const& vertexCollections,
    std::vector<CollectionID> const& edgeCollections,
    std::string const& algoName,
    VPackSlice const& userConfig
  );

  ~Conductor();

  void start();
  void cancel();
  void startRecovery();
  void collectAQLResults(velocypack::Builder& outBuilder);
  VPackBuilder toVelocyPack() const;

  double totalRuntimeSecs() const {
    return _endTimeSecs == 0 ? TRI_microtime() - _startTimeSecs
                             : _endTimeSecs - _startTimeSecs;
  }
 
 private:
  void cancelNoLock();
};
}
}
#endif
