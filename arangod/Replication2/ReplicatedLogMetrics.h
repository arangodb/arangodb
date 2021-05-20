////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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

#include <cstdint>

template <typename T>
class Histogram;
template <typename T>
class Gauge;
template <typename T>
struct log_scale_t;

namespace arangodb {
class MetricsFeature;
}

namespace arangodb::replication2::replicated_log {

struct ReplicatedLogMetrics {
  explicit ReplicatedLogMetrics(arangodb::MetricsFeature& metricsFeature);

  Gauge<uint64_t>& replicatedLogNumber;
  Histogram<log_scale_t<std::uint64_t>>& replicatedLogAppendEntriesRttUs;

#ifdef ARANGODB_USE_GOOGLE_TESTS
 protected:
  ReplicatedLogMetrics(Gauge<uint64_t>& replicatedLogNumber,
                       Histogram<log_scale_t<std::uint64_t>>& replicatedLogAppendEntriesRttUs);
#endif
};

}  // namespace arangodb::replication2::replicated_log
