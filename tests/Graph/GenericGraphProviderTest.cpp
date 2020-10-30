////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "./MockGraph.h"
#include "./MockGraphProvider.h"

#include <unordered_set>

using namespace arangodb;
using namespace arangodb::tests;
using namespace arangodb::tests::graph;

namespace arangodb {
namespace tests {
namespace generic_graph_provider_test {

class GraphProviderTest : public ::testing::Test {
 protected:
  GraphProviderTest() {}
  ~GraphProviderTest() {}

  auto makeProvider(MockGraph const& graph) -> MockGraphProvider {
    return MockGraphProvider(graph);
  }
};

TEST_F(GraphProviderTest, no_results_if_graph_is_empty) {
  MockGraph empty{};
  
  auto testee = makeProvider(empty);
  auto start = testee.startVertex(0);
  auto res = testee.expand(start, 0);
  EXPECT_EQ(res.size(), 0);
}

TEST_F(GraphProviderTest, should_enumerate_a_single_edge) {
  MockGraph g{};
  g.addEdge(0, 1);
  
  auto testee = makeProvider(g);
  auto start = testee.startVertex(0);
  auto res = testee.expand(start, 0);
  ASSERT_EQ(res.size(), 1);
  auto const& f = res.front();
  EXPECT_EQ(f.vertex.data(), 1);
  EXPECT_EQ(f.previous, 0);
}

TEST_F(GraphProviderTest, should_enumerate_all_edges) {
  MockGraph g{};
  g.addEdge(0, 1);
  g.addEdge(0, 2);
  g.addEdge(0, 3);
  std::unordered_set<size_t> found{};
  
  auto testee = makeProvider(g);
  auto start = testee.startVertex(0);
  auto res = testee.expand(start, 0);
  ASSERT_EQ(res.size(), 3);
  for (auto const& f : res) {
    // All expand of the same previous
    EXPECT_EQ(f.previous, 0);
    auto const& v = f.vertex.data();
    // We can only range from 1 to 3
    EXPECT_GE(v, 1);
    EXPECT_LE(v, 3);
    // We need to find each exactly once
    auto const [_, didInsert] = found.emplace(v);
    EXPECT_TRUE(didInsert);
  }
}

}
}
}