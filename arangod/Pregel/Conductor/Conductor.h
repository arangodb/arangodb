////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
#include "Cluster/ClusterTypes.h"
#include "Pregel/Reports.h"
#include "Pregel/Statistics.h"
#include "Pregel/WorkerConductorMessages.h"
#include "Scheduler/Scheduler.h"
#include "Utils/DatabaseGuard.h"

#include "Pregel/ExecutionNumber.h"
#include "Pregel/Conductor/States/CanceledState.h"
#include "Pregel/Conductor/States/ComputingState.h"
#include "Pregel/Conductor/States/DoneState.h"
#include "Pregel/Conductor/States/FatalErrorState.h"
#include "Pregel/Conductor/States/InErrorState.h"
#include "Pregel/Conductor/States/InitialState.h"
#include "Pregel/Conductor/States/LoadingState.h"
#include "Pregel/Conductor/States/State.h"
#include "Pregel/Conductor/States/StoringState.h"
#include "Pregel/Status/ConductorStatus.h"
#include "Pregel/Status/ExecutionStatus.h"
#include "Pregel/WorkerInterface.h"
#include "velocypack/Builder.h"

#include <chrono>
#include <cstdint>
#include <memory>

namespace arangodb {
namespace pregel {

struct ClusterWorkerOnConductor : NewIWorker {
  ClusterWorkerOnConductor(ExecutionNumber executionNumber, ServerID serverId,
                           TRI_vocbase_t& vocbase)
      : _executionNumber{std::move(executionNumber)},
        _serverId{std::move(serverId)},
        _vocbaseGuard{vocbase} {}
  [[nodiscard]] auto loadGraph(LoadGraph const& graph)
      -> futures::Future<ResultT<GraphLoaded>> override;

 private:
  ExecutionNumber _executionNumber;
  ServerID _serverId;
  const DatabaseGuard _vocbaseGuard;
};

struct SingleServerWorkerOnConductor : NewIWorker {
  SingleServerWorkerOnConductor(ExecutionNumber executionNumber,
                                ServerID serverId, PregelFeature& feature,
                                TRI_vocbase_t& vocbase)
      : _executionNumber{std::move(executionNumber)},
        _serverId{std::move(serverId)},
        _feature{feature},
        _vocbaseGuard{vocbase} {}
  [[nodiscard]] auto loadGraph(LoadGraph const& graph)
      -> futures::Future<ResultT<GraphLoaded>> override;

 private:
  ExecutionNumber _executionNumber;
  ServerID _serverId;
  PregelFeature& _feature;
  const DatabaseGuard _vocbaseGuard;
};

enum ExecutionState {
  DEFAULT = 0,  // before calling start
  LOADING,      // load graph into memory
  RUNNING,      // during normal operation
  STORING,      // store results
  DONE,         // after everyting is done
  CANCELED,     // after an terminal error or manual canceling
  IN_ERROR,     // after an error which should allow recovery
  FATAL_ERROR,  // execution can not continue because of errors
};

class PregelFeature;
class MasterContext;
class AggregatorHandler;
struct IAlgorithm;

struct Error {
  std::string message;
};

class Conductor : public std::enable_shared_from_this<Conductor> {
  friend class PregelFeature;
  friend struct conductor::Initial;
  friend struct conductor::Loading;
  friend struct conductor::Computing;
  friend struct conductor::Storing;
  friend struct conductor::Canceled;
  friend struct conductor::Done;
  friend struct conductor::InError;
  friend struct conductor::FatalError;

  ExecutionState _state = ExecutionState::DEFAULT;
  PregelFeature& _feature;
  std::chrono::system_clock::time_point _created;
  std::chrono::seconds _ttl = std::chrono::seconds(300);
  const DatabaseGuard _vocbaseGuard;
  const ExecutionNumber _executionNumber;
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
  /// an unique response, not necessarily during the async mode
  std::set<ServerID> _respondedServers;
  uint64_t _globalSuperstep = 0;
  /// adjustable maximum gss for some algorithms
  uint64_t _maxSuperstep = 500;
  /// determines whether we support async execution
  bool _asyncMode = false;
  bool _useMemoryMaps = true;
  bool _storeResults = false;
  bool _inErrorAbort = false;

  /// persistent tracking of active vertices, send messages, runtimes
  StatsManager _statistics;
  ReportManager _reports;
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
  auto _initializeWorkers(VPackSlice additional) -> futures::Future<std::vector<
      futures::Try<arangodb::ResultT<arangodb::pregel::GraphLoaded>>>>;
  template<typename OutType, typename InType>
  auto _sendToAllDBServers(std::string const& path, InType const& message)
      -> ResultT<std::vector<OutType>>;
  void _ensureUniqueResponse(std::string const& body);
  auto _startGssEvent(bool activateAll) const -> StartGss;

  // === REST callbacks ===
  void workerStatusUpdate(VPackSlice const& data);
  void finishedWorkerStartup(VPackSlice const& data);
  VPackBuilder finishedWorkerStep(VPackSlice const& data);
  void finishedWorkerFinalize(VPackSlice data);

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
  auto collectAQLResults(bool withId) -> PregelResults;
  void toVelocyPack(arangodb::velocypack::Builder& result) const;

  bool canBeGarbageCollected() const;

  ExecutionNumber executionNumber() const { return _executionNumber; }

 private:
  std::unordered_map<ServerID, std::unique_ptr<NewIWorker>> workers;
  void updateState(ExecutionState state);
  void cleanup();

  std::unique_ptr<conductor::State> state =
      std::make_unique<conductor::Initial>(*this);
  auto changeState(conductor::StateType name) -> void;
  auto run() -> void { state->run(); }
  auto receive(Message const& message) -> void { state->receive(message); }
};
}  // namespace pregel
}  // namespace arangodb
