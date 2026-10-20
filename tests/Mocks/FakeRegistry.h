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

#include "Metrics/IRegistry.h"
#include "Metrics/Builder.h"
#include "Metrics/Metric.h"

#include <vector>
#include <mutex>

namespace arangodb::metrics {

// A lightweight registry that populates metrics but does not register them
// with any actual metrics endpoint.
// The registry holds the ownership of all the metrics. So, the reference
// returned by IRegistry::add() is valid for this registry's lifetime.
struct FakeRegistry : public IRegistry {
 protected:
  std::shared_ptr<metrics::Metric> doAdd(metrics::Builder& builder) override {
    auto metrics = builder.build();
    std::lock_guard lock{_mutex};
    _metrics.push_back(metrics);
    return metrics;
  }

 private:
  std::mutex _mutex;
  // "add()" hands out references, so we we have to keep the metrics alive here
  std::vector<std::shared_ptr<metrics::Metric>> _metrics;
};

}  // namespace arangodb::metrics