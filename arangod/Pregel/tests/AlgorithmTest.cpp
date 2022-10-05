#include <iostream>
#include <vector>

#include <gtest/gtest.h>

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

TEST(Pregel, test_graph_setup) {
  auto graphJson =
      R"({ "vertices": [ {"_key": "A", "value": 5}, {"_key": "B"}, {"_key": "C"} ],
           "edges":    [ {"_key": "", "_from": "A", "_to": "B"},
                         {"_key": "", "_from": "B", "_to": "C"} ] })"_vpack;

  auto graph = Graph<VertexProperties, EmptyEdgeProperties>{};

  auto vs = graphJson["vertices"];
  for (size_t i = 0; i < vs.length(); ++i) {
    auto v = Vertex<VertexProperties>{};
    auto res = deserializeWithStatus(vs[i].slice(), v);
    if (res.ok()) {
      graph.vertices.emplace_back(v);
    } else {
      fmt::print("error reading vertex: {}", res.error());
    }
    fmt::print("v: {}\n", vs[i].toJson());
  }

  auto es = graphJson["edges"];
  for (size_t i = 0; i < es.length(); ++i) {
    auto e = Edge{};
    auto res = deserializeWithStatus(es[i].slice(), e);

    if (res.ok()) {
      graph.edges.emplace_back(e);
    } else {
      fmt::print("error reading vertex: {}", res.error());
    }

    fmt::print("e: {}\n", es[i].toJson());
  }

  //  fmt::print("{}", graphJson.toJson());
  EXPECT_TRUE(false) << "Expected true to be false and black equal to white "
                        "and be killed on the next zebra crossing";
}

#if 0
auto test_conductor_setup() {
  auto c = Conductor(Settings{.iterations = 100});

  auto h = c.setup();

  while (c.continue_huh(h)) {
    h = c.step(h);
    fmt::print("{:f}", h);
  }
}

auto test_vertex_computation() { auto v = VertexComputation(); }
#endif

auto main(int argc, char **argv) -> int {
  testing::InitGoogleTest();
  fmt::print("Hello, world\n");

  return RUN_ALL_TESTS();
}
