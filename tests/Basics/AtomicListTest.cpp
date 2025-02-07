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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "Containers/AtomicList.h"

#include "gtest/gtest.h"

using namespace arangodb;

TEST(AtomicListTests, testBasicOperation) {
  AtomicList<int> list;
  list.prepend(1);
  list.prepend(2);
  list.prepend(3);
  AtomicList<int>::Node* p = list.getSnapshot();
  ASSERT_TRUE(p != nullptr);
  ASSERT_EQ(p->_data, 3);
  p = p->next();
  ASSERT_TRUE(p != nullptr);
  ASSERT_EQ(p->_data, 2);
  p = p->next();
  ASSERT_TRUE(p != nullptr);
  ASSERT_EQ(p->_data, 1);
  p = p->next();
  ASSERT_TRUE(p == nullptr);
}

struct Entry {
  int a;
  Entry(int a) : a(a) {}
  size_t memoryUsage() const { return sizeof(Entry); }
};

TEST(BoundedListTest, testBasicOperation) {
  BoundedList<Entry> list(1024 * 1024, 3);
  list.prepend(Entry{1});
  list.prepend(Entry{2});
  list.prepend(Entry{3});
  std::shared_ptr<AtomicList<Entry>> a = list.getCurrentSnapshot();
  AtomicList<Entry>::Node* p = a->getSnapshot();
  ASSERT_TRUE(p != nullptr);
  ASSERT_EQ(p->_data.a, 3);
  p = p->next();
  ASSERT_TRUE(p != nullptr);
  ASSERT_EQ(p->_data.a, 2);
  p = p->next();
  ASSERT_TRUE(p != nullptr);
  ASSERT_EQ(p->_data.a, 1);
  p = p->next();
  ASSERT_TRUE(p == nullptr);
}
