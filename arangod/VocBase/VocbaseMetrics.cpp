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
////////////////////////////////////////////////////////////////////////////////
#include "VocbaseMetrics.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/MetricsFeature.h"
#include "Cluster/ServerState.h"

using namespace arangodb;

DECLARE_GAUGE(arangodb_vocbase_shards_read_only_by_write_concern, std::uint64_t,
              "Number of shards that are in read-only mode because the number "
              "of in-sync replicas is lower than the write-concern");
DECLARE_COUNTER(
    arangodb_vocbase_transactions_lost_subordinates_total,
    "Counts the number of lost subordinate transactions on database servers.");

std::unique_ptr<VocbaseMetrics> VocbaseMetrics::create(
    metrics::MetricsFeature& mf, std::string_view databaseName) {
  auto metrics = std::make_unique<VocbaseMetrics>();
  auto instance = std::to_string(reinterpret_cast<uint64_t>(metrics.get()));

  auto createMetric = [&](auto builder) {
    if (!databaseName.empty()) {
      builder.addLabel("database", databaseName);
    }
    // This voc instance is required to disambiguate metrics of multiple
    // instances of the same vocbase. This happens regularly on a coordinator
    // and causes a lot of problems when deleting metrics.
    builder.addLabel("vocinstance", instance);
    return &mf.ensureMetric(std::move(builder));
  };

  if (ServerState::instance()->isDBServer()) {
    metrics->shards_read_only_by_write_concern =
        createMetric(arangodb_vocbase_shards_read_only_by_write_concern{});
  }
  if (ServerState::instance()->isCoordinator()) {
    metrics->transactions_lost_subordinates =
        createMetric(arangodb_vocbase_transactions_lost_subordinates_total{});
  }
  metrics->_metricsFeature = &mf;

  return metrics;
}

VocbaseMetrics::~VocbaseMetrics() {
  // delete all metrics
  if (_metricsFeature == nullptr) {
    return;
  }

#define DELETE_METRIC(m)           \
  if ((m) != nullptr) {            \
    _metricsFeature->remove(*(m)); \
  }
  DELETE_METRIC(shards_read_only_by_write_concern);
  DELETE_METRIC(transactions_lost_subordinates);
}
