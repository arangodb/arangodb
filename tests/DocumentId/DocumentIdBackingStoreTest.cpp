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
#include "DocumentId/DocumentIdBackingStore.h"

#include "Basics/GlobalResourceMonitor.h"

using namespace arangodb;

struct DocumentIdBackingStoreTest : testing::Test {
  DocumentIdBackingStoreTest() : _global{}, _monitor{_global} {}

  GlobalResourceMonitor _global;
  ResourceMonitor _monitor;
};

TEST_F(DocumentIdBackingStoreTest, create) {
  auto testee = DocumentIdBackingStore(_monitor);
  ASSERT_TRUE(true);
}

TEST_F(DocumentIdBackingStoreTest, add_a_document_id) {
  auto testee = DocumentIdBackingStore(_monitor);

  ASSERT_EQ(testee.numberOfIds(), 0);

  auto sv_id = std::string_view{"collection/key"};

  auto g = testee.addFromStringView(sv_id);

  ASSERT_EQ(testee.numberOfIds(), 1);

  ASSERT_TRUE(g.has_value());
  ASSERT_EQ(g->collectionName(), "collection");
  ASSERT_EQ(g->key(), "key");

  auto h = testee.addFromDocumentIdView(g.value());

  ASSERT_EQ(h.key(), "key");

  ASSERT_EQ(testee.numberOfIds(), 1);

  auto i = testee.addFromStringView(std::string_view{"fooonana/barabar"});

  ASSERT_EQ(i->key(), "barabar");
  ASSERT_EQ(testee.numberOfIds(), 2);
}

TEST_F(DocumentIdBackingStoreTest, add_a_document_id_x) {
  auto testee = DocumentIdBackingStore(_monitor);

  auto maybeId = DocumentIdView::fromStringView(std::string_view{"foo/bar"});

  auto g = testee.addFromDocumentIdView(maybeId.value());

  ASSERT_EQ(g.key(), "bar");

  ASSERT_EQ(testee.numberOfIds(), 1);
}
