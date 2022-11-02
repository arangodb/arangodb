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

#pragma once

#include "Metrics/CounterBuilder.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/HistogramBuilder.h"
#include "Metrics/LogScale.h"

#include <cstdint>

namespace arangodb {

DECLARE_GAUGE(arangodb_pregel_conductors_number, std::uint64_t,
              "Number of live Pregel conductors");

DECLARE_GAUGE(arangodb_pregel_conductors_loading_number, std::uint64_t,
              "Number of Pregel conductors in loading state");

DECLARE_GAUGE(arangodb_pregel_conductors_running_number, std::uint64_t,
              "Number of Pregel conductors in running state");

DECLARE_GAUGE(arangodb_pregel_conductors_storing_number, std::uint64_t,
              "Number of Pregel conductors in storing state");

DECLARE_GAUGE(arangodb_pregel_workers_number, std::uint64_t,
              "Number of live Pregel workers");

DECLARE_GAUGE(arangodb_pregel_workers_loading_number, std::uint64_t,
              "Number of Pregel conductors in loading state");

DECLARE_GAUGE(arangodb_pregel_workers_running_number, std::uint64_t,
              "Number of Pregel conductors in running state");

DECLARE_GAUGE(arangodb_pregel_workers_storing_number, std::uint64_t,
              "Number of Pregel conductors in storing state");

DECLARE_COUNTER(arangodb_pregel_messages_sent_total,
                "Number of messages sent by Pregel");

DECLARE_COUNTER(arangodb_pregel_messages_received_total,
                "Number of messages received by Pregel");

DECLARE_GAUGE(arangodb_pregel_threads_number, std::uint64_t,
              "Number of threads used by Pregel");

DECLARE_GAUGE(arangodb_pregel_graph_memory_bytes_number, std::uint64_t,
              "Amount of memory in use for Pregel graph storage");

}  // namespace arangodb
