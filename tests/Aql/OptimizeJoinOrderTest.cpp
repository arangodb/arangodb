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

#include "Aql/Optimizer/Rule/OptimizeJoinOrder/JoinOrderSearch.h"

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

  auto result = getBestOrderForComponent(g, components.front(), estimator);
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
  // Variable const*, and within a single process calling
  // getBestOrderForComponent twice on the same graph would see the same map
  // iteration order whether or not nodesInIdOrder sorts, so that could not have
  // caught the sort being removed. What this test does catch: id-ascending
  // order coinciding by chance with Variable-pointer order is exceedingly
  // unlikely, so if the sort in nodesInIdOrder is ever deleted, this is very
  // likely to fail.
  FakeCostEstimator estimator;

  auto result = getBestOrderForComponent(g, components.front(), estimator);
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

  auto result = getBestOrderForComponent(g, components.front(), estimator);
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

  getEstimateForOrder(g, estimator, {a, b, c});
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

  EXPECT_DOUBLE_EQ(getEstimateForOrder(g, estimator, {a, b}).cost, 7.0 + 13.0);
  EXPECT_DOUBLE_EQ(getEstimateForOrder(g, estimator, {b, a}).cost, 3.0 + 11.0);
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
  //
  // Neither component beats its own written order (each ties, so the margin
  // check declines). What fires is the resequencing guard: candidate cost 5
  // vs baseline 104, past 104/1.25 = 83.2. So this is a reorder with no
  // component's internal order moving -- the resequencing accept path.
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
  // cd is promoted ahead of ab: this is a resequencing-only reorder.
  EXPECT_EQ(namesOf(*chosen), (std::vector<std::string>{"c", "d", "a", "b"}));
}

TEST_F(OptimizeJoinOrderTest,
       defaulted_component_keeps_written_order_while_confident_reorders) {
  // Two independent joins: a-b confidently estimated, c-d defaulted. Only
  // c-d must decline.
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FOR c IN c3 FOR d IN c1 FILTER c.x == d.y RETURN [a, b, c, d]");
  auto g = buildGraph(*q);
  ASSERT_EQ(g.connectedComponents().size(), 2u);
  auto current = collectEnumerationOrder(firstEnumeration(q->plan()), nullptr);

  // a-b: seed(b)=1 + step(a)=1 = 2 against a written cost of seed(a)=100 +
  // step(b)=1 = 101 -- clears the margin (2 < 101/1.25 = 80.8), and neither
  // vertex is defaulted, so this component is accepted and flips to [b, a].
  //
  // c-d: same seed/step shape, so greedy would also want [d, c] at cost 2
  // against a written cost of 101 -- but both c and d are marked defaulted,
  // so this component must decline regardless of the cost numbers and keep
  // its written order [c, d].
  FakeCostEstimator estimator;
  estimator.seedCost = {{"a", 100.0}, {"b", 1.0}, {"c", 100.0}, {"d", 1.0}};
  estimator.stepCost = {{"a", 1.0}, {"b", 1.0}, {"c", 1.0}, {"d", 1.0}};
  estimator.defaultedVertices = {"c", "d"};

  auto chosen = chooseJoinOrder(g, estimator, current);
  ASSERT_TRUE(chosen.has_value());
  // a-b flips to [b, a]; c-d keeps its written [c, d]; both stay contiguous.
  EXPECT_EQ(namesOf(*chosen), (std::vector<std::string>{"b", "a", "c", "d"}));
}

TEST_F(OptimizeJoinOrderTest,
       marginal_component_keeps_written_order_while_another_reorders) {
  // Two independent joins: a-b clears the margin, c-d is confidently
  // estimated (no defaulted statistic) but its improvement is only ~10%,
  // well short of the required 20%. c-d must keep its written order even
  // though it is not defaulted -- the margin guard is independent of the
  // defaulted guard, and both are now per component.
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FOR c IN c3 FOR d IN c1 FILTER c.x == d.y RETURN [a, b, c, d]");
  auto g = buildGraph(*q);
  ASSERT_EQ(g.connectedComponents().size(), 2u);
  auto current = collectEnumerationOrder(firstEnumeration(q->plan()), nullptr);

  // a-b: seed(b)=1 + step(a)=1 = 2 against written seed(a)=100 + step(b)=1 =
  // 101 -- clears the margin (2 < 80.8), accepted, flips to [b, a].
  //
  // c-d: greedy is [d, c] at seed(d)=90 + step(c)=1 = 91, against a written
  // cost of seed(c)=100 + step(d)=1 = 101. 91 is cheaper than 101, but
  // 91 >= 101/1.25 = 80.8, so the margin does not clear -- c-d must keep its
  // written order [c, d].
  FakeCostEstimator estimator;
  estimator.seedCost = {{"a", 100.0}, {"b", 1.0}, {"c", 100.0}, {"d", 90.0}};
  estimator.stepCost = {{"a", 1.0}, {"b", 1.0}, {"c", 1.0}, {"d", 1.0}};

  auto chosen = chooseJoinOrder(g, estimator, current);
  ASSERT_TRUE(chosen.has_value());
  EXPECT_EQ(namesOf(*chosen), (std::vector<std::string>{"b", "a", "c", "d"}));
}

TEST_F(OptimizeJoinOrderTest,
       defaulted_statistics_block_resequencing_even_when_cost_favours_it) {
  // Two independent joins: a-b (confidently estimated, and internally
  // reordered) and c-d (statistics defaulted, so it correctly keeps its
  // written order [c, d] per the per-component guard). c-d is scripted to be
  // very cheap standalone, so the cheapest-concatenation candidate wants to
  // hoist it ahead of a-b, reversing the written component sequence
  // [a,b,c,d] -> [c,d,a,b]. Cost alone would even clear the margin for that
  // swap (3.2 vs 4.1/1.25=3.28) -- but c-d's defaulted flag propagates
  // through every replay it appears in, so both the candidate and the
  // written-sequence baseline come out `defaulted`, and the resequencing
  // guard must decline regardless of the cost numbers. The written
  // component sequence -- a-b before c-d -- must survive.
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FOR c IN c3 FOR d IN c1 FILTER c.x == d.y RETURN [a, b, c, d]");
  auto g = buildGraph(*q);
  ASSERT_EQ(g.connectedComponents().size(), 2u);
  auto current = collectEnumerationOrder(firstEnumeration(q->plan()), nullptr);

  // a-b: seed(b)=1 + step(a)=1 = 2 against a written cost of seed(a)=100 +
  // step(b)=1 = 101 -- clears the margin, accepted, flips to [b, a].
  //
  // c-d: seed(c)=0.1 + step(d)=0.1 = 0.2 standalone -- cheap enough that the
  // sequencing loop wants it first -- but c and d are both defaulted, so the
  // per-component guard keeps c-d's written order [c, d] regardless.
  FakeCostEstimator estimator;
  estimator.seedCost = {{"a", 100.0}, {"b", 1.0}, {"c", 0.1}, {"d", 1.0}};
  estimator.stepCost = {{"a", 1.0}, {"b", 1.0}, {"c", 1.0}, {"d", 0.1}};
  estimator.defaultedVertices = {"c", "d"};

  auto chosen = chooseJoinOrder(g, estimator, current);
  ASSERT_TRUE(chosen.has_value());
  // a-b flips to [b, a] (a genuine, confident reorder), but the component
  // sequence itself -- a-b before c-d -- must stay exactly as written.
  EXPECT_EQ(namesOf(*chosen), (std::vector<std::string>{"b", "a", "c", "d"}));
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

TEST_F(OptimizeJoinOrderTest, rewrite_needs_cost_below_eight_tenths) {
  // The written order [a, b] always costs 100 (seed(a) = 50, step(b) = 50).
  // The greedy order [b, a] costs seed(b) + 40: 79 rewrites, 80 and 81 do not.
  auto q = prepare("FOR a IN c1 FOR b IN c2 FILTER a.x == b.y RETURN [a, b]");
  auto g = buildGraph(*q);
  auto current = collectEnumerationOrder(firstEnumeration(q->plan()), nullptr);
  ASSERT_EQ(namesOf(current), (std::vector<std::string>{"a", "b"}));

  auto chosenWithSeedB = [&](double seedB) {
    FakeCostEstimator estimator;
    estimator.seedCost = {{"a", 50.0}, {"b", seedB}};
    estimator.stepCost = {{"a", 40.0}, {"b", 50.0}};
    return chooseJoinOrder(g, estimator, current);
  };

  auto chosen = chosenWithSeedB(39.0);
  ASSERT_TRUE(chosen.has_value());
  EXPECT_EQ(namesOf(*chosen), (std::vector<std::string>{"b", "a"}));
  EXPECT_FALSE(chosenWithSeedB(40.0).has_value()) << "80 is not below 80";
  EXPECT_FALSE(chosenWithSeedB(41.0).has_value());
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
  rewritePlan(*plan, current, desired);

  auto after = collectEnumerationOrder(plan->root()->getSingleton(), nullptr);
  EXPECT_EQ(namesOf(after), (std::vector<std::string>{"c", "b", "a"}));
}

TEST_F(OptimizeJoinOrderTest, rewrite_keeps_the_plan_valid) {
  auto q = prepare("FOR a IN c1 FOR b IN c2 FILTER a.x == b.y RETURN [a, b]");
  auto* plan = q->plan();
  auto* first = firstEnumeration(plan);
  auto current = collectEnumerationOrder(first, nullptr);

  std::vector<EnumerateCollectionNode*> desired{current[1], current[0]};
  rewritePlan(*plan, current, desired);

  // findVarUsage() only records which node sets and uses each variable; it
  // does not check that a used variable was already set. planRegisters() is
  // the check that actually enforces "set before use" -- it throws
  // MissingVariablesException from the register planner, and it needs
  // findVarUsage()'s recorded set/use relationships to run at all.
  plan->findVarUsage();
  EXPECT_NO_THROW(plan->planRegisters());
}

TEST_F(OptimizeJoinOrderTest, decline_is_not_reported_as_applied) {
  // The mock collections carry no indexes, so every statistic is defaulted
  // and the rule declines. A decline must not show up in explain's applied
  // rules.
  EXPECT_FALSE(
      assertRules(server.getSystemDatabase(),
                  "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y RETURN [a, b]",
                  {OptimizerRule::optimizeJoinOrder}, nullptr,
                  R"({"optimizer":{"rules":["+optimize-join-order",)"
                  R"("-interchange-adjacent-enumerations"]}})"));
}

TEST_F(OptimizeJoinOrderTest, interchange_still_runs_when_join_order_declines) {
  // No indexes in this fixture, so the rule reaches its decision and
  // declines; interchange, enabled by default, must then still run.
  EXPECT_TRUE(assertRules(
      server.getSystemDatabase(),
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y FOR c IN c3 RETURN 1",
      {OptimizerRule::interchangeAdjacentEnumerationsRule}, nullptr,
      R"({"optimizer":{"rules":["+optimize-join-order"]}})"))
      << "declining to reorder must not silently disable interchange too";
}

}  // namespace arangodb::tests::aql
