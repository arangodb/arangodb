////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include "Basics/Common.h"

#include "Basics/Mutex.h"
#include "Basics/system-functions.h"
#include "Cluster/ClusterInfo.h"
#include "Pregel/Worker/Messages.h"
#include "Scheduler/Scheduler.h"
#include "Utils/DatabaseGuard.h"

#include "Pregel/ExecutionNumber.h"
#include "Pregel/Worker/Messages.h"
#include "Pregel/Statistics.h"
#include "Pregel/Status/ConductorStatus.h"
#include "Pregel/Status/ExecutionStatus.h"

#include <chrono>
#include <set>

namespace arangodb {
namespace pregel {

enum ExecutionState {
  DEFAULT = 0,  // before calling start
  LOADING,      // load graph into memory
  RUNNING,      // during normal operation
  STORING,      // store results
  DONE,         // after everyting is done
  CANCELED,     // after an terminal error or manual canceling
  FATAL_ERROR,  // execution can not continue because of errors
};
extern const char* ExecutionStateNames[9];

class PregelFeature;
class MasterContext;
class AggregatorHandler;
struct IAlgorithm;

struct Error {
  std::string message;
};

class Conductor : public std::enable_shared_from_this<Conductor> {
  friend class PregelFeature;

  ExecutionState _state = ExecutionState::DEFAULT;
  PregelFeature& _feature;
  std::chrono::system_clock::time_point _created;
  std::chrono::system_clock::time_point _expires;
  std::chrono::seconds _ttl = std::chrono::seconds(300);
  const DatabaseGuard _vocbaseGuard;
  ExecutionNumber const _executionNumber;
  VPackBuilder _userParams;
  std::unique_ptr<IAlgorithm> _algorithm;
  mutable Mutex
      _callbackMutex;  // prevents concurrent calls to finishedGlobalStep

  std::vector<CollectionID> _vertexCollections;
  std::vector<CollectionID> _edgeCollections;
  std::vector<ServerID> _dbServers;
  std::vector<ShardID> _allShards;  // persistent shard list

  // maps from vertex collection name to a list of edge collections that this
  // vertex collection is restricted to. only use for a collection if there is
  // at least one entry for the collection!
  std::unordered_map<CollectionID, std::vector<CollectionID>>
      _edgeCollectionRestrictions;

  // initialized on startup
  std::unique_ptr<AggregatorHandler> _aggregators;
  std::unique_ptr<MasterContext> _masterContext;
  /// tracks the servers which responded, only used for stages where we expect
  /// an unique response
  std::set<ServerID> _respondedServers;
  uint64_t _globalSuperstep = 0;
  /// adjustable maximum gss for some algorithms
  /// some algorithms need several gss per iteration and it is more natural
  /// for the user to give a maximum number of iterations
  /// If Utils::maxNumIterations is given, _maxSuperstep is set to infinity.
  /// In that case, Utils::maxNumIterations can be captured in the algorithm
  /// (when the algorithm is created in AlgoRegistry, parameter userParams)
  /// and used in MasterContext::postGlobalSuperstep which returns whether to
  /// continue.
  uint64_t _maxSuperstep = 500;

  bool _useMemoryMaps = true;
  bool _storeResults = false;

  /// persistent tracking of active vertices, send messages, runtimes
  StatsManager _statistics;
  /// Current number of vertices
  uint64_t _totalVerticesCount = 0;
  uint64_t _totalEdgesCount = 0;

  /// Timings
  ExecutionTimings _timing;

  Scheduler::WorkHandle _workHandle;

  // Work in Progress: Move data incrementally into this
  // struct; sort it into categories and make it (de)serialisable
  // with the Inspecotr framework
  ConductorStatus _status;

  bool _startGlobalStep();
  ErrorCode _initializeWorkers();
  ErrorCode _finalizeWorkers();
  ErrorCode _sendToAllDBServers(std::string const& path,
                                VPackBuilder const& message);
  ErrorCode _sendToAllDBServers(std::string const& path,
                                VPackBuilder const& message,
                                std::function<void(VPackSlice)> handle);
  void _ensureUniqueResponse(VPackSlice body);
  void _ensureUniqueResponse(std::string const& sender);

  // === REST callbacks ===
  void workerStatusUpdate(StatusUpdated&& data);
  void finishedWorkerStartup(GraphLoaded const& data);
  void finishedWorkerStep(GlobalSuperStepFinished const& data);
  void finishedWorkerFinalize(Finished const& data);

  std::vector<ShardID> getShardIds(ShardID const& collection) const;

 public:
  Conductor(ExecutionNumber executionNumber, TRI_vocbase_t& vocbase,
            std::vector<CollectionID> const& vertexCollections,
            std::vector<CollectionID> const& edgeCollections,
            std::unordered_map<std::string, std::vector<std::string>> const&
                edgeCollectionRestrictions,
            std::string const& algoName, VPackSlice const& userConfig,
            PregelFeature& feature);

  ~Conductor();

  void start();
  void cancel();
  void collectAQLResults(velocypack::Builder& outBuilder, bool withId);
  void toVelocyPack(arangodb::velocypack::Builder& result) const;

  bool canBeGarbageCollected() const;

  ExecutionNumber executionNumber() const { return _executionNumber; }

 private:
  void cancelNoLock();
  void updateState(ExecutionState state);
};
}  // namespace pregel
}  // namespace arangodb
