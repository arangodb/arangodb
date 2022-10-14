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

#include <gtest/gtest.h>

#include <Basics/VelocyPackStringLiteral.h>

#include <velocypack/vpack.h>

#include <fmt/core.h>
#include <fmt/format.h>

#include <Inspection/VPackPure.h>

#include <string>
#include <variant>

using namespace arangodb::velocypack;

struct VertexProperties {
  uint64_t value;
};
template<typename Inspector>
auto inspect(Inspector& f, VertexProperties& x) {
  return f.object(x).fields(f.field("value", x.value));
}

struct EmptyEdgeProperties {};
template<typename Inspector>
auto inspect(Inspector& f, EmptyEdgeProperties& x) {
  return f.object(x).fields();
}

using VertexKey = std::string;
using EdgeKey = std::string;

template<typename VertexProperties>
struct Vertex {
  VertexKey _key;
  VertexProperties properties;
};

template<typename Inspector, typename VertexProperties>
auto inspect(Inspector& f, Vertex<VertexProperties>& p) {
  return f.object(p).fields(f.field("_key", p._key),
                            f.embedFields(p.properties));
}

template<typename EdgeProperties = EmptyEdgeProperties>
struct Edge {
  EdgeKey _key;
  VertexKey _from;
  VertexKey _to;
  EdgeProperties properties;
};

template<typename Inspector, typename EdgeProperties=EmptyEdgeProperties>
auto inspect(Inspector& f, Edge<EdgeProperties>& p) {
  return f.object(p).fields(f.field("_key", p._key),
                            f.field("_from", p._from),
                            f.field("_to", p._to),
                            f.embedFields(p.properties));
}

template<typename VertexProperties, typename EdgeProperties>
struct Graph {
  std::vector<Vertex<VertexProperties>> vertices;
  std::vector<Edge<EdgeProperties>> edges;
};

void setup_graph() {
  auto graphJson =
      R"({ "vertices": [ {"_key": "A", "value": 5},
                         {"_key": "B", "value": 10},
                         {"_key": "C", "value": 15} ],
           "edges":    [ {"_key": "", "_from": "A", "_to": "B"},
                         {"_key": "", "_from": "B", "_to": "C"} ] })"_vpack;

  auto graph = Graph<VertexProperties, EmptyEdgeProperties>{};

  auto vs = graphJson["vertices"];
//  for (size_t i = 0; i < vs.length(); ++i) {
//    auto res = readVertex(graph, vs[i]);
//  }

  auto es = graphJson["edges"];
  for (size_t i = 0; i < es.length(); ++i) {
    auto edge = Edge<EmptyEdgeProperties>{};
    auto res = deserializeWithStatus(es[i].slice(), edge);
  }
}


using namespace arangodb::velocypack;

TEST(GWEN_WCC, test_wcc) {
  EXPECT_TRUE(false) << "Proved that true = false and got killed on the next zebra crossing";
}

auto main(int argc, char** argv) -> int {
  testing::InitGoogleTest();

  return RUN_ALL_TESTS();
}
