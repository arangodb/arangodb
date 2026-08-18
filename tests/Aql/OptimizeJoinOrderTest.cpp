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

TEST_F(OptimizeJoinOrderTest, single_variable_residual_attaches_to_node) {
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FILTER a.p < 5 RETURN [a, b]");
  auto g = buildGraph(*q);

  EXPECT_TRUE(g.residuals.empty()) << "should have been attached to node a";
  auto* a = nodeByName(g, "a");
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->residuals.size(), 1u);
  auto* b = nodeByName(g, "b");
  ASSERT_NE(b, nullptr);
  EXPECT_TRUE(b->residuals.empty());
}

TEST_F(OptimizeJoinOrderTest, two_variable_residual_stays_graph_level) {
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FILTER a.p < b.q RETURN [a, b]");
  auto g = buildGraph(*q);

  EXPECT_EQ(g.residuals.size(), 1u);
  EXPECT_TRUE(nodeByName(g, "a")->residuals.empty());
  EXPECT_TRUE(nodeByName(g, "b")->residuals.empty());
}

TEST_F(OptimizeJoinOrderTest,
       residual_without_graph_variable_stays_graph_level) {
  // NOOPT() is load-bearing: Ast::optimizeBinaryOperatorRelational
  // constant-folds a comparison whose both sides are constant, so a literal `1
  // < 2` collapses to `true` during AST optimization and never reaches
  // addResidual at all. NOOPT keeps the left side non-constant so the predicate
  // survives as a real residual that happens to reference no graph variable. Do
  // not "simplify" this back to `1 < 2`.
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FILTER NOOPT(1) < 2 RETURN [a, b]");
  auto g = buildGraph(*q);

  EXPECT_EQ(g.residuals.size(), 1u);
  EXPECT_TRUE(nodeByName(g, "a")->residuals.empty());
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

namespace {
// Reads an order back as variable names, so expectations are readable.
std::vector<std::string> namesOf(
    std::vector<EnumerateCollectionNode*> const& order) {
  std::vector<std::string> names;
  names.reserve(order.size());
  for (auto const* node : order) {
    names.emplace_back(node->outVariable()->name);
  }
  return names;
}
}  // namespace

TEST_F(OptimizeJoinOrderTest, greedy_starts_at_the_cheapest_vertex) {
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FOR c IN c3 FILTER b.z == c.w RETURN [a, b, c]");
  auto g = buildGraph(*q);
  auto components = g.connectedComponents();
  ASSERT_EQ(components.size(), 1u);

  FakeCostEstimator estimator;
  estimator.seedCost = {{"a", 100.0}, {"b", 1.0}, {"c", 100.0}};
  estimator.stepCost = {{"a", 1.0}, {"b", 1.0}, {"c", 1.0}};

  auto result = orderComponent(g, components.front(), estimator);
  EXPECT_EQ(namesOf(result.order), (std::vector<std::string>{"b", "a", "c"}))
      << "cheapest seed is b; a and c then tie and break on node id";
}

TEST_F(OptimizeJoinOrderTest, greedy_can_start_in_the_middle_of_a_chain) {
  // a - b - c: starting at b then taking c is reachable, i.e. the search is
  // not restricted to the ends of the chain.
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FOR c IN c3 FILTER b.z == c.w RETURN [a, b, c]");
  auto g = buildGraph(*q);
  auto components = g.connectedComponents();

  FakeCostEstimator estimator;
  estimator.seedCost = {{"a", 10.0}, {"b", 1.0}, {"c", 10.0}};
  estimator.stepCost = {{"a", 50.0}, {"b", 1.0}, {"c", 1.0}};

  auto result = orderComponent(g, components.front(), estimator);
  EXPECT_EQ(namesOf(result.order), (std::vector<std::string>{"b", "c", "a"}));
}

TEST_F(OptimizeJoinOrderTest, greedy_only_extends_along_edges) {
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FOR c IN c3 FILTER b.z == c.w RETURN [a, b, c]");
  auto g = buildGraph(*q);
  auto components = g.connectedComponents();

  // c is by far the cheapest step, but it is not adjacent to a, so starting
  // from a the greedy must take b first regardless.
  FakeCostEstimator estimator;
  estimator.seedCost = {{"a", 1.0}, {"b", 100.0}, {"c", 100.0}};
  estimator.stepCost = {{"a", 1.0}, {"b", 10.0}, {"c", 0.1}};

  auto result = orderComponent(g, components.front(), estimator);
  EXPECT_EQ(namesOf(result.order), (std::vector<std::string>{"a", "b", "c"}));
}

TEST_F(OptimizeJoinOrderTest, greedy_order_is_deterministic_across_runs) {
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FOR c IN c3 FILTER b.z == c.w RETURN [a, b, c]");
  auto g = buildGraph(*q);
  auto components = g.connectedComponents();

  // every cost identical, so the outcome is decided entirely by the tie-break
  FakeCostEstimator estimator;

  auto first = orderComponent(g, components.front(), estimator);
  auto second = orderComponent(g, components.front(), estimator);
  EXPECT_EQ(namesOf(first.order), namesOf(second.order));
  EXPECT_EQ(first.order.size(), 3u);
}

TEST_F(OptimizeJoinOrderTest, estimate_order_replays_a_complete_order) {
  auto q = prepare("FOR a IN c1 FOR b IN c2 FILTER a.x == b.y RETURN [a, b]");
  auto g = buildGraph(*q);

  FakeCostEstimator estimator;
  estimator.seedCost = {{"a", 7.0}, {"b", 3.0}};
  estimator.stepCost = {{"a", 11.0}, {"b", 13.0}};

  auto* a = nodeByName(g, "a")->executionNode;
  auto* b = nodeByName(g, "b")->executionNode;

  EXPECT_DOUBLE_EQ(estimateOrder(g, estimator, {a, b}).cost, 7.0 + 13.0);
  EXPECT_DOUBLE_EQ(estimateOrder(g, estimator, {b, a}).cost, 3.0 + 11.0);
}

TEST_F(OptimizeJoinOrderTest, estimate_order_charges_a_cross_product) {
  auto q = prepare("FOR a IN c1 FOR b IN c2 RETURN [a, b]");
  auto g = buildGraph(*q);
  ASSERT_TRUE(g.edges.empty());

  FakeCostEstimator estimator;
  estimator.seedCost = {{"a", 1.0}, {"b", 1.0}};
  estimator.stepCost = {{"a", 5.0}, {"b", 5.0}};

  auto* a = nodeByName(g, "a")->executionNode;
  auto* b = nodeByName(g, "b")->executionNode;
  // the fake doubles a cross-product step
  EXPECT_DOUBLE_EQ(estimateOrder(g, estimator, {a, b}).cost, 1.0 + 10.0);
}

}  // namespace arangodb::tests::aql
