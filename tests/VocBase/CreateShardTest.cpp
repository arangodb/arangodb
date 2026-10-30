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

#include "Mocks/LogLevels.h"
#include "Mocks/Servers.h"

#include "Basics/StaticStrings.h"
#include "Logger/LogMacros.h"
#include "StorageEngine/PhysicalCollection.h"
#include "Utils/OperationOptions.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Properties/CollectionDescriptor.h"
#include "VocBase/vocbase.h"

#include <velocypack/Parser.h>

#include <absl/strings/str_cat.h>

using namespace arangodb;

namespace {

// A Plan entry as the maintenance hands it to createShard: the collection's
// properties, and the collection's identity, which the shard must not take.
std::shared_ptr<velocypack::Builder> docket(std::string_view computedValues) {
  return velocypack::Parser::fromJson(absl::StrCat(R"({
    "name": "someCollection",
    "type": 2,
    "planId": "9988489",
    "id": "11",
    "globallyUniqueId": "h1234/11",
    "objectId": "777",
    "numberOfShards": 3,
    "shardingStrategy": "hash",
    "shardKeys": ["_key"],
    "replicationFactor": 1,
    "writeConcern": 1,
    "computedValues": )",
                                                   computedValues, "}"));
}

}  // namespace

class CreateShardTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::CLUSTER,
                                            arangodb::LogLevel::FATAL> {
 protected:
  arangodb::tests::mocks::MockDBServer server;

  CreateShardTest() : server("PRMR-1234") {}

  Result create(std::shared_ptr<velocypack::Builder> const& body,
                std::shared_ptr<LogicalCollection>& shard) {
    auto* vocbase = server.createDatabase("testDatabase");
    EXPECT_NE(vocbase, nullptr);
    OperationOptions options;
    return methods::Collections::createShard(
        *vocbase, options, ShardID{"s100001"}, TRI_COL_TYPE_DOCUMENT,
        CollectionDescriptor::fromVelocyPack(body->slice()), shard);
  }
};

// The internal context parses the identity the Plan entry carries, so
// createShard has to replace it: a shard is a data source of its own.
TEST_F(CreateShardTest, mintsAFreshIdentity) {
  std::shared_ptr<LogicalCollection> shard;
  auto res = create(docket("null"), shard);
  ASSERT_TRUE(res.ok()) << res.errorMessage();
  ASSERT_NE(shard, nullptr);

  EXPECT_EQ(shard->name(), "s100001");
  EXPECT_EQ(shard->type(), TRI_COL_TYPE_DOCUMENT);

  // planId is the one identity field a shard keeps: it names the collection
  EXPECT_EQ(shard->planId(), DataSourceId{9988489});

  EXPECT_NE(shard->id(), DataSourceId{11});
  EXPECT_NE(shard->guid(), "h1234/11");
  EXPECT_NE(shard->getPhysical()->objectId(), 777U);
}

// The coordinator accepted the definition, but a DBServer that cannot build it
// must refuse the shard rather than create one that silently stores documents
// without computed values.
TEST_F(CreateShardTest, refusesComputedValuesItCannotBuild) {
  std::shared_ptr<LogicalCollection> shard;
  auto res = create(docket(R"([{"foo": true}])"), shard);
  EXPECT_TRUE(res.fail());
  EXPECT_EQ(res.errorNumber(), TRI_ERROR_BAD_PARAMETER);
  EXPECT_EQ(shard, nullptr);
}
