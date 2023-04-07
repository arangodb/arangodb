////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Aditya Mukhopadhyay
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <variant>
#include <cstdint>
#include "Inspection/Types.h"

namespace arangodb::pregel::message {
enum class GaugeOp { INCR, DECR };
template<typename Inspector>
auto inspect(Inspector& f, GaugeOp& x) {
  return f.enumeration(x).values(GaugeOp::DECR, "Decrement", GaugeOp::INCR,
                                 "Increment");
}

enum class Gauge {
  CONDUCTORS_TOTAL,
  CONDUCTORS_LOADING,
  CONDUCTORS_RUNNING,
  CONDUCTORS_STORING,

  WORKERS_TOTAL,
  WORKERS_LOADING,
  WORKERS_RUNNING,
  WORKERS_STORING,

  THREAD_COUNT,
  MEMORY_FOR_GRAPH,
};
template<typename Inspector>
auto inspect(Inspector& f, Gauge& x) {
  return f.enumeration(x).values(
      Gauge::CONDUCTORS_TOTAL, "CONDUCTORS_TOTAL", Gauge::CONDUCTORS_LOADING,
      "CONDUCTORS_LOADING", Gauge::CONDUCTORS_STORING, "CONDUCTORS_STORING",
      Gauge::CONDUCTORS_RUNNING, "CONDUCTORS_RUNNING", Gauge::WORKERS_TOTAL,
      "WORKERS_TOTAL", Gauge::WORKERS_LOADING, "WORKERS_LOADING",
      Gauge::WORKERS_STORING, "WORKERS_STORING", Gauge::WORKERS_RUNNING,
      "WORKERS_RUNNING", Gauge::THREAD_COUNT, "THREAD_COUNT",
      Gauge::MEMORY_FOR_GRAPH, "MEMORY_FOR_GRAPH");
}

struct MetricsStart {};
template<typename Inspector>
auto inspect(Inspector& f, MetricsStart& x) {
  return f.object(x).fields();
}

template<Gauge G, GaugeOp O>
struct GaugeUpdate {
  uint64_t amount;
};
template<typename Inspector, Gauge G, GaugeOp O>
auto inspect(Inspector& f, GaugeUpdate<G, O>& x) {
  return f.object(x).fields(f.field("Gauge", G), f.field("GaugeOp", O),
                            f.field("amount", x.amount));
}

enum class Counter { MESSAGES_SENT, MESSAGES_RECEIVED };
template<typename Inspector>
auto inspect(Inspector& f, Counter& x) {
  return f.enumeration(x).values(Counter::MESSAGES_SENT, "MESSAGES_SENT",
                                 Counter::MESSAGES_RECEIVED,
                                 "MESSAGES_RECEIVED");
}

template<Counter C>
struct CounterUpdate {
  uint64_t amount;
};
template<typename Inspector, Counter C>
auto inspect(Inspector& f, CounterUpdate<C>& x) {
  return f.object(x).fields(f.field("Counter", C), f.field("amount", x.amount));
}

using MetricsMessagesBase =
    std::variant<MetricsStart,
                 GaugeUpdate<Gauge::CONDUCTORS_TOTAL, GaugeOp::INCR>,
                 GaugeUpdate<Gauge::CONDUCTORS_LOADING, GaugeOp::INCR>,
                 GaugeUpdate<Gauge::CONDUCTORS_RUNNING, GaugeOp::INCR>,
                 GaugeUpdate<Gauge::CONDUCTORS_STORING, GaugeOp::INCR>,
                 GaugeUpdate<Gauge::WORKERS_TOTAL, GaugeOp::INCR>,
                 GaugeUpdate<Gauge::WORKERS_LOADING, GaugeOp::INCR>,
                 GaugeUpdate<Gauge::WORKERS_RUNNING, GaugeOp::INCR>,
                 GaugeUpdate<Gauge::WORKERS_STORING, GaugeOp::INCR>,
                 GaugeUpdate<Gauge::THREAD_COUNT, GaugeOp::INCR>,
                 GaugeUpdate<Gauge::MEMORY_FOR_GRAPH, GaugeOp::INCR>,
                 GaugeUpdate<Gauge::CONDUCTORS_TOTAL, GaugeOp::DECR>,
                 GaugeUpdate<Gauge::CONDUCTORS_LOADING, GaugeOp::DECR>,
                 GaugeUpdate<Gauge::CONDUCTORS_RUNNING, GaugeOp::DECR>,
                 GaugeUpdate<Gauge::CONDUCTORS_STORING, GaugeOp::DECR>,
                 GaugeUpdate<Gauge::WORKERS_TOTAL, GaugeOp::DECR>,
                 GaugeUpdate<Gauge::WORKERS_LOADING, GaugeOp::DECR>,
                 GaugeUpdate<Gauge::WORKERS_RUNNING, GaugeOp::DECR>,
                 GaugeUpdate<Gauge::WORKERS_STORING, GaugeOp::DECR>,
                 GaugeUpdate<Gauge::THREAD_COUNT, GaugeOp::DECR>,
                 GaugeUpdate<Gauge::MEMORY_FOR_GRAPH, GaugeOp::DECR>,
                 CounterUpdate<Counter::MESSAGES_SENT>,
                 CounterUpdate<Counter::MESSAGES_RECEIVED>>;

struct MetricsMessages : MetricsMessagesBase {
  using MetricsMessagesBase::variant;
};
template<typename Inspector, Gauge G, GaugeOp O, Counter C>
auto inspect(Inspector& f, MetricsMessages& x) {
  namespace insp = arangodb::inspection;

  return f.variant(x).unqualified().alternatives(
      insp::type<MetricsStart>("MetricsStart"),
      insp::type<GaugeUpdate<G, O>>("GaugeUpdate"),
      insp::type<CounterUpdate<C>>("CounterUpdate"));
}

// template<typename Inspector>
// auto inspect(Inspector& f, MetricsMessages& x) {
//   namespace insp = arangodb::inspection;
//
//   return f.variant(x).unqualified().alternatives(
//       insp::type<GaugeUpdate<Gauge::CONDUCTORS_TOTAL>>(
//           "GaugeUpdate (CONDUCTORS_TOTAL)"),
//       insp::type<GaugeUpdate<Gauge::CONDUCTORS_LOADING>>(
//           "GaugeUpdate (CONDUCTORS_LOADING)"),
//       insp::type<GaugeUpdate<Gauge::CONDUCTORS_STORING>>(
//           "GaugeUpdate (CONDUCTORS_STORING)"),
//       insp::type<GaugeUpdate<Gauge::CONDUCTORS_RUNNING>>(
//           "GaugeUpdate (CONDUCTORS_RUNNING)"),
//       insp::type<GaugeUpdate<Gauge::WORKERS_TOTAL>>(
//           "GaugeUpdate (WORKERS_TOTAL)"),
//       insp::type<GaugeUpdate<Gauge::WORKERS_LOADING>>(
//           "GaugeUpdate (WORKERS_LOADING)"),
//       insp::type<GaugeUpdate<Gauge::WORKERS_STORING>>(
//           "GaugeUpdate (WORKERS_STORING)"),
//       insp::type<GaugeUpdate<Gauge::WORKERS_RUNNING>>(
//           "GaugeUpdate (WORKERS_RUNNING)"),
//       insp::type<GaugeUpdate<Gauge::THREAD_COUNT>>(
//           "GaugeUpdate (THREAD_COUNT)"),
//       insp::type<GaugeUpdate<Gauge::MEMORY_FOR_GRAPH>>(
//           "GaugeUpdate (MEMORY_FOR_GRAPH)"),
//       insp::type<CounterUpdate<Counter::MESSAGES_SENT>>(
//           "CounterUpdate (MESSAGES_SENT)"),
//       insp::type<CounterUpdate<Counter::MESSAGES_RECEIVED>>(
//           "CounterUpdate (MESSAGES_RECEIVED)"));
// }
}  // namespace arangodb::pregel::message
