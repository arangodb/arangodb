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

#include "Indexes/IndexTypeCatalog.h"
#include "IResearch/IResearchCommon.h"
#include "Mocks/Servers.h"
#include "VectorIndex/IVectorIndexProvider.h"

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
  tests::mocks::MockRestServer mockServer;
  DisabledVectorIndexProvider vectorIndexProvider;
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
