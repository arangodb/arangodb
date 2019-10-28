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
#include "RestServer/QueryRegistryFeature.h"

#include "velocypack/Builder.h"
#include "velocypack/Slice.h"
#include "velocypack/velocypack-aliases.h"

#include "../IResearch/IResearchQueryCommon.h"
#include "../Mocks/Servers.h"

#include "Logger/LogMacros.h"

using namespace arangodb::aql;
using namespace arangodb::containers;

namespace {
struct Comparator final : public WalkerWorker<ExecutionNode> {
  Comparator(Comparator const&) = delete;
  Comparator& operator=(Comparator const&) = delete;

  Comparator(ExecutionPlan* other) : _other(other) {}

  bool isSubqueryRelated(ExecutionNode* en) {
    return en->getType() == ExecutionNode::SUBQUERY ||
           en->getType() == ExecutionNode::SUBQUERY_START ||
           en->getType() == ExecutionNode::SUBQUERY_END ||
           en->getType() == ExecutionNode::SINGLETON;
  }

  bool before(ExecutionNode* en) override final {
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

  void after(ExecutionNode* en) override final {}

  bool enterSubquery(ExecutionNode*, ExecutionNode* sub) override final {
    EXPECT_TRUE(false) << "Optimized plan must not contain SUBQUERY nodes";
    return false;
  }
  ExecutionPlan* _other;
};

}  // namespace

namespace arangodb {
namespace tests {
namespace aql {

class SpliceSubqueryNodeOptimizerRuleTest : public ::testing::Test {
 protected:
  mocks::MockAqlServer server;
  QueryRegistry* queryRegistry{QueryRegistryFeature::registry()};

  std::string const enableRuleOptions() const {
    return R"({"optimizer": { "rules": [ "+splice-subqueries" ] } })";
  }

  std::string const disableRuleOptions() const {
    return R"({"optimizer": { "rules": [ "-splice-subqueries" ] } })";
  }

  void verifySubquerySplicing(std::string const& querystring, size_t expectedNumberOfNodes) {
    ASSERT_NE(queryRegistry, nullptr);
    ASSERT_EQ(queryRegistry->numberRegisteredQueries(), 0);

    auto const bindParameters = VPackParser::fromJson("{ }");
    arangodb::aql::Query notSplicedQuery(false, server.getSystemDatabase(),
                                         arangodb::aql::QueryString(querystring), nullptr,
                                         VPackParser::fromJson(disableRuleOptions()),
                                         arangodb::aql::PART_MAIN);
    notSplicedQuery.prepare(queryRegistry, SerializationFormat::SHADOWROWS);
    ASSERT_EQ(queryRegistry->numberRegisteredQueries(), 0);

    auto notSplicedPlan = notSplicedQuery.plan();
    ASSERT_NE(notSplicedPlan, nullptr);

    SmallVector<ExecutionNode*>::allocator_type::arena_type a;
    SmallVector<ExecutionNode*> notSplicedSubqueryNodes{a},
        notSplicedSubqueryStartNodes{a}, notSplicedSubqueryEndNodes{a};

    notSplicedPlan->findNodesOfType(notSplicedSubqueryNodes, ExecutionNode::SUBQUERY, true);
    notSplicedPlan->findNodesOfType(notSplicedSubqueryStartNodes,
                                    ExecutionNode::SUBQUERY_START, true);
    notSplicedPlan->findNodesOfType(notSplicedSubqueryEndNodes,
                                    ExecutionNode::SUBQUERY_END, true);

    arangodb::aql::Query splicedQuery(false, server.getSystemDatabase(),
                                      arangodb::aql::QueryString(querystring), nullptr,
                                      VPackParser::fromJson(enableRuleOptions()),
                                      arangodb::aql::PART_MAIN);
    splicedQuery.prepare(queryRegistry, SerializationFormat::SHADOWROWS);
    ASSERT_EQ(queryRegistry->numberRegisteredQueries(), 0);

    auto splicedPlan = splicedQuery.plan();
    ASSERT_NE(splicedPlan, nullptr);

    SmallVector<ExecutionNode*> splicedSubqueryNodes{a}, splicedSubqueryStartNodes{a},
        splicedSubqueryEndNodes{a}, splicedSubquerySingletonNodes{a};
    splicedPlan->findNodesOfType(splicedSubqueryNodes, ExecutionNode::SUBQUERY, true);
    splicedPlan->findNodesOfType(splicedSubqueryStartNodes,
                                 ExecutionNode::SUBQUERY_START, true);
    splicedPlan->findNodesOfType(splicedSubqueryEndNodes, ExecutionNode::SUBQUERY_END, true);

    splicedPlan->findNodesOfType(splicedSubquerySingletonNodes,
                                 ExecutionNode::SINGLETON, true);

    EXPECT_EQ(notSplicedSubqueryStartNodes.size(), 0);
    EXPECT_EQ(notSplicedSubqueryEndNodes.size(), 0);

    EXPECT_EQ(splicedSubqueryNodes.size(), 0);
    EXPECT_EQ(notSplicedSubqueryNodes.size(), splicedSubqueryStartNodes.size());
    EXPECT_EQ(notSplicedSubqueryNodes.size(), splicedSubqueryEndNodes.size());
    EXPECT_EQ(notSplicedSubqueryNodes.size(), expectedNumberOfNodes);

    EXPECT_EQ(splicedSubquerySingletonNodes.size(), 1);

    // Make sure no nodes got lost (currently does not check SubqueryNodes,
    // SubqueryStartNode, SubqueryEndNode correctness)
    Comparator compare(notSplicedPlan);
    splicedPlan->root()->walk(compare);
  }

  void verifyQueryResult(std::string const& query, VPackSlice expected) {
    auto const bindParameters = VPackParser::fromJson("{ }");
    SCOPED_TRACE("Query: " + query);
    // First test original Query (rule-disabled)
    {
      auto queryResult =
          arangodb::tests::executeQuery(server.getSystemDatabase(), query,
                                        bindParameters, disableRuleOptions());
      SCOPED_TRACE("rule was disabled");
      AssertQueryResultToSlice(queryResult, expected);
    }

    // Second test optimized Query (rule-enabled)
    {
      auto queryResult =
          arangodb::tests::executeQuery(server.getSystemDatabase(), query,
                                        bindParameters, enableRuleOptions());
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

TEST_F(SpliceSubqueryNodeOptimizerRuleTest, DISABLED_splice_subquery_single_input) {
  auto query = R"aql(FOR d IN 1..1
                      LET first =
                        (RETURN 1)
                      RETURN first)aql";
  verifySubquerySplicing(query, 1);

  auto expected = arangodb::velocypack::Parser::fromJson(R"([[1]])");
  verifyQueryResult(query, expected->slice());
}

TEST_F(SpliceSubqueryNodeOptimizerRuleTest, DISABLED_splice_subquery_plan) {
  auto query = R"aql(FOR d IN 1..2
                      LET first =
                        (FOR e IN 1..2 FILTER d == e RETURN e)
                      RETURN first)aql";
  verifySubquerySplicing(query, 1);

  auto expected = arangodb::velocypack::Parser::fromJson(R"([[1],[2]])");
  verifyQueryResult(query, expected->slice());
}

TEST_F(SpliceSubqueryNodeOptimizerRuleTest, DISABLED_splice_subquery_in_subquery_plan) {
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

TEST_F(SpliceSubqueryNodeOptimizerRuleTest, DISABLED_splice_subquery_after_subquery_plan) {
  auto query = R"aql(FOR d IN 1..2
                        LET first = (FOR e IN 1..2 FILTER d == e RETURN e)
                        LET second = (FOR e IN 1..2 FILTER d != e RETURN e)
                        RETURN [first, second])aql";
  verifySubquerySplicing(query, 2);

  auto expected = arangodb::velocypack::Parser::fromJson(R"([[[1],[2]],[[2],[1]]])");
  verifyQueryResult(query, expected->slice());
}

TEST_F(SpliceSubqueryNodeOptimizerRuleTest, DISABLED_splice_subquery_with_sort) {
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

TEST_F(SpliceSubqueryNodeOptimizerRuleTest, DISABLED_splice_subquery_with_limit_and_offset) {
  auto query = R"aql(
    FOR i IN 2..4
        LET a = (FOR j IN [0, i, i+10] LIMIT 1, 1 RETURN j)
        RETURN FIRST(a))aql";
  verifySubquerySplicing(query, 1);

  auto expected = arangodb::velocypack::Parser::fromJson(R"([2, 3, 4])");
  verifyQueryResult(query, expected->slice());
}

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

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
