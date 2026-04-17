////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2026 ArangoDB GmbH, Hyderabad, India
/// Copyright 2026 triAGENS GmbH, Hyderabad, India
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
/// Copyright holder is ArangoDB GmbH, Hyderabad, India
///
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/OptionsProvider.h"
#include "ClusterUpgradeFeatureOptions.h"
#include <memory>

namespace arangodb::options {
class ProgramOptions;
}

namespace arangodb {

class DatabaseFeature;

namespace upgrade {

struct ClusterUpgradeOptionsProvider
    : OptionsProvider<ClusterUpgradeFeatureOptions> {
  ClusterUpgradeOptionsProvider() = default;

  void declareOptions(std::shared_ptr<options::ProgramOptions> opts,
                      ClusterUpgradeFeatureOptions& options) override;

  /// Applies `--cluster.upgrade` mode to DatabaseFeature (coordinator only).
  void validateClusterUpgradeOptions(
      std::shared_ptr<options::ProgramOptions> opts,
      ClusterUpgradeFeatureOptions& options, DatabaseFeature& databaseFeature);
};

}  // namespace upgrade
}  // namespace arangodb
