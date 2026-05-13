////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <optional>
#include <string_view>

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/Result.h"
#include "Futures/Future.h"
#include "ProgramOptions/ProgramOptions.h"
#include "VectorIndex/VectorIndexBuildManager.h"
#include "VectorIndex/VectorIndexFeatureOptions.h"
#include "VocBase/Identifiers/IndexId.h"

#include <memory>

namespace arangodb {

class Index;
class DatabaseFeature;

class VectorIndexFeature final
    : public application_features::ApplicationFeature {
 public:
  VectorIndexFeature(application_features::ApplicationServer& server,
                     DatabaseFeature& databaseFeature);

  static constexpr std::string_view name() noexcept { return "VectorIndex"; }

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;

  void start() override final;

  void beginShutdown() override final;

  void stop() override final;

  bool isVectorIndexEnabled() const;

  // Wait until the given vector index is trained. On single server or
  // DBServer, returns a future that resolves when the build manager finishes
  // (or fails) the build. If the build manager is not initialized (e.g. on
  // Coordinator), returns an immediate success.
  futures::Future<Result> waitForIndexReady(IndexId indexId);

  // Enqueue a retrain job for an existing, ready vector index on this
  // DBServer / single-server. The build manager constructs a shadow index
  // with a fresh IndexId/objectId, drives it through the standard
  // train+ingest pipeline, and atomically drops the old index once the
  // shadow reaches the ready state. Returns immediately; progress can be
  // observed by polling the index state.
  Result requestRetrain(std::shared_ptr<Index> const& oldIndex);

 private:
  bool shouldRunBuildManager() const;

  DatabaseFeature& _databaseFeature;
  VectorIndexFeatureOptions _options;
  std::optional<vector::VectorIndexBuildManager> _buildManager;
};

}  // namespace arangodb
