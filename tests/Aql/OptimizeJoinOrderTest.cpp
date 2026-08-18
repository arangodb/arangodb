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

#include "Aql/OptimizerRule.h"

#include <algorithm>
#include <cstdlib>
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

  // The scripted costs must make the mid-chain start actually WIN, or the test
  // cannot demonstrate what it claims. Starting at b costs 1 + 1 + 5 = 7, while
  // either end costs 100 + 1 + 5 = 106 or 100 + 1 + 1 = 102. And from b, c is
  // cheaper to absorb than a (1 vs 5), which fixes the rest of the order.
  FakeCostEstimator estimator;
  estimator.seedCost = {{"a", 100.0}, {"b", 1.0}, {"c", 100.0}};
  estimator.stepCost = {{"a", 5.0}, {"b", 1.0}, {"c", 1.0}};

  auto result = orderComponent(g, components.front(), estimator);
  EXPECT_EQ(namesOf(result.order), (std::vector<std::string>{"b", "c", "a"}));
  EXPECT_DOUBLE_EQ(result.estimate.cost, 7.0);
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

TEST_F(OptimizeJoinOrderTest, equal_costs_break_ties_by_node_id) {
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FOR c IN c3 FILTER b.z == c.w RETURN [a, b, c]");
  auto g = buildGraph(*q);
  auto components = g.connectedComponents();

  // Every cost is identical, so the outcome is decided entirely by the
  // tie-break rule: ascending ExecutionNode::id(). This is necessary but not
  // sufficient to prove run-to-run stability -- JoinGraph::nodes is keyed by
  // Variable const*, and within a single process calling orderComponent
  // twice on the same graph would see the same map iteration order whether
  // or not nodesInIdOrder sorts, so that could not have caught the sort
  // being removed. What this test does catch: id-ascending order coinciding
  // by chance with Variable-pointer order is exceedingly unlikely, so if the
  // sort in nodesInIdOrder is ever deleted, this is very likely to fail.
  FakeCostEstimator estimator;

  auto result = orderComponent(g, components.front(), estimator);
  EXPECT_EQ(namesOf(result.order), (std::vector<std::string>{"a", "b", "c"}));
}

TEST_F(OptimizeJoinOrderTest, multi_start_beats_picking_the_cheapest_seed) {
  // a - b - c, same shape as the other chain tests.
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FOR c IN c3 FILTER b.z == c.w RETURN [a, b, c]");
  auto g = buildGraph(*q);
  auto components = g.connectedComponents();

  // a has by far the cheapest SEED cost, so a shortcut that starts at
  // argmin(seedCost) and then just descends once would start at a. Doing so
  // forces the chain order a, b, c, which pays b's enormous step cost. Only
  // trying every start and costing the *complete* order finds that starting
  // at b instead -- paying a slightly higher seed but never paying b's own
  // step cost -- is actually cheapest overall (11, vs. 1006 either other
  // way): seed(b)=5 + step(a)=1 + step(c)=5 = 11.
  FakeCostEstimator estimator;
  estimator.seedCost = {{"a", 1.0}, {"b", 5.0}, {"c", 5.0}};
  estimator.stepCost = {{"a", 1.0}, {"b", 1000.0}, {"c", 5.0}};

  auto result = orderComponent(g, components.front(), estimator);
  EXPECT_EQ(namesOf(result.order), (std::vector<std::string>{"b", "a", "c"}));
  EXPECT_DOUBLE_EQ(result.estimate.cost, 11.0);
}

TEST_F(OptimizeJoinOrderTest, estimate_order_passes_every_connecting_edge) {
  // a cycle: a-b, b-c, a-c. Replaying the fixed order a, b, c means c joins a
  // prefix it is doubly constrained against, so extend must be offered both
  // edges, not just one.
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FOR c IN c3 "
      "FILTER a.x == b.y FILTER b.z == c.w FILTER a.q == c.r "
      "RETURN [a, b, c]");
  auto g = buildGraph(*q);
  ASSERT_EQ(g.edges.size(), 3u);

  FakeCostEstimator estimator;
  auto* a = nodeByName(g, "a")->executionNode;
  auto* b = nodeByName(g, "b")->executionNode;
  auto* c = nodeByName(g, "c")->executionNode;

  estimateOrder(g, estimator, {a, b, c});
  EXPECT_EQ(estimator.edgesSeen.at("c"), 2u);
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

TEST_F(OptimizeJoinOrderTest, chooses_a_cheaper_order_when_the_win_is_large) {
  auto q = prepare("FOR a IN c1 FOR b IN c2 FILTER a.x == b.y RETURN [a, b]");
  auto g = buildGraph(*q);
  auto current = collectEnumerationOrder(firstEnumeration(q->plan()), nullptr);
  ASSERT_EQ(namesOf(current), (std::vector<std::string>{"a", "b"}));

  FakeCostEstimator estimator;
  estimator.seedCost = {{"a", 1000.0}, {"b", 1.0}};
  estimator.stepCost = {{"a", 1.0}, {"b", 1.0}};

  auto chosen = chooseJoinOrder(g, estimator, current);
  ASSERT_TRUE(chosen.has_value());
  EXPECT_EQ(namesOf(*chosen), (std::vector<std::string>{"b", "a"}));
}

TEST_F(OptimizeJoinOrderTest,
       keeps_the_written_order_when_the_win_is_marginal) {
  auto q = prepare("FOR a IN c1 FOR b IN c2 FILTER a.x == b.y RETURN [a, b]");
  auto g = buildGraph(*q);
  auto current = collectEnumerationOrder(firstEnumeration(q->plan()), nullptr);

  // b-first is cheaper, but only by 10%, well inside estimator error
  FakeCostEstimator estimator;
  estimator.seedCost = {{"a", 100.0}, {"b", 90.0}};
  estimator.stepCost = {{"a", 1.0}, {"b", 1.0}};

  EXPECT_FALSE(chooseJoinOrder(g, estimator, current).has_value());
}

TEST_F(OptimizeJoinOrderTest, identical_order_is_a_no_op) {
  // All costs are uniform, so orderComponent's and the sequencing loop's
  // strict "<" comparisons converge on the written order: chosen ==
  // currentOrder, and the early-equality guard short-circuits before the
  // margin comparison is even reached. This pins that short-circuit, not the
  // ">=" vs ">" boundary at the margin -- see
  // exactly_meeting_the_margin_does_not_rewrite for that.
  auto q = prepare("FOR a IN c1 FOR b IN c2 FILTER a.x == b.y RETURN [a, b]");
  auto g = buildGraph(*q);
  auto current = collectEnumerationOrder(firstEnumeration(q->plan()), nullptr);

  FakeCostEstimator estimator;  // all costs identical

  EXPECT_FALSE(chooseJoinOrder(g, estimator, current).has_value())
      << "an identical order must be a no-op, not a coin flip on node id";
}

TEST_F(OptimizeJoinOrderTest, bails_out_when_any_statistic_was_defaulted) {
  auto q = prepare("FOR a IN c1 FOR b IN c2 FILTER a.x == b.y RETURN [a, b]");
  auto g = buildGraph(*q);
  auto current = collectEnumerationOrder(firstEnumeration(q->plan()), nullptr);

  FakeCostEstimator estimator;
  estimator.seedCost = {{"a", 1000.0}, {"b", 1.0}};
  estimator.stepCost = {{"a", 1.0}, {"b", 1.0}};
  estimator.defaulted = true;  // nothing was index-backed

  EXPECT_FALSE(chooseJoinOrder(g, estimator, current).has_value())
      << "rewriting on guessed statistics measured worse than not rewriting";
}

TEST_F(OptimizeJoinOrderTest, keeps_components_contiguous) {
  // two independent joins: a-b and c-d. The chosen order must finish one
  // component before starting the other, never interleave them.
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FOR c IN c3 FOR d IN c1 FILTER c.x == d.y RETURN [a, b, c, d]");
  auto g = buildGraph(*q);
  ASSERT_EQ(g.connectedComponents().size(), 2u);
  auto current = collectEnumerationOrder(firstEnumeration(q->plan()), nullptr);

  FakeCostEstimator estimator;
  estimator.seedCost = {{"a", 100.0}, {"b", 100.0}, {"c", 1.0}, {"d", 100.0}};
  estimator.stepCost = {{"a", 1.0}, {"b", 1.0}, {"c", 1.0}, {"d", 1.0}};

  auto chosen = chooseJoinOrder(g, estimator, current);
  ASSERT_TRUE(chosen.has_value());
  auto names = namesOf(*chosen);
  ASSERT_EQ(names.size(), 4u);

  auto positionOf = [&](std::string const& name) {
    return std::distance(names.begin(),
                         std::find(names.begin(), names.end(), name));
  };
  // {a,b} occupy adjacent positions, and so do {c,d}
  EXPECT_EQ(std::abs(positionOf("a") - positionOf("b")), 1);
  EXPECT_EQ(std::abs(positionOf("c") - positionOf("d")), 1);
}

TEST_F(OptimizeJoinOrderTest, component_sequencing_ties_break_by_node_id) {
  // Two disconnected components, each costed so the two components tie
  // exactly at the top level: whichever one is sequenced first is then
  // decided entirely by the tie-break on ExecutionNode::id(), because
  // connectedComponents() iterates a std::map<Variable const*, Node> and is
  // therefore address-ordered -- the same hazard nodesInIdOrder guards
  // against within a component (see equal_costs_break_ties_by_node_id
  // above), but here at the level of which *component* goes first. As
  // there, this is necessary but not sufficient to prove run-to-run
  // stability -- id-ascending order coinciding by chance with the address
  // order in this one process is not ruled out -- but it is exactly what
  // would very likely fail if the sort in chooseJoinOrder were ever
  // deleted.
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FOR c IN c3 FOR d IN c1 FILTER c.x == d.y RETURN [a, b, c, d]");
  auto g = buildGraph(*q);
  ASSERT_EQ(g.connectedComponents().size(), 2u);
  auto current = collectEnumerationOrder(firstEnumeration(q->plan()), nullptr);

  FakeCostEstimator estimator;
  // Within each component, the cheapest start is the *second* written
  // variable (b, d), not the first (a, c) -- so the chosen order differs
  // from the written one and chooseJoinOrder actually returns a rewrite
  // instead of declining as a no-op. Both components are costed
  // identically once oriented that way: seed(b) + step(a) == seed(d) +
  // step(c) == 2, a tie.
  estimator.seedCost = {{"a", 100.0}, {"b", 1.0}, {"c", 100.0}, {"d", 1.0}};
  estimator.stepCost = {{"a", 1.0}, {"b", 100.0}, {"c", 1.0}, {"d", 100.0}};

  auto chosen = chooseJoinOrder(g, estimator, current);
  ASSERT_TRUE(chosen.has_value());
  // {a,b}'s component is oriented [b, a]; {c,d}'s is oriented [d, c]. Tied
  // on cost, the component starting at the lower id (b, id(b) < id(d))
  // must be sequenced first.
  EXPECT_EQ(namesOf(*chosen), (std::vector<std::string>{"b", "a", "d", "c"}));
}

TEST_F(OptimizeJoinOrderTest,
       chosen_order_is_a_permutation_of_the_current_one) {
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FOR c IN c3 FILTER b.z == c.w RETURN [a, b, c]");
  auto g = buildGraph(*q);
  auto current = collectEnumerationOrder(firstEnumeration(q->plan()), nullptr);

  FakeCostEstimator estimator;
  estimator.seedCost = {{"a", 1000.0}, {"b", 1000.0}, {"c", 1.0}};
  estimator.stepCost = {{"a", 1.0}, {"b", 1.0}, {"c", 1.0}};

  auto chosen = chooseJoinOrder(g, estimator, current);
  ASSERT_TRUE(chosen.has_value());

  auto sortedCurrent = namesOf(current);
  auto sortedChosen = namesOf(*chosen);
  std::sort(sortedCurrent.begin(), sortedCurrent.end());
  std::sort(sortedChosen.begin(), sortedChosen.end());
  EXPECT_EQ(sortedCurrent, sortedChosen)
      << "every enumeration must appear exactly once, or the rewrite deletes a "
         "FOR loop";
}

TEST_F(OptimizeJoinOrderTest, skips_graphs_above_the_enumeration_cap) {
  // 17 enumerations exceeds kMaxEnumerationsToReorder. Script costs that
  // would clearly win a rewrite if the cap were not checked: v0's seed is
  // expensive (1000) while every other seed/step defaults to 1.0, so any
  // order starting elsewhere costs about 17 against the written order's
  // 1016 -- nowhere near the 20% margin -- so without the cap this would
  // rewrite. EXPECT_FALSE is therefore attributable to the cap alone.
  std::string query = "FOR v0 IN c1 ";
  for (int i = 1; i < 17; ++i) {
    query += "FOR v" + std::to_string(i) + " IN c1 FILTER v" +
             std::to_string(i - 1) + ".x == v" + std::to_string(i) + ".y ";
  }
  query += "RETURN 1";

  auto q = prepare(query);
  auto g = buildGraph(*q);
  ASSERT_GT(g.nodes.size(), kMaxEnumerationsToReorder);
  auto current = collectEnumerationOrder(firstEnumeration(q->plan()), nullptr);

  FakeCostEstimator estimator;
  estimator.seedCost = {{"v0", 1000.0}};

  EXPECT_FALSE(chooseJoinOrder(g, estimator, current).has_value());
}

TEST_F(OptimizeJoinOrderTest, improvement_margin_requires_twenty_percent) {
  // kImprovementMargin = 0.25 is applied as `chosen >= current / 1.25`, i.e.
  // the chosen order must cost at most 0.8 * current to trigger a rewrite --
  // a 20% reduction, not 25%. Pin that boundary exactly: current always costs
  // 100 (seed(a)=50, step(b)=50), and the chosen order [b, a] costs 79 in one
  // case (just inside 0.8 * 100 = 80: rewrite) and 81 in the other (just
  // outside: no rewrite).
  auto q = prepare("FOR a IN c1 FOR b IN c2 FILTER a.x == b.y RETURN [a, b]");
  auto g = buildGraph(*q);
  auto current = collectEnumerationOrder(firstEnumeration(q->plan()), nullptr);
  ASSERT_EQ(namesOf(current), (std::vector<std::string>{"a", "b"}));

  {
    // chosen cost = seed(b) + step(a) = 39 + 40 = 79 < 80.
    FakeCostEstimator estimator;
    estimator.seedCost = {{"a", 50.0}, {"b", 39.0}};
    estimator.stepCost = {{"a", 40.0}, {"b", 50.0}};

    auto chosen = chooseJoinOrder(g, estimator, current);
    ASSERT_TRUE(chosen.has_value())
        << "79 is inside 0.8 * 100 = 80, so the rewrite should fire";
    EXPECT_EQ(namesOf(*chosen), (std::vector<std::string>{"b", "a"}));
  }

  {
    // chosen cost = seed(b) + step(a) = 41 + 40 = 81 >= 80.
    FakeCostEstimator estimator;
    estimator.seedCost = {{"a", 50.0}, {"b", 41.0}};
    estimator.stepCost = {{"a", 40.0}, {"b", 50.0}};

    EXPECT_FALSE(chooseJoinOrder(g, estimator, current).has_value())
        << "81 is outside 0.8 * 100 = 80, so the rewrite should not fire";
  }
}

TEST_F(OptimizeJoinOrderTest, exactly_meeting_the_margin_does_not_rewrite) {
  // Unlike identical_order_is_a_no_op, chosen != currentOrder here, so this
  // test actually reaches the margin comparison. current costs exactly 100
  // (seed(a)=50, step(b)=50); chosen ([b, a]) costs exactly 80
  // (seed(b)=40, step(a)=40) -- precisely current / (1 + kImprovementMargin)
  // = 100 / 1.25 = 80. The guard is `chosen >= current / (1 + margin)`, so
  // 80 >= 80 declines; a ">" comparison would rewrite here instead. This is
  // the boundary the measured 7.84x-worse tie makes consequential.
  auto q = prepare("FOR a IN c1 FOR b IN c2 FILTER a.x == b.y RETURN [a, b]");
  auto g = buildGraph(*q);
  auto current = collectEnumerationOrder(firstEnumeration(q->plan()), nullptr);
  ASSERT_EQ(namesOf(current), (std::vector<std::string>{"a", "b"}));

  FakeCostEstimator estimator;
  estimator.seedCost = {{"a", 50.0}, {"b", 40.0}};
  estimator.stepCost = {{"a", 40.0}, {"b", 50.0}};

  EXPECT_FALSE(chooseJoinOrder(g, estimator, current).has_value())
      << "80 exactly meets current / 1.25 = 80, which must decline, not "
         "rewrite";
}

TEST_F(OptimizeJoinOrderTest, collect_enumeration_order_stops_at_run_boundary) {
  // Two joins separated by a SORT (same query shape as
  // separate_runs_produce_separate_graphs): the first run is a, b; the
  // second is c, d. collectEnumerationOrder must stop at `next` -- the node
  // that terminated the first run -- rather than continuing into the run
  // that follows it, or a rewrite of the first run would silently pull in
  // the second run's FOR loops. Every other test passes nullptr for `next`,
  // which cannot exercise this.
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "SORT a.x "
      "FOR c IN c3 FOR d IN c1 FILTER c.x == d.y RETURN [a, b, c, d]");
  auto* plan = q->plan();
  auto* first = firstEnumeration(plan);
  ASSERT_NE(first, nullptr);

  ExecutionNode* next = nullptr;
  auto g = buildJoinGraph(plan, first, next);
  ASSERT_NE(next, nullptr) << "the SORT should have terminated the first run";
  ASSERT_EQ(g.nodes.size(), 2u);

  auto order = collectEnumerationOrder(first, next);
  EXPECT_EQ(namesOf(order), (std::vector<std::string>{"a", "b"}))
      << "must stop at the run boundary, not continue into the next run's "
         "enumerations";
}

TEST_F(OptimizeJoinOrderTest, rewrite_splices_the_chosen_order) {
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FOR c IN c3 FILTER b.z == c.w RETURN [a, b, c]");
  auto* plan = q->plan();
  auto* first = firstEnumeration(plan);
  auto current = collectEnumerationOrder(first, nullptr);
  ASSERT_EQ(namesOf(current), (std::vector<std::string>{"a", "b", "c"}));

  std::vector<EnumerateCollectionNode*> desired{current[2], current[1],
                                                current[0]};
  rewriteJoinGraph(*plan, first, nullptr, desired);

  auto after = collectEnumerationOrder(plan->root()->getSingleton(), nullptr);
  EXPECT_EQ(namesOf(after), (std::vector<std::string>{"c", "b", "a"}));
}

TEST_F(OptimizeJoinOrderTest, rewrite_keeps_the_plan_valid) {
  auto q = prepare("FOR a IN c1 FOR b IN c2 FILTER a.x == b.y RETURN [a, b]");
  auto* plan = q->plan();
  auto* first = firstEnumeration(plan);
  auto current = collectEnumerationOrder(first, nullptr);

  std::vector<EnumerateCollectionNode*> desired{current[1], current[0]};
  rewriteJoinGraph(*plan, first, nullptr, desired);

  // findVarUsage() only records which node sets and uses each variable; it
  // does not check that a used variable was already set. planRegisters() is
  // the check that actually enforces "set before use" -- it throws
  // MissingVariablesException from the register planner, and it needs
  // findVarUsage()'s recorded set/use relationships to run at all.
  plan->findVarUsage();
  EXPECT_NO_THROW(plan->planRegisters());
}

TEST_F(OptimizeJoinOrderTest, non_deterministic_calculation_is_flagged) {
  auto q = prepare(
      "FOR a IN c1 LET r = RAND() FILTER a.x > r "
      "FOR b IN c2 FILTER a.y == b.z RETURN [a, b]");
  auto g = buildGraph(*q);
  EXPECT_TRUE(g.hasNonDeterministicCalculation);
}

TEST_F(OptimizeJoinOrderTest, deterministic_run_is_not_flagged) {
  auto q = prepare("FOR a IN c1 FOR b IN c2 FILTER a.x == b.y RETURN [a, b]");
  auto g = buildGraph(*q);
  EXPECT_FALSE(g.hasNonDeterministicCalculation);
}

TEST_F(OptimizeJoinOrderTest, decline_is_not_reported_as_applied) {
  // The mock collections carry no indexes, so every statistic is defaulted and
  // the rule declines. Declining must not be reported as applied, or every join
  // query needlessly re-triggers the downstream rules. -interchange-adjacent-
  // enumerations isolates the subject, as in the neighbouring test.
  EXPECT_FALSE(
      assertRules(server.getSystemDatabase(),
                  "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y RETURN [a, b]",
                  {OptimizerRule::optimizeJoinOrder}, nullptr,
                  R"({"optimizer":{"rules":["+optimize-join-order",)"
                  R"("-interchange-adjacent-enumerations"]}})"));
}

TEST_F(OptimizeJoinOrderTest, rule_leaves_the_plan_alone_without_statistics) {
  // The mock collections carry no secondary indexes, so every distinct lookup
  // is defaulted and the rule must decline. This is the documented behaviour,
  // not a limitation of the test.
  auto ctx = std::make_shared<transaction::StandaloneContext>(
      server.getSystemDatabase(), transaction::OperationOriginTestCase{});
  // interchange-adjacent-enumerations is enabled by default and also
  // permutes adjacent FOR loops; with it left on, it -- not
  // optimize-join-order -- could be the one deciding the enumeration order
  // this test checks. It must be off so the test isolates its actual
  // subject. The two rules are made mutually exclusive in a later task.
  auto options = velocypack::Parser::fromJson(
      R"({"optimizer":{"rules":["+optimize-join-order",)"
      R"("-interchange-adjacent-enumerations"]}})");
  auto query = Query::create(
      std::move(ctx),
      QueryString(std::string{"FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
                              "RETURN [a, b]"}),
      nullptr, QueryOptions(options->slice()));
  waitForAsync(query->prepareQuery());

  auto after =
      collectEnumerationOrder(query->plan()->root()->getSingleton(), nullptr);
  EXPECT_EQ(namesOf(after), (std::vector<std::string>{"a", "b"}));
}

TEST_F(OptimizeJoinOrderTest, interchange_yields_when_join_order_is_enabled) {
  // With optimize-join-order enabled, interchange-adjacent-enumerations must
  // not fan out: reordering is this rule's job, and the n! candidates would
  // otherwise be discriminated by the generic cost estimate.
  std::string const query = "FOR a IN c1 FOR b IN c2 FOR c IN c3 RETURN 1";

  // assertRules returns true when every listed rule appears in the explain
  // output's applied-rules list.
  EXPECT_TRUE(assertRules(
      server.getSystemDatabase(), query,
      {OptimizerRule::interchangeAdjacentEnumerationsRule}, nullptr,
      R"({"optimizer":{"rules":["+interchange-adjacent-enumerations"]}})"))
      << "interchange should fire when it is the only reordering rule on";

  EXPECT_FALSE(assertRules(
      server.getSystemDatabase(), query,
      {OptimizerRule::interchangeAdjacentEnumerationsRule}, nullptr,
      R"({"optimizer":{"rules":["+interchange-adjacent-enumerations",)"
      R"("+optimize-join-order"]}})"))
      << "interchange must yield to cost-based reordering";

  // The realistic configuration: the user enables cost-based reordering and
  // leaves interchange at its default-enabled state. Interchange must still
  // yield, without needing to be named explicitly.
  EXPECT_FALSE(
      assertRules(server.getSystemDatabase(), query,
                  {OptimizerRule::interchangeAdjacentEnumerationsRule}, nullptr,
                  R"({"optimizer":{"rules":["+optimize-join-order"]}})"));
}

}  // namespace arangodb::tests::aql
