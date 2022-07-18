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

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/arangod.h"
#include "Scheduler/Scheduler.h"
#include "Pregel/PregelMetrics.h"

struct TRI_vocbase_t;

namespace arangodb::pregel {

class Conductor;
class IWorker;
class RecoveryManager;

class PregelFeature final : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept { return "Pregel"; }

  explicit PregelFeature(Server& server);
  ~PregelFeature();

  std::pair<Result, uint64_t> startExecution(
      TRI_vocbase_t& vocbase, std::string algorithm,
      std::vector<std::string> const& vertexCollections,
      std::vector<std::string> const& edgeCollections,
      std::unordered_map<std::string, std::vector<std::string>> const&
          edgeCollectionRestrictions,
      VPackSlice const& params);

  void collectOptions(std::shared_ptr<arangodb::options::ProgramOptions>
                          options) override final;
  void validateOptions(std::shared_ptr<arangodb::options::ProgramOptions>
                           options) override final;
  void start() override final;
  void beginShutdown() override final;
  void unprepare() override final;

  bool isStopping() const noexcept;

  uint64_t createExecutionNumber();
  void addConductor(std::shared_ptr<Conductor>&&, uint64_t executionNumber);
  std::shared_ptr<Conductor> conductor(uint64_t executionNumber);

  void garbageCollectConductors();

  void addWorker(std::shared_ptr<IWorker>&&, uint64_t executionNumber);
  std::shared_ptr<IWorker> worker(uint64_t executionNumber);

  void cleanupConductor(uint64_t executionNumber);
  void cleanupWorker(uint64_t executionNumber);

  RecoveryManager* recoveryManager() {
    return _recoveryManagerPtr.load(std::memory_order_acquire);
  }

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

  std::unique_ptr<RecoveryManager> _recoveryManager;
  /// @brief _recoveryManagerPtr always points to the same object as
  /// _recoveryManager, but allows the pointer to be read atomically. This is
  /// necessary because _recoveryManager is initialized lazily at a time when
  /// other threads are already running and potentially trying to read the
  /// pointer. This only works because _recoveryManager is only initialzed once
  /// and lives until the owning PregelFeature instance is also destroyed.
  std::atomic<RecoveryManager*> _recoveryManagerPtr{nullptr};

  Scheduler::WorkHandle _gcHandle;

  struct ConductorEntry {
    std::string user;
    std::chrono::steady_clock::time_point expires;
    std::shared_ptr<Conductor> conductor;
  };

  std::unordered_map<uint64_t, ConductorEntry> _conductors;
  std::unordered_map<uint64_t, std::pair<std::string, std::shared_ptr<IWorker>>>
      _workers;

  std::atomic<bool> _softShutdownOngoing;

  std::shared_ptr<PregelMetrics> _metrics;
};

}  // namespace arangodb::pregel
