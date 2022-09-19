#include <iostream>
#include <vector>

#include <Inspection/VPack.h>
#include <Inspection/VPackSaveInspector.h>

#include <velocypack/vpack.h>

#include <fmt/core.h>
#include <fmt/format.h>

#include <Algorithm/Example.h>

using namespace arangodb::pregel::algorithm_sdk;
using namespace arangodb::pregel::algorithms::example;

auto test_topology_setup() {
  auto b = arangodb::velocypack::Parser::fromJson("{}");
  auto t = Topology();
  auto foo = t.readVertex_(b->slice());

  fmt::print("{:f}", foo);
}

auto test_conductor_setup() {
  auto c = Conductor(Settings{.iterations = 100});

  auto h = c.setup();

  while (c.continue_huh(h)) {
    h = c.step(h);
    fmt::print("{:f}", h);
  }
}

auto test_vertex_computation() { auto v = VertexComputation(); }

auto main(int argc, char** argv) -> int {
  fmt::print("Hello, world\n");

  test_topology_setup();
  test_conductor_setup();
  test_vertex_computation();

  return 0;
}
