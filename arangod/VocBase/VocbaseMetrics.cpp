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
#include "Metrics/GaugeBuilder.h"
#include "Metrics/MetricsFeature.h"

using namespace arangodb;

DECLARE_GAUGE(arangodb_vocbase_shards_read_only_by_write_concern, std::uint64_t,
              "Number of shards that are in read-only mode because the number "
              "of in-sync replicas is lower than the write-concern");

std::unique_ptr<VocbaseMetrics> VocbaseMetrics::create(
    metrics::MetricsFeature& mf, std::string_view databaseName) {
  auto createMetric = [&](auto builder) {
    if (!databaseName.empty()) {
      builder.addLabel("database", databaseName);
    }
    return &mf.ensureMetric(std::move(builder));
  };

  auto metrics = std::make_unique<VocbaseMetrics>();
  metrics->shards_read_only_by_write_concern =
      createMetric(arangodb_vocbase_shards_read_only_by_write_concern{});

  return metrics;
}
