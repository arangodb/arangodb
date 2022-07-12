////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "PregelMetrics.h"

#include "Pregel/PregelMetricsDeclarations.h"
#include "Metrics/MetricsFeature.h"

using namespace arangodb::pregel;

PregelMetrics::PregelMetrics(metrics::MetricsFeature& metricsFeature)
    : PregelMetrics(&metricsFeature) {}

template<typename Builder, bool mock>
auto PregelMetrics::createMetric(metrics::MetricsFeature* metricsFeature)
    -> std::shared_ptr<typename Builder::MetricT> {
  TRI_ASSERT((metricsFeature == nullptr) == mock);
  if constexpr (!mock) {
    return metricsFeature->addShared(Builder{});
  } else {
    return std::dynamic_pointer_cast<typename Builder::MetricT>(
        Builder{}.build());
  }
}

template<
    typename MFP,
    std::enable_if_t<std::is_same_v<arangodb::metrics::MetricsFeature*, MFP> ||
                         std::is_null_pointer_v<MFP>,
                     int>,
    bool mock>
PregelMetrics::PregelMetrics(MFP metricsFeature)
    : pregelConductorsNumber(
          createMetric<arangodb_pregel_conductors_number, mock>(
              metricsFeature)),
      pregelConductorsLoadingNumber(
          createMetric<arangodb_pregel_conductors_loading_number, mock>(
              metricsFeature)),
      pregelConductorsRunningNumber(
          createMetric<arangodb_pregel_conductors_running_number, mock>(
              metricsFeature)),
      pregelConductorsStoringNumber(
          createMetric<arangodb_pregel_conductors_storing_number, mock>(
              metricsFeature)),

      pregelWorkersNumber(
          createMetric<arangodb_pregel_workers_number, mock>(metricsFeature)),
      pregelWorkersLoadingNumber(
          createMetric<arangodb_pregel_workers_loading_number, mock>(
              metricsFeature)),
      pregelWorkersRunningNumber(
          createMetric<arangodb_pregel_workers_running_number, mock>(
              metricsFeature)),
      pregelWorkersStoringNumber(
          createMetric<arangodb_pregel_workers_storing_number, mock>(
              metricsFeature)),

      pregelMessagesSent(
          createMetric<arangodb_pregel_messages_sent_total, mock>(
              metricsFeature)),
      pregelMessagesReceived(
          createMetric<arangodb_pregel_messages_received_total, mock>(
              metricsFeature)),

      pregelNumberOfThreads(
          createMetric<arangodb_pregel_threads_number, mock>(metricsFeature)),

      pregelMemoryUsedForGraph(
          createMetric<arangodb_pregel_graph_memory_bytes_number, mock>(
              metricsFeature))

{
#ifndef ARANGODB_USE_GOOGLE_TESTS
  static_assert(!mock);
  static_assert(!std::is_null_pointer_v<MFP>);
#endif
}

template arangodb::pregel::PregelMetrics::PregelMetrics(
    arangodb::metrics::MetricsFeature*);
#ifdef ARANGODB_USE_GOOGLE_TESTS
template arangodb::pregel::PregelMetrics::PregelMetrics(std::nullptr_t);
#endif
