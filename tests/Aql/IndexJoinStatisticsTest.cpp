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

#include "Aql/Optimizer/Rule/OptimizeJoinOrder/IndexJoinStatistics.h"

#include <array>
#include <string_view>
#include <vector>

using namespace arangodb::aql;

namespace arangodb::tests::aql {

// The selection rules over scripted IndexFacts. Every rejection case is
// paired with a qualifying index, so removing the guard under test changes
// the answer rather than passing vacuously.

namespace {

auto singleField(std::string_view name, bool expand = false)
    -> std::vector<basics::AttributeName> {
  return {basics::AttributeName{name, expand}};
}

// A persistent index (the type this model trusts for a selectivity estimate)
// covering exactly one field, with a controllable estimate.
auto qualifyingIndex(std::string_view name, double selectivity) -> IndexFacts {
  IndexFacts facts;
  facts.type = IndexType::Persistent;
  facts.fields = {singleField(name)};
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
  // distinct(x,z) >= distinct(x), so a superset index would over-estimate;
  // allowed, its selectivity of 1.0 would push the answer from 10 to 100.
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
  // a sparse index's estimate is relative to the indexed documents only.
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
  // only primary/edge/persistent indexes are trusted.
  auto disallowed = qualifyingIndex("x", 1.0);
  disallowed.type = IndexType::Inverted;
  std::array<IndexFacts, 2> candidates{qualifyingIndex("x", 0.1), disallowed};
  std::array<AttributePath, 1> attributes{AttributePath{"x"}};

  auto est = distinctFromIndexFacts(candidates, 100.0, attributes);
  EXPECT_DOUBLE_EQ(est.value, 10.0);
  EXPECT_FALSE(est.defaulted);
}

TEST(IndexFactsRulesTest,
     expanded_field_is_rejected_even_with_a_qualifying_index) {
  // An index on x[*] has different selectivity semantics than a plain field.
  auto expanded = qualifyingIndex("x", 1.0);
  expanded.fields = {singleField("x", /*expand*/ true)};
  std::array<IndexFacts, 2> candidates{qualifyingIndex("x", 0.1), expanded};
  std::array<AttributePath, 1> attributes{AttributePath{"x"}};

  auto est = distinctFromIndexFacts(candidates, 100.0, attributes);
  EXPECT_DOUBLE_EQ(est.value, 10.0);
  EXPECT_FALSE(est.defaulted);
}

TEST(IndexFactsRulesTest, zero_selectivity_means_no_estimate) {
  // 0.0 is how toIndexFacts records an index without an estimate. Tested
  // solo: beside a qualifying index a 0 contribution never changes a max().
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
  // a qualifying index over an empty collection reports 1, not 0
  std::array<IndexFacts, 1> candidates{qualifyingIndex("x", 1.0)};
  std::array<AttributePath, 1> attributes{AttributePath{"x"}};

  auto est = distinctFromIndexFacts(candidates, /*count*/ 0.0, attributes);
  EXPECT_DOUBLE_EQ(est.value, 1.0);
  EXPECT_FALSE(est.defaulted);
}

TEST(IndexFactsRulesTest, covering_needs_the_leading_field) {
  // an index on (y,x) cannot serve a probe by x alone
  IndexFacts compound;
  compound.type = IndexType::Persistent;
  compound.fields = {singleField("y"), singleField("x")};
  compound.selectivityEstimate = 1.0;
  std::array<IndexFacts, 1> candidates{compound};

  std::array<AttributePath, 1> byX{AttributePath{"x"}};
  std::array<AttributePath, 1> byY{AttributePath{"y"}};
  EXPECT_FALSE(coveringFromIndexFacts(candidates, byX));
  EXPECT_TRUE(coveringFromIndexFacts(candidates, byY));
}

TEST(IndexFactsRulesTest, covering_succeeds_without_a_selectivity_estimate) {
  // an estimate is a distinctValues() concern, not a hasIndexCovering() one
  IndexFacts noEstimate;
  noEstimate.type = IndexType::Persistent;
  noEstimate.fields = {singleField("x")};
  noEstimate.selectivityEstimate = 0.0;
  std::array<IndexFacts, 1> candidates{noEstimate};
  std::array<AttributePath, 1> byX{AttributePath{"x"}};

  EXPECT_TRUE(coveringFromIndexFacts(candidates, byX));
  EXPECT_TRUE(distinctFromIndexFacts(candidates, 100.0, byX).defaulted);
}

}  // namespace arangodb::tests::aql
