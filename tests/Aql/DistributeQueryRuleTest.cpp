////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Aql/Query.h"
#include "Transaction/StandaloneContext.h"

#include "../Mocks/Servers.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::transaction;

namespace arangodb {
namespace tests {
namespace aql {

#define useOptimize 1

namespace {
std::string nodeNames(VPackSlice nodes) {
  std::string result;
  for (auto n : VPackArrayIterator(nodes)) {
    if (!result.empty()) {
      result += ", ";
    }
    result += n.get("type").copyString();
  }
  return result;
}
}  // namespace

class DistributeQueryRuleTest : public ::testing::Test {
 protected:
  mocks::MockCoordinator server;

  std::shared_ptr<VPackBuilder> prepareQuery(std::string const& queryString) const {
    auto ctx = std::make_shared<StandaloneContext>(server.getSystemDatabase());
    auto const bindParamVPack = VPackParser::fromJson("{}");
#if useOptimize
    auto const optionsVPack =
        VPackParser::fromJson(R"json({"optimizer": {"rules": ["insert-distribute-calculations", "distribute-query"]}})json");
#else
    auto const optionsVPack = VPackParser::fromJson(R"json({})json");
#endif
    auto query = Query::create(ctx, QueryString(queryString), bindParamVPack,
                               QueryOptions(optionsVPack->slice()));

    // NOTE: We can only get a VPacked Variant of the Plan, we cannot inject deep enough into the query.
    auto res = query->explain();
    if (!res.ok()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(res.errorNumber(), res.errorMessage());
    }
    return res.data;
  }

  void assertNodesMatch(VPackSlice actualNodes, std::vector<std::string> const& expectedNodes) {
    ASSERT_TRUE(actualNodes.isArray());
    for (size_t i = 0; i < expectedNodes.size(); ++i) {
      if (actualNodes.length() > i) {
        auto node = actualNodes.at(i);
        EXPECT_TRUE(node.get("type").isEqualString(expectedNodes.at(i)))
            << node.get("type").toString() << " != " << expectedNodes.at(i);
      } else {
        EXPECT_TRUE(false) << "Too few nodes. expected: " << expectedNodes.size()
                           << ", got: " << actualNodes.length();
      }
    }
    EXPECT_EQ(actualNodes.length(), expectedNodes.size())
        << "Non matching amount of nodes.";
  }

  DistributeQueryRuleTest() {
    // We can register them, but then the API will call count, and the servers
    // do not respond. Now we just get "no endpoint found" but this seems to be
    // okay :shrug: server.registerFakedDBServer("DB1");
    // server.registerFakedDBServer("DB2");
    server.createCollection(server.getSystemDatabase().name(), "collection",
                            {{"s123", "DB1"}, {"s234", "DB2"}},
                            TRI_col_type_e::TRI_COL_TYPE_DOCUMENT);
  }
};

TEST_F(DistributeQueryRuleTest, single_enumerate_collection) {
  auto queryString = "FOR x IN collection RETURN x";
  auto plan = prepareQuery(queryString);

  auto planSlice = plan->slice();
  ASSERT_TRUE(planSlice.hasKey("nodes"));
  planSlice = planSlice.get("nodes");
  assertNodesMatch(planSlice, {"SingletonNode", "EnumerateCollectionNode",
                               "RemoteNode", "GatherNode", "ReturnNode"});
}

TEST_F(DistributeQueryRuleTest, no_collection_access) {
  auto queryString = "FOR x IN [1,2,3] RETURN x";
  auto plan = prepareQuery(queryString);

  auto planSlice = plan->slice();
  ASSERT_TRUE(planSlice.hasKey("nodes"));
  planSlice = planSlice.get("nodes");
  assertNodesMatch(planSlice, {"SingletonNode", "CalculationNode",
                               "EnumerateListNode", "ReturnNode"});
}

TEST_F(DistributeQueryRuleTest, no_collection_access_multiple) {
  auto queryString = "FOR x IN [1,2,3] FOR y IN [1,2,3] RETURN x * y";
  auto plan = prepareQuery(queryString);

  auto planSlice = plan->slice();
  ASSERT_TRUE(planSlice.hasKey("nodes"));
  planSlice = planSlice.get("nodes");
  LOG_DEVEL << nodeNames(planSlice);
  assertNodesMatch(planSlice, {"SingletonNode", "CalculationNode",
                               "EnumerateListNode", "EnumerateListNode",
                               "CalculationNode", "ReturnNode"});
}

TEST_F(DistributeQueryRuleTest, document_then_enumerate) {
  auto queryString = R"aql(
    LET doc = DOCUMENT("collection/abc")
      FOR x IN collection
      FILTER x._key == doc.name
      RETURN x)aql";
  auto plan = prepareQuery(queryString);

  auto planSlice = plan->slice();
  ASSERT_TRUE(planSlice.hasKey("nodes"));
  planSlice = planSlice.get("nodes");
  assertNodesMatch(planSlice,
                   {"SingletonNode", "CalculationNode", "ScatterNode",
                    "RemoteNode", "EnumerateCollectionNode", "CalculationNode",
                    "FilterNode", "RemoteNode", "GatherNode", "ReturnNode"});
}

TEST_F(DistributeQueryRuleTest, many_enumerate_collections) {
  auto queryString = R"aql(
    FOR x IN collection
      FOR y IN collection
      RETURN {x,y})aql";
  auto plan = prepareQuery(queryString);

  auto planSlice = plan->slice();
  ASSERT_TRUE(planSlice.hasKey("nodes"));
  planSlice = planSlice.get("nodes");
  assertNodesMatch(planSlice,
                   {"SingletonNode", "EnumerateCollectionNode", "RemoteNode",
                    "GatherNode", "ScatterNode", "RemoteNode",
                    "EnumerateCollectionNode", "CalculationNode", "RemoteNode",
                    "GatherNode", "ReturnNode"});
}

TEST_F(DistributeQueryRuleTest, single_insert) {
  auto queryString = R"aql( INSERT {} INTO collection)aql";
  auto plan = prepareQuery(queryString);

  auto planSlice = plan->slice();
  ASSERT_TRUE(planSlice.hasKey("nodes"));
  planSlice = planSlice.get("nodes");
  assertNodesMatch(planSlice, {"SingletonNode", "CalculationNode",
                               "SingleRemoteOperationNode"});
}

TEST_F(DistributeQueryRuleTest, multiple_inserts) {
  auto queryString = R"aql(
    FOR x IN 1..3
    INSERT {} INTO collection)aql";
  auto plan = prepareQuery(queryString);

  auto planSlice = plan->slice();
  ASSERT_TRUE(planSlice.hasKey("nodes"));
  planSlice = planSlice.get("nodes");
  assertNodesMatch(planSlice,
                   {"SingletonNode", "CalculationNode", "CalculationNode",
                    "EnumerateListNode", "CalculationNode", "DistributeNode",
                    "RemoteNode", "InsertNode", "RemoteNode", "GatherNode"});
}

TEST_F(DistributeQueryRuleTest, enumerate_before_insert) {
  auto queryString = R"aql(
    FOR x IN collection
    INSERT {} INTO collection)aql";
  auto plan = prepareQuery(queryString);

  auto planSlice = plan->slice();
  ASSERT_TRUE(planSlice.hasKey("nodes"));
  planSlice = planSlice.get("nodes");
  LOG_DEVEL << nodeNames(planSlice);
  assertNodesMatch(planSlice,
                   {"SingletonNode", "CalculationNode",
                    "EnumerateCollectionNode", "RemoteNode", "GatherNode",
                    "CalculationNode", "DistributeNode", "RemoteNode",
                    "InsertNode", "RemoteNode", "GatherNode"});
}

TEST_F(DistributeQueryRuleTest, enumerate_before_update) {
  // Special case here, we enumerate and Update the same docs.
  // We could get away without network requests in between
  auto queryString = R"aql(
    FOR x IN collection
    UPDATE x WITH {value: 1} INTO collection)aql";
  auto plan = prepareQuery(queryString);

  auto planSlice = plan->slice();
  ASSERT_TRUE(planSlice.hasKey("nodes"));
  planSlice = planSlice.get("nodes");
  LOG_DEVEL << nodeNames(planSlice);
  assertNodesMatch(planSlice, {"SingletonNode", "CalculationNode",
                               "EnumerateCollectionNode", "UpdateNode",
                               "RemoteNode", "GatherNode"});
}

TEST_F(DistributeQueryRuleTest, distributed_sort) {
  auto queryString = R"aql(
    FOR x IN collection
      SORT x.value DESC
      RETURN x)aql";
  auto plan = prepareQuery(queryString);

  auto planSlice = plan->slice();
  ASSERT_TRUE(planSlice.hasKey("nodes"));
  planSlice = planSlice.get("nodes");
  LOG_DEVEL << nodeNames(planSlice);
  assertNodesMatch(planSlice, {"SingletonNode", "EnumerateCollectionNode",
                               "CalculationNode", "SortNode", "RemoteNode",
                               "GatherNode", "ReturnNode"});
  auto gatherNode = planSlice.at(5);
  ASSERT_TRUE(gatherNode.isObject());
  EXPECT_TRUE(gatherNode.get("sortmode").isEqualString("minelement"));
  auto sortBy = gatherNode.get("elements");
  ASSERT_TRUE(sortBy.isArray());
  ASSERT_EQ(sortBy.length() , 1);
  auto sortVar = sortBy.at(0);
  // We sort by a temp variable named 1
  EXPECT_TRUE(sortVar.get("inVariable").get("name").isEqualString("1"));
  // We need to keep DESC sort
  EXPECT_FALSE(sortVar.get("ascending").getBool());
}

TEST_F(DistributeQueryRuleTest, distributed_subquery_dbserver) {
  auto queryString = R"aql(
    FOR y IN 1..3
    LET sub = (
      FOR x IN collection
        FILTER x.value == y
        RETURN x)
     RETURN sub)aql";
  auto plan = prepareQuery(queryString);

  auto planSlice = plan->slice();
  ASSERT_TRUE(planSlice.hasKey("nodes"));
  planSlice = planSlice.get("nodes");
  LOG_DEVEL << nodeNames(planSlice);
  assertNodesMatch(planSlice,
                   {"SingletonNode", "CalculationNode", "EnumerateListNode",
                    "SubqueryStartNode", "ScatterNode", "RemoteNode",
                    "EnumerateCollectionNode", "CalculationNode", "FilterNode",
                    "RemoteNode", "GatherNode", "SubqueryEndNode",
                    "ReturnNode"});
}

TEST_F(DistributeQueryRuleTest, single_remove) {
  auto queryString = R"aql( REMOVE {_key: "test"} IN collection)aql";
  auto plan = prepareQuery(queryString);

  auto planSlice = plan->slice();
  ASSERT_TRUE(planSlice.hasKey("nodes"));
  planSlice = planSlice.get("nodes");
  LOG_DEVEL << nodeNames(planSlice);
  assertNodesMatch(planSlice, {"SingletonNode", "SingleRemoteOperationNode"});
}

TEST_F(DistributeQueryRuleTest, distributed_remove) {
  auto queryString = R"aql(
    FOR y IN 1..3
    REMOVE {_key: CONCAT("test", y)} IN collection)aql";
  auto plan = prepareQuery(queryString);

  auto planSlice = plan->slice();
  ASSERT_TRUE(planSlice.hasKey("nodes"));
  planSlice = planSlice.get("nodes");
  LOG_DEVEL << nodeNames(planSlice);
  assertNodesMatch(planSlice,
                   {"SingletonNode", "CalculationNode", "EnumerateListNode",
                    "CalculationNode", "CalculationNode", "RemoveNode",
                    "RemoteNode", "GatherNode"});
}

TEST_F(DistributeQueryRuleTest, distributed_insert) {
  auto queryString = R"aql(
    FOR y IN 1..3
    INSERT {value: CONCAT("test", y)} IN collection)aql";
  auto plan = prepareQuery(queryString);

  auto planSlice = plan->slice();
  ASSERT_TRUE(planSlice.hasKey("nodes"));
  planSlice = planSlice.get("nodes");
  LOG_DEVEL << nodeNames(planSlice);
  assertNodesMatch(planSlice,
                   {"SingletonNode", "CalculationNode", "EnumerateListNode",
                    "CalculationNode", "CalculationNode", "DistributeNode",
                    "RemoteNode", "InsertNode", "RemoteNode", "GatherNode"});
}

TEST_F(DistributeQueryRuleTest, distributed_insert_using_shardkey) {
  auto queryString = R"aql(
    FOR y IN 1..3
    INSERT {_key: CONCAT("test", y)} IN collection)aql";
  auto plan = prepareQuery(queryString);

  auto planSlice = plan->slice();
  ASSERT_TRUE(planSlice.hasKey("nodes"));
  planSlice = planSlice.get("nodes");
  LOG_DEVEL << nodeNames(planSlice);
  assertNodesMatch(planSlice,
                   {"SingletonNode", "CalculationNode", "EnumerateListNode",
                    "CalculationNode", "CalculationNode", "DistributeNode",
                    "RemoteNode", "InsertNode", "RemoteNode", "GatherNode"});
}


TEST_F(DistributeQueryRuleTest, distributed_subquery_remove) {
  auto queryString = R"aql(
    FOR y IN 1..3
    LET sub = (
      REMOVE {_key: CONCAT("test", y)} IN collection
    )
    RETURN sub)aql";
  auto plan = prepareQuery(queryString);

  auto planSlice = plan->slice();
  ASSERT_TRUE(planSlice.hasKey("nodes"));
  planSlice = planSlice.get("nodes");
  LOG_DEVEL << nodeNames(planSlice);
  assertNodesMatch(planSlice,
                   {"SingletonNode", "CalculationNode", "EnumerateListNode",
                    "CalculationNode", "CalculationNode",
                    "DistributeNode", "RemoteNode", "SubqueryStartNode", "RemoveNode",
                    "SubqueryEndNode", "RemoteNode", "GatherNode",
                    "ReturnNode"});
}

TEST_F(DistributeQueryRuleTest, subquery_as_first_node) {
  auto queryString = R"aql(
    LET sub = (
      FOR x IN collection
      RETURN 1
    )
    RETURN LENGTH(sub))aql";
  auto plan = prepareQuery(queryString);

  auto planSlice = plan->slice();
  ASSERT_TRUE(planSlice.hasKey("nodes"));
  planSlice = planSlice.get("nodes");
  LOG_DEVEL << nodeNames(planSlice);
  assertNodesMatch(planSlice,
                   {"SingletonNode", "SubqueryStartNode", "ScatterNode",
                    "RemoteNode", "EnumerateCollectionNode", "RemoteNode",
                    "GatherNode", "SubqueryEndNode", "CalculationNode",
                    "ReturnNode"});
}

TEST_F(DistributeQueryRuleTest, enumerate_remove) {
  auto queryString = R"aql(
    FOR doc IN collection
    REMOVE doc IN collection)aql";
  auto plan = prepareQuery(queryString);

  auto planSlice = plan->slice();
  ASSERT_TRUE(planSlice.hasKey("nodes"));
  planSlice = planSlice.get("nodes");
  LOG_DEVEL << nodeNames(planSlice);
  assertNodesMatch(planSlice,
                   {"SingletonNode", "EnumerateCollectionNode", "RemoveNode",
                    "RemoteNode", "GatherNode"});
}

TEST_F(DistributeQueryRuleTest, enumerate_remove_key) {
  auto queryString = R"aql(
    FOR doc IN collection
    REMOVE doc._key IN collection)aql";
  auto plan = prepareQuery(queryString);

  auto planSlice = plan->slice();
  ASSERT_TRUE(planSlice.hasKey("nodes"));
  planSlice = planSlice.get("nodes");
  LOG_DEVEL << nodeNames(planSlice);
  assertNodesMatch(planSlice,
                   {"SingletonNode", "EnumerateCollectionNode", "CalculationNode",
                    "RemoveNode", "RemoteNode", "GatherNode"});
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
