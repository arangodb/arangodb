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

#include "ReplicatedLogMetrics.h"

#include "RestServer/Metrics.h"
#include "RestServer/MetricsFeature.h"

using namespace arangodb::replication2::replicated_log;

struct AppendEntriesRttScale {
  static log_scale_t<std::uint64_t> scale() { return {2, 1, 120000, 16}; }
};

DECLARE_GAUGE(arangodb_replication2_replicated_log_number, std::uint64_t,
              "Number of replicated logs on this arangodb instance");

DECLARE_HISTOGRAM(arangodb_replication2_replicated_log_append_entries_rtt_ms,
                  AppendEntriesRttScale, "RTT for AppendEntries requests [ms]");

auto ReplicatedLogMetrics::metricReplicatedLogNumber() -> Gauge<uint64_t>& {
  return _replicated_log_number;
}

auto ReplicatedLogMetrics::metricReplicatedLogAppendEntriesRtt()
    -> Histogram<log_scale_t<std::uint64_t>>& {
  return _replicated_log_append_entries_rtt_ms;
}

ReplicatedLogMetrics::ReplicatedLogMetrics(arangodb::MetricsFeature& metricsFeature)
    : _replicated_log_number(
          metricsFeature.add(arangodb_replication2_replicated_log_number{})),
      _replicated_log_append_entries_rtt_ms(metricsFeature.add(
          arangodb_replication2_replicated_log_append_entries_rtt_ms{})) {}
