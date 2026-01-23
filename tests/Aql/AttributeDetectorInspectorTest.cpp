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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

#include "AttributeDetectorTestBase.h"
#include "Aql/AttributeDetector.h"
#include "Inspection/VPack.h"

namespace arangodb::tests::aql {

TEST_F(AttributeDetectorTest, InspectorFormat) {
  AttributeDetector::CollectionAccess access;
  access.collectionName = "testCollection";
  // Attributes are now paths (vectors of strings)
  access.readAttributes.insert({"name"});
  access.readAttributes.insert({"age"});
  access.requiresAllAttributesRead = false;
  access.requiresAllAttributesWrite = true;

  velocypack::Builder builder;
  velocypack::serialize(builder, access);

  velocypack::Slice slice = builder.slice();
  ASSERT_TRUE(slice.isObject());

  EXPECT_EQ(slice["collection"].copyString(), "testCollection");

  velocypack::Slice read = slice["read"];
  ASSERT_TRUE(read.isObject());
  EXPECT_FALSE(read["requiresAll"].getBool());

  // Attributes are now arrays of arrays (each attribute is a path)
  velocypack::Slice readAttrs = read["attributes"];
  ASSERT_TRUE(readAttrs.isArray());
  EXPECT_EQ(readAttrs.length(), 2);
  // Each attribute is itself an array (the path)
  ASSERT_TRUE(readAttrs[0].isArray());

  velocypack::Slice write = slice["write"];
  ASSERT_TRUE(write.isObject());
  EXPECT_TRUE(write["requiresAll"].getBool());
}

TEST_F(AttributeDetectorTest, InspectorFormatMultipleCollections) {
  std::vector<AttributeDetector::CollectionAccess> accesses;

  AttributeDetector::CollectionAccess access1;
  access1.collectionName = "users";
  access1.readAttributes.insert({"name"});
  access1.requiresAllAttributesRead = false;
  access1.requiresAllAttributesWrite = false;

  AttributeDetector::CollectionAccess access2;
  access2.collectionName = "orders";
  access2.readAttributes.insert({"id"});
  access2.readAttributes.insert({"total"});
  access2.requiresAllAttributesRead = false;
  access2.requiresAllAttributesWrite = false;

  accesses.push_back(access1);
  accesses.push_back(access2);

  velocypack::Builder builder;
  velocypack::serialize(builder, accesses);

  velocypack::Slice slice = builder.slice();
  ASSERT_TRUE(slice.isArray());
  ASSERT_EQ(slice.length(), 2);

  velocypack::Slice first = slice[0];
  EXPECT_EQ(first["collection"].copyString(), "users");
  EXPECT_EQ(first["read"]["attributes"].length(), 1);

  velocypack::Slice second = slice[1];
  EXPECT_EQ(second["collection"].copyString(), "orders");
  EXPECT_EQ(second["read"]["attributes"].length(), 2);
}

TEST_F(AttributeDetectorTest, InspectorRoundTrip) {
  // Test read-only access (no write)
  AttributeDetector::CollectionAccess readOnly;
  readOnly.collectionName = "products";
  readOnly.readAttributes.insert({"price"});
  readOnly.readAttributes.insert({"stock"});
  readOnly.requiresAllAttributesRead = false;
  readOnly.requiresAllAttributesWrite = false;

  velocypack::Builder builder;
  velocypack::serialize(builder, readOnly);
  velocypack::Slice slice = builder.slice();

  EXPECT_EQ(slice["collection"].copyString(), "products");

  velocypack::Slice read = slice["read"];
  EXPECT_FALSE(read["requiresAll"].getBool());
  ASSERT_TRUE(read["attributes"].isArray());
  EXPECT_EQ(read["attributes"].length(), 2);

  // Write attributes array is always empty - write operations set
  // requiresAllAttributesWrite=true instead of tracking individual attributes
  velocypack::Slice write = slice["write"];
  EXPECT_FALSE(write["requiresAll"].getBool());
  ASSERT_TRUE(write["attributes"].isArray());
  EXPECT_EQ(write["attributes"].length(), 0);
}

TEST_F(AttributeDetectorTest, InspectorNestedAttributes) {
  // Test nested attribute paths
  AttributeDetector::CollectionAccess access;
  access.collectionName = "documents";
  // Top-level attribute
  access.readAttributes.insert({"name"});
  // Nested attribute: meta.lang
  access.readAttributes.insert({"meta", "lang"});
  // Deeply nested: item.value1.value2
  access.readAttributes.insert({"item", "value1", "value2"});
  access.requiresAllAttributesRead = false;
  access.requiresAllAttributesWrite = false;

  velocypack::Builder builder;
  velocypack::serialize(builder, access);
  velocypack::Slice slice = builder.slice();

  EXPECT_EQ(slice["collection"].copyString(), "documents");

  velocypack::Slice readAttrs = slice["read"]["attributes"];
  ASSERT_TRUE(readAttrs.isArray());
  EXPECT_EQ(readAttrs.length(), 3);

  // Each attribute is an array representing the path
  // Note: std::set orders paths lexicographically
  // ["item", "value1", "value2"], ["meta", "lang"], ["name"]
  bool foundNested = false;
  for (size_t i = 0; i < readAttrs.length(); ++i) {
    velocypack::Slice attr = readAttrs[i];
    ASSERT_TRUE(attr.isArray());
    if (attr.length() == 2 && attr[0].copyString() == "meta" &&
        attr[1].copyString() == "lang") {
      foundNested = true;
    }
  }
  EXPECT_TRUE(foundNested) << "Expected to find nested path [\"meta\", \"lang\"]";
}

TEST_F(AttributeDetectorTest, InspectorWriteOperation) {
  // Test write operation - always requires all attributes for both read/write
  AttributeDetector::CollectionAccess writeOp;
  writeOp.collectionName = "products";
  writeOp.requiresAllAttributesRead = true;
  writeOp.requiresAllAttributesWrite = true;

  velocypack::Builder builder;
  velocypack::serialize(builder, writeOp);
  velocypack::Slice slice = builder.slice();

  EXPECT_EQ(slice["collection"].copyString(), "products");

  velocypack::Slice read = slice["read"];
  EXPECT_TRUE(read["requiresAll"].getBool());

  velocypack::Slice write = slice["write"];
  EXPECT_TRUE(write["requiresAll"].getBool());
  // Write attributes array is always empty
  ASSERT_TRUE(write["attributes"].isArray());
  EXPECT_EQ(write["attributes"].length(), 0);
}

}  // namespace arangodb::tests::aql
