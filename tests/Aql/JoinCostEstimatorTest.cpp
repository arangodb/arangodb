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
///
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Aql/Optimizer/Rule/OptimizeJoinOrder/JoinCostEstimator.h"
#include "Aql/Optimizer/Rule/OptimizeJoinOrder/SystemRCostEstimator.h"

#include "JoinGraphTestHelper.h"

#include <array>
#include <cmath>
#include <limits>
#include <memory>

using namespace arangodb::aql;

namespace arangodb::tests::aql {

TEST(JoinCostFunctionsTest, probe_cost_is_rows_times_log2_of_collection) {
  EXPECT_DOUBLE_EQ(probeCost(100.0, 1000.0), 100.0 * std::log2(1000.0));
  EXPECT_DOUBLE_EQ(probeCost(1000.0, 100.0), 1000.0 * std::log2(100.0));
}

TEST(JoinCostFunctionsTest, probe_cost_per_row_is_floored_at_one) {
  // log2(1) == 0 would make a join into a one-document collection free.
  EXPECT_DOUBLE_EQ(probeCost(50.0, 1.0), 50.0);
  EXPECT_DOUBLE_EQ(probeCost(50.0, 0.0), 50.0);
}

TEST(JoinCostFunctionsTest, scan_cost_is_rows_times_collection) {
  EXPECT_DOUBLE_EQ(scanCost(100.0, 1000.0), 100000.0);
  // an empty collection still costs one touch per outer row
  EXPECT_DOUBLE_EQ(scanCost(100.0, 0.0), 100.0);
}

TEST(JoinCostFunctionsTest, probing_beats_scanning_the_larger_side) {
  // scan the small collection, probe the large one
  double const scanSmallProbeLarge = 100.0 + probeCost(100.0, 1000.0);
  double const scanLargeProbeSmall = 1000.0 + probeCost(1000.0, 100.0);
  EXPECT_LT(scanSmallProbeLarge, scanLargeProbeSmall);
}

TEST(JoinCostFunctionsTest, estimates_clamp_to_a_finite_ceiling) {
  EXPECT_DOUBLE_EQ(clampEstimate(std::numeric_limits<double>::infinity()),
                   kMaxEstimate);
  EXPECT_DOUBLE_EQ(clampEstimate(std::nan("")), kMaxEstimate);
  EXPECT_DOUBLE_EQ(clampEstimate(1e300), kMaxEstimate);
  EXPECT_DOUBLE_EQ(clampEstimate(-5.0), 0.0);
  EXPECT_DOUBLE_EQ(clampEstimate(42.0), 42.0);
  EXPECT_DOUBLE_EQ(scanCost(1e200, 1e200), kMaxEstimate);
}

TEST(JoinCostFunctionsTest, probe_cost_guards_log2_against_a_nonpositive_size) {
  // The inner max(collectionSize, 1.0) keeps log2's argument >= 1. Without it a
  // negative size yields log2(negative) == NaN, and std::max(NaN, 1.0) returns
  // NaN rather than the floor, so the NaN would propagate into the cost.
  // clampEstimate would catch it downstream, but the guard is what keeps the
  // per-row cost meaningful in the first place.
  EXPECT_DOUBLE_EQ(probeCost(10.0, -5.0), 10.0);
  EXPECT_TRUE(std::isfinite(probeCost(10.0, -5.0)));
}

class SystemRCostEstimatorTest : public testing::Test {
 protected:
  mocks::MockAqlServer server;

  SystemRCostEstimatorTest() {
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

  // The estimator takes ownership of the statistics, so hand back a raw
  // pointer for the test to keep scripting through.
  static std::pair<std::unique_ptr<SystemRCostEstimator>, FakeJoinStatistics*>
  makeEstimator() {
    auto stats = std::make_unique<FakeJoinStatistics>();
    auto* raw = stats.get();
    return {std::make_unique<SystemRCostEstimator>(std::move(stats)), raw};
  }
};

// The design note's worked example: with unique indexes on both sides the
// current estimator says 1000, System-R says 100.
TEST_F(SystemRCostEstimatorTest, worked_example_from_the_design_note) {
  auto q = prepare("FOR a IN c1 FOR b IN c2 FILTER a.x == b.y RETURN [a, b]");
  auto g = buildGraph(*q);
  ASSERT_EQ(g.edges.size(), 1u);

  auto [estimator, stats] = makeEstimator();
  stats->counts = {{"a", 1000.0}, {"b", 100.0}};
  stats->distinct["a"]["x"] = {1000.0, false};  // unique
  stats->distinct["b"]["y"] = {100.0, false};   // unique

  auto* a = nodeByName(g, "a");
  auto* b = nodeByName(g, "b");
  std::array<JoinGraph::Edge const*, 1> connecting{&g.edges.front()};

  auto est = estimator->extend(estimator->seed(*a), *b, connecting);
  EXPECT_DOUBLE_EQ(est.cardinality, 100.0);
  EXPECT_FALSE(est.defaulted);
}

TEST_F(SystemRCostEstimatorTest, cardinality_is_symmetric_but_cost_is_not) {
  auto q = prepare("FOR a IN c1 FOR b IN c2 FILTER a.x == b.y RETURN [a, b]");
  auto g = buildGraph(*q);
  auto [estimator, stats] = makeEstimator();
  stats->counts = {{"a", 1000.0}, {"b", 100.0}};
  stats->distinct["a"]["x"] = {1000.0, false};
  stats->distinct["b"]["y"] = {100.0, false};
  stats->indexed["a"] = {"x"};
  stats->indexed["b"] = {"y"};

  auto* a = nodeByName(g, "a");
  auto* b = nodeByName(g, "b");
  std::array<JoinGraph::Edge const*, 1> connecting{&g.edges.front()};

  auto ab = estimator->extend(estimator->seed(*a), *b, connecting);
  auto ba = estimator->extend(estimator->seed(*b), *a, connecting);

  EXPECT_DOUBLE_EQ(ab.cardinality, ba.cardinality);
  // scanning the 100-row side and probing the 1000-row side is cheaper
  EXPECT_LT(ba.cost, ab.cost);
  EXPECT_DOUBLE_EQ(ba.cost, 100.0 + probeCost(100.0, 1000.0));
  EXPECT_DOUBLE_EQ(ab.cost, 1000.0 + probeCost(1000.0, 100.0));
}

TEST_F(SystemRCostEstimatorTest, unindexed_join_attribute_costs_a_scan) {
  auto q = prepare("FOR a IN c1 FOR b IN c2 FILTER a.x == b.y RETURN [a, b]");
  auto g = buildGraph(*q);
  auto [estimator, stats] = makeEstimator();
  stats->counts = {{"a", 1000.0}, {"b", 100.0}};
  stats->distinct["a"]["x"] = {1000.0, false};
  stats->distinct["b"]["y"] = {100.0, false};
  // no entry in stats->indexed, so hasIndexCovering is false

  auto* a = nodeByName(g, "a");
  auto* b = nodeByName(g, "b");
  std::array<JoinGraph::Edge const*, 1> connecting{&g.edges.front()};

  auto est = estimator->extend(estimator->seed(*a), *b, connecting);
  EXPECT_DOUBLE_EQ(est.cost, 1000.0 + scanCost(1000.0, 100.0));
}

TEST_F(SystemRCostEstimatorTest, empty_connecting_span_is_a_cross_product) {
  auto q = prepare("FOR a IN c1 FOR b IN c2 RETURN [a, b]");
  auto g = buildGraph(*q);
  ASSERT_TRUE(g.edges.empty());

  auto [estimator, stats] = makeEstimator();
  stats->counts = {{"a", 1000.0}, {"b", 100.0}};

  auto* a = nodeByName(g, "a");
  auto* b = nodeByName(g, "b");
  auto est = estimator->extend(estimator->seed(*a), *b, {});

  EXPECT_DOUBLE_EQ(est.cardinality, 100000.0);  // no reduction
  EXPECT_DOUBLE_EQ(est.cost, 1000.0 + scanCost(1000.0, 100.0));
}

TEST_F(SystemRCostEstimatorTest, missing_statistic_defaults_to_one_and_flags) {
  auto q = prepare("FOR a IN c1 FOR b IN c2 FILTER a.x == b.y RETURN [a, b]");
  auto g = buildGraph(*q);
  auto [estimator, stats] = makeEstimator();
  stats->counts = {{"a", 1000.0}, {"b", 100.0}};
  stats->distinct["b"]["y"] = {100.0, false};  // only b is known

  auto* a = nodeByName(g, "a");
  auto* b = nodeByName(g, "b");
  std::array<JoinGraph::Edge const*, 1> connecting{&g.edges.front()};

  auto est = estimator->extend(estimator->seed(*a), *b, connecting);
  // max(1, 100) defers to the known side: 1000 * 100 / 100
  EXPECT_DOUBLE_EQ(est.cardinality, 1000.0);
  EXPECT_TRUE(est.defaulted);
}

TEST_F(SystemRCostEstimatorTest, constant_restriction_shrinks_the_base) {
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FILTER a.k == 'v' RETURN [a, b]");
  auto g = buildGraph(*q);
  auto [estimator, stats] = makeEstimator();
  stats->counts = {{"a", 1000.0}, {"b", 100.0}};
  stats->distinct["a"]["k"] = {10.0, false};  // 10 distinct k values

  auto* a = nodeByName(g, "a");
  auto seeded = estimator->seed(*a);
  EXPECT_DOUBLE_EQ(seeded.cardinality, 100.0);  // 1000 / 10
  // no index covers k, so the seed still pays a full scan
  EXPECT_DOUBLE_EQ(seeded.cost, 1000.0);
}

TEST_F(SystemRCostEstimatorTest,
       indexed_constant_restriction_cheapens_the_seed) {
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FILTER a.k == 'v' RETURN [a, b]");
  auto g = buildGraph(*q);
  auto [estimator, stats] = makeEstimator();
  stats->counts = {{"a", 1000.0}, {"b", 100.0}};
  stats->distinct["a"]["k"] = {10.0, false};
  stats->indexed["a"] = {"k"};

  auto seeded = estimator->seed(*nodeByName(g, "a"));
  EXPECT_DOUBLE_EQ(seeded.cardinality, 100.0);
  EXPECT_DOUBLE_EQ(seeded.cost, 100.0);  // restricted(v), not |C_v|
}

TEST_F(SystemRCostEstimatorTest, range_residual_applies_a_third) {
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FILTER a.p < 5 RETURN [a, b]");
  auto g = buildGraph(*q);
  auto [estimator, stats] = makeEstimator();
  stats->counts = {{"a", 900.0}, {"b", 100.0}};

  auto seeded = estimator->seed(*nodeByName(g, "a"));
  EXPECT_DOUBLE_EQ(seeded.cardinality, 300.0);  // 900 * 1/3
  // the residual does not cheapen the scan: restricted(v) is unchanged
  EXPECT_DOUBLE_EQ(seeded.cost, 900.0);
}

TEST_F(SystemRCostEstimatorTest, residual_factor_does_not_set_defaulted) {
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FILTER a.p < 5 RETURN [a, b]");
  auto g = buildGraph(*q);
  auto [estimator, stats] = makeEstimator();
  stats->counts = {{"a", 900.0}, {"b", 100.0}};

  // 'a' has no constant restrictions, so its condition lookup uses the empty
  // set, which is not a defaulted lookup. The 1/3 range constant is a
  // heuristic, not a missing statistic, so it must not raise the flag either.
  EXPECT_FALSE(estimator->seed(*nodeByName(g, "a")).defaulted);
}

TEST_F(SystemRCostEstimatorTest, distinct_is_capped_by_the_restricted_base) {
  auto q = prepare("FOR a IN c1 FOR b IN c2 FILTER a.x == b.y RETURN [a, b]");
  auto g = buildGraph(*q);
  auto [estimator, stats] = makeEstimator();
  stats->counts = {{"a", 10.0}, {"b", 10.0}};
  // an absurd distinct estimate: more distinct values than rows
  stats->distinct["a"]["x"] = {1e6, false};
  stats->distinct["b"]["y"] = {5.0, false};

  auto* a = nodeByName(g, "a");
  auto* b = nodeByName(g, "b");
  std::array<JoinGraph::Edge const*, 1> connecting{&g.edges.front()};

  // dp is capped at base(a) = 10, so the factor is 1/max(10,5) = 1/10
  auto est = estimator->extend(estimator->seed(*a), *b, connecting);
  EXPECT_DOUBLE_EQ(est.cardinality, 10.0);
}

TEST_F(SystemRCostEstimatorTest, multiple_edges_multiply_their_factors) {
  // a cycle: a-b, b-c, a-c. Adding c to prefix {a,b} is constrained twice.
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FOR c IN c3 "
      "FILTER a.x == b.y FILTER b.z == c.w FILTER a.q == c.r "
      "RETURN [a, b, c]");
  auto g = buildGraph(*q);
  ASSERT_EQ(g.edges.size(), 3u);

  auto [estimator, stats] = makeEstimator();
  stats->counts = {{"a", 100.0}, {"b", 100.0}, {"c", 100.0}};
  stats->distinct["b"]["z"] = {10.0, false};
  stats->distinct["c"]["w"] = {10.0, false};
  stats->distinct["a"]["q"] = {5.0, false};
  stats->distinct["c"]["r"] = {5.0, false};

  auto* c = nodeByName(g, "c");
  std::vector<JoinGraph::Edge const*> connecting;
  for (auto const& e : g.edges) {
    if (e.from == c || e.to == c) {
      connecting.emplace_back(&e);
    }
  }
  ASSERT_EQ(connecting.size(), 2u);

  JoinEstimate prefix{.cardinality = 1000.0, .cost = 0.0, .defaulted = false};
  auto est = estimator->extend(prefix, *c, connecting);
  // 1000 * 100 * (1/10) * (1/5)
  EXPECT_DOUBLE_EQ(est.cardinality, 2000.0);
}

TEST_F(SystemRCostEstimatorTest, distinct_caps_pair_with_their_own_node) {
  // Unlike the equal-base cap test above, base(a) and base(b) are clearly
  // different here (100 vs. 1000), so a bug that caps otherDistinct against
  // nextRestricted.base (or vice versa) changes the answer instead of
  // silently agreeing with the correct pairing.
  auto q = prepare("FOR a IN c1 FOR b IN c2 FILTER a.x == b.y RETURN [a, b]");
  auto g = buildGraph(*q);
  auto [estimator, stats] = makeEstimator();
  stats->counts = {{"a", 100.0}, {"b", 1000.0}};
  stats->distinct["a"]["x"] = {1e6, false};  // capped by base(a) = 100
  stats->distinct["b"]["y"] = {5.0, false};  // stays under base(b) = 1000

  auto* a = nodeByName(g, "a");
  auto* b = nodeByName(g, "b");
  std::array<JoinGraph::Edge const*, 1> connecting{&g.edges.front()};

  // correct: dp = min(1e6, 100) = 100, dn = min(5, 1000) = 5,
  // factor = 1/max(100, 5) = 1/100; cardinality = 100 * 1000 / 100 = 1000.
  // a swapped pairing gives dp = min(1e6, 1000) = 1000, dn = min(5, 100) = 5,
  // factor = 1/1000, cardinality = 100.
  auto est = estimator->extend(estimator->seed(*a), *b, connecting);
  EXPECT_DOUBLE_EQ(est.cardinality, 1000.0);
}

TEST_F(SystemRCostEstimatorTest, extend_costs_against_the_unrestricted_count) {
  // Give `next` (b) a constant restriction that shrinks restricted(b) well
  // below |C_b|, and check that extend()'s cost still uses the full
  // documentCount(b), not restricted(b) -- an index descent traverses the
  // whole index regardless of how selective the outer restriction is.
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FILTER b.k == 'v' RETURN [a, b]");
  auto g = buildGraph(*q);
  auto [estimator, stats] = makeEstimator();
  stats->counts = {{"a", 500.0}, {"b", 1000.0}};
  stats->distinct["b"]["k"] = {100.0, false};  // restricted(b) = 1000/100 = 10
  stats->indexed["b"] = {"y"};                 // extend() probes, not scans

  auto* a = nodeByName(g, "a");
  auto* b = nodeByName(g, "b");
  std::array<JoinGraph::Edge const*, 1> connecting{&g.edges.front()};

  auto est = estimator->extend(estimator->seed(*a), *b, connecting);
  // seed(a).cost == documentCount(a) == 500 (no index on a's empty
  // conditions); the probe is against |C_b| = 1000, not restricted(b) = 10.
  EXPECT_DOUBLE_EQ(est.cost, 500.0 + probeCost(500.0, 1000.0));
}

TEST_F(SystemRCostEstimatorTest, defaulted_propagates_from_the_prefix) {
  // Every lookup this extend() touches is fully scripted (not defaulted);
  // only the prefix itself carries the flag. The result must still be
  // defaulted -- once a fallback statistic has fed the estimate, nothing
  // downstream can un-flag it.
  auto q = prepare("FOR a IN c1 FOR b IN c2 FILTER a.x == b.y RETURN [a, b]");
  auto g = buildGraph(*q);
  auto [estimator, stats] = makeEstimator();
  stats->counts = {{"a", 100.0}, {"b", 100.0}};
  stats->distinct["a"]["x"] = {10.0, false};
  stats->distinct["b"]["y"] = {10.0, false};

  auto* b = nodeByName(g, "b");
  std::array<JoinGraph::Edge const*, 1> connecting{&g.edges.front()};

  JoinEstimate prefix{.cardinality = 100.0, .cost = 0.0, .defaulted = true};
  auto est = estimator->extend(prefix, *b, connecting);
  EXPECT_TRUE(est.defaulted);
}

TEST_F(SystemRCostEstimatorTest,
       defaulted_propagates_from_next_s_own_condition_lookup) {
  // `next` (b) carries a constant restriction whose distinct count is not
  // scripted, so restrictedFor(b) is itself defaulted -- distinct from every
  // edge-level lookup, which are all scripted here, and from the prefix,
  // which starts clean.
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FILTER b.k == 'v' RETURN [a, b]");
  auto g = buildGraph(*q);
  auto [estimator, stats] = makeEstimator();
  stats->counts = {{"a", 100.0}, {"b", 100.0}};
  stats->distinct["a"]["x"] = {10.0, false};
  stats->distinct["b"]["y"] = {10.0, false};
  // stats->distinct["b"]["k"] intentionally left unscripted

  auto* a = nodeByName(g, "a");
  auto* b = nodeByName(g, "b");
  std::array<JoinGraph::Edge const*, 1> connecting{&g.edges.front()};

  auto est = estimator->extend(estimator->seed(*a), *b, connecting);
  EXPECT_TRUE(est.defaulted);
}

TEST_F(SystemRCostEstimatorTest, in_residual_uses_min_of_one_and_the_ratio) {
  // The ratio must exceed 1 to exercise the min at all, and the node needs a
  // separate constant restriction so restricted(a) < count(a): otherwise
  // restricted == count and the [1, count] clamp on base() would mask a
  // missing min() just as easily as a correct one. distinct(c) = 10 shrinks
  // restricted(a) to 100; the IN array of 5 over distinct(k) = 2 gives a raw
  // ratio of 2.5, so 100 * 2.5 = 250 sits well inside [1, 1000] and is
  // genuinely visible if the min is dropped.
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FILTER a.c == 'v' "
      "FILTER a.k IN ['p', 'q', 'r', 's', 't'] RETURN [a, b]");
  auto g = buildGraph(*q);
  auto [estimator, stats] = makeEstimator();
  stats->counts = {{"a", 1000.0}, {"b", 100.0}};
  stats->distinct["a"]["c"] = {10.0, false};  // restricted(a) = 1000/10 = 100
  stats->distinct["a"]["k"] = {2.0, false};   // ratio = 5/2 = 2.5

  auto seeded = estimator->seed(*nodeByName(g, "a"));
  // base(a) = 100 * min(1, 2.5) = 100. Without the min, base(a) = 250.
  EXPECT_DOUBLE_EQ(seeded.cardinality, 100.0);
}

TEST_F(SystemRCostEstimatorTest,
       in_residual_on_a_different_variable_is_neutral) {
  // The residual is attached to `a` because it is the only *graph* variable
  // it references, but the IN's own attribute access (t.k) belongs to a
  // variable outside the graph entirely. distinct["a"]["k"] is scripted and
  // non-defaulted on purpose: an unguarded lookup would not fall back to the
  // masking defaulted-1.0 case, it would compute a visibly wrong factor
  // (3 array members / 50 = 0.06) instead of the correct neutral 1.0.
  auto q = prepare(
      "LET t = NOOPT({k: 1}) FOR a IN c1 FOR b IN c2 "
      "FILTER a.x == b.y FILTER t.k IN [a.q, 1, 2] RETURN [a, b]");
  auto g = buildGraph(*q);
  auto* a = nodeByName(g, "a");
  ASSERT_NE(a, nullptr);
  ASSERT_EQ(a->residuals.size(), 1u);

  FakeJoinStatistics stats;
  stats.distinct["a"]["k"] = {50.0, false};
  EXPECT_DOUBLE_EQ(residualSelectivityFactor(a->residuals.front(), stats, *a),
                   1.0);
}

TEST_F(SystemRCostEstimatorTest, unmodelled_residual_is_the_neutral_element) {
  // a != comparison has no principled selectivity constant. Its factor must
  // be exactly 1.0, so base(v) ends up equal to restricted(v) even though a
  // residual is present.
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FILTER a.k == 'v' FILTER a.m != 5 RETURN [a, b]");
  auto g = buildGraph(*q);
  auto [estimator, stats] = makeEstimator();
  stats->counts = {{"a", 1000.0}, {"b", 100.0}};
  stats->distinct["a"]["k"] = {10.0, false};

  auto* a = nodeByName(g, "a");
  ASSERT_EQ(a->residuals.size(), 1u);
  EXPECT_DOUBLE_EQ(residualSelectivityFactor(a->residuals.front(), *stats, *a),
                   1.0);

  // restricted(a) = 1000 / 10 = 100; base(a) = 100 * 1.0 == restricted(a).
  auto seeded = estimator->seed(*a);
  EXPECT_DOUBLE_EQ(seeded.cardinality, 100.0);
}

TEST_F(SystemRCostEstimatorTest, extend_floors_cardinality_at_one) {
  // Two connecting edges each divide by ~100, and c's own equality
  // restriction is so over-selective that restricted(c)/base(c) clamp to
  // their floor of 1. Unfloored, 100 * 1 * (1/100) * (1/100) == 0.01; a join
  // cannot meaningfully produce a fraction of a row, and letting it through
  // would make every later probeCost/scanCost on this prefix collapse
  // towards zero.
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FOR c IN c3 "
      "FILTER a.x == b.y FILTER b.z == c.w FILTER a.q == c.r "
      "FILTER c.k == 'v' RETURN [a, b, c]");
  auto g = buildGraph(*q);
  ASSERT_EQ(g.edges.size(), 3u);

  auto [estimator, stats] = makeEstimator();
  stats->counts = {{"a", 100.0}, {"b", 100.0}, {"c", 100.0}};
  stats->distinct["b"]["z"] = {100.0, false};
  stats->distinct["c"]["w"] = {100.0, false};
  stats->distinct["a"]["q"] = {100.0, false};
  stats->distinct["c"]["r"] = {100.0, false};
  // 100 distinct k values over 100 documents in c: restricted(c) clamps to 1.
  stats->distinct["c"]["k"] = {1000.0, false};

  auto* c = nodeByName(g, "c");
  std::vector<JoinGraph::Edge const*> connecting;
  for (auto const& e : g.edges) {
    if (e.from == c || e.to == c) {
      connecting.emplace_back(&e);
    }
  }
  ASSERT_EQ(connecting.size(), 2u);

  JoinEstimate prefix{.cardinality = 100.0, .cost = 0.0, .defaulted = false};
  auto est = estimator->extend(prefix, *c, connecting);
  EXPECT_DOUBLE_EQ(est.cardinality, 1.0);
}

}  // namespace arangodb::tests::aql
