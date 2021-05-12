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

#include "ReplicatedLogFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "FeaturePhases/DatabaseFeaturePhase.h"
#include "RestServer/Metrics.h"
#include "RestServer/MetricsFeature.h"

using namespace arangodb;
using namespace arangodb::application_features;

struct AppendEntriesRttScale {
  static log_scale_t<std::uint64_t> scale() { return {2, 1, 120000, 16}; }
};

DECLARE_GAUGE(arangodb_replication2_replicated_log_number, std::uint64_t,
              "Number of replicated logs on this arangodb instance");

DECLARE_HISTOGRAM(arangodb_replication2_replicated_log_append_entries_rtt_ms,
                  AppendEntriesRttScale, "RTT for AppendEntries requests [ms]");

ReplicatedLogFeature::ReplicatedLogFeature(ApplicationServer& server)
    : ApplicationFeature(server, "ReplicatedLog"),
      _replicated_log_number(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_replication2_replicated_log_number{})),
      _replicated_log_append_entries_rtt_ms(
          server.getFeature<arangodb::MetricsFeature>().add(
              arangodb_replication2_replicated_log_append_entries_rtt_ms{})) {
  startsAfter<CommunicationFeaturePhase>();
  startsAfter<DatabaseFeaturePhase>();
}
auto ReplicatedLogFeature::metricReplicatedLogNumber() -> Gauge<uint64_t>& {
  return _replicated_log_number;
}
auto ReplicatedLogFeature::metricReplicatedLogAppendEntriesRtt()
    -> Histogram<log_scale_t<std::uint64_t>>& {
  return _replicated_log_append_entries_rtt_ms;
}
