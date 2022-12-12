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
#include "Pregel/Statistics.h"
#include "Pregel/Messaging/Message.h"
#include "Scheduler/Scheduler.h"
#include "Utils/DatabaseGuard.h"

#include "Pregel/ExecutionNumber.h"
#include "Pregel/Conductor/States/CanceledState.h"
#include "Pregel/Conductor/States/ComputingState.h"
#include "Pregel/Conductor/States/DoneState.h"
#include "Pregel/Conductor/States/FatalErrorState.h"
#include "Pregel/Conductor/States/InitialState.h"
#include "Pregel/Conductor/States/LoadingState.h"
#include "Pregel/Conductor/States/State.h"
#include "Pregel/Conductor/States/StoringState.h"
#include "Pregel/Conductor/GraphSource.h"
#include "Pregel/Status/ConductorStatus.h"
#include "Pregel/Status/ExecutionStatus.h"
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
  friend struct conductor::FatalError;

  std::unique_ptr<conductor::State> _state;
  PregelFeature& _feature;
  std::chrono::system_clock::time_point _created;
  std::chrono::seconds _ttl = std::chrono::seconds(300);
  const DatabaseGuard _vocbaseGuard;
  const ExecutionNumber _executionNumber;
  VPackBuilder _userParams;
  std::unique_ptr<IAlgorithm> _algorithm;
  mutable Mutex
      _callbackMutex;  // prevents concurrent calls to finishedGlobalStep

  conductor::PregelGraphSource _graphSource;

  // initialized on startup
  std::unique_ptr<AggregatorHandler> _aggregators;
  std::unique_ptr<MasterContext> _masterContext;
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

  std::unordered_map<ShardID, ServerID> _leadingServerForShard;

  auto _run() -> void;
  auto _changeState(std::unique_ptr<conductor::State> newState) -> void;
  auto _preGlobalSuperStep() -> void;
  auto _postGlobalSuperStep() -> PostGlobalSuperStepResult;

 public:
  Conductor(ExecutionNumber executionNumber, TRI_vocbase_t& vocbase,
            conductor::PregelGraphSource graphStore,
            std::string const& algoName, VPackSlice const& userConfig,
            PregelFeature& feature);

  ~Conductor();

  auto receive(MessagePayload message) -> void;
  void start();
  void cancel();
  auto collectAQLResults(bool withId) -> ResultT<PregelResults>;
  void workerStatusUpdated(StatusUpdated const& data);
  void toVelocyPack(arangodb::velocypack::Builder& result) const;

  bool canBeGarbageCollected() const;

  ExecutionNumber executionNumber() const { return _executionNumber; }
};
}  // namespace pregel
}  // namespace arangodb
