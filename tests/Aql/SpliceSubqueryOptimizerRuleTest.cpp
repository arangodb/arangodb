////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "QueryHelper.h"

#include "Aql/AqlItemBlockSerializationFormat.h"
#include "Aql/Ast.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Query.h"
#include "Aql/SubqueryEndExecutionNode.h"
#include "Aql/SubqueryStartExecutionNode.h"
#include "Aql/WalkerWorker.h"
#include "Logger/LogMacros.h"
#include "RestServer/QueryRegistryFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

#include "velocypack/Builder.h"
#include "velocypack/Slice.h"
#include "velocypack/velocypack-aliases.h"

#include "../IResearch/IResearchQueryCommon.h"
#include "../Mocks/Servers.h"


using namespace arangodb::aql;
using namespace arangodb::containers;

namespace {
struct Comparator final : public WalkerWorker<ExecutionNode> {
  Comparator(Comparator const&) = delete;
  Comparator& operator=(Comparator const&) = delete;

  Comparator(ExecutionPlan* other, bool expectNormalSubqueries)
      : _other(other), _expectNormalSubqueries(expectNormalSubqueries) {}

  static bool isSubqueryRelated(ExecutionNode* en) {
    return en->getType() == ExecutionNode::SUBQUERY ||
           en->getType() == ExecutionNode::SUBQUERY_START ||
           en->getType() == ExecutionNode::SUBQUERY_END ||
           en->getType() == ExecutionNode::SINGLETON;
  }

  bool before(ExecutionNode* en) final {
    std::set<size_t> depids, otherdepids;

    if (!isSubqueryRelated(en)) {
      try {
        auto otherNode = _other->getNodeById(en->id());
        auto otherDeps = otherNode->getDependencies();

        for (auto d : otherDeps) {
          if (!isSubqueryRelated(d)) {
            otherdepids.insert(d->id());
          }
        }

        for (auto d : en->getDependencies()) {
          if (!isSubqueryRelated(d)) {
            depids.insert(d->id());
          }
        }

        EXPECT_EQ(depids, otherdepids);
      } catch (...) {
        EXPECT_TRUE(false) << "expected node with id " << en->id() << " of type "
                           << en->getTypeString() << " to be present in optimized plan";
      }
    }
    return false;
  }

  void after(ExecutionNode* en) final {}

  bool enterSubquery(ExecutionNode*, ExecutionNode* sub) final {
    EXPECT_TRUE(_expectNormalSubqueries) << "Optimized plan must not contain SUBQUERY nodes";
    return false;
  }

 private:
  ExecutionPlan* _other{};
  bool _expectNormalSubqueries{};
};

}  // namespace

namespace arangodb {
namespace tests {
namespace aql {

class SpliceSubqueryNodeOptimizerRuleTest : public ::testing::Test {
 protected:
  mocks::MockAqlServer server;
  QueryRegistry* queryRegistry{QueryRegistryFeature::registry()};

  std::shared_ptr<VPackBuilder> enableRuleOptions(const char* const additionalOptionsString) const {
    auto const additionalOptions = VPackParser::fromJson(additionalOptionsString);
    auto const enableRuleOptionString = R"({"optimizer": { "rules": [ "+splice-subqueries" ] } })";
    return std::make_shared<VPackBuilder>(basics::VelocyPackHelper::merge(
        VPackParser::fromJson(enableRuleOptionString)->slice(),
        additionalOptions->slice(), false, false));
  }

  std::shared_ptr<VPackBuilder> disableRuleOptions(const char* const additionalOptionsString) const {
    auto const additionalOptions = VPackParser::fromJson(additionalOptionsString);
    auto const disableRuleOptionString =
        R"({"optimizer": { "rules": [ "-splice-subqueries" ] } })";
    return std::make_shared<VPackBuilder>(basics::VelocyPackHelper::merge(
        VPackParser::fromJson(disableRuleOptionString)->slice(),
        additionalOptions->slice(), false, false));
  }

  // The last parameter, expectedNumberOfUnsplicedSubqueries, can be removed
  // (and replaced with zero everywhere), as soon as the optimizer rule
  // splice-subqueries is enabled for all subqueries, after the subquery
  // implementation with shadow rows works with skipping, too.
  void verifySubquerySplicing(std::string const& querystring,
                              size_t const expectedNumberOfSplicedSubqueries,
                              size_t const expectedNumberOfUnsplicedSubqueries = 0,
                              const char* const bindParameters = "{}",
                              const char* const additionalOptions = "{}") {
    auto const expectedNumberOfSubqueries =
        expectedNumberOfSplicedSubqueries + expectedNumberOfUnsplicedSubqueries;
    ASSERT_NE(queryRegistry, nullptr)
              << "query string: " << querystring;
    ASSERT_EQ(queryRegistry->numberRegisteredQueries(), 0)
              << "query string: " << querystring;

    auto const bindParamVpack = VPackParser::fromJson(bindParameters);
    arangodb::aql::Query notSplicedQuery(false, server.getSystemDatabase(),
                                         arangodb::aql::QueryString(querystring), bindParamVpack,
                                         disableRuleOptions(additionalOptions),
                                         arangodb::aql::PART_MAIN);
    notSplicedQuery.prepare(queryRegistry, SerializationFormat::SHADOWROWS);
    ASSERT_EQ(queryRegistry->numberRegisteredQueries(), 0)
              << "query string: " << querystring;

    auto notSplicedPlan = notSplicedQuery.plan();
    ASSERT_NE(notSplicedPlan, nullptr)
              << "query string: " << querystring;

    SmallVector<ExecutionNode*>::allocator_type::arena_type a;
    SmallVector<ExecutionNode*> notSplicedSubqueryNodes{a};
    SmallVector<ExecutionNode*> notSplicedSubqueryStartNodes{a};
    SmallVector<ExecutionNode*> notSplicedSubqueryEndNodes{a};

    notSplicedPlan->findNodesOfType(notSplicedSubqueryNodes, ExecutionNode::SUBQUERY, true);
    notSplicedPlan->findNodesOfType(notSplicedSubqueryStartNodes,
                                    ExecutionNode::SUBQUERY_START, true);
    notSplicedPlan->findNodesOfType(notSplicedSubqueryEndNodes,
                                    ExecutionNode::SUBQUERY_END, true);

    arangodb::aql::Query splicedQuery(false, server.getSystemDatabase(),
                                      arangodb::aql::QueryString(querystring), bindParamVpack,
                                      enableRuleOptions(additionalOptions),
                                      arangodb::aql::PART_MAIN);
    splicedQuery.prepare(queryRegistry, SerializationFormat::SHADOWROWS);
    ASSERT_EQ(queryRegistry->numberRegisteredQueries(), 0)
              << "query string: " << querystring;

    auto splicedPlan = splicedQuery.plan();
    ASSERT_NE(splicedPlan, nullptr)
              << "query string: " << querystring;

    SmallVector<ExecutionNode*> splicedSubqueryNodes{a};
    SmallVector<ExecutionNode*> splicedSubqueryStartNodes{a};
    SmallVector<ExecutionNode*> splicedSubqueryEndNodes{a};
    SmallVector<ExecutionNode*> splicedSubquerySingletonNodes{a};
    splicedPlan->findNodesOfType(splicedSubqueryNodes, ExecutionNode::SUBQUERY, true);
    splicedPlan->findNodesOfType(splicedSubqueryStartNodes,
                                 ExecutionNode::SUBQUERY_START, true);
    splicedPlan->findNodesOfType(splicedSubqueryEndNodes, ExecutionNode::SUBQUERY_END, true);

    splicedPlan->findNodesOfType(splicedSubquerySingletonNodes,
                                 ExecutionNode::SINGLETON, true);

    EXPECT_EQ(0, notSplicedSubqueryStartNodes.size())
      << "query string: " << querystring;
    EXPECT_EQ(0, notSplicedSubqueryEndNodes.size())
              << "query string: " << querystring;

    EXPECT_EQ(expectedNumberOfUnsplicedSubqueries, splicedSubqueryNodes.size())
              << "query string: " << querystring;
    EXPECT_EQ(expectedNumberOfSplicedSubqueries, splicedSubqueryStartNodes.size())
              << "query string: " << querystring;
    EXPECT_EQ(expectedNumberOfSplicedSubqueries, splicedSubqueryEndNodes.size())
              << "query string: " << querystring;
    EXPECT_EQ(expectedNumberOfSubqueries, notSplicedSubqueryNodes.size())
              << "query string: " << querystring;

    EXPECT_EQ(splicedSubquerySingletonNodes.size(), 1 + expectedNumberOfUnsplicedSubqueries)
              << "query string: " << querystring;

    // Make sure no nodes got lost (currently does not check SubqueryNodes,
    // SubqueryStartNode, SubqueryEndNode correctness)
    Comparator compare(notSplicedPlan, expectedNumberOfSubqueries > 0);
    splicedPlan->root()->walk(compare);
  }

  void verifyQueryResult(std::string const& query, VPackSlice const expected,
                         const char* const bindParameters = "{}",
                         const char* const additionalOptions = "{}") {
    auto const bindParamVPack = VPackParser::fromJson(bindParameters);
    SCOPED_TRACE("Query: " + query);
    // First test original Query (rule-disabled)
    {
      auto queryResult =
          arangodb::tests::executeQuery(server.getSystemDatabase(), query, bindParamVPack,
                                        disableRuleOptions(additionalOptions)->toJson());
      SCOPED_TRACE("rule was disabled");
      AssertQueryResultToSlice(queryResult, expected);
    }

    // Second test optimized Query (rule-enabled)
    {
      auto queryResult =
          arangodb::tests::executeQuery(server.getSystemDatabase(), query, bindParamVPack,
                                        enableRuleOptions(additionalOptions)->toJson());
      SCOPED_TRACE("rule was enabled");
      AssertQueryResultToSlice(queryResult, expected);
    }
  }

 public:
  SpliceSubqueryNodeOptimizerRuleTest() {}

};  // namespace aql

TEST_F(SpliceSubqueryNodeOptimizerRuleTest, splice_subquery_no_subquery_plan) {
  auto query = "RETURN 15";
  verifySubquerySplicing(query, 0);

  auto expected = arangodb::velocypack::Parser::fromJson(R"([15])");
  verifyQueryResult(query, expected->slice());
}

TEST_F(SpliceSubqueryNodeOptimizerRuleTest, splice_subquery_single_input) {
  auto query = R"aql(FOR d IN 1..1
                      LET first =
                        (RETURN 1)
                      RETURN first)aql";
  verifySubquerySplicing(query, 1);

  auto expected = arangodb::velocypack::Parser::fromJson(R"([[1]])");
  verifyQueryResult(query, expected->slice());
}

TEST_F(SpliceSubqueryNodeOptimizerRuleTest, splice_subquery_plan) {
  auto query = R"aql(FOR d IN 1..2
                      LET first =
                        (FOR e IN 1..2 FILTER d == e RETURN e)
                      RETURN first)aql";
  verifySubquerySplicing(query, 1);

  auto expected = arangodb::velocypack::Parser::fromJson(R"([[1],[2]])");
  verifyQueryResult(query, expected->slice());
}

TEST_F(SpliceSubqueryNodeOptimizerRuleTest, splice_subquery_in_subquery_plan) {
  auto query = R"aql(FOR d IN 1..2
                        LET first = (FOR e IN 1..2
                                        LET second = (FOR f IN 1..2 RETURN f)
                                        FILTER d == e RETURN [e, second])
                         RETURN first)aql";
  verifySubquerySplicing(query, 2);

  auto expected = arangodb::velocypack::Parser::fromJson(R"([
    [[1, [1,2]]],
    [[2, [1,2]]]
  ])");
  verifyQueryResult(query, expected->slice());
}

TEST_F(SpliceSubqueryNodeOptimizerRuleTest, splice_subquery_after_subquery_plan) {
  auto query = R"aql(FOR d IN 1..2
                        LET first = (FOR e IN 1..2 FILTER d == e RETURN e)
                        LET second = (FOR e IN 1..2 FILTER d != e RETURN e)
                        RETURN [first, second])aql";
  verifySubquerySplicing(query, 2);

  auto expected = arangodb::velocypack::Parser::fromJson(R"([[[1],[2]],[[2],[1]]])");
  verifyQueryResult(query, expected->slice());
}

TEST_F(SpliceSubqueryNodeOptimizerRuleTest, splice_subquery_with_sort) {
  auto query = R"aql(
    FOR i IN 1..2
      LET sorted = (
        FOR k IN [1,5,3,2,4,6]
          LET h = k * i
          SORT h DESC
        RETURN h
      )
      RETURN sorted)aql";
  verifySubquerySplicing(query, 1);

  auto expected = arangodb::velocypack::Parser::fromJson(R"([[6,5,4,3,2,1], [12,10,8,6,4,2]])");
  verifyQueryResult(query, expected->slice());
}

// Must be changed as soon as the subquery implementation with shadow rows handle skipping,
// and the splice-subqueries optimizer rule is changed to allow it.
TEST_F(SpliceSubqueryNodeOptimizerRuleTest, dont_splice_subquery_with_skip__inner_limit_offset) {
  auto const queryString = R"aql(FOR i IN 0..2
    LET a = (FOR j IN 0..2 LIMIT 1, 1 RETURN 3*i + j)
    RETURN FIRST(a))aql";
  auto const expectedString = R"res([1, 4, 7])res";

  verifySubquerySplicing(queryString, 0, 1);
  auto expected = arangodb::velocypack::Parser::fromJson(expectedString);
  verifyQueryResult(queryString, expected->slice());
}

// Must be changed as soon as the subquery implementation with shadow rows handle skipping,
// and the splice-subqueries optimizer rule is changed to allow it.
TEST_F(SpliceSubqueryNodeOptimizerRuleTest, dont_splice_subquery_with_skip__outer_limit_offset) {
  auto const queryString = R"aql(FOR i IN 0..2
    LET a = (FOR j IN 0..2 RETURN 3*i + j)
    LIMIT 1, 1
    RETURN FIRST(a))aql";
  auto const expectedString = R"res([3])res";

  verifySubquerySplicing(queryString, 0, 1);
  auto expected = arangodb::velocypack::Parser::fromJson(expectedString);
  verifyQueryResult(queryString, expected->slice());
}

// Must be changed as soon as the subquery implementation with shadow rows handle skipping,
// and the splice-subqueries optimizer rule is changed to allow it.
TEST_F(SpliceSubqueryNodeOptimizerRuleTest, dont_splice_subquery_with_skip__inner_collect_count) {
  auto const queryString = R"aql(FOR i IN 0..2
    LET a = (FOR j IN 0..i COLLECT WITH COUNT INTO n RETURN n)
    RETURN FIRST(a))aql";
  auto const expectedString = R"res([1, 2, 3])res";

  verifySubquerySplicing(queryString, 0, 1);
  auto expected = arangodb::velocypack::Parser::fromJson(expectedString);
  verifyQueryResult(queryString, expected->slice());
}

// Must be changed as soon as the subquery implementation with shadow rows handle skipping,
// and the splice-subqueries optimizer rule is changed to allow it.
TEST_F(SpliceSubqueryNodeOptimizerRuleTest, dont_splice_subquery_with_skip__outer_collect_count) {
  // the RAND() is there to avoid the subquery being removed
  auto const queryString = R"aql(FOR i IN 0..2
    LET a = (FOR j IN 0..FLOOR(2*RAND()) RETURN 1)
    COLLECT WITH COUNT INTO n
    RETURN n)aql";
  auto const expectedString = R"res([3])res";

  verifySubquerySplicing(queryString, 0, 1);
  auto expected = arangodb::velocypack::Parser::fromJson(expectedString);
  verifyQueryResult(queryString, expected->slice());
}

// Must be changed as soon as the subquery implementation with shadow rows handle skipping,
// and the splice-subqueries optimizer rule is changed to allow it.
TEST_F(SpliceSubqueryNodeOptimizerRuleTest, dont_splice_subquery_with_skip__full_count) {
  // the RAND() is there to avoid the subquery being removed
  auto const queryString = R"aql(FOR i IN 0..2
    LET a = (FOR j IN 0..FLOOR(2*RAND()) RETURN 1)
    LIMIT 1
    RETURN i)aql";
  auto const expectedString = R"res([0])res";

  verifySubquerySplicing(queryString, 0, 1, "{}", R"opts({"fullCount": true})opts");
  auto expected = arangodb::velocypack::Parser::fromJson(expectedString);
  verifyQueryResult(queryString, expected->slice());
}

// Must be changed as soon as the subquery implementation with shadow rows handle skipping,
// and the splice-subqueries optimizer rule is changed to allow it.
TEST_F(SpliceSubqueryNodeOptimizerRuleTest, splice_nested_subquery) {
  auto const queryString = R"aql(
    FOR i IN 0..1
    LET js = ( // this subquery should be spliced
      FOR j IN 0..1 + FLOOR(RAND())
      LET ks = ( // this subquery should be spliced
        FOR k IN 0..1 + FLOOR(RAND())
        RETURN 4*i + 2*j + k
      )
      RETURN ks
    )
    RETURN js
  )aql";
  auto const expectedString = R"res([[[0, 1], [2, 3]], [[4, 5], [6, 7]]])res";

  verifySubquerySplicing(queryString, 2);
  auto expected = arangodb::velocypack::Parser::fromJson(expectedString);
  verifyQueryResult(queryString, expected->slice());
}

// Must be changed as soon as the subquery implementation with shadow rows handle skipping,
// and the splice-subqueries optimizer rule is changed to allow it.
TEST_F(SpliceSubqueryNodeOptimizerRuleTest, splice_nested_subquery_with_innermost_skip) {
  auto const queryString = R"aql(
    FOR i IN 0..1
    LET js = ( // this subquery should be spliced
      FOR j IN 0..1 + FLOOR(RAND())
      LET ks = ( // this subquery should not be spliced
        FOR k IN 0..2 + FLOOR(RAND())
        LIMIT 1, 2
        RETURN 6*i + 3*j + k
      )
      RETURN ks
    )
    RETURN js
  )aql";
  auto const expectedString = R"res([[[1, 2], [4, 5]], [[7, 8], [10, 11]]])res";

  verifySubquerySplicing(queryString, 1, 1);
  auto expected = arangodb::velocypack::Parser::fromJson(expectedString);
  verifyQueryResult(queryString, expected->slice());
}

// Must be changed as soon as the subquery implementation with shadow rows handle skipping,
// and the splice-subqueries optimizer rule is changed to allow it.
// This is a test for a specific problem where constant subqueries did not work
// inside spliced subqueries correctly.
TEST_F(SpliceSubqueryNodeOptimizerRuleTest, splice_nested_subquery_with_innermost_constant_skip) {
  auto const queryString = R"aql(
    FOR a IN 1..2
      LET b = (RETURN (FOR c IN 2..4 LIMIT 1, null RETURN c) )
    RETURN {a, b}
  )aql";
  auto const expectedString = R"res([{"a": 1, "b": [[3, 4]]}, {"a": 2, "b": [[3, 4]]}])res";

  verifySubquerySplicing(queryString, 1, 1);
  auto expected = arangodb::velocypack::Parser::fromJson(expectedString);
  verifyQueryResult(queryString, expected->slice());
}

// Must be changed as soon as the subquery implementation with shadow rows handle skipping,
// and the splice-subqueries optimizer rule is changed to allow it.
TEST_F(SpliceSubqueryNodeOptimizerRuleTest, splice_nested_subquery_with_outermost_skip) {
  auto const queryString = R"aql(
    FOR i IN 0..2
    LET js = ( // this subquery should not be spliced
      FOR j IN 0..1 + FLOOR(RAND())
      LET ks = ( // this subquery should be spliced
        FOR k IN 0..1 + FLOOR(RAND())
        RETURN 4*i + 2*j + k
      )
      RETURN ks
    )
    LIMIT 1, 2
    RETURN js
  )aql";
  auto const expectedString = R"res([[[4, 5], [6, 7]], [[8, 9], [10, 11]]])res";

  verifySubquerySplicing(queryString, 1, 1);
  auto expected = arangodb::velocypack::Parser::fromJson(expectedString);
  verifyQueryResult(queryString, expected->slice());
}

// Must be changed as soon as the subquery implementation with shadow rows handle skipping,
// and the splice-subqueries optimizer rule is changed to allow it.
TEST_F(SpliceSubqueryNodeOptimizerRuleTest, dont_splice_subquery_with_limit_and_no_offset) {
  auto query = R"aql(
    FOR i IN 2..4
      LET a = (FOR j IN [i, i+10, i+20] LIMIT 0, 1 RETURN j)
      RETURN FIRST(a))aql";
  verifySubquerySplicing(query, 0, 1);

  auto expected = arangodb::velocypack::Parser::fromJson(R"([2, 3, 4])");
  verifyQueryResult(query, expected->slice());
}

// Regression test for https://github.com/arangodb/arangodb/issues/10852
TEST_F(SpliceSubqueryNodeOptimizerRuleTest, splice_nested_empty_subqueries) {
  auto const queryString = R"aql(
    LET results = (
        FOR doc IN []
            LET docs = (
                FOR doc2 in []
                    RETURN doc2
            )
            RETURN [docs]
    )

    RETURN [results]
  )aql";
  auto const expectedString = R"res([[[]]])res";

  verifySubquerySplicing(queryString, 2);
  auto expected = arangodb::velocypack::Parser::fromJson(expectedString);
  verifyQueryResult(queryString, expected->slice());
}

// Regression test for https://github.com/arangodb/arangodb/issues/10852
TEST_F(SpliceSubqueryNodeOptimizerRuleTest, splice_subquery_with_upsert) {
  TRI_vocbase_t& vocbase{server.getSystemDatabase()};
  auto const info = VPackParser::fromJson(R"({"name":"UnitTestCollection"})");
  auto const collection = vocbase.createCollection(info->slice());
  auto const queryString = R"aql(
    LET new_id = (
        UPSERT { _key: @key }
        INSERT { _key: @key }
        UPDATE { _key: @key }
        IN UnitTestCollection
        RETURN NEW._id
    )
    RETURN new_id
  )aql";

  auto const bindString = R"bind({"key": "myKey"})bind";
  auto const expectedString = R"res([["UnitTestCollection/myKey"]])res";

  verifySubquerySplicing(queryString, 1, 1, bindString);
  auto expected = arangodb::velocypack::Parser::fromJson(expectedString);
  verifyQueryResult(queryString, expected->slice(), bindString);

  auto const noCollections = std::vector<std::string>{};
  auto const readCollection = std::vector<std::string>{"UnitTestCollection"};
  transaction::Options opts;
  auto ctx = transaction::StandaloneContext::Create(server.getSystemDatabase());
  auto trx = std::make_unique<arangodb::transaction::Methods>(ctx, readCollection, noCollections,
                                                              noCollections, opts);
  ASSERT_EQ(1, collection->numberDocuments(trx.get(), transaction::CountType::Normal));
  auto mdr = ManagedDocumentResult{};
  auto result = collection->read(trx.get(), VPackStringRef{"myKey"}, mdr, false);
  ASSERT_TRUE(result.ok());
  ASSERT_NE(nullptr, mdr.vpack());
  auto const document = VPackSlice{mdr.vpack()};
  ASSERT_TRUE(document.isObject());
  ASSERT_TRUE(document.get("_key").isString());
  ASSERT_EQ(std::string{"myKey"}, document.get("_key").copyString());
}

// Disabled as long as the subquery implementation with shadow rows cannot yet handle skipping.
TEST_F(SpliceSubqueryNodeOptimizerRuleTest, DISABLED_splice_subquery_with_limit_and_offset) {
  auto query = R"aql(
    FOR i IN 2..4
      LET a = (FOR j IN [0, i, i+10] LIMIT 1, 1 RETURN j)
      RETURN FIRST(a))aql";
  verifySubquerySplicing(query, 1);

  auto expected = arangodb::velocypack::Parser::fromJson(R"([2, 3, 4])");
  verifyQueryResult(query, expected->slice());
}

// Disabled as long as the subquery implementation with shadow rows cannot yet handle skipping.
TEST_F(SpliceSubqueryNodeOptimizerRuleTest,
       DISABLED_splice_subquery_collect_within_empty_nested_subquery) {
  auto query = R"aql(
    FOR k IN 1..2
      LET sub1 = (
        FOR x IN []
          LET sub2 = (
            COLLECT WITH COUNT INTO q
            RETURN q
          )
        RETURN sub2
      )
    RETURN [k, sub1])aql";
  verifySubquerySplicing(query, 2);

  auto expected = arangodb::velocypack::Parser::fromJson(R"([[1, []], [2, []]])");
  verifyQueryResult(query, expected->slice());
}

// TODO Check isInSplicedSubquery
// TODO Test cluster rules

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
