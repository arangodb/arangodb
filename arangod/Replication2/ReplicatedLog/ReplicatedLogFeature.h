////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"

#include <cstdint>

namespace arangodb::replication2::replicated_log {
struct ReplicatedLogMetrics;
}

namespace arangodb {
class ReplicatedLogFeature final : public application_features::ApplicationFeature {
 public:
  explicit ReplicatedLogFeature(application_features::ApplicationServer& server);
  ~ReplicatedLogFeature() override;

  auto metrics() const noexcept
      -> std::shared_ptr<replication2::replicated_log::ReplicatedLogMetrics> const&;

  void prepare() override;

 private:
  std::shared_ptr<replication2::replicated_log::ReplicatedLogMetrics> _replicatedLogMetrics;
};

}  // namespace arangodb
