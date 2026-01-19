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
  QueryOptions options;
  options.memoryLimit = memoryLimit;

  auto ctx = std::make_shared<transaction::StandaloneContext>(
      vocbase, transaction::OperationOriginTestCase{});

  auto query = Query::create(ctx, QueryString(queryString), nullptr, options);

  QueryResult result = query->executeSync();
  return result;
}
}  // namespace

struct AqlMergeTest : ::testing::Test {
  tests::mocks::MockAqlServer server;
  TRI_vocbase_t& vocbase;
  AqlMergeTest() : server(), vocbase(server.getSystemDatabase()) {}
};

TEST_F(AqlMergeTest, MergeBasicSupervised) {
  auto queryRes = runQuery(vocbase, "RETURN MERGE({a:1,b:2},{a:5,c:3},{b:42})",
                           64 * 1024 * 1024);
  ASSERT_TRUE(queryRes.result.ok()) << queryRes.result.errorMessage();
  Slice outSlice = queryRes.data->slice();
  ASSERT_TRUE(outSlice.isArray());
  ASSERT_EQ(outSlice.length(), 1);
  Slice objSlice = outSlice.at(0);
  EXPECT_EQ(objSlice.get("a").getNumber<int>(), 5);
  EXPECT_EQ(objSlice.get("b").getNumber<int>(), 42);
  EXPECT_EQ(objSlice.get("c").getNumber<int>(), 3);
}

TEST_F(AqlMergeTest, MergeExceedsMemoryLimitSupervised) {
  // 4096 objects each with a 4096 byte value
  std::string q =
      "RETURN MERGE((FOR i IN 1..4096 "
      "              RETURN { [CONCAT('a', i)]: REPEAT('b', 4096) }))";
  auto queryRes = runQuery(vocbase, q, 8 * 1024 * 1024);
  ASSERT_FALSE(queryRes.result.ok());
  EXPECT_EQ(TRI_ERROR_RESOURCE_LIMIT, queryRes.result.errorNumber());
}

TEST_F(AqlMergeTest, MergeRespectsMemoryLimitSupervised) {
  // result should contain 4096 keys
  std::string query =
      "RETURN MERGE((FOR i IN 1..4096 "
      "              RETURN { [CONCAT('a', i)]: REPEAT('b', 4096) }))";
  auto queryRes = runQuery(vocbase, query, 64 * 1024 * 1024);
  ASSERT_TRUE(queryRes.result.ok()) << queryRes.result.errorMessage();
  Slice outSlice = queryRes.data->slice();
  ASSERT_TRUE(outSlice.isArray());
  ASSERT_EQ(outSlice.length(), 1);
  Slice objSlice = outSlice.at(0);
  ASSERT_TRUE(objSlice.isObject());
  EXPECT_EQ(objSlice.length(), 4096);
}
