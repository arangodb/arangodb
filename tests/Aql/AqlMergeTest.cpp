////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Aql/AqlValue.h"
#include "Aql/Query.h"
#include "Aql/QueryResult.h"
#include "Aql/QueryString.h"
#include "Aql/SharedQueryState.h"
#include "Mocks/Servers.h"
#include "Transaction/OperationOrigin.h"
#include "Transaction/StandaloneContext.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Slice.h>
#include <velocypack/Value.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::velocypack;

namespace {
static QueryResult runQuery(TRI_vocbase_t& vocbase,
                            std::string const& queryString,
                            uint64_t memoryLimit) {
  std::string optsJson =
      "{\"memoryLimit\": " + std::to_string(memoryLimit) + "}";
  auto optsBuilder = velocypack::Parser::fromJson(optsJson);
  QueryOptions options(optsBuilder->slice());

  auto ctx = std::make_shared<transaction::StandaloneContext>(
      vocbase, transaction::OperationOriginTestCase{});

  auto query = Query::create(ctx, QueryString(queryString), nullptr, options);

  QueryResult result;
  while (true) {
    auto state = query->execute(result);
    if (state == ExecutionState::WAITING) {
      query->sharedState()->waitForAsyncWakeup();
    } else {
      break;
    }
  }
  return result;
}
}  // namespace

struct AqlMergeTest : ::testing::Test {
  tests::mocks::MockAqlServer server;
  TRI_vocbase_t& vocbase;
  AqlMergeTest() : server(), vocbase(server.getSystemDatabase()) {}
};

TEST_F(AqlMergeTest, Merge_Basic_Supervised) {
  auto r = runQuery(vocbase, "RETURN MERGE({a:1,b:2},{a:5,c:3},{b:42})",
                    64ULL * 1024ULL * 1024ULL);
  ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
  Slice outSlice = r.data->slice();
  ASSERT_TRUE(outSlice.isArray());
  ASSERT_EQ(outSlice.length(), 1UL);
  Slice objSlice = outSlice.at(0);
  EXPECT_EQ(objSlice.get("a").getNumber<int>(), 5);
  EXPECT_EQ(objSlice.get("b").getNumber<int>(), 42);
  EXPECT_EQ(objSlice.get("c").getNumber<int>(), 3);
}

TEST_F(AqlMergeTest, Merge_HitsMemoryLimit_Supervised) {
  // 4096 objects each with a 4096 byte value
  std::string q =
      "RETURN MERGE((FOR i IN 1..4096 "
      "              RETURN { [CONCAT('a', i)]: REPEAT('b', 4096) }))";
  auto queryRes = runQuery(vocbase, q, 8ULL * 1024ULL * 1024ULL);
  ASSERT_FALSE(queryRes.result.ok());
  EXPECT_EQ(TRI_ERROR_RESOURCE_LIMIT, queryRes.result.errorNumber());
}

TEST_F(AqlMergeTest, Merge_RespectsMemoryLimit_Supervised) {
  // result should contain 4096 keys
  std::string q =
      "RETURN MERGE((FOR i IN 1..4096 "
      "              RETURN { [CONCAT('a', i)]: REPEAT('b', 4096) }))";
  auto queryRes = runQuery(vocbase, q, 64ULL * 1024ULL * 1024ULL);
  ASSERT_TRUE(queryRes.result.ok()) << queryRes.result.errorMessage();
  Slice outSlice = queryRes.data->slice();
  ASSERT_TRUE(outSlice.isArray());
  ASSERT_EQ(outSlice.length(), 1UL);
  Slice objSlice = outSlice.at(0);
  ASSERT_TRUE(objSlice.isObject());
  EXPECT_EQ(objSlice.length(), 4096UL);
}

TEST_F(AqlMergeTest, Merge_Basic_Unsupervised) {
  // Merge two objects directly with VPack to check for unsupervised
  Builder arrBuilder;
  arrBuilder.openArray();
  {
    Builder objBuilder;
    objBuilder.openObject();
    objBuilder.add("a", Value(1));
    objBuilder.add("b", Value(2));
    objBuilder.close();
    arrBuilder.add(objBuilder.slice());
  }
  {
    Builder objBuilder;
    objBuilder.openObject();
    objBuilder.add("a", Value(5));
    objBuilder.add("c", Value(3));
    objBuilder.close();
    arrBuilder.add(objBuilder.slice());
  }
  arrBuilder.close();
  Slice slice = arrBuilder.slice();
  ASSERT_TRUE(slice.isArray());
  VPackSlice obj1 = slice.at(0);
  VPackSlice obj2 = slice.at(1);

  Builder outBuilder;
  outBuilder.openObject();
  velocypack::Collection::merge(outBuilder, obj1, obj2,
                                /*mergeObjects*/ false,
                                /*nullMeansRemove*/ false);
  outBuilder.close();
  VPackSlice outSlice = outBuilder.slice();
  ASSERT_TRUE(outSlice.isObject());
  EXPECT_EQ(outSlice.get("a").getNumber<int>(), 5);
  EXPECT_EQ(outSlice.get("b").getNumber<int>(), 2);
  EXPECT_EQ(outSlice.get("c").getNumber<int>(), 3);
}
