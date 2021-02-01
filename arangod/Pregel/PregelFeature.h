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

#ifndef ARANGODB_PREGEL_FEATURE_H
#define ARANGODB_PREGEL_FEATURE_H 1

#include <cstdint>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/Common.h"
#include "Basics/Mutex.h"

struct TRI_vocbase_t;

namespace arangodb {
namespace pregel {

class Conductor;
class IWorker;
class RecoveryManager;

class PregelFeature final : public application_features::ApplicationFeature {
 public:
  explicit PregelFeature(application_features::ApplicationServer& server);
  ~PregelFeature();

  static std::shared_ptr<PregelFeature> instance();
  static size_t availableParallelism();

  static std::pair<Result, uint64_t> startExecution(
      TRI_vocbase_t& vocbase, std::string algorithm,
      std::vector<std::string> const& vertexCollections,
      std::vector<std::string> const& edgeCollections, 
      std::unordered_map<std::string, std::vector<std::string>> const& edgeCollectionRestrictions,
      VPackSlice const& params);

  void start() override final;
  void beginShutdown() override final;
  void stop() override final;
  void unprepare() override final;

  uint64_t createExecutionNumber();
  void addConductor(std::shared_ptr<Conductor>&&, uint64_t executionNumber);
  std::shared_ptr<Conductor> conductor(uint64_t executionNumber);

  void addWorker(std::shared_ptr<IWorker>&&, uint64_t executionNumber);
  std::shared_ptr<IWorker> worker(uint64_t executionNumber);

  void cleanupConductor(uint64_t executionNumber);
  void cleanupWorker(uint64_t executionNumber);
  void cleanupAll();

  // ThreadPool* threadPool() { return _threadPool.get(); }
  RecoveryManager* recoveryManager() {
    if (_recoveryManager) {
      return _recoveryManager.get();
    }
    return nullptr;
  }

  static void handleConductorRequest(TRI_vocbase_t& vocbase, std::string const& path,
                                     VPackSlice const& body, VPackBuilder& outResponse);
  static void handleWorkerRequest(TRI_vocbase_t& vocbase, std::string const& path,
                                  VPackSlice const& body, VPackBuilder& outBuilder);

 private:
  Mutex _mutex;
  std::unique_ptr<RecoveryManager> _recoveryManager;
  std::unordered_map<uint64_t, std::pair<std::string, std::shared_ptr<Conductor>>> _conductors;
  std::unordered_map<uint64_t, std::pair<std::string, std::shared_ptr<IWorker>>> _workers;
};

}  // namespace pregel
}  // namespace arangodb

#endif
