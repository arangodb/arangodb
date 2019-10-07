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

  std::string const enableRuleOptions() const {
    return R"({"optimizer": { "rules": [ "+splice-subqueries" ] } })";
  }

  std::string const disableRuleOptions() const {
    return R"({"optimizer": { "rules": [ "-splice-subqueries" ] } })";
  }

  void verifySubquerySplicing(std::string const& querystring, size_t expectedNumberOfNodes) {
    auto const bindParameters = VPackParser::fromJson("{ }");
    /*
    {
      arangodb::aql::Query hund(false, server.getSystemDatabase(),
                                arangodb::aql::QueryString(querystring),
    nullptr, disableRuleOptions(), arangodb::aql::PART_MAIN); auto res =
    hund.explain(); LOG_DEVEL << res.data->toJson();
    }

    arangodb::aql::Query notSplicedQuery(false, server.getSystemDatabase(),
                                         arangodb::aql::QueryString(querystring),
                                         nullptr, disableRuleOptions(),
                                         arangodb::aql::PART_MAIN);
    auto explainResult = notSplicedQuery.explain();
    ASSERT_TRUE(explainResult.ok()) << "unable to parse the given plan";
    auto notSplicedPlan = notSplicedQuery.plan();
*/
    auto notSplicedPlan =
        arangodb::tests::planFromQuery(server.getSystemDatabase(), querystring,
                                       bindParameters, disableRuleOptions());

    ASSERT_NE(notSplicedPlan, nullptr);

    SmallVector<ExecutionNode*>::allocator_type::arena_type a;
    SmallVector<ExecutionNode*> notSplicedSubqueryNodes{a},
        notSplicedSubqueryStartNodes{a}, notSplicedSubqueryEndNodes{a};

    notSplicedPlan->findNodesOfType(notSplicedSubqueryNodes, ExecutionNode::SUBQUERY, true);
    notSplicedPlan->findNodesOfType(notSplicedSubqueryStartNodes,
                                    ExecutionNode::SUBQUERY_START, true);
    notSplicedPlan->findNodesOfType(notSplicedSubqueryEndNodes,
                                    ExecutionNode::SUBQUERY_END, true);
    /*
    arangodb::aql::Query splicedQuery(false, server.getSystemDatabase(),
                                      arangodb::aql::QueryString(querystring),
    nullptr, enableRuleOptions(), arangodb::aql::PART_MAIN);
    splicedQuery.parse();
        auto splicedPlan = std::move(splicedQuery.stealPlan());
    */

    auto splicedPlan =
        arangodb::tests::planFromQuery(server.getSystemDatabase(), querystring,
                                       bindParameters, disableRuleOptions());
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
    {
      LOG_DEVEL << "Check nodes";
      for (auto const& it : notSplicedSubqueryNodes) {
        LOG_DEVEL << "Found node: " << it->id() << " of type " << it->getTypeString();
      }
      LOG_DEVEL << notSplicedPlan->toVelocyPack(notSplicedPlan->getAst(), false)->toJson();
    }

    EXPECT_EQ(splicedSubquerySingletonNodes.size(), 1);

    // Make sure no nodes got lost (currently does not check SubqueryNodes,
    // SubqueryStartNode, SubqueryEndNode correctness)
    Comparator compare(notSplicedPlan.get());
    splicedPlan->root()->walk(compare);
  }

  void compareQueryResultToSlice(QueryResult const& result, bool ruleEnabled,
                                 VPackSlice expected) {
    ASSERT_TRUE(expected.isArray()) << "Invalid input";
    ASSERT_TRUE(result.ok())
        << "Reason: " << result.errorNumber() << " => " << result.errorMessage()
        << " rule was on: " << std::boolalpha << ruleEnabled;
    auto resultSlice = result.data->slice();
    ASSERT_TRUE(resultSlice.isArray()) << " rule was on: " << std::boolalpha << ruleEnabled;
    ASSERT_EQ(expected.length(), resultSlice.length())
        << " rule was on: " << std::boolalpha << ruleEnabled;
    for (VPackValueLength i = 0; i < expected.length(); ++i) {
      EXPECT_TRUE(basics::VelocyPackHelper::equal(resultSlice.at(i), expected.at(i), false))
          << "Line " << i << ": " << resultSlice.at(i).toJson()
          << " != " << expected.at(i).toJson()
          << " rule was on: " << std::boolalpha << ruleEnabled;
    }
  }

  void verifyQueryResult(std::string const& query, VPackSlice expected) {
    auto const bindParameters = VPackParser::fromJson("{ }");

    // First test original Query (rule-disabled)
    {
      auto queryResult =
          arangodb::tests::executeQuery(server.getSystemDatabase(), query,
                                        bindParameters, disableRuleOptions());
      compareQueryResultToSlice(queryResult, false, expected);
    }
    // Second test optimized Query (rule-enabled)
    /*
    {
      auto queryResult =
          arangodb::tests::executeQuery(server.getSystemDatabase(), query, bindParameters,
                                        enableRuleOptions());
      compareQueryResultToSlice(queryResult, true, expected);
    }
    */
  }

 public:
  SpliceSubqueryNodeOptimizerRuleTest() {}
};

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

  auto expected = arangodb::velocypack::Parser::fromJson(R"([[1,2],[2,1]])");
  verifyQueryResult(query, expected->slice());
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
