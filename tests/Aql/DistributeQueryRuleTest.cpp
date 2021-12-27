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
#include "Basics/VelocyPackHelper.h"

#include "../Mocks/Servers.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::transaction;

namespace arangodb {
namespace tests {
namespace aql {

#define useOptimize 1

namespace {
std::string nodeNames(std::vector<std::string> const& nodes) {
  std::string result;
  for (auto const& n : nodes) {
    if (!result.empty()) {
      result += ", ";
    }
    result += n;
  }
  return result;
}
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
    ASSERT_EQ(actualNodes.length(), expectedNodes.size()) 
        << "Unequal number of nodes.\n"
        << "Actual:   " << actualNodes.length() << ": " << nodeNames(actualNodes) << "\n"
        << "Expected: " << expectedNodes.size() << ": " << nodeNames(expectedNodes) << "\n";
    for (size_t i = 0; i < expectedNodes.size(); ++i) {
      ASSERT_EQ(actualNodes.at(i).get("type").copyString(), expectedNodes[i])
          << "Unequal node at position #" << i << "\n"
          << "Actual:   " << nodeNames(actualNodes) << "\n"
          << "Expected: " << nodeNames(expectedNodes) << "\n";
    }
  }

  DistributeQueryRuleTest() {
    // We can register them, but then the API will call count, and the servers
    // do not respond. Now we just get "no endpoint found" but this seems to be
    // okay :shrug: server.registerFakedDBServer("DB1");
    // server.registerFakedDBServer("DB2");
    server.createCollection(server.getSystemDatabase().name(), "collection",
                            {{"s100", "DB1"}, {"s101", "DB2"}},
                            TRI_col_type_e::TRI_COL_TYPE_DOCUMENT);
   
    // this collection has the same number of shards as "collection", but it is
    // not necessarily sharded in the same way
    server.createCollection(server.getSystemDatabase().name(), "otherCollection",
                            {{"s110", "DB1"}, {"s111", "DB2"}},
                            TRI_col_type_e::TRI_COL_TYPE_DOCUMENT);
    
    // this collection is sharded like "collection"
    auto optionsVPack = VPackParser::fromJson(R"json({"distributeShardsLike": "collection"})json");
    server.createCollection(server.getSystemDatabase().name(), "followerCollection",
                            {{"s120", "DB1"}, {"s121", "DB2"}},
                            TRI_col_type_e::TRI_COL_TYPE_DOCUMENT, optionsVPack->slice());
    
    // this collection has custom shard keys
    optionsVPack = VPackParser::fromJson(R"json({"shardKeys": ["id"]})json");
    server.createCollection(server.getSystemDatabase().name(), "customKeysCollection",
                            {{"s123", "DB1"}, {"s234", "DB2"}},
                            TRI_col_type_e::TRI_COL_TYPE_DOCUMENT, optionsVPack->slice());
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

TEST_F(DistributeQueryRuleTest, enumerate_insert) {
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

TEST_F(DistributeQueryRuleTest, enumerate_update) {
  // Special case here, we enumerate and update the same docs.
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

TEST_F(DistributeQueryRuleTest, enumerate_update_key) {
  // Special case here, we enumerate and update the same docs.
  // We could get away without network requests in between
  auto queryString = R"aql(
    FOR x IN collection
    UPDATE x._key WITH {value: 1} INTO collection)aql";
  auto plan = prepareQuery(queryString);

  auto planSlice = plan->slice();
  ASSERT_TRUE(planSlice.hasKey("nodes"));
  planSlice = planSlice.get("nodes");
  LOG_DEVEL << nodeNames(planSlice);
  assertNodesMatch(planSlice, {"SingletonNode", "CalculationNode",
                               "EnumerateCollectionNode", "CalculationNode",
                               "UpdateNode", "RemoteNode", "GatherNode"});
}

TEST_F(DistributeQueryRuleTest, enumerate_update_custom_shardkey_known) {
  // Special case here, we enumerate and update the same docs.
  // We could get away without network requests in between
  auto queryString = R"aql(
    FOR x IN customKeysCollection
    UPDATE {_key: x._key, id: x.id} WITH {value: 1} INTO customKeysCollection)aql";
  auto plan = prepareQuery(queryString);

  auto planSlice = plan->slice();
  ASSERT_TRUE(planSlice.hasKey("nodes"));
  planSlice = planSlice.get("nodes");
  LOG_DEVEL << nodeNames(planSlice);

  assertNodesMatch(planSlice, {"SingletonNode", "CalculationNode", 
                               "EnumerateCollectionNode", "CalculationNode",
                               "UpdateNode", "RemoteNode", "GatherNode"});
}

TEST_F(DistributeQueryRuleTest, enumerate_update_custom_shardkey_unknown) {
  // Special case here, we enumerate and update the same docs.
  // We could get away without network requests in between
  auto queryString = R"aql(
    FOR x IN customKeysCollection
    UPDATE x WITH {value: 1} INTO customKeysCollection)aql";
  auto plan = prepareQuery(queryString);

  auto planSlice = plan->slice();
  ASSERT_TRUE(planSlice.hasKey("nodes"));
  planSlice = planSlice.get("nodes");
  LOG_DEVEL << nodeNames(planSlice);
 
  assertNodesMatch(planSlice, {"SingletonNode", "CalculationNode",
                               "EnumerateCollectionNode", 
                               "UpdateNode", "RemoteNode", "GatherNode"});
}

TEST_F(DistributeQueryRuleTest, enumerate_replace) {
  // Special case here, we enumerate and replace the same docs.
  // We could get away without network requests in between
  auto queryString = R"aql(
    FOR x IN collection
    REPLACE x WITH {value: 1} INTO collection)aql";
  auto plan = prepareQuery(queryString);

  auto planSlice = plan->slice();
  ASSERT_TRUE(planSlice.hasKey("nodes"));
  planSlice = planSlice.get("nodes");
  LOG_DEVEL << nodeNames(planSlice);
  assertNodesMatch(planSlice, {"SingletonNode", "CalculationNode",
                               "EnumerateCollectionNode", "ReplaceNode",
                               "RemoteNode", "GatherNode"});
}

TEST_F(DistributeQueryRuleTest, enumerate_replace_key) {
  // Special case here, we enumerate and replace the same docs.
  // We could get away without network requests in between
  auto queryString = R"aql(
    FOR x IN collection
    REPLACE x._key WITH {value: 1} INTO collection)aql";
  auto plan = prepareQuery(queryString);

  auto planSlice = plan->slice();
  ASSERT_TRUE(planSlice.hasKey("nodes"));
  planSlice = planSlice.get("nodes");
  LOG_DEVEL << nodeNames(planSlice);
  assertNodesMatch(planSlice, {"SingletonNode", "CalculationNode",
                               "EnumerateCollectionNode", "CalculationNode",
                               "ReplaceNode", "RemoteNode", "GatherNode"});
}

TEST_F(DistributeQueryRuleTest, enumerate_replace_custom_shardkey_known) {
  // Special case here, we enumerate and replace the same docs.
  // We could get away without network requests in between
  auto queryString = R"aql(
    FOR x IN customKeysCollection
    REPLACE {_key: x._key, id: x.id} WITH {value: 1} INTO customKeysCollection)aql";
  auto plan = prepareQuery(queryString);

  auto planSlice = plan->slice();
  ASSERT_TRUE(planSlice.hasKey("nodes"));
  planSlice = planSlice.get("nodes");
  LOG_DEVEL << nodeNames(planSlice);

  assertNodesMatch(planSlice, {"SingletonNode", "CalculationNode", 
                               "EnumerateCollectionNode", "CalculationNode", 
                               "ReplaceNode", "RemoteNode", "GatherNode"});
}

TEST_F(DistributeQueryRuleTest, enumerate_replace_custom_shardkey_unknown) {
  // Special case here, we enumerate and replace the same docs.
  // We could get away without network requests in between
  auto queryString = R"aql(
    FOR x IN customKeysCollection
    REPLACE x WITH {value: 1} INTO customKeysCollection)aql";
  auto plan = prepareQuery(queryString);

  auto planSlice = plan->slice();
  ASSERT_TRUE(planSlice.hasKey("nodes"));
  planSlice = planSlice.get("nodes");
  LOG_DEVEL << nodeNames(planSlice);
 
  assertNodesMatch(planSlice, {"SingletonNode", "CalculationNode",
                               "EnumerateCollectionNode",
                               "ReplaceNode", "RemoteNode", "GatherNode"});
}

TEST_F(DistributeQueryRuleTest, enumerate_remove_custom_shardkey) {
  // Special case here, we enumerate and remove the same docs.
  // We could get away without network requests in between
  auto queryString = R"aql(
    FOR x IN customKeysCollection
    REMOVE x INTO customKeysCollection)aql";
  auto plan = prepareQuery(queryString);

  auto planSlice = plan->slice();
  ASSERT_TRUE(planSlice.hasKey("nodes"));
  planSlice = planSlice.get("nodes");
  LOG_DEVEL << nodeNames(planSlice);

  assertNodesMatch(planSlice, {"SingletonNode", "EnumerateCollectionNode",
                               "RemoveNode", "RemoteNode", "GatherNode"});
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
  EXPECT_EQ(gatherNode.get("sortmode").copyString(), "minelement");
  auto sortBy = gatherNode.get("elements");
  ASSERT_TRUE(sortBy.isArray());
  ASSERT_EQ(sortBy.length() , 1);
  auto sortVar = sortBy.at(0);
  // We sort by a temp variable named 1
  EXPECT_EQ(sortVar.get("inVariable").get("name").copyString(), "1");
  // We need to keep DESC sort
  EXPECT_FALSE(sortVar.get("ascending").getBool());
}

TEST_F(DistributeQueryRuleTest, distributed_collect) {
  auto queryString = R"aql(
    FOR x IN collection
      COLLECT val = x.value
      RETURN val)aql";
  auto plan = prepareQuery(queryString);

  auto planSlice = plan->slice();
  LOG_DEVEL << planSlice.toJson();
  ASSERT_TRUE(planSlice.hasKey("nodes"));
  planSlice = planSlice.get("nodes");
  LOG_DEVEL << nodeNames(planSlice);
  assertNodesMatch(planSlice, {"SingletonNode", "EnumerateCollectionNode",
                               "CalculationNode", "CollectNode", "RemoteNode",
                               "GatherNode", "CollectNode", "SortNode", "ReturnNode"});
  auto dbServerCollect = planSlice.at(3);
  auto gatherNode = planSlice.at(5);
  auto coordinatorCollect = planSlice.at(6);
  // TODO Why is there a SORT node?
  auto sort = planSlice.at(7);
  LOG_DEVEL << dbServerCollect.toJson();
  LOG_DEVEL << gatherNode.toJson();
  LOG_DEVEL << coordinatorCollect.toJson();
  LOG_DEVEL << sort.toJson();
  {
    // TODO assert In Variable in DBServer
    // TODO assert Out Variable in DBServer
    // TODO assert collectOptions
    // TODO assert parallelism
    ASSERT_TRUE(dbServerCollect.isObject());
    ASSERT_TRUE(coordinatorCollect.isObject());
    // Assert that the OutVariable of the DBServer is the inVariable or Coordinator
    auto dbServerCollectOut = dbServerCollect.get("groups").at(0).get("outVariable");
    auto coordinatorCollectIn = coordinatorCollect.get("groups").at(0).get("inVariable");
    EXPECT_TRUE(basics::VelocyPackHelper::equal(dbServerCollectOut, coordinatorCollectIn, false));
  }

  ASSERT_TRUE(gatherNode.isObject());
  EXPECT_EQ(gatherNode.get("sortmode").copyString(), "unset");
  auto sortBy = gatherNode.get("elements");
  ASSERT_TRUE(sortBy.isArray());
  ASSERT_EQ(sortBy.length(), 0);
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
  // NOTE: This test is known to be red right now, it waits for an optimizer rule
  // that can move Calculations out of subqueries.
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
