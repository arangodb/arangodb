////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <vector>

#include "Metrics/IRegistry.h"

namespace arangodb::tests {

// Minimal IRegistry implementation for tests. Keeps registered metric objects
// alive so that the references handed out by add() remain valid.
struct MetricsCollector : metrics::IRegistry {
  std::shared_ptr<metrics::Metric> doAdd(metrics::Builder& builder) override {
    auto metric = builder.build();
    _metrics.emplace_back(metric);
    return metric;
  }

 private:
  std::vector<std::shared_ptr<metrics::Metric>> _metrics;
};

}  // namespace arangodb::tests
