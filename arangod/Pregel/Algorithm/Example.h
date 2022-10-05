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

#include <Algorithm/Algorithm.h>

#include <Inspection/VPackPure.h>

#include "Algorithm.h"
#include <string>
#include <variant>

namespace arangodb::pregel::algorithms::example {

struct Settings {
  uint64_t iterations;
  std::string resultField;
};
template <typename Inspector> auto inspect(Inspector &f, Settings &x) {
  return f.object(x).fields(f.field("iterations", x.iterations),
                            f.field("resultField", x.resultField));
}

struct VertexProperties {
  uint64_t value;
};
template <typename Inspector> auto inspect(Inspector &f, VertexProperties &x) {
  return f.object(x).fields(f.field("value", x.value));
}

struct Global {
  uint64_t currentIteration;
};
template <typename Inspector> auto inspect(Inspector &f, Global &x) {
  return f.object(x).fields(f.field("currentIteration", x.currentIteration));
}

struct Message {
  uint64_t value;
};
template <typename Inspector> auto inspect(Inspector &f, Message &x) {
  return f.object(x).fields(f.field("value", x.value));
}

struct Aggregators {
  //  MaxAggregator<double> difference;
};

struct Data {
  using Settings = Settings;
  using VertexProperties = VertexProperties;
  using EdgeProperties = algorithm_sdk::EmptyEdgeProperties;
  using Message = Message;

  using Global = Global;

  using Aggregators = Aggregators;
};

struct Topology : public algorithm_sdk::TopologyBase<Data, Topology> {
  [[nodiscard]] auto readVertex(VPackSlice const &doc) -> VertexProperties {
    return VertexProperties{.value = 0};
  }
  [[nodiscard]] auto readEdge(VPackSlice const &doc)
      -> algorithm_sdk::EmptyEdgeProperties {
    return algorithm_sdk::EmptyEdgeProperties{};
  }
};

struct Conductor : public algorithm_sdk::ConductorBase<Data, Conductor> {
  Conductor(Data::Settings settings) : ConductorBase(settings) {}
  auto setup() -> Global { return Global{.currentIteration = 0}; }
  auto step(Global const &global) -> Global {
    auto newGlobal = global;
    newGlobal.currentIteration += 1;
    return newGlobal;
  }
  auto continue_huh(Global const &global) -> bool {
    return global.currentIteration < settings.iterations;
  }
};

struct VertexComputation
    : public algorithm_sdk::VertexComputationBase<Data, VertexComputation> {};

} // namespace arangodb::pregel::algorithms::example
