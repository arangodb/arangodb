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
///
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#include "Feature.h"

#include "Metrics/CounterBuilder.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/MetricsFeature.h"

using namespace arangodb::async_registry;

DECLARE_GAUGE(arangodb_async_active_functions, std::uint64_t,
              "Number of currently active asyncronous functions");
DECLARE_GAUGE(
    arangodb_async_ready_for_deletion_functions, std::uint64_t,
    "Number of finished asyncronous functions that wait for their garbage "
    "collection");
DECLARE_COUNTER(arangodb_async_functions_total,
                "Total number of created asyncronous functions");

DECLARE_GAUGE(arangodb_async_active_threads, std::uint64_t,
              "Number of active threads that execute asyncronous functions");
DECLARE_GAUGE(
    arangodb_async_registered_threads, std::uint64_t,
    "Number of registered threads that execute asyncronous functions");
DECLARE_COUNTER(
    arangodb_async_threads_total,
    "Total number of created threads that execute asyncronous functions");

auto Feature::create_metrics(arangodb::metrics::MetricsFeature& metrics_feature)
    -> std::shared_ptr<const Metrics> {
  return std::make_shared<Metrics>(
      metrics_feature.addShared(arangodb_async_active_functions{}),
      metrics_feature.addShared(arangodb_async_ready_for_deletion_functions{}),
      metrics_feature.addShared(arangodb_async_active_threads{}),
      metrics_feature.addShared(arangodb_async_registered_threads{}),
      metrics_feature.addShared(arangodb_async_threads_total{}),
      metrics_feature.addShared(arangodb_async_functions_total{}));
}
