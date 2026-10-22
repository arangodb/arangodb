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

#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Indexes/IndexTypeCatalog.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchInvertedIndexDefinition.h"
#include "Mocks/Servers.h"
#include "RocksDBEngine/RocksDBIndexFactory.h"
#include "VectorIndex/IVectorIndexProvider.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>

using namespace arangodb;

namespace {

struct DisabledVectorIndexProvider final : IVectorIndexProvider {
  bool isVectorIndexEnabled() const noexcept override { return false; }
};

size_t countObjectId(velocypack::Slice normalized) {
  size_t count = 0;
  for (auto it : velocypack::ObjectIterator(normalized)) {
    if (it.key.stringView() == StaticStrings::ObjectId) {
      ++count;
    }
  }
  return count;
}

}  // namespace

class RocksDBIndexFactoryTest : public ::testing::Test {
 protected:
  // mirrors what IResearchFeature::registerIndexFactory does at startup
  void SetUp() override {
    auto res = catalog.add(iresearch::IRESEARCH_INVERTED_INDEX_TYPE,
                           invertedDefinition);
    ASSERT_TRUE(res.ok()) << res.errorMessage();
  }

  // needs IResearchAnalyzerFeature, which MockRestServer lacks
  tests::mocks::MockAqlServer mockServer;
  DisabledVectorIndexProvider vectorIndexProvider;
  // must outlive catalog
  iresearch::IResearchInvertedIndexDefinition invertedDefinition{
      mockServer.server()};
  IndexTypeCatalog catalog{mockServer.server(), vectorIndexProvider};
  RocksDBIndexFactory factory{mockServer.server(), catalog};
};

TEST_F(RocksDBIndexFactoryTest, invertedIndexGetsExactlyOneObjectId) {
  auto definition =
      velocypack::Parser::fromJson(R"({"type": "inverted", "fields": ["x"]})");
  velocypack::Builder normalized;
  auto res = factory.enhanceIndexDefinition(definition->slice(), normalized,
                                            /*isCreation*/ true,
                                            mockServer.getSystemDatabase());
  ASSERT_TRUE(res.ok()) << res.errorMessage();
  EXPECT_EQ(countObjectId(normalized.slice()), 1);
}

TEST_F(RocksDBIndexFactoryTest, noObjectIdWhenNotCreating) {
  auto definition =
      velocypack::Parser::fromJson(R"({"type": "inverted", "fields": ["x"]})");
  velocypack::Builder normalized;
  auto res = factory.enhanceIndexDefinition(definition->slice(), normalized,
                                            /*isCreation*/ false,
                                            mockServer.getSystemDatabase());
  ASSERT_TRUE(res.ok()) << res.errorMessage();
  EXPECT_EQ(countObjectId(normalized.slice()), 0);
}

TEST_F(RocksDBIndexFactoryTest, noExtraObjectIdWhenAlreadyPresent) {
  auto definition = velocypack::Parser::fromJson(
      R"({"type": "inverted", "fields": ["x"], "objectId": "123"})");
  velocypack::Builder normalized;
  auto res = factory.enhanceIndexDefinition(definition->slice(), normalized,
                                            /*isCreation*/ true,
                                            mockServer.getSystemDatabase());
  ASSERT_TRUE(res.ok()) << res.errorMessage();
  EXPECT_EQ(countObjectId(normalized.slice()), 1);
}

TEST_F(RocksDBIndexFactoryTest, noObjectIdWhenNormalizeFails) {
  auto definition =
      velocypack::Parser::fromJson(R"({"type": "ttl", "fields": ["x"]})");
  velocypack::Builder normalized;
  auto res = factory.enhanceIndexDefinition(definition->slice(), normalized,
                                            /*isCreation*/ true,
                                            mockServer.getSystemDatabase());
  ASSERT_TRUE(res.fail());
  EXPECT_EQ(countObjectId(normalized.slice()), 0);
}

TEST_F(RocksDBIndexFactoryTest, noObjectIdForEdgeOrPrimaryOnCreation) {
  for (auto const& definition :
       {velocypack::Parser::fromJson(R"({"type": "edge"})"),
        velocypack::Parser::fromJson(R"({"type": "primary"})")}) {
    velocypack::Builder normalized;
    auto res = factory.enhanceIndexDefinition(definition->slice(), normalized,
                                              /*isCreation*/ true,
                                              mockServer.getSystemDatabase());
    ASSERT_TRUE(res.fail());
    EXPECT_EQ(countObjectId(normalized.slice()), 0);
  }
}

TEST_F(RocksDBIndexFactoryTest, edgeIndexCannotBeCreatedDirectly) {
  auto collectionJson =
      velocypack::Parser::fromJson(R"({"name": "edges", "type": 3})");
  auto collection =
      mockServer.getSystemDatabase().createCollection(collectionJson->slice());

  auto definition =
      velocypack::Parser::fromJson(R"({"type": "edge", "fields": ["_from"]})");
  IndexCreationContext ctx{*collection, definition->slice(), IndexId{1},
                           /*isClusterConstructor*/ false};
  try {
    factory.createIndex(IndexType::Edge, ctx);
    FAIL() << "expected an exception";
  } catch (basics::Exception const& ex) {
    EXPECT_EQ(ex.code(), TRI_ERROR_INTERNAL);
  }
}

TEST_F(RocksDBIndexFactoryTest, primaryIndexCannotBeCreatedDirectly) {
  auto collectionJson = velocypack::Parser::fromJson(R"({"name": "docs"})");
  auto collection =
      mockServer.getSystemDatabase().createCollection(collectionJson->slice());

  auto definition = velocypack::Parser::fromJson(R"({"type": "primary"})");
  IndexCreationContext ctx{*collection, definition->slice(), IndexId{1},
                           /*isClusterConstructor*/ false};
  try {
    factory.createIndex(IndexType::Primary, ctx);
    FAIL() << "expected an exception";
  } catch (basics::Exception const& ex) {
    EXPECT_EQ(ex.code(), TRI_ERROR_INTERNAL);
  }
}
