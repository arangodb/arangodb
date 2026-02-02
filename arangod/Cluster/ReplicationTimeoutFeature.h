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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Cluster/ReplicationTimeoutFeatureOptions.h"

#include <string_view>

namespace arangodb {

class ReplicationTimeoutFeature
    : public application_features::ApplicationFeature {
 public:
  static const std::string_view name() noexcept { return "ReplicationTimeout"; }

  explicit ReplicationTimeoutFeature(
      application_features::ApplicationServer& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;

  double timeoutFactor() const noexcept { return _options.timeoutFactor; }
  double timeoutPer4k() const noexcept { return _options.timeoutPer4k; }
  double lowerLimit() const noexcept { return _options.lowerLimit; }
  double upperLimit() const noexcept { return _options.upperLimit; }

  double shardSynchronizationAttemptTimeout() const noexcept {
    return _options.shardSynchronizationAttemptTimeout;
  }

 private:
  ReplicationTimeoutFeatureOptions _options;
};

}  // namespace arangodb
