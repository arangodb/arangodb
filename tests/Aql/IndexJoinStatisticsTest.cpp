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
// trx.abort() below needs the full transaction::Methods definition, which
// none of the above only-forward-declaring headers provide.
#include "Transaction/Methods.h"

// createIndex() is called through a shared_ptr<LogicalCollection>, so the full
// definition is required here -- JoinGraphTestHelper.h only forward-declares
// it.
#include "VocBase/LogicalCollection.h"

#include <array>
#include <string>
#include <vector>

using namespace arangodb::aql;

namespace arangodb::tests::aql {
namespace {

// -----------------------------------------------------------------------------
// IndexJoinStatistics itself, against a real MockAqlServer collection. This
// exercises the adapter (real Ast/Query/transaction, real Collection) end to
// end -- exactly what the IndexFacts-level tests below cannot do.
//
// Under MockAqlServer, a collection's physical index list is empty -- not
// even the implicit primary index is registered -- so no test here can rely
// on an actually-created index being *found*. Only the "no index is ever
// found" paths (the default, and the not-RUNNING short-circuit, which takes
// that path regardless of what indexes exist) can be exercised honestly
// against this fixture. Everything about *which* index qualifies belongs to
// the IndexFacts-level tests further down, which script the facts directly.
// -----------------------------------------------------------------------------

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
  auto q = prepareJoinPlanForStatistics(server, "FOR a IN s1 RETURN a");
  auto g = buildGraph(*q);
  IndexJoinStatistics stats{*q->plan()};

  EXPECT_DOUBLE_EQ(stats.documentCount(*nodeByName(g, "a")), 100.0);
}

TEST_F(IndexJoinStatisticsTest, empty_attribute_set_is_one_and_not_defaulted) {
  makeCollection("s1", 100);
  auto q = prepareJoinPlanForStatistics(server, "FOR a IN s1 RETURN a");
  auto g = buildGraph(*q);
  IndexJoinStatistics stats{*q->plan()};

  auto est = stats.distinctValues(*nodeByName(g, "a"), {});
  EXPECT_DOUBLE_EQ(est.value, 1.0);
  EXPECT_FALSE(est.defaulted)
      << "an unrestricted node must not be reported as a guess";
}

TEST_F(IndexJoinStatisticsTest, no_covering_index_defaults_to_one) {
  makeCollection("s1", 100);
  auto q = prepareJoinPlanForStatistics(server, "FOR a IN s1 RETURN a");
  auto g = buildGraph(*q);
  IndexJoinStatistics stats{*q->plan()};

  std::array<AttributePath, 1> attributes{path("x")};
  auto est = stats.distinctValues(*nodeByName(g, "a"), attributes);
  EXPECT_DOUBLE_EQ(est.value, 1.0);
  EXPECT_TRUE(est.defaulted);
}

TEST_F(IndexJoinStatisticsTest,
       unusable_transaction_defaults_even_with_a_qualifying_index) {
  // If the transaction cannot be consulted, documentCount() falls back to 0
  // (its only fallback -- the interface has no defaulted channel of its
  // own). Naively feeding that into `selectivity * count` for a qualifying
  // index would produce a confident-looking {1.0, defaulted = false}: a
  // fabricated statistic wearing a trustworthy flag. distinctValues() must
  // reject this case outright rather than let it fall out of the
  // arithmetic.
  makeCollection("s1", 100,
                 {R"({"type":"persistent","fields":["x"],"unique":true})"});
  auto q = prepareJoinPlanForStatistics(server, "FOR a IN s1 RETURN a");
  auto g = buildGraph(*q);
  IndexJoinStatistics stats{*q->plan()};

  auto& trx = q->trxForOptimization();
  ASSERT_TRUE(trx.abort().ok());

  std::array<AttributePath, 1> attributes{path("x")};
  auto est = stats.distinctValues(*nodeByName(g, "a"), attributes);
  EXPECT_DOUBLE_EQ(est.value, 1.0);
  EXPECT_TRUE(est.defaulted);
}

// -----------------------------------------------------------------------------
// The selection rules themselves, against scripted IndexFacts. No collection,
// no server, no storage engine:
// distinctFromIndexFacts()/coveringFromIndexFacts() are pure functions of the
// facts and the attribute set, which is exactly what lets these be exact and
// environment-independent.
//
// Every rejection case below is paired with a qualifying index (or, where a
// companion index would be numerically invisible against a max(), a solo
// index whose exclusion is instead observable through `defaulted`) so that an
// implementation with the corresponding guard removed would fail the test,
// not pass it vacuously.
// -----------------------------------------------------------------------------

namespace {

auto singleField(std::string_view name, bool expand = false)
    -> std::vector<basics::AttributeName> {
  return {basics::AttributeName{name, expand}};
}

// A persistent index (the type this model trusts for a selectivity estimate)
// covering exactly one field, with a controllable estimate.
auto qualifyingIndex(std::string_view name, double selectivity) -> IndexFacts {
  IndexFacts facts;
  facts.type = Index::TRI_IDX_TYPE_PERSISTENT_INDEX;
  facts.fields = {singleField(name)};
  facts.hasSelectivityEstimate = true;
  facts.selectivityEstimate = selectivity;
  return facts;
}

}  // namespace

TEST(IndexFactsRulesTest, subset_index_qualifies) {
  // index on {x} is a subset of {x,y}: distinct(x) <= distinct(x,y), a valid
  // lower bound, so it may be used.
  std::array<IndexFacts, 1> candidates{qualifyingIndex("x", 1.0)};
  std::array<AttributePath, 2> attributes{AttributePath{"x"},
                                          AttributePath{"y"}};

  auto est = distinctFromIndexFacts(candidates, 100.0, attributes);
  EXPECT_DOUBLE_EQ(est.value, 100.0);
  EXPECT_FALSE(est.defaulted);
}

TEST(IndexFactsRulesTest,
     superset_index_is_rejected_even_with_a_qualifying_index) {
  // index on {x,z} is a superset of {x}: distinct(x,z) >= distinct(x), so
  // using it would over-estimate distinctness and under-estimate the join.
  // The qualifying {x} index alone would report 10 (selectivity 0.1); if the
  // superset were wrongly allowed too, its selectivity of 1.0 would push the
  // answer to 100 instead.
  auto superset = qualifyingIndex("x", 1.0);
  superset.fields = {singleField("x"), singleField("z")};
  std::array<IndexFacts, 2> candidates{qualifyingIndex("x", 0.1), superset};
  std::array<AttributePath, 1> attributes{AttributePath{"x"}};

  auto est = distinctFromIndexFacts(candidates, 100.0, attributes);
  EXPECT_DOUBLE_EQ(est.value, 10.0);
  EXPECT_FALSE(est.defaulted);
}

TEST(IndexFactsRulesTest,
     sparse_index_is_rejected_even_with_a_qualifying_index) {
  // a sparse index's estimate is relative to the indexed documents only. The
  // qualifying non-sparse {x} index alone would report 10; if the sparse one
  // were wrongly allowed too, its selectivity of 1.0 would push the answer
  // to 100 instead.
  auto sparse = qualifyingIndex("x", 1.0);
  sparse.sparse = true;
  std::array<IndexFacts, 2> candidates{qualifyingIndex("x", 0.1), sparse};
  std::array<AttributePath, 1> attributes{AttributePath{"x"}};

  auto est = distinctFromIndexFacts(candidates, 100.0, attributes);
  EXPECT_DOUBLE_EQ(est.value, 10.0);
  EXPECT_FALSE(est.defaulted);
}

TEST(IndexFactsRulesTest,
     disallowed_type_is_rejected_even_with_a_qualifying_index) {
  // Inverted, geo, ttl, mdi and vector indexes report unrelated numbers, so
  // only primary/edge/persistent indexes are trusted. The qualifying
  // persistent {x} index alone would report 10; if the inverted one were
  // wrongly allowed too, its selectivity of 1.0 would push the answer to 100
  // instead.
  auto disallowed = qualifyingIndex("x", 1.0);
  disallowed.type = Index::TRI_IDX_TYPE_INVERTED_INDEX;
  std::array<IndexFacts, 2> candidates{qualifyingIndex("x", 0.1), disallowed};
  std::array<AttributePath, 1> attributes{AttributePath{"x"}};

  auto est = distinctFromIndexFacts(candidates, 100.0, attributes);
  EXPECT_DOUBLE_EQ(est.value, 10.0);
  EXPECT_FALSE(est.defaulted);
}

TEST(IndexFactsRulesTest,
     hidden_index_is_rejected_even_with_a_qualifying_index) {
  // A hidden index (e.g. internal bookkeeping) is never surfaced to this
  // model. The qualifying {x} index alone would report 10; if the hidden
  // one were wrongly allowed too, its selectivity of 1.0 would push the
  // answer to 100 instead.
  auto hidden = qualifyingIndex("x", 1.0);
  hidden.hidden = true;
  std::array<IndexFacts, 2> candidates{qualifyingIndex("x", 0.1), hidden};
  std::array<AttributePath, 1> attributes{AttributePath{"x"}};

  auto est = distinctFromIndexFacts(candidates, 100.0, attributes);
  EXPECT_DOUBLE_EQ(est.value, 10.0);
  EXPECT_FALSE(est.defaulted);
}

TEST(IndexFactsRulesTest,
     in_progress_index_is_rejected_even_with_a_qualifying_index) {
  // An index still being built has no reliable estimate yet. The qualifying
  // {x} index alone would report 10; if the in-progress one were wrongly
  // allowed too, its selectivity of 1.0 would push the answer to 100
  // instead.
  auto inProgress = qualifyingIndex("x", 1.0);
  inProgress.inProgress = true;
  std::array<IndexFacts, 2> candidates{qualifyingIndex("x", 0.1), inProgress};
  std::array<AttributePath, 1> attributes{AttributePath{"x"}};

  auto est = distinctFromIndexFacts(candidates, 100.0, attributes);
  EXPECT_DOUBLE_EQ(est.value, 10.0);
  EXPECT_FALSE(est.defaulted);
}

TEST(IndexFactsRulesTest,
     expanded_field_is_rejected_even_with_a_qualifying_index) {
  // An index on x[*] has different selectivity semantics than a plain field
  // and is never trusted here. The qualifying plain {x} index alone would
  // report 10; if the expanded one were wrongly allowed too, its selectivity
  // of 1.0 would push the answer to 100 instead.
  auto expanded = qualifyingIndex("x", 1.0);
  expanded.fields = {singleField("x", /*expand*/ true)};
  std::array<IndexFacts, 2> candidates{qualifyingIndex("x", 0.1), expanded};
  std::array<AttributePath, 1> attributes{AttributePath{"x"}};

  auto est = distinctFromIndexFacts(candidates, 100.0, attributes);
  EXPECT_DOUBLE_EQ(est.value, 10.0);
  EXPECT_FALSE(est.defaulted);
}

TEST(IndexFactsRulesTest,
     missing_selectivity_estimate_is_rejected_even_with_a_qualifying_index) {
  // hasSelectivityEstimate() == false must be honoured even if some stale or
  // garbage value sits in the estimate field regardless. The qualifying {x}
  // index alone would report 10; if the no-estimate one were wrongly
  // trusted too, its (unusable) stored value of 1.0 would push the answer to
  // 100 instead.
  auto noEstimate = qualifyingIndex("x", 1.0);
  noEstimate.hasSelectivityEstimate = false;
  std::array<IndexFacts, 2> candidates{qualifyingIndex("x", 0.1), noEstimate};
  std::array<AttributePath, 1> attributes{AttributePath{"x"}};

  auto est = distinctFromIndexFacts(candidates, 100.0, attributes);
  EXPECT_DOUBLE_EQ(est.value, 10.0);
  EXPECT_FALSE(est.defaulted);
}

TEST(IndexFactsRulesTest, zero_selectivity_is_rejected_despite_the_flag) {
  // A selectivity of 0.0 divides badly downstream and is never a genuine
  // "every row is distinct" reading, so it is rejected even though
  // hasSelectivityEstimate() says true. Unlike the other rejections, a
  // companion qualifying index would make this invisible: a 0-contribution
  // never changes a max(). So this is a solo positive control instead --
  // without the guard, this index alone would be accepted and report
  // {1.0, defaulted = false} rather than the correct {1.0, defaulted = true}.
  std::array<IndexFacts, 1> candidates{qualifyingIndex("x", 0.0)};
  std::array<AttributePath, 1> attributes{AttributePath{"x"}};

  auto est = distinctFromIndexFacts(candidates, 100.0, attributes);
  EXPECT_DOUBLE_EQ(est.value, 1.0);
  EXPECT_TRUE(est.defaulted);
}

TEST(IndexFactsRulesTest,
     distinct_takes_the_max_over_several_qualifying_indexes) {
  std::array<IndexFacts, 3> candidates{
      qualifyingIndex("x", 0.1),
      qualifyingIndex("x", 0.3),
      qualifyingIndex("x", 0.05),
  };
  std::array<AttributePath, 1> attributes{AttributePath{"x"}};

  auto est = distinctFromIndexFacts(candidates, 100.0, attributes);
  EXPECT_DOUBLE_EQ(est.value, 30.0);
  EXPECT_FALSE(est.defaulted);
}

TEST(IndexFactsRulesTest,
     distinct_is_floored_at_one_even_for_an_empty_collection) {
  // selectivity is always <= 1, so selectivity * count can never exceed
  // count -- the clamp's upper bound is never actually reached. Its lower
  // bound is the interesting side: a qualifying index over an empty (or
  // fully-restricted-to-nothing) collection must still report 1, not 0.
  std::array<IndexFacts, 1> candidates{qualifyingIndex("x", 1.0)};
  std::array<AttributePath, 1> attributes{AttributePath{"x"}};

  auto est = distinctFromIndexFacts(candidates, /*count*/ 0.0, attributes);
  EXPECT_DOUBLE_EQ(est.value, 1.0);
  EXPECT_FALSE(est.defaulted);
}

TEST(IndexFactsRulesTest, covering_needs_the_leading_field) {
  // an index on (y,x) cannot serve a probe by x alone
  IndexFacts compound;
  compound.type = Index::TRI_IDX_TYPE_PERSISTENT_INDEX;
  compound.fields = {singleField("y"), singleField("x")};
  compound.hasSelectivityEstimate = true;
  compound.selectivityEstimate = 1.0;
  std::array<IndexFacts, 1> candidates{compound};

  std::array<AttributePath, 1> byX{AttributePath{"x"}};
  std::array<AttributePath, 1> byY{AttributePath{"y"}};
  EXPECT_FALSE(coveringFromIndexFacts(candidates, byX));
  EXPECT_TRUE(coveringFromIndexFacts(candidates, byY));
}

TEST(IndexFactsRulesTest, covering_succeeds_without_a_selectivity_estimate) {
  // The index still exists and can still serve a probe even with no
  // selectivity estimate at all -- that is a distinctValues() concern, not a
  // hasIndexCovering() one.
  IndexFacts noEstimate;
  noEstimate.type = Index::TRI_IDX_TYPE_PERSISTENT_INDEX;
  noEstimate.fields = {singleField("x")};
  noEstimate.hasSelectivityEstimate = false;
  std::array<IndexFacts, 1> candidates{noEstimate};
  std::array<AttributePath, 1> byX{AttributePath{"x"}};

  EXPECT_TRUE(coveringFromIndexFacts(candidates, byX));
  EXPECT_TRUE(distinctFromIndexFacts(candidates, 100.0, byX).defaulted);
}

}  // namespace arangodb::tests::aql
