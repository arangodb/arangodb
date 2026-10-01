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
#include <memory>

using namespace arangodb::aql;

namespace arangodb::tests::aql {

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

  // the estimator owns the statistics; the raw pointer keeps them scriptable
  static std::pair<std::unique_ptr<SystemRCostEstimator>, FakeJoinStatistics*>
  makeEstimator(Query const& q) {
    auto stats = std::make_unique<FakeJoinStatistics>(*q.plan());
    auto* raw = stats.get();
    return {std::make_unique<SystemRCostEstimator>(std::move(stats)), raw};
  }
};

TEST_F(SystemRCostEstimatorTest, worked_example) {
  auto q = prepare("FOR a IN c1 FOR b IN c2 FILTER a.x == b.y RETURN [a, b]");
  auto g = buildGraph(*q);
  ASSERT_EQ(g.edges.size(), 1u);

  auto [estimator, stats] = makeEstimator(*q);
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
  auto [estimator, stats] = makeEstimator(*q);
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
  EXPECT_DOUBLE_EQ(ba.cost, 100.0 + 100.0 * std::log2(1000.0));
  EXPECT_DOUBLE_EQ(ab.cost, 1000.0 + 1000.0 * std::log2(100.0));
}

TEST_F(SystemRCostEstimatorTest, unindexed_join_attribute_costs_a_scan) {
  auto q = prepare("FOR a IN c1 FOR b IN c2 FILTER a.x == b.y RETURN [a, b]");
  auto g = buildGraph(*q);
  auto [estimator, stats] = makeEstimator(*q);
  stats->counts = {{"a", 1000.0}, {"b", 100.0}};
  stats->distinct["a"]["x"] = {1000.0, false};
  stats->distinct["b"]["y"] = {100.0, false};
  // no entry in stats->indexed, so hasIndexCovering is false

  auto* a = nodeByName(g, "a");
  auto* b = nodeByName(g, "b");
  std::array<JoinGraph::Edge const*, 1> connecting{&g.edges.front()};

  auto est = estimator->extend(estimator->seed(*a), *b, connecting);
  EXPECT_DOUBLE_EQ(est.cost, 1000.0 + 1000.0 * 100.0);
}

TEST_F(SystemRCostEstimatorTest, empty_connecting_span_is_a_cross_product) {
  auto q = prepare("FOR a IN c1 FOR b IN c2 RETURN [a, b]");
  auto g = buildGraph(*q);
  ASSERT_TRUE(g.edges.empty());

  auto [estimator, stats] = makeEstimator(*q);
  stats->counts = {{"a", 1000.0}, {"b", 100.0}};

  auto* a = nodeByName(g, "a");
  auto* b = nodeByName(g, "b");
  auto est = estimator->extend(estimator->seed(*a), *b, {});

  EXPECT_DOUBLE_EQ(est.cardinality, 100000.0);  // no reduction
  EXPECT_DOUBLE_EQ(est.cost, 1000.0 + 1000.0 * 100.0);
}

TEST_F(SystemRCostEstimatorTest, missing_statistic_defaults_to_one_and_flags) {
  auto q = prepare("FOR a IN c1 FOR b IN c2 FILTER a.x == b.y RETURN [a, b]");
  auto g = buildGraph(*q);
  auto [estimator, stats] = makeEstimator(*q);
  stats->counts = {{"a", 1000.0}, {"b", 100.0}};
  stats->distinct["b"]["y"] = {100.0, false};  // only b is known

  auto* a = nodeByName(g, "a");
  auto* b = nodeByName(g, "b");
  std::array<JoinGraph::Edge const*, 1> connecting{&g.edges.front()};

  auto est = estimator->extend(estimator->seed(*a), *b, connecting);
  // max(1, 100) defers to the known side: 1000 * 100 / 100
  EXPECT_DOUBLE_EQ(est.cardinality, 1000.0);
  EXPECT_TRUE(est.defaulted);

  // once set, the flag survives fully scripted later steps
  stats->distinct["a"]["x"] = {1000.0, false};
  JoinEstimate prefix{.cardinality = 100.0, .cost = 0.0, .defaulted = true};
  EXPECT_TRUE(estimator->extend(prefix, *b, connecting).defaulted);

  // and next's own unscripted constant restriction sets it too
  auto q2 = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FILTER b.k == 'v' RETURN [a, b]");
  auto g2 = buildGraph(*q2);
  std::array<JoinGraph::Edge const*, 1> connecting2{&g2.edges.front()};
  EXPECT_TRUE(estimator
                  ->extend(estimator->seed(*nodeByName(g2, "a")),
                           *nodeByName(g2, "b"), connecting2)
                  .defaulted);
}

TEST_F(SystemRCostEstimatorTest, constant_restriction_shrinks_the_row_count) {
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FILTER a.k == 'v' RETURN [a, b]");
  auto g = buildGraph(*q);
  auto [estimator, stats] = makeEstimator(*q);
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
  auto [estimator, stats] = makeEstimator(*q);
  stats->counts = {{"a", 1000.0}, {"b", 100.0}};
  stats->distinct["a"]["k"] = {10.0, false};
  stats->indexed["a"] = {"k"};

  auto seeded = estimator->seed(*nodeByName(g, "a"));
  EXPECT_DOUBLE_EQ(seeded.cardinality, 100.0);
  EXPECT_DOUBLE_EQ(seeded.cost, 100.0);  // restricted, not the full count
}

TEST_F(SystemRCostEstimatorTest, distinct_is_capped_by_the_restricted_count) {
  auto q = prepare("FOR a IN c1 FOR b IN c2 FILTER a.x == b.y RETURN [a, b]");
  auto g = buildGraph(*q);
  auto [estimator, stats] = makeEstimator(*q);
  stats->counts = {{"a", 100.0}, {"b", 1000.0}};
  stats->distinct["a"]["x"] = {1e6, false};  // more distinct values than rows
  stats->distinct["b"]["y"] = {5.0, false};

  auto* a = nodeByName(g, "a");
  auto* b = nodeByName(g, "b");
  std::array<JoinGraph::Edge const*, 1> connecting{&g.edges.front()};

  // dp = min(1e6, 100), dn = min(5, 1000): factor 1/100, so 100 * 1000 / 100.
  // The unequal counts make a swapped pairing visible: it would give 100.
  auto est = estimator->extend(estimator->seed(*a), *b, connecting);
  EXPECT_DOUBLE_EQ(est.cardinality, 1000.0);
}

TEST_F(SystemRCostEstimatorTest, multiple_edges_multiply_their_factors) {
  // a cycle: a-b, b-c, a-c. Adding c to prefix {a,b} is constrained twice.
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FOR c IN c3 "
      "FILTER a.x == b.y FILTER b.z == c.w FILTER a.q == c.r "
      "RETURN [a, b, c]");
  auto g = buildGraph(*q);
  ASSERT_EQ(g.edges.size(), 3u);

  auto [estimator, stats] = makeEstimator(*q);
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

TEST_F(SystemRCostEstimatorTest, extend_costs_against_the_unrestricted_count) {
  // b's constant restriction shrinks restricted(b) well below |C_b|; the
  // probe still costs log |C_b|, since the descent walks the whole index.
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FILTER b.k == 'v' RETURN [a, b]");
  auto g = buildGraph(*q);
  auto [estimator, stats] = makeEstimator(*q);
  stats->counts = {{"a", 500.0}, {"b", 1000.0}};
  stats->distinct["b"]["k"] = {100.0, false};  // restricted(b) = 1000/100 = 10
  stats->indexed["b"] = {"y"};                 // extend() probes, not scans

  auto* a = nodeByName(g, "a");
  auto* b = nodeByName(g, "b");
  std::array<JoinGraph::Edge const*, 1> connecting{&g.edges.front()};

  auto est = estimator->extend(estimator->seed(*a), *b, connecting);
  // seed(a) = 500 scanned; the probe is against |C_b| = 1000, not 10
  EXPECT_DOUBLE_EQ(est.cost, 500.0 + 500.0 * std::log2(1000.0));
}

TEST_F(SystemRCostEstimatorTest, extend_floors_cardinality_at_one) {
  // two edges each divide by 100 and restricted(c) is 1: unfloored, the
  // prefix would carry 0.01 rows and every later step would cost nothing
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FOR c IN c3 "
      "FILTER a.x == b.y FILTER b.z == c.w FILTER a.q == c.r "
      "FILTER c.k == 'v' RETURN [a, b, c]");
  auto g = buildGraph(*q);
  ASSERT_EQ(g.edges.size(), 3u);

  auto [estimator, stats] = makeEstimator(*q);
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
