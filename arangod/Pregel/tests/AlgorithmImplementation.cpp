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

#include <velocypack/vpack.h>

#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

#include <Basics/VelocyPackStringLiteral.h>
#include <Inspection/VPackPure.h>

#include <velocypack/vpack.h>

#include <fmt/core.h>
#include <fmt/format.h>

#include <Algorithm/Example.h>
#include <Algorithm/Graph.h>

using namespace arangodb::velocypack;
using namespace arangodb::pregel::graph;
using namespace arangodb::pregel::algorithm_sdk;
using namespace arangodb::pregel::algorithms::example;

void setup_graph() {
  auto graphJson =
      R"({ "vertices": [ {"_key": "A", "value": 5},
                         {"_key": "B", "value": 10},
                         {"_key": "C", "value": 15} ],
           "edges":    [ {"_key": "", "_from": "A", "_to": "B"},
                         {"_key": "", "_from": "B", "_to": "C"} ] })"_vpack;

  auto graph = Graph<VertexProperties, EmptyEdgeProperties>{};

  auto vs = graphJson["vertices"];
  for (size_t i = 0; i < vs.length(); ++i) {
    auto res = readVertex(graph, vs[i]);
  }

  auto es = graphJson["edges"];
  for (size_t i = 0; i < es.length(); ++i) {
    auto res = readEdge(graph, es[i]);
  }
}

void setup() {
  auto const& settings = Settings{.iterations = 10, .resultField = "result"};

  auto g = createConductor<Data>(settings);

  std::vector<Worker<Data>> workers;
  for (size_t i = 0; i < 16; i++) {
    fmt::print("foo: {}\n", i);
    workers.emplace_back(createWorker<Data>(settings));
  }
}

auto main(int argc, char** argv) -> int {
  fmt::print("GWEN testbed\n");
  setup();

  return 0;
}
