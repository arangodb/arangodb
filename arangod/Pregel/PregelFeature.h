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
#include <string>
#include <unordered_map>
#include <vector>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Scheduler/Scheduler.h"

struct TRI_vocbase_t;

namespace arangodb::pregel {

class Conductor;
class IWorker;
class RecoveryManager;

class PregelFeature final : public application_features::ApplicationFeature {
 public:
  explicit PregelFeature(application_features::ApplicationServer& server);
  ~PregelFeature() override;

  static size_t availableParallelism();

  /* Make a conductor, register it in PregelFeature and start it.
   * Return the Result and the execution number of the conductor.
   * todo (Roman) could be ResultT
   * */
  std::pair<Result, uint64_t> startExecution(
      TRI_vocbase_t& vocbase, const std::string& algorithm,
      std::vector<std::string> const& vertexCollections,
      std::vector<std::string> const& edgeCollections,
      std::unordered_map<std::string, std::vector<std::string>> const&
          edgeCollectionRestrictions,
      VPackSlice const& params);

  /* A Coordinator stores a new recoveryManager, an Agent runs
   * scheduleGarbageCollection.
   * */
  void start() final;
  /* Cancel all conductors and workers.
   * */
  void beginShutdown() final;
  /* Runs garbage collectors, in maintainer mode checks that all references
   * to conductors and workers have been dropped.
   * */
  void unprepare() final;

  bool isStopping() const noexcept;

  static uint64_t createExecutionNumber();
  void addConductor(std::shared_ptr<Conductor>&&, uint64_t executionNumber);
  std::shared_ptr<Conductor> conductor(uint64_t executionNumber);

  void garbageCollectConductors();

  void addWorker(std::shared_ptr<IWorker>&&, uint64_t executionNumber);
  /* If the worker exists in _workers, return it, otherwise return nullptr.
   * */
  std::shared_ptr<IWorker> worker(uint64_t executionNumber);

  /* Erase the objects with executionNumber from _conductors and _workers.
   * */
  void cleanupConductor(uint64_t executionNumber);
  /* Enqueues the task to erase the object with executionNumber from _workers.
   * */
  void cleanupWorker(uint64_t executionNumber);

  RecoveryManager* recoveryManager() {
    return _recoveryManagerPtr.load(std::memory_order_acquire);
  }

  /**
   *
   * @param vocbase
   * @param path the state of the state machine
   * @param body may have Utils::executionNumberKey, Utils::vertexCountKey,
   * Utils::edgeCountKey, Utils::senderKey, Utils::globalSuperstepKey,
   * "reports", Utils::aggregatorValuesKey, Utils::receivedCountKey,
   * Utils::sendCountKey
   * @param outResponse
   */
  void handleConductorRequest(TRI_vocbase_t& vocbase, std::string const& path,
                              VPackSlice const& body,
                              VPackBuilder& outResponse);
  void handleWorkerRequest(TRI_vocbase_t& vocbase, std::string const& path,
                           VPackSlice const& body, VPackBuilder& outBuilder);

  uint64_t numberOfActiveConductors() const;

  void initiateSoftShutdown() final {
    _softShutdownOngoing.store(true, std::memory_order_relaxed);
  }

  Result toVelocyPack(TRI_vocbase_t& vocbase,
                      arangodb::velocypack::Builder& result, bool allDatabases,
                      bool fanout) const;

 private:
  void scheduleGarbageCollection();

  mutable Mutex _mutex;

  std::unique_ptr<RecoveryManager> _recoveryManager;
  /// @brief _recoveryManagerPtr always points to the same object as
  /// _recoveryManager, but allows the pointer to be read atomically. This is
  /// necessary because _recoveryManager is initialized lazily at a time when
  /// other threads are already running and potentially trying to read the
  /// pointer. This only works because _recoveryManager is only initialized once
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
      _workers; // executionNumber -> (user -> worker)

  std::atomic<bool> _softShutdownOngoing;
};

}  // namespace arangodb::pregel
