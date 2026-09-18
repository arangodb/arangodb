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

#include "Aql/Optimizer/Rule/OptimizeJoinOrder/CachingJoinStatistics.h"

#include "JoinGraphTestHelper.h"

#include <array>
#include <cstddef>
#include <memory>
#include <span>
#include <utility>

using namespace arangodb::aql;

namespace arangodb::tests::aql {
namespace {

// Answers every call with a fresh, strictly increasing value. That makes a
// cache hit visible twice over: the delegation counter does not move, and the
// value would have changed if it had. Scripted tables are deliberately absent
// -- nothing here is about what the statistics say, only about how often they
// are asked.
class CountingStatistics final : public JoinStatistics {
 public:
  mutable std::size_t documentCountCalls = 0;
  mutable std::size_t distinctValuesCalls = 0;
  mutable std::size_t hasIndexCoveringCalls = 0;
  bool defaulted = false;

  auto documentCount(JoinGraph::Node const&) const -> double override {
    return static_cast<double>(++documentCountCalls);
  }

  auto distinctValues(JoinGraph::Node const&,
                      std::span<AttributePath const>) const
      -> DistinctEstimate override {
    return {static_cast<double>(++distinctValuesCalls), defaulted};
  }

  auto hasIndexCovering(JoinGraph::Node const&,
                        std::span<AttributePath const>) const -> bool override {
    // Alternates, so a repeat that was *not* cached reports the opposite.
    return ++hasIndexCoveringCalls % 2 == 1;
  }
};

class CachingJoinStatisticsTest : public testing::Test {
 protected:
  mocks::MockAqlServer server;

  CachingJoinStatisticsTest() {
    auto& vocbase = server.getSystemDatabase();
    for (auto const& name : {"c1", "c2"}) {
      auto json = velocypack::Parser::fromJson(std::string{R"({"name":")"} +
                                               name + "\"}");
      vocbase.createCollection(json->slice());
    }
  }

  std::shared_ptr<Query> prepare(std::string const& query) {
    return prepareJoinPlan(server, query);
  }

  // The inner pointer stays valid: the cache owns it and never reseats it.
  static std::pair<std::unique_ptr<CachingJoinStatistics>, CountingStatistics*>
  makeCache() {
    auto inner = std::make_unique<CountingStatistics>();
    auto* raw = inner.get();
    return {std::make_unique<CachingJoinStatistics>(std::move(inner)), raw};
  }

  std::shared_ptr<Query> twoNodeQuery() {
    return prepare("FOR a IN c1 FOR b IN c2 FILTER a.x == b.y RETURN [a, b]");
  }
};

}  // namespace

// The key's hash and equality cannot be tested through the map: a
// discriminating hash means Eq is never consulted, and a discriminating Eq
// means a weak hash only costs probe length. Both mutations pass every test
// below that goes through CachingJoinStatistics, so the key is tested here
// directly.

TEST(StatsKeyTest, equality_distinguishes_the_attribute_set) {
  StatsKey const byX{ExecutionNodeId{1}, {AttributePath{"x"}}};
  StatsKey const byY{ExecutionNodeId{1}, {AttributePath{"y"}}};

  EXPECT_TRUE(StatsKeyEq{}(byX, byX));
  EXPECT_FALSE(StatsKeyEq{}(byX, byY));
}

TEST(StatsKeyTest, equality_distinguishes_the_node) {
  StatsKey const onOne{ExecutionNodeId{1}, {AttributePath{"x"}}};
  StatsKey const onTwo{ExecutionNodeId{2}, {AttributePath{"x"}}};

  EXPECT_FALSE(StatsKeyEq{}(onOne, onTwo));
}

TEST(StatsKeyTest, equality_distinguishes_nesting_from_arity) {
  StatsKey const twoAttributes{ExecutionNodeId{1},
                               {AttributePath{"p"}, AttributePath{"q"}}};
  StatsKey const oneNested{ExecutionNodeId{1}, {AttributePath{"p", "q"}}};

  EXPECT_FALSE(StatsKeyEq{}(twoAttributes, oneNested));
}

TEST(StatsKeyTest, a_view_compares_equal_to_the_key_it_views) {
  StatsKey const key{ExecutionNodeId{1},
                     {AttributePath{"p"}, AttributePath{"q"}}};

  // The whole point of the transparent comparator: a probe built from a span
  // must match the stored key without materialising one.
  EXPECT_TRUE(StatsKeyEq{}(key, key.view()));
  EXPECT_TRUE(StatsKeyEq{}(key.view(), key));
  EXPECT_EQ(StatsKeyHash{}(key), StatsKeyHash{}(key.view()));
}

TEST(StatsKeyTest, hash_separates_the_keys_equality_separates) {
  // Not required for correctness -- equality is what decides -- but a hash
  // that collided on these would degrade every probe, since they are the
  // shapes the estimator actually asks about.
  StatsKey const byX{ExecutionNodeId{1}, {AttributePath{"x"}}};
  StatsKey const byY{ExecutionNodeId{1}, {AttributePath{"y"}}};
  StatsKey const onTwo{ExecutionNodeId{2}, {AttributePath{"x"}}};
  StatsKey const twoAttributes{ExecutionNodeId{1},
                               {AttributePath{"p"}, AttributePath{"q"}}};
  StatsKey const oneNested{ExecutionNodeId{1}, {AttributePath{"p", "q"}}};

  StatsKeyHash const hash;
  EXPECT_NE(hash(byX), hash(byY));
  EXPECT_NE(hash(byX), hash(onTwo));
  EXPECT_NE(hash(twoAttributes), hash(oneNested));
}

TEST_F(CachingJoinStatisticsTest, document_count_delegates_once_per_node) {
  auto q = twoNodeQuery();
  auto g = buildGraph(*q);
  auto [cache, inner] = makeCache();
  auto* a = nodeByName(g, "a");

  EXPECT_DOUBLE_EQ(cache->documentCount(*a), 1.0);
  EXPECT_DOUBLE_EQ(cache->documentCount(*a), 1.0);
  EXPECT_EQ(inner->documentCountCalls, 1u);
}

TEST_F(CachingJoinStatisticsTest, document_count_keys_on_the_node) {
  auto q = twoNodeQuery();
  auto g = buildGraph(*q);
  auto [cache, inner] = makeCache();

  EXPECT_DOUBLE_EQ(cache->documentCount(*nodeByName(g, "a")), 1.0);
  EXPECT_DOUBLE_EQ(cache->documentCount(*nodeByName(g, "b")), 2.0);
  EXPECT_EQ(inner->documentCountCalls, 2u);
}

TEST_F(CachingJoinStatisticsTest,
       distinct_values_delegates_once_per_attribute_set) {
  auto q = twoNodeQuery();
  auto g = buildGraph(*q);
  auto [cache, inner] = makeCache();
  auto* a = nodeByName(g, "a");
  std::array<AttributePath, 1> attributes{AttributePath{"x"}};

  EXPECT_DOUBLE_EQ(cache->distinctValues(*a, attributes).value, 1.0);
  EXPECT_DOUBLE_EQ(cache->distinctValues(*a, attributes).value, 1.0);
  EXPECT_EQ(inner->distinctValuesCalls, 1u);
}

TEST_F(CachingJoinStatisticsTest, distinct_values_key_includes_the_attributes) {
  auto q = twoNodeQuery();
  auto g = buildGraph(*q);
  auto [cache, inner] = makeCache();
  auto* a = nodeByName(g, "a");
  std::array<AttributePath, 1> byX{AttributePath{"x"}};
  std::array<AttributePath, 1> byY{AttributePath{"y"}};

  EXPECT_DOUBLE_EQ(cache->distinctValues(*a, byX).value, 1.0);
  EXPECT_DOUBLE_EQ(cache->distinctValues(*a, byY).value, 2.0);
  EXPECT_EQ(inner->distinctValuesCalls, 2u);
}

TEST_F(CachingJoinStatisticsTest, distinct_values_key_includes_the_node) {
  auto q = twoNodeQuery();
  auto g = buildGraph(*q);
  auto [cache, inner] = makeCache();
  std::array<AttributePath, 1> attributes{AttributePath{"x"}};

  // Same attribute set, different nodes: the node must be part of the key or
  // b would read a's statistic.
  EXPECT_DOUBLE_EQ(cache->distinctValues(*nodeByName(g, "a"), attributes).value,
                   1.0);
  EXPECT_DOUBLE_EQ(cache->distinctValues(*nodeByName(g, "b"), attributes).value,
                   2.0);
  EXPECT_EQ(inner->distinctValuesCalls, 2u);
}

TEST_F(CachingJoinStatisticsTest,
       a_nested_attribute_is_not_a_two_attribute_set) {
  auto q = twoNodeQuery();
  auto g = buildGraph(*q);
  auto [cache, inner] = makeCache();
  auto* a = nodeByName(g, "a");

  // {["p"],["q"]} is a two-attribute set; {["p","q"]} is the single attribute
  // a.p.q. Flattening the two levels would make these one key, and the
  // distinct-tuple counts they ask for are different questions.
  std::array<AttributePath, 2> twoAttributes{AttributePath{"p"},
                                             AttributePath{"q"}};
  std::array<AttributePath, 1> oneNested{AttributePath{"p", "q"}};

  EXPECT_DOUBLE_EQ(cache->distinctValues(*a, twoAttributes).value, 1.0);
  EXPECT_DOUBLE_EQ(cache->distinctValues(*a, oneNested).value, 2.0);
  EXPECT_EQ(inner->distinctValuesCalls, 2u);
}

TEST_F(CachingJoinStatisticsTest,
       a_defaulted_estimate_is_cached_with_its_flag) {
  auto q = twoNodeQuery();
  auto g = buildGraph(*q);
  auto [cache, inner] = makeCache();
  inner->defaulted = true;
  auto* a = nodeByName(g, "a");
  std::array<AttributePath, 1> attributes{AttributePath{"x"}};

  // A guess must stay marked as one: the rule declines to reorder whenever a
  // statistic is defaulted, so a flag dropped by the cache would let it act on
  // a fallback.
  EXPECT_TRUE(cache->distinctValues(*a, attributes).defaulted);
  EXPECT_TRUE(cache->distinctValues(*a, attributes).defaulted);
  EXPECT_EQ(inner->distinctValuesCalls, 1u);
}

TEST_F(CachingJoinStatisticsTest, the_empty_attribute_set_is_cached) {
  auto q = twoNodeQuery();
  auto g = buildGraph(*q);
  auto [cache, inner] = makeCache();
  auto* a = nodeByName(g, "a");

  // The common case: a node with no constant restriction is asked with an
  // empty set, and the hash never reaches its per-path mixing.
  EXPECT_DOUBLE_EQ(cache->distinctValues(*a, {}).value, 1.0);
  EXPECT_DOUBLE_EQ(cache->distinctValues(*a, {}).value, 1.0);
  EXPECT_EQ(inner->distinctValuesCalls, 1u);
}

TEST_F(CachingJoinStatisticsTest, index_covering_delegates_once) {
  auto q = twoNodeQuery();
  auto g = buildGraph(*q);
  auto [cache, inner] = makeCache();
  auto* a = nodeByName(g, "a");
  std::array<AttributePath, 1> attributes{AttributePath{"x"}};

  EXPECT_TRUE(cache->hasIndexCovering(*a, attributes));
  EXPECT_TRUE(cache->hasIndexCovering(*a, attributes));
  EXPECT_EQ(inner->hasIndexCoveringCalls, 1u);
}

TEST_F(CachingJoinStatisticsTest, index_covering_has_its_own_cache) {
  auto q = twoNodeQuery();
  auto g = buildGraph(*q);
  auto [cache, inner] = makeCache();
  auto* a = nodeByName(g, "a");
  std::array<AttributePath, 1> attributes{AttributePath{"x"}};

  // Same node and attributes, different question: one must not answer for the
  // other.
  EXPECT_DOUBLE_EQ(cache->distinctValues(*a, attributes).value, 1.0);
  EXPECT_TRUE(cache->hasIndexCovering(*a, attributes));
  EXPECT_EQ(inner->distinctValuesCalls, 1u);
  EXPECT_EQ(inner->hasIndexCoveringCalls, 1u);
}

}  // namespace arangodb::tests::aql
