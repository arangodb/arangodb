////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "JoinGraphTestHelper.h"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using namespace arangodb::aql;

namespace arangodb::tests::aql {
namespace {
class OptimizeJoinOrderTest : public testing::Test {
 protected:
  mocks::MockAqlServer server;

  OptimizeJoinOrderTest() {
    auto& vocbase = server.getSystemDatabase();
    for (auto const& name : {"c1", "c2", "c3"}) {
      auto json = velocypack::Parser::fromJson(std::string{R"({"name":")"} +
                                               name + "\"}");
      vocbase.createCollection(json->slice());
    }
  }

  std::shared_ptr<Query> prepare(std::string const& query) {
    return prepareJoinPlan(server, query);
  }
};
}  // namespace

TEST_F(OptimizeJoinOrderTest, linear_three_way_chain) {
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FOR c IN c3 FILTER b.z == c.w RETURN [a, b, c]");
  auto g = buildGraph(*q);

  EXPECT_EQ(g.nodes.size(), 3u);
  EXPECT_EQ(g.edges.size(), 2u);
  EXPECT_TRUE(g.residuals.empty());
  EXPECT_TRUE(g.hasJoin());
  EXPECT_EQ(g.connectedComponents().size(), 1u);
}

TEST_F(OptimizeJoinOrderTest, equijoin_edge_records_attribute_paths) {
  auto q = prepare("FOR a IN c1 FOR b IN c2 FILTER a.x == b.y RETURN [a, b]");
  auto g = buildGraph(*q);

  ASSERT_EQ(g.edges.size(), 1u);
  auto const& e = g.edges.front();
  ASSERT_EQ(e.fromAttributes.size(), 1u);
  ASSERT_EQ(e.toAttributes.size(), 1u);
  // one side is {"x"} and the other {"y"} (orientation depends on map order)
  auto const& fromPath = e.fromAttributes.front();
  auto const& toPath = e.toAttributes.front();
  ASSERT_EQ(fromPath.size(), 1u);
  ASSERT_EQ(toPath.size(), 1u);
  std::vector got{fromPath[0], toPath[0]};
  std::ranges::sort(got);
  EXPECT_EQ(got, (std::vector<std::string_view>{"x", "y"}));
}

TEST_F(OptimizeJoinOrderTest, constant_restriction_becomes_node_condition) {
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FILTER a.k == 'v' RETURN [a, b]");
  auto g = buildGraph(*q);

  EXPECT_EQ(g.nodes.size(), 2u);
  EXPECT_EQ(g.edges.size(), 1u);
  EXPECT_TRUE(g.residuals.empty());

  auto const* a = nodeByName(g, "a");
  ASSERT_NE(a, nullptr);
  ASSERT_EQ(a->conditions.size(), 1u);
  ASSERT_EQ(a->conditions.front().size(), 1u);
  EXPECT_EQ(a->conditions.front().front(), "k");
}

TEST_F(OptimizeJoinOrderTest, non_equijoin_predicate_becomes_residual) {
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FILTER a.p < b.q RETURN [a, b]");
  auto g = buildGraph(*q);

  EXPECT_EQ(g.edges.size(), 1u);
  EXPECT_FALSE(g.residuals.empty());
}

TEST_F(OptimizeJoinOrderTest, id_is_remapped_to_key) {
  auto q =
      prepare("FOR a IN c1 FOR b IN c2 FILTER a._id == b._id RETURN [a, b]");
  auto g = buildGraph(*q);

  ASSERT_EQ(g.edges.size(), 1u);
  auto const& e = g.edges.front();
  ASSERT_EQ(e.fromAttributes.size(), 1u);
  ASSERT_EQ(e.toAttributes.size(), 1u);
  EXPECT_EQ(e.fromAttributes.front(),
            (AttributePath{std::string_view{"_key"}}));
  EXPECT_EQ(e.toAttributes.front(), (AttributePath{std::string_view{"_key"}}));
}

TEST_F(OptimizeJoinOrderTest, disconnected_graph_has_two_components) {
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FOR c IN c3 FOR d IN c1 FILTER c.x == d.y RETURN [a, b, c, d]");
  auto g = buildGraph(*q);

  EXPECT_EQ(g.nodes.size(), 4u);
  EXPECT_EQ(g.edges.size(), 2u);
  EXPECT_EQ(g.connectedComponents().size(), 2u);
}

TEST_F(OptimizeJoinOrderTest, single_enumeration_has_no_join) {
  auto q = prepare("FOR a IN c1 FILTER a.x == 1 RETURN a");
  auto g = buildGraph(*q);

  EXPECT_EQ(g.nodes.size(), 1u);
  EXPECT_TRUE(g.edges.empty());
  EXPECT_FALSE(g.hasJoin());
  EXPECT_EQ(g.connectedComponents().size(), 1u);
}

TEST_F(OptimizeJoinOrderTest, separate_runs_produce_separate_graphs) {
  // The two joins are split by a SORT, which terminates the first run of
  // adjacent enumerations, so the walk produces one graph per run.
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "SORT a.x "
      "FOR c IN c3 FOR d IN c1 FILTER c.x == d.y RETURN [a, b, c, d]");
  auto graphs = buildAllGraphs(*q);

  ASSERT_EQ(graphs.size(), 2u);
  for (auto const& g : graphs) {
    EXPECT_EQ(g.nodes.size(), 2u);
    EXPECT_EQ(g.edges.size(), 1u);
    EXPECT_TRUE(g.hasJoin());
    EXPECT_EQ(g.connectedComponents().size(), 1u);
  }
}

}  // namespace arangodb::tests::aql
