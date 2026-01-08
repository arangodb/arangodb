////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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

#include <gtest/gtest.h>
#include "DocumentId/DocumentId.h"

using namespace arangodb;

TEST(DocumentIdTest, does_not_create_document_id_without_separator) {
  auto testee = DocumentId::fromString("foo");
  ASSERT_FALSE(testee.has_value());
}

TEST(DocumentIdTest, can_create_document_id_with_separator) {
  auto testee = DocumentId::fromString("collection/key");
  ASSERT_TRUE(testee.has_value());
}

TEST(DocumentIdTest, separates_collection_and_key) {
  auto maybeTestee = DocumentId::fromString("collection/key");
  auto testee = maybeTestee.value();

  auto collectionName = testee.collectionName();
  ASSERT_EQ(collectionName, "collection");

  auto collectionNameView = testee.collectionNameView();
  ASSERT_EQ(collectionNameView, "collection");

  auto key = testee.key();
  ASSERT_EQ(key, "key");

  auto keyView = testee.keyView();
  ASSERT_EQ(keyView, std::string_view{"key"});
}

TEST(DocumentIdTest, equality_works) {
  auto testee_a = DocumentId::fromString("collection/key").value();
  auto testee_b = DocumentId::fromString("collection/key").value();
  auto testee_c = DocumentId::fromString("collection2/key").value();
  auto testee_d = DocumentId::fromString("collection/key2").value();

  EXPECT_EQ(testee_a, testee_b);
  EXPECT_NE(testee_a, testee_c);
  EXPECT_NE(testee_b, testee_d);
}
