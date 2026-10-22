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

#include "Basics/StaticStrings.h"
#include "Indexes/IndexTypeCatalog.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchInvertedIndexDefinition.h"
#include "Mocks/Servers.h"
#include "VectorIndex/IVectorIndexProvider.h"

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>

#include <string_view>
#include <utility>
#include <vector>

using namespace arangodb;

namespace {

struct DisabledVectorIndexProvider final : IVectorIndexProvider {
  bool isVectorIndexEnabled() const noexcept override { return false; }
};

}  // namespace

class IndexTypeCatalogTest : public ::testing::Test {
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
};

TEST_F(IndexTypeCatalogTest, resolvesBuiltInNamesToIndexTypes) {
  EXPECT_EQ(catalog.resolve("edge"), IndexType::Edge);
  EXPECT_EQ(catalog.resolve("fulltext"), IndexType::Fulltext);
  EXPECT_EQ(catalog.resolve("geo"), IndexType::Geo);
  EXPECT_EQ(catalog.resolve("geo1"), IndexType::Geo1);
  EXPECT_EQ(catalog.resolve("geo2"), IndexType::Geo2);
  EXPECT_EQ(catalog.resolve("hash"), IndexType::Hash);
  EXPECT_EQ(catalog.resolve("persistent"), IndexType::Persistent);
  EXPECT_EQ(catalog.resolve("skiplist"), IndexType::Skiplist);
  EXPECT_EQ(catalog.resolve("primary"), IndexType::Primary);
  EXPECT_EQ(catalog.resolve("ttl"), IndexType::TTL);
  EXPECT_EQ(catalog.resolve("zkd"), IndexType::Zkd);
  EXPECT_EQ(catalog.resolve("mdi"), IndexType::MDI);
  EXPECT_EQ(catalog.resolve("mdi-prefixed"), IndexType::MDIPrefixed);
  EXPECT_EQ(catalog.resolve("vector"), IndexType::Vector);
  EXPECT_EQ(catalog.resolve(iresearch::IRESEARCH_INVERTED_INDEX_TYPE),
            IndexType::Inverted);
}

TEST_F(IndexTypeCatalogTest, legacyRocksdbNameResolvesToPersistent) {
  EXPECT_EQ(catalog.resolve("rocksdb"), IndexType::Persistent);
}

TEST_F(IndexTypeCatalogTest, unknownNamesResolveToUnknown) {
  EXPECT_EQ(catalog.resolve("nosuchindex"), IndexType::Unknown);
  // registered by IResearchFeature in the engine, not by the catalog itself
  EXPECT_EQ(catalog.resolve(iresearch::StaticStrings::ViewArangoSearchType),
            IndexType::Unknown);
}

TEST_F(IndexTypeCatalogTest, aliasesForApiVersion0) {
  using Alias = std::pair<std::string_view, std::string_view>;
  EXPECT_EQ(catalog.aliases(0), (std::vector<Alias>{{"hash", "persistent"},
                                                    {"skiplist", "persistent"},
                                                    {"zkd", "mdi"}}));
}

TEST_F(IndexTypeCatalogTest, aliasesForApiVersion1) {
  using Alias = std::pair<std::string_view, std::string_view>;
  EXPECT_EQ(catalog.aliases(1), (std::vector<Alias>{{"zkd", "mdi"}}));
}

TEST_F(IndexTypeCatalogTest, geo2EqualChecksGeoJson) {
  auto a = velocypack::Parser::fromJson(
      R"({"type": "geo2", "fields": ["x"], "geoJson": true})");
  auto b = velocypack::Parser::fromJson(
      R"({"type": "geo2", "fields": ["x"], "geoJson": false})");
  EXPECT_FALSE(catalog.equal(a->slice(), b->slice(), "_system"));
}

TEST_F(IndexTypeCatalogTest, geo1EqualChecksGeoJson) {
  auto a = velocypack::Parser::fromJson(
      R"({"type": "geo1", "fields": ["x"], "geoJson": true})");
  auto b = velocypack::Parser::fromJson(
      R"({"type": "geo1", "fields": ["x"], "geoJson": false})");
  EXPECT_FALSE(catalog.equal(a->slice(), b->slice(), "_system"));
}

TEST_F(IndexTypeCatalogTest, zkdIsAliasOfMdiForEqual) {
  auto zkd =
      velocypack::Parser::fromJson(R"({"type": "zkd", "fields": ["x"]})");
  auto mdi =
      velocypack::Parser::fromJson(R"({"type": "mdi", "fields": ["x"]})");
  EXPECT_TRUE(catalog.equal(zkd->slice(), mdi->slice(), "_system"));
}

TEST_F(IndexTypeCatalogTest, invertedEqualChecksFields) {
  auto a =
      velocypack::Parser::fromJson(R"({"type": "inverted", "fields": ["x"]})");
  auto b =
      velocypack::Parser::fromJson(R"({"type": "inverted", "fields": ["y"]})");
  EXPECT_FALSE(catalog.equal(a->slice(), b->slice(), "_system"));
}

TEST_F(IndexTypeCatalogTest, equalDoesNotThrowOnNonStringType) {
  auto a = velocypack::Parser::fromJson(R"({"type": "persistent"})");
  auto b = velocypack::Parser::fromJson(R"({"type": 1})");
  EXPECT_FALSE(catalog.equal(a->slice(), b->slice(), "_system"));
}

TEST_F(IndexTypeCatalogTest, rocksdbAliasNormalizesToPersistent) {
  auto definition =
      velocypack::Parser::fromJson(R"({"type": "rocksdb", "fields": ["x"]})");
  velocypack::Builder normalized;
  normalized.openObject();
  auto res = catalog.normalizeType("rocksdb", normalized, definition->slice(),
                                   /*isCreation*/ true,
                                   mockServer.getSystemDatabase());
  normalized.close();
  ASSERT_TRUE(res.ok()) << res.errorMessage();
  EXPECT_EQ(normalized.slice().get(StaticStrings::IndexType).stringView(),
            "persistent");
}

TEST_F(IndexTypeCatalogTest, zkdNormalizeKeepsOwnName) {
  auto definition = velocypack::Parser::fromJson(
      R"({"type": "zkd", "fields": ["x"], "fieldValueTypes": "double"})");
  velocypack::Builder normalized;
  normalized.openObject();
  auto res = catalog.normalizeType("zkd", normalized, definition->slice(),
                                   /*isCreation*/ true,
                                   mockServer.getSystemDatabase());
  normalized.close();
  ASSERT_TRUE(res.ok()) << res.errorMessage();
  EXPECT_EQ(normalized.slice().get(StaticStrings::IndexType).stringView(),
            "zkd");
}

TEST_F(IndexTypeCatalogTest, ttlNormalizeRequiresExpireAfter) {
  auto definition = velocypack::Parser::fromJson(
      R"({"type": "ttl", "fields": ["x"], "expireAfter": 600})");
  velocypack::Builder normalized;
  normalized.openObject();
  auto res = catalog.normalizeType("ttl", normalized, definition->slice(),
                                   /*isCreation*/ true,
                                   mockServer.getSystemDatabase());
  normalized.close();
  ASSERT_TRUE(res.ok()) << res.errorMessage();
  EXPECT_EQ(normalized.slice().get(StaticStrings::IndexType).stringView(),
            "ttl");

  auto missingExpire =
      velocypack::Parser::fromJson(R"({"type": "ttl", "fields": ["x"]})");
  velocypack::Builder normalized2;
  normalized2.openObject();
  auto res2 = catalog.normalizeType("ttl", normalized2, missingExpire->slice(),
                                    /*isCreation*/ true,
                                    mockServer.getSystemDatabase());
  EXPECT_TRUE(res2.fail());
}

TEST_F(IndexTypeCatalogTest, vectorNormalizeFailsWhenFeatureDisabled) {
  auto definition = velocypack::Parser::fromJson(R"({"type": "vector"})");
  velocypack::Builder normalized;
  normalized.openObject();
  auto res = catalog.normalizeType("vector", normalized, definition->slice(),
                                   /*isCreation*/ true,
                                   mockServer.getSystemDatabase());
  EXPECT_TRUE(res.fail());
}
