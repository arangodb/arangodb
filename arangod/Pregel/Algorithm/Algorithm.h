////////////////////////////////////////////////////////////////////////////////
///
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

#include <velocypack/vpack.h>

#include <memory>
#include <string_view>
#include <vector>

#include <fmt/core.h>

namespace arangodb::pregel::algorithm_sdk {
// These structs can be used by an algorithm
// implementor to signal that the respective
// data is empty, so we do not allocate *any*
// space for them.
//
// This becomes really quite important when
// we are talking about a billion vertices/edges
struct EmptyVertexProperties {};

template<typename Inspector>
auto inspect(Inspector& f, EmptyVertexProperties& x) {
  return f.object(x).fields();
}

struct EmptyEdgeProperties {};
template<typename Inspector>
auto inspect(Inspector& f, EmptyEdgeProperties& x) {
  return f.object(x).fields();
}

struct EmptyMessage {};
template<typename Inspector>
auto inspect(Inspector& f, EmptyMessage& x) {
  return f.object(x).fields();
}

struct VertexId {};

template<typename AlgorithmData, typename Topology>
struct TopologyBase {
  [[nodiscard]] auto readVertex_(VPackSlice const& doc) ->
      typename AlgorithmData::VertexProperties {
    return static_cast<Topology*>(this)->readVertex(doc);
  }
  [[nodiscard]] auto readEdge_(VPackSlice const& doc) ->
      typename AlgorithmData::EdgeProperties {
    return static_cast<Topology*>(this)->readEdge(doc);
  }

  std::vector<typename AlgorithmData::VertexProperties> vertices;
  std::vector<typename AlgorithmData::EdgeProperties> edges;
};

template<typename AlgorithmData, typename VertexComputation>
struct VertexComputationBase {
  auto processMessage(
      typename AlgorithmData::Global const& global,
      typename AlgorithmData::VertexProperties const& properties,
      size_t const& outEdges, VertexId const& from,
      typename AlgorithmData::Message const& payload) -> void {
    static_cast<VertexComputation*>(this)->processMessage(
        global, properties, outEdges, from, payload);
  }
  auto finish() -> void { static_cast<VertexComputation*>(this)->finish(); };
};

template<typename AlgorithmData, typename Conductor>
struct ConductorBase {
  ConductorBase() = delete;
  ConductorBase(typename AlgorithmData::Settings settings)
      : settings(settings) {}

  auto setup() -> typename AlgorithmData::Global {
    static_cast<Conductor*>(this)->setup();
  };
  auto step(typename AlgorithmData::Global const& global) ->
      typename AlgorithmData::Global {
    static_cast<Conductor*>(this)->step(global);
  };

  typename AlgorithmData::Settings settings;
};

template<typename AlgorithmData>
struct Conductor {
  typename AlgorithmData::Settings settings;
  typename AlgorithmData::Global global;
};

template<typename AlgorithmData>
auto createConductor(typename AlgorithmData::Settings settings)
    -> Conductor<AlgorithmData> {
  return Conductor<AlgorithmData>{.settings = std::move(settings),
                                  .global = typename AlgorithmData::Global{}};
}

template<typename AlgorithmData>
struct Worker {
  typename AlgorithmData::Settings settings;
};

template<typename AlgorithmData>
auto createWorker(typename AlgorithmData::Settings settings)
    -> Worker<AlgorithmData> {
  return Worker<AlgorithmData>{.settings = std::move(settings)};
}

}  // namespace arangodb::pregel::algorithm_sdk
