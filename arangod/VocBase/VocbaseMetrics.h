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
#pragma once
#include <memory>
#include "Metrics/Fwd.h"

namespace arangodb {

struct VocbaseMetrics {
  metrics::Gauge<uint64_t>* shards_read_only_by_write_concern{nullptr};
  ~VocbaseMetrics();
  static std::unique_ptr<VocbaseMetrics> create(metrics::MetricsFeature& mf,
                                                std::string_view databaseName);

 private:
  metrics::MetricsFeature* _metricsFeature{nullptr};
};

}  // namespace arangodb
