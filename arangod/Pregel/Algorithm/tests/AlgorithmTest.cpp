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
      R"({ "vertices": [ {"_key": "A", "value": 5},
                         {"_key": "B", "value": 10},
                         {"_key": "C", "value": 15} ],
           "edges":    [ {"_key": "", "_from": "A", "_to": "B"},
                         {"_key": "", "_from": "B", "_to": "C"} ] })"_vpack;

  auto graph = Graph<VertexProperties, EmptyEdgeProperties>{};

  auto vs = graphJson["vertices"];
  for (size_t i = 0; i < vs.length(); ++i) {
    auto res = readVertex(graph, vs[i]);

    EXPECT_TRUE(res.ok()) << fmt::format("error reading vertex: {}\n",
                                         res.error());
  }

  auto es = graphJson["edges"];
  for (size_t i = 0; i < es.length(); ++i) {
    auto res = readEdge(graph, es[i]);

    EXPECT_TRUE(res.ok()) << fmt::format("error reading edge: {}\n",
                                         res.error());
  }
}

TEST(Pregel, test_conductor_setup) {
  auto const& settings = Settings{.iterations = 10, .resultField = "result"};
  auto g = createConductor<Data>(settings);

  // What even to test ;)
}

TEST(Pregel, test_worker_setup) {
  auto const& settings = Settings{.iterations = 10, .resultField = "result"};
  auto g = createWorker<Data>(settings);
}

auto main(int argc, char** argv) -> int {
  testing::InitGoogleTest();

  return RUN_ALL_TESTS();
}
