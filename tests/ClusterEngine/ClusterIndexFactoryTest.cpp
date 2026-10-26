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
#include "Indexes/IndexFactory.h"
#include "Mocks/Servers.h"
#include "StorageEngine/StorageEngine.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/voc-types.h"

#include <velocypack/Parser.h>

using namespace arangodb;

class ClusterIndexFactoryTest : public ::testing::Test {
 protected:
  tests::mocks::MockCoordinator server{"CRDN_0001"};
};

TEST_F(ClusterIndexFactoryTest, edgeIndexCannotBeCreatedDirectly) {
  auto* vocbase = server.createDatabase("testDatabase");
  auto collection = server.createCollection(
      vocbase->name(), "edges", {{"s100", "PRMR_0001"}}, TRI_COL_TYPE_EDGE);
  ASSERT_NE(collection, nullptr);

  auto definition =
      velocypack::Parser::fromJson(R"({"type": "edge", "fields": ["_from"]})");
  IndexCreationContext ctx{*collection, definition->slice(), IndexId{1},
                           /*isClusterConstructor*/ false};
  try {
    server.getFeature<StorageEngine>().indexFactory().createIndex(
        IndexType::Edge, ctx);
    FAIL() << "expected an exception";
  } catch (basics::Exception const& ex) {
    EXPECT_EQ(ex.code(), TRI_ERROR_INTERNAL);
  }
}

TEST_F(ClusterIndexFactoryTest, primaryIndexCannotBeCreatedDirectly) {
  auto* vocbase = server.createDatabase("testDatabase");
  auto collection = server.createCollection(
      vocbase->name(), "docs", {{"s101", "PRMR_0001"}}, TRI_COL_TYPE_DOCUMENT);
  ASSERT_NE(collection, nullptr);

  auto definition = velocypack::Parser::fromJson(R"({"type": "primary"})");
  IndexCreationContext ctx{*collection, definition->slice(), IndexId{1},
                           /*isClusterConstructor*/ false};
  try {
    server.getFeature<StorageEngine>().indexFactory().createIndex(
        IndexType::Primary, ctx);
    FAIL() << "expected an exception";
  } catch (basics::Exception const& ex) {
    EXPECT_EQ(ex.code(), TRI_ERROR_INTERNAL);
  }
}
