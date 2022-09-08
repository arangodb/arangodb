#include <iostream>
#include <vector>

#include <Inspection/VPack.h>
#include <Inspection/VPackSaveInspector.h>

#include <velocypack/vpack.h>

#include <fmt/core.h>

#include <Algorithm/Algorithm.h>
#include <Algorithm/Conductor.h>
#include <Algorithm/PageRank.h>
#include <Algorithm/Worker.h>

using namespace arangodb::pregel::algorithm_sdk;
using namespace arangodb::pregel::algorithms::pagerank;

using Mailbox = std::vector<PageRankData::Message>;


auto test_vertex_computation() {
  auto r =
      PageRank(PageRankData::Settings{.epsilon = 0.001, .dampingFactor = 0.85});

  auto vertices =
      std::vector<PageRankData::VertexProperties>{{.pageRank = 0.0},
                                                  {.pageRank = 1.0},
                                                  {.pageRank = 2.0},
                                                  {.pageRank = 3.0}};
  // adjacency list
  auto edges = std::vector<std::vector<size_t>>{{0, 1}, {1, 2}, {0, 2}};
  auto inbxs = std::make_unique<std::vector<Mailbox>>>();
  auto obxs = std::make_unique<std::vector<Mailbox>>>();

  auto global = r.conductorSetup();

  auto oneActive = bool{false};
  do {
    for (auto& v : vertices) {
      oneActive |= r.vertexStep(global, v);
    }
    r.conductorStep(global);
  } while(oneActive);
}

auto main(int argc, char** argv) -> int {
  fmt::print("Hello, world\n");

  test_vertex_computation();

  return 0;
}
