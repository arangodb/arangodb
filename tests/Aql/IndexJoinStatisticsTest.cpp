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

#include "Aql/Optimizer/Utils/IndexJoinStatistics.h"
#include "JoinGraphTestHelper.h"

#include <array>
#include <string>
#include <vector>

using namespace arangodb::aql;

namespace arangodb::tests::aql {
namespace {

class IndexJoinStatisticsTest : public testing::Test {
 protected:
  mocks::MockAqlServer server;

  // Creates collection `name` with `count` documents where x = i, y = i % 10,
  // z = i, then applies `indexes` (each a full index definition).
  void makeCollection(std::string const& name, int count,
                      std::vector<std::string> const& indexes = {}) {
    auto& vocbase = server.getSystemDatabase();
    auto json = velocypack::Parser::fromJson(R"({"name":")" + name + R"("})");
    auto collection = vocbase.createCollection(json->slice());
    for (auto const& definition : indexes) {
      bool created = false;
      collection
          ->createIndex(velocypack::Parser::fromJson(definition)->slice(),
                        created)
          .waitAndGet();
    }
    executeQuery(vocbase, "FOR i IN 1.." + std::to_string(count) +
                              " INSERT {x: i, y: i % 10, z: i} INTO " + name);
  }

  static AttributePath path(std::string_view name) {
    return AttributePath{name};
  }
};

}  // namespace

TEST_F(IndexJoinStatisticsTest, document_count_matches_the_collection) {
  makeCollection("s1", 100);
  auto q = prepareJoinPlan(server, "FOR a IN s1 RETURN a");
  auto g = buildGraph(*q);
  IndexJoinStatistics stats{*q->plan()};

  EXPECT_DOUBLE_EQ(stats.documentCount(*nodeByName(g, "a")), 100.0);
}

TEST_F(IndexJoinStatisticsTest, empty_attribute_set_is_one_and_not_defaulted) {
  makeCollection("s1", 100);
  auto q = prepareJoinPlan(server, "FOR a IN s1 RETURN a");
  auto g = buildGraph(*q);
  IndexJoinStatistics stats{*q->plan()};

  auto est = stats.distinctValues(*nodeByName(g, "a"), {});
  EXPECT_DOUBLE_EQ(est.value, 1.0);
  EXPECT_FALSE(est.defaulted)
      << "an unrestricted node must not be reported as a guess";
}

TEST_F(IndexJoinStatisticsTest, no_covering_index_defaults_to_one) {
  makeCollection("s1", 100);
  auto q = prepareJoinPlan(server, "FOR a IN s1 RETURN a");
  auto g = buildGraph(*q);
  IndexJoinStatistics stats{*q->plan()};

  std::array<AttributePath, 1> attributes{path("x")};
  auto est = stats.distinctValues(*nodeByName(g, "a"), attributes);
  EXPECT_DOUBLE_EQ(est.value, 1.0);
  EXPECT_TRUE(est.defaulted);
}

TEST_F(IndexJoinStatisticsTest, unique_index_gives_an_exact_distinct_count) {
  // A unique index reports selectivity exactly 1.0, so this is deterministic;
  // non-unique estimates are approximate and must never be asserted exactly.
  makeCollection("s1", 100,
                 {R"({"type":"persistent","fields":["x"],"unique":true})"});
  auto q = prepareJoinPlan(server, "FOR a IN s1 RETURN a");
  auto g = buildGraph(*q);
  IndexJoinStatistics stats{*q->plan()};

  std::array<AttributePath, 1> attributes{path("x")};
  auto est = stats.distinctValues(*nodeByName(g, "a"), attributes);
  EXPECT_DOUBLE_EQ(est.value, 100.0);
  EXPECT_FALSE(est.defaulted);
}

TEST_F(IndexJoinStatisticsTest, primary_index_serves_key) {
  makeCollection("s1", 100);
  auto q = prepareJoinPlan(server, "FOR a IN s1 RETURN a");
  auto g = buildGraph(*q);
  IndexJoinStatistics stats{*q->plan()};

  std::array<AttributePath, 1> attributes{path("_key")};
  auto est = stats.distinctValues(*nodeByName(g, "a"), attributes);
  EXPECT_DOUBLE_EQ(est.value, 100.0);
  EXPECT_FALSE(est.defaulted);
}

TEST_F(IndexJoinStatisticsTest, subset_index_is_used_for_a_larger_set) {
  // index on {x} is a subset of {x,y}: distinct(x) <= distinct(x,y), a valid
  // lower bound, so it may be used.
  makeCollection("s1", 100,
                 {R"({"type":"persistent","fields":["x"],"unique":true})"});
  auto q = prepareJoinPlan(server, "FOR a IN s1 RETURN a");
  auto g = buildGraph(*q);
  IndexJoinStatistics stats{*q->plan()};

  std::array<AttributePath, 2> attributes{path("x"), path("y")};
  auto est = stats.distinctValues(*nodeByName(g, "a"), attributes);
  EXPECT_DOUBLE_EQ(est.value, 100.0);
  EXPECT_FALSE(est.defaulted);
}

TEST_F(IndexJoinStatisticsTest, superset_index_is_rejected) {
  // index on {x,z} is a superset of {x}: distinct(x,z) >= distinct(x), so
  // using it would over-estimate distinctness and under-estimate the join.
  makeCollection("s1", 100,
                 {R"({"type":"persistent","fields":["x","z"],"unique":true})"});
  auto q = prepareJoinPlan(server, "FOR a IN s1 RETURN a");
  auto g = buildGraph(*q);
  IndexJoinStatistics stats{*q->plan()};

  std::array<AttributePath, 1> attributes{path("x")};
  auto est = stats.distinctValues(*nodeByName(g, "a"), attributes);
  EXPECT_DOUBLE_EQ(est.value, 1.0);
  EXPECT_TRUE(est.defaulted);
}

TEST_F(IndexJoinStatisticsTest, sparse_index_is_rejected) {
  // a sparse index's estimate is relative to the indexed documents only
  makeCollection(
      "s1", 100,
      {R"({"type":"persistent","fields":["x"],"unique":true,"sparse":true})"});
  auto q = prepareJoinPlan(server, "FOR a IN s1 RETURN a");
  auto g = buildGraph(*q);
  IndexJoinStatistics stats{*q->plan()};

  std::array<AttributePath, 1> attributes{path("x")};
  auto est = stats.distinctValues(*nodeByName(g, "a"), attributes);
  EXPECT_TRUE(est.defaulted);
}

TEST_F(IndexJoinStatisticsTest, distinct_never_exceeds_the_document_count) {
  makeCollection("s1", 100,
                 {R"({"type":"persistent","fields":["x"],"unique":true})"});
  auto q = prepareJoinPlan(server, "FOR a IN s1 RETURN a");
  auto g = buildGraph(*q);
  IndexJoinStatistics stats{*q->plan()};

  std::array<AttributePath, 1> attributes{path("x")};
  auto est = stats.distinctValues(*nodeByName(g, "a"), attributes);
  EXPECT_LE(est.value, stats.documentCount(*nodeByName(g, "a")));
  EXPECT_GE(est.value, 1.0);
}

TEST_F(IndexJoinStatisticsTest,
       has_index_covering_is_a_leading_field_question) {
  // an index on (y,x) cannot serve a probe by x alone
  makeCollection("s1", 100, {R"({"type":"persistent","fields":["y","x"]})"});
  auto q = prepareJoinPlan(server, "FOR a IN s1 RETURN a");
  auto g = buildGraph(*q);
  IndexJoinStatistics stats{*q->plan()};
  auto* a = nodeByName(g, "a");

  std::array<AttributePath, 1> byX{path("x")};
  std::array<AttributePath, 1> byY{path("y")};
  EXPECT_FALSE(stats.hasIndexCovering(*a, byX));
  EXPECT_TRUE(stats.hasIndexCovering(*a, byY));
}

TEST_F(IndexJoinStatisticsTest,
       has_index_covering_does_not_need_a_selectivity_estimate) {
  // "estimates":false switches the selectivity estimate off, but the index
  // still exists and can still serve a probe.
  makeCollection("s1", 100,
                 {R"({"type":"persistent","fields":["x"],"estimates":false})"});
  auto q = prepareJoinPlan(server, "FOR a IN s1 RETURN a");
  auto g = buildGraph(*q);
  IndexJoinStatistics stats{*q->plan()};
  auto* a = nodeByName(g, "a");

  std::array<AttributePath, 1> byX{path("x")};
  EXPECT_TRUE(stats.hasIndexCovering(*a, byX));
  EXPECT_TRUE(stats.distinctValues(*a, byX).defaulted);
}

TEST_F(IndexJoinStatisticsTest, edge_index_serves_from_and_to) {
  auto& vocbase = server.getSystemDatabase();
  auto vertices = velocypack::Parser::fromJson(R"({"name":"v1"})");
  vocbase.createCollection(vertices->slice());
  auto edges = velocypack::Parser::fromJson(R"({"name":"e1","type":3})");
  vocbase.createCollection(edges->slice());
  executeQuery(vocbase, "FOR i IN 1..10 INSERT {_key: CONCAT('k', i)} INTO v1");
  executeQuery(vocbase,
               "FOR i IN 1..10 INSERT {_from: CONCAT('v1/k', i), "
               "_to: CONCAT('v1/k', i)} INTO e1");

  auto q = prepareJoinPlan(server, "FOR e IN e1 RETURN e");
  auto g = buildGraph(*q);
  IndexJoinStatistics stats{*q->plan()};
  auto* e = nodeByName(g, "e");

  std::array<AttributePath, 1> byFrom{path("_from")};
  std::array<AttributePath, 1> byTo{path("_to")};
  EXPECT_TRUE(stats.hasIndexCovering(*e, byFrom));
  EXPECT_TRUE(stats.hasIndexCovering(*e, byTo));
  // per-direction estimates are read through Index::selectivityEstimate();
  // never from the index listing, which merges the two directions.
  EXPECT_FALSE(stats.distinctValues(*e, byFrom).defaulted);
}

}  // namespace arangodb::tests::aql
