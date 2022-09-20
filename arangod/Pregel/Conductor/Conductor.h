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
#include "Pregel/Conductor/WorkerApi.h"
#include "velocypack/Builder.h"

#include <chrono>
#include <cstdint>
#include <memory>

namespace arangodb {
namespace pregel {

class PregelFeature;
class MasterContext;
class AggregatorHandler;
struct IAlgorithm;

struct Error {
  std::string message;
};

struct PostGlobalSuperStepResult {
  bool activateAll;
  bool finished;
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

  using GraphLoadedFuture = futures::Future<std::vector<
      futures::Try<arangodb::ResultT<arangodb::pregel::GraphLoaded>>>>;

  auto _postGlobalSuperStep(VPackBuilder messagesFromWorkers)
      -> PostGlobalSuperStepResult;
  auto _preGlobalSuperStep() -> bool;
  auto _initializeWorkers(VPackSlice additional) -> GraphLoadedFuture;
  template<typename InType>
  auto _sendToAllDBServers(InType const& message)
      -> ResultT<std::vector<ModernMessage>>;
  void _ensureUniqueResponse(std::string const& body);

  // === REST callbacks ===
  void workerStatusUpdate(StatusUpdated const& data);

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

  auto process(MessagePayload const& message) -> Result;
  void start();
  void cancel();
  auto collectAQLResults(bool withId) -> ResultT<PregelResults>;
  void toVelocyPack(arangodb::velocypack::Builder& result) const;

  bool canBeGarbageCollected() const;

  ExecutionNumber executionNumber() const { return _executionNumber; }

 private:
  std::unordered_map<ServerID, conductor::WorkerApi> workers;
  void cleanup();

  std::unique_ptr<conductor::State> state =
      std::make_unique<conductor::Initial>(*this);
  auto changeState(std::unique_ptr<conductor::State> newState) -> void;
};
}  // namespace pregel
}  // namespace arangodb
