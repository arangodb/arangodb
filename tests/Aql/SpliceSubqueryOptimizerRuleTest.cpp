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
#include "Aql/Query.h"
#include "RestServer/QueryRegistryFeature.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/SubqueryStartExecutionNode.h"
#include "Aql/SubqueryEndExecutionNode.h"
#include "Aql/WalkerWorker.h"

#include "Logger/LogMacros.h"

#include "velocypack/Builder.h"
#include "velocypack/velocypack-aliases.h"

#include "../Mocks/Servers.h"

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
      } catch(...) {
        EXPECT_TRUE(false) <<
          "expected node with id " << en->id() << " of type " <<
          en->getTypeString() << " to be present in optimized plan";
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

  void verifySubquerySplicing(std::string querystring) {
    auto notSplicedOptions = arangodb::velocypack::Parser::fromJson(
        "{\"optimizer\": { \"rules\": [ \"-splice-subqueries\" ] } }");

    arangodb::aql::Query notSplicedQuery(false, server.getSystemDatabase(),
                                         arangodb::aql::QueryString(querystring), nullptr,
                                         notSplicedOptions, arangodb::aql::PART_MAIN);
    notSplicedQuery.parse();
    std::unique_ptr<arangodb::aql::ExecutionPlan> notSplicedPlan(notSplicedQuery.stealPlan());

    ASSERT_NE(notSplicedPlan, nullptr);

    SmallVector<ExecutionNode*>::allocator_type::arena_type a;
    SmallVector<ExecutionNode*> notSplicedSubqueryNodes{a},
        notSplicedSubqueryStartNodes{a}, notSplicedSubqueryEndNodes{a};

    notSplicedPlan->findNodesOfType(notSplicedSubqueryNodes, ExecutionNode::SUBQUERY, true);
    notSplicedPlan->findNodesOfType(notSplicedSubqueryStartNodes,
                                    ExecutionNode::SUBQUERY_START, true);
    notSplicedPlan->findNodesOfType(notSplicedSubqueryEndNodes,
                                    ExecutionNode::SUBQUERY_END, true);

    auto splicedOptions = arangodb::velocypack::Parser::fromJson(
        "{\"optimizer\": { \"rules\": [ \"splice-subqueries\" ] } }");
    arangodb::aql::Query splicedQuery(false, server.getSystemDatabase(),
                                      arangodb::aql::QueryString(querystring), nullptr,
                                      splicedOptions, arangodb::aql::PART_MAIN);
    splicedQuery.parse();
    
    std::unique_ptr<arangodb::aql::ExecutionPlan> splicedPlan(splicedQuery.stealPlan());
    ASSERT_NE(notSplicedPlan, nullptr);

    SmallVector<ExecutionNode*> splicedSubqueryNodes{a},
      splicedSubqueryStartNodes{a}, splicedSubqueryEndNodes{a}, splicedSubquerySingletonNodes{a};
    splicedPlan->findNodesOfType(splicedSubqueryNodes, ExecutionNode::SUBQUERY, true);
    splicedPlan->findNodesOfType(splicedSubqueryStartNodes,
                                 ExecutionNode::SUBQUERY_START, true);
    splicedPlan->findNodesOfType(splicedSubqueryEndNodes, ExecutionNode::SUBQUERY_END, true);

    splicedPlan->findNodesOfType(splicedSubquerySingletonNodes, ExecutionNode::SINGLETON, true);

    EXPECT_EQ(notSplicedSubqueryStartNodes.size(), 0);
    EXPECT_EQ(notSplicedSubqueryEndNodes.size(), 0);

    EXPECT_EQ(splicedSubqueryNodes.size(), 0);
    EXPECT_EQ(notSplicedSubqueryNodes.size(), splicedSubqueryStartNodes.size());
    EXPECT_EQ(notSplicedSubqueryNodes.size(), splicedSubqueryEndNodes.size());

    EXPECT_EQ(splicedSubquerySingletonNodes.size(), 1);

    // Make sure no nodes got lost (currently does not check SubqueryNodes,
    // SubqueryStartNode, SubqueryEndNode correctness)
    Comparator compare(notSplicedPlan.get());
    splicedPlan->root()->walk(compare);
  }

 public:
  SpliceSubqueryNodeOptimizerRuleTest() {}
};

TEST_F(SpliceSubqueryNodeOptimizerRuleTest, splice_subquery_no_subquery_plan) {
  verifySubquerySplicing("RETURN 15");
}

TEST_F(SpliceSubqueryNodeOptimizerRuleTest, splice_subquery_plan) {
  verifySubquerySplicing(R"aql(FOR d IN [1..2]
                                 LET first =
                                   (FOR e IN [1..2] FILTER d == e RETURN e)
                               RETURN first)aql");
}

TEST_F(SpliceSubqueryNodeOptimizerRuleTest, splice_subquery_in_subquery_plan) {
  verifySubquerySplicing(R"aql(FOR d IN [1..2]
                                 LET first = (FOR e IN [1..2]
                                                LET second = (FOR f IN [1..2] RETURN f)
                                              FILTER d == e RETURN e)
                                     RETURN first)aql");
}

TEST_F(SpliceSubqueryNodeOptimizerRuleTest, splice_subquery_after_subquery_plan) {
  verifySubquerySplicing(R"aql(FOR d IN [1..2]
                                 LET first =
                                   (FOR e IN [1..2] FILTER d == e RETURN e)
                               RETURN first)aql");
}

}  // namespace aql
} // namespace tests
} // namespace arangodb
