#include <iostream>

#include <fmt/core.h>

#include <Algorithm/Algorithm.h>
#include <Algorithm/Conductor.h>
#include <Algorithm/PageRank.h>
#include <Algorithm/Worker.h>

using namespace arangodb::pregel::algorithm_sdk;
using namespace arangodb::pregel::algorithms::pagerank;

auto main(int argc, char** argv) -> int {
  fmt::print("Hello, world\n");

  auto conductor = makeConductor<PageRank>();
  auto worker = makeWorker<PageRank>();

  return 0;
}
