////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
///
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"
#include "RestServer/BootstrapFeatureOptions.h"

namespace arangodb {
class V8DealerFeature;
class ClusterUpgradeFeature;
class SystemDatabaseFeature;
class DatabaseFeature;
class EngineSelectorFeature;
class ClusterFeature;

class BootstrapFeature final : public application_features::ApplicationFeature {
 public:
  static constexpr std::string_view name() noexcept { return "Bootstrap"; }

  explicit BootstrapFeature(application_features::ApplicationServer& server,
                            ClusterFeature& clusterFeature,
                            EngineSelectorFeature& engineSelectorFeature,
                            DatabaseFeature& databaseFeature,
                            SystemDatabaseFeature* systemDatabaseFeature,
                            ClusterUpgradeFeature* clusterUpgradeFeature,
                            V8DealerFeature* v8DealerFeature);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void start() override final;
  void stop() override final;
  void unprepare() override final;

  bool isReady() const;

  ClusterFeature& clusterFeature();
  EngineSelectorFeature& engineSelectorFeature();
  DatabaseFeature& databaseFeature();
  SystemDatabaseFeature* systemDatabaseFeature();
  ClusterUpgradeFeature* clusterUpgradeFeature();
  V8DealerFeature* v8DealerFeature();

 private:
  void killRunningQueries();
  void waitForHealthEntry();
  /// @brief wait for databases to appear in Plan and Current
  void waitForDatabases() const;

  ClusterFeature& _clusterFeature;
  EngineSelectorFeature& _engineSelectorFeature;
  DatabaseFeature& _databaseFeature;
  SystemDatabaseFeature* _systemDatabaseFeature{};
  ClusterUpgradeFeature* _clusterUpgradeFeature{};
  V8DealerFeature* _v8DealerFeature{};

  BootstrapFeatureOptions _options;
  bool _isReady;
};

}  // namespace arangodb
