////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright $YEAR-2021 ArangoDB GmbH, Cologne, Germany
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

template <typename T>
class Histogram;
template <typename T>
class Gauge;
template <typename T>
struct log_scale_t;

namespace arangodb {
class ReplicatedLogFeature : public application_features::ApplicationFeature {
 public:
  explicit ReplicatedLogFeature(application_features::ApplicationServer& server);
  ~ReplicatedLogFeature() override = default;

  auto metricReplicatedLogNumber() -> Gauge<uint64_t>&;
  auto metricReplicatedLogAppendEntriesRtt() -> Histogram<log_scale_t<std::uint64_t>>&;

 private:
  Gauge<uint64_t>& _replicated_log_number;
  Histogram<log_scale_t<std::uint64_t>>& _replicated_log_append_entries_rtt_ms;
};

}  // namespace arangodb
