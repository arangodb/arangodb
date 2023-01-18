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

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include "Pregel/ArangoExternalDispatcher.h"
#include "Actor/Runtime.h"
#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Pregel/ExecutionNumber.h"
#include "Pregel/SpawnMessages.h"
#include "Pregel/PregelOptions.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/arangod.h"
#include "Scheduler/Scheduler.h"
#include "Pregel/PregelMetrics.h"

struct TRI_vocbase_t;

namespace arangodb::pregel {

struct MockScheduler {
  auto operator()(auto fn) { fn(); };
};

class Conductor;
class IWorker;

class PregelFeature final : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept { return "Pregel"; }

  explicit PregelFeature(Server& server);
  ~PregelFeature();

  ResultT<ExecutionNumber> startExecution(TRI_vocbase_t& vocbase,
                                          PregelOptions options);

  void collectOptions(std::shared_ptr<arangodb::options::ProgramOptions>
                          options) override final;
  void validateOptions(std::shared_ptr<arangodb::options::ProgramOptions>
                           options) override final;
  void start() override final;
  void beginShutdown() override final;
  void unprepare() override final;

  bool isStopping() const noexcept;

  ExecutionNumber createExecutionNumber();
  void addConductor(std::shared_ptr<Conductor>&&,
                    ExecutionNumber executionNumber);
  std::shared_ptr<Conductor> conductor(ExecutionNumber executionNumber);

  void garbageCollectConductors();

  void addWorker(std::shared_ptr<IWorker>&&, ExecutionNumber executionNumber);
  std::shared_ptr<IWorker> worker(ExecutionNumber executionNumber);

  void cleanupConductor(ExecutionNumber executionNumber);
  void cleanupWorker(ExecutionNumber executionNumber);

  void handleConductorRequest(TRI_vocbase_t& vocbase, std::string const& path,
                              VPackSlice const& body,
                              VPackBuilder& outResponse);
  void handleWorkerRequest(TRI_vocbase_t& vocbase, std::string const& path,
                           VPackSlice const& body, VPackBuilder& outBuilder);

  uint64_t numberOfActiveConductors() const;

  void initiateSoftShutdown() override final {
    _softShutdownOngoing.store(true, std::memory_order_relaxed);
  }

  Result toVelocyPack(TRI_vocbase_t& vocbase,
                      arangodb::velocypack::Builder& result, bool allDatabases,
                      bool fanout) const;

  size_t defaultParallelism() const noexcept;
  size_t minParallelism() const noexcept;
  size_t maxParallelism() const noexcept;
  std::string tempPath() const;
  bool useMemoryMaps() const noexcept;

  auto metrics() -> std::shared_ptr<PregelMetrics> { return _metrics; }

  void spawnActor(actor::ServerID server, actor::ActorPID sender,
                  SpawnMessages msg);

 private:
  void scheduleGarbageCollection();

  // default parallelism to use per Pregel job
  size_t _defaultParallelism;

  // min parallelism usable per Pregel job
  size_t _minParallelism;

  // max parallelism usable per Pregel job
  size_t _maxParallelism;

  // type of temporary directory location ("custom", "temp-directory",
  // "database-directory")
  std::string _tempLocationType;

  // custom path for temporary directory. only populated if _tempLocationType ==
  // "custom"
  std::string _tempLocationCustomPath;

  // default "useMemoryMaps" value per Pregel job
  bool _useMemoryMaps;

  mutable Mutex _mutex;

  Scheduler::WorkHandle _gcHandle;

  struct ConductorEntry {
    std::string user;
    std::chrono::steady_clock::time_point expires;
    std::shared_ptr<Conductor> conductor;
  };

  std::unordered_map<ExecutionNumber, ConductorEntry> _conductors;
  std::unordered_map<ExecutionNumber,
                     std::pair<std::string, std::shared_ptr<IWorker>>>
      _workers;

  std::atomic<bool> _softShutdownOngoing;

  std::shared_ptr<PregelMetrics> _metrics;

 public:
  std::shared_ptr<actor::Runtime<MockScheduler, ArangoExternalDispatcher>>
      _actorRuntime;
};

}  // namespace arangodb::pregel
