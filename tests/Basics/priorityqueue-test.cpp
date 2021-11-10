////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
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
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "gtest/gtest.h"

#include "Basics/voc-errors.h"
#include "Graph/ShortestPathPriorityQueue.h"

using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

struct MyValue {
  std::string _key;
  unsigned int _weight;
  unsigned int weight() const { return _weight; }
  std::string const& getKey() const { return _key; }
  void setWeight(unsigned int const w) { _weight = w; }
  MyValue(std::string k, unsigned int w) : _key(k), _weight(w) {}
};

TEST(CPriorityQueueTest, tst_deque_case) {
  arangodb::graph::ShortestPathPriorityQueue<std::string, MyValue, unsigned int> pq;

  EXPECT_EQ(0, (int)pq.size());
  EXPECT_TRUE(pq.empty());

  bool b;
  MyValue* v = nullptr;

  b = pq.insert("a", std::make_unique<MyValue>("a", 1));
  EXPECT_TRUE(b);
  b = pq.insert("b", std::make_unique<MyValue>("b", 2));
  EXPECT_TRUE(b);
  b = pq.insert("c", std::make_unique<MyValue>("c", 2));
  EXPECT_TRUE(b);
  b = pq.insert("d", std::make_unique<MyValue>("d", 4));
  EXPECT_TRUE(b);
  b = pq.insert("c", std::make_unique<MyValue>("c", 5));
  EXPECT_FALSE(b);

  EXPECT_EQ(4, (int)pq.size());
  EXPECT_FALSE(pq.empty());

  MyValue const* p;

  p = pq.find("a");
  EXPECT_EQ((int)p->_weight, 1);
  p = pq.find("b");
  EXPECT_EQ((int)p->_weight, 2);
  p = pq.find("c");
  EXPECT_EQ((int)p->_weight, 2);
  p = pq.find("d");
  EXPECT_EQ((int)p->_weight, 4);
  p = pq.find("abc");
  EXPECT_EQ(p, nullptr);

  std::string k;

  p = pq.getMinimal();
  EXPECT_EQ(p->_key, "a");
  b = pq.popMinimal(k, v);
  EXPECT_TRUE(b);
  EXPECT_EQ(k, "a");
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(v->_key, "a");
  EXPECT_EQ((int)v->_weight, 1);

  p = pq.getMinimal();
  EXPECT_EQ(p->_key, "b");
  b = pq.popMinimal(k, v);
  EXPECT_TRUE(b);
  EXPECT_EQ(k, "b");
  EXPECT_EQ(v->_key, "b");
  EXPECT_EQ((int)v->_weight, 2);

  p = pq.getMinimal();
  EXPECT_EQ(p->_key, "c");
  b = pq.popMinimal(k, v);
  EXPECT_TRUE(b);
  EXPECT_EQ(k, "c");
  EXPECT_EQ(v->_key, "c");
  EXPECT_EQ((int)v->_weight, 2);

  EXPECT_EQ((int)pq.size(), 1);
  EXPECT_FALSE(pq.empty());

  p = pq.getMinimal();
  EXPECT_EQ(p->_key, "d");
  b = pq.popMinimal(k, v);
  EXPECT_TRUE(b);
  EXPECT_EQ(k, "d");
  EXPECT_EQ(v->_key, "d");
  EXPECT_EQ((int)v->_weight, 4);

  EXPECT_EQ((int)pq.size(), 0);
  EXPECT_TRUE(pq.empty());

  p = pq.getMinimal();
  EXPECT_EQ(p, nullptr);
  b = pq.popMinimal(k, v);
  EXPECT_FALSE(b);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test filling in random weight order
////////////////////////////////////////////////////////////////////////////////

TEST(CPriorityQueueTest, tst_heap_case) {
  arangodb::graph::ShortestPathPriorityQueue<std::string, MyValue, unsigned int> pq;

  EXPECT_EQ(0, (int)pq.size());
  EXPECT_TRUE(pq.empty());

  bool b;
  MyValue* v = nullptr;

  b = pq.insert("a", std::make_unique<MyValue>("a", 4));
  EXPECT_TRUE(b);
  b = pq.insert("b", std::make_unique<MyValue>("b", 1));
  EXPECT_TRUE(b);
  b = pq.insert("c", std::make_unique<MyValue>("c", 2));
  EXPECT_TRUE(b);
  b = pq.insert("d", std::make_unique<MyValue>("d", 2));
  EXPECT_TRUE(b);
  b = pq.insert("c", std::make_unique<MyValue>("c", 5));
  EXPECT_FALSE(b);

  EXPECT_EQ(4, (int)pq.size());
  EXPECT_FALSE(pq.empty());

  MyValue const* p;

  p = pq.find("a");
  EXPECT_EQ((int)p->_weight, 4);
  p = pq.find("b");
  EXPECT_EQ((int)p->_weight, 1);
  p = pq.find("c");
  EXPECT_EQ((int)p->_weight, 2);
  p = pq.find("d");
  EXPECT_EQ((int)p->_weight, 2);
  p = pq.find("abc");
  EXPECT_EQ(p, nullptr);

  std::string k;

  p = pq.getMinimal();
  EXPECT_EQ(p->_key, "b");
  b = pq.popMinimal(k, v);
  EXPECT_TRUE(b);
  EXPECT_EQ(k, "b");
  EXPECT_EQ(v->_key, "b");
  EXPECT_EQ((int)v->_weight, 1);

  p = pq.getMinimal();
  EXPECT_EQ(p->_key, "d");
  b = pq.popMinimal(k, v);
  EXPECT_TRUE(b);
  EXPECT_EQ(k, "d");
  EXPECT_EQ(v->_key, "d");
  EXPECT_EQ((int)v->_weight, 2);

  p = pq.getMinimal();
  EXPECT_EQ(p->_key, "c");
  b = pq.popMinimal(k, v);
  EXPECT_TRUE(b);
  EXPECT_EQ(k, "c");
  EXPECT_EQ(v->_key, "c");
  EXPECT_EQ((int)v->_weight, 2);

  EXPECT_EQ((int)pq.size(), 1);
  EXPECT_FALSE(pq.empty());

  p = pq.getMinimal();
  EXPECT_EQ(p->_key, "a");
  b = pq.popMinimal(k, v);
  EXPECT_TRUE(b);
  EXPECT_EQ(k, "a");
  EXPECT_EQ(v->_key, "a");
  EXPECT_EQ((int)v->_weight, 4);

  EXPECT_EQ((int)pq.size(), 0);
  EXPECT_TRUE(pq.empty());

  p = pq.getMinimal();
  EXPECT_EQ(p, nullptr);
  b = pq.popMinimal(k, v);
  EXPECT_FALSE(b);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test filling in ascending weight order, but then doing lowerWeight
////////////////////////////////////////////////////////////////////////////////

TEST(CPriorityQueueTest, tst_deque_case_with_lowering) {
  arangodb::graph::ShortestPathPriorityQueue<std::string, MyValue, unsigned int> pq;

  EXPECT_EQ(0, (int)pq.size());
  EXPECT_TRUE(pq.empty());

  bool b;
  MyValue* v = nullptr;

  b = pq.insert("a", std::make_unique<MyValue>("a", 1));
  EXPECT_TRUE(b);
  b = pq.insert("b", std::make_unique<MyValue>("b", 2));
  EXPECT_TRUE(b);
  b = pq.insert("c", std::make_unique<MyValue>("c", 2));
  EXPECT_TRUE(b);
  b = pq.insert("d", std::make_unique<MyValue>("d", 4));
  EXPECT_TRUE(b);
  b = pq.insert("c", std::make_unique<MyValue>("c", 5));
  EXPECT_FALSE(b);

  EXPECT_EQ(4, (int)pq.size());
  EXPECT_FALSE(pq.empty());

  pq.lowerWeight("d", 1);  // This moves "d" before "b" and "c"

  MyValue const* p;

  p = pq.find("a");
  EXPECT_EQ((int)p->_weight, 1);
  p = pq.find("b");
  EXPECT_EQ((int)p->_weight, 2);
  p = pq.find("c");
  EXPECT_EQ((int)p->_weight, 2);
  p = pq.find("d");
  EXPECT_EQ((int)p->_weight, 1);
  p = pq.find("abc");
  EXPECT_EQ(p, nullptr);

  std::string k;

  p = pq.getMinimal();
  EXPECT_EQ(p->_key, "a");
  b = pq.popMinimal(k, v);
  EXPECT_TRUE(b);
  EXPECT_EQ(k, "a");
  EXPECT_EQ(v->_key, "a");
  EXPECT_EQ((int)v->_weight, 1);

  p = pq.getMinimal();
  EXPECT_EQ(p->_key, "d");
  b = pq.popMinimal(k, v);
  EXPECT_TRUE(b);
  EXPECT_EQ(k, "d");
  EXPECT_EQ(v->_key, "d");
  EXPECT_EQ((int)v->_weight, 1);

  p = pq.getMinimal();
  EXPECT_EQ(p->_key, "c");
  b = pq.popMinimal(k, v);
  EXPECT_TRUE(b);
  EXPECT_EQ(k, "c");
  EXPECT_EQ(v->_key, "c");
  EXPECT_EQ((int)v->_weight, 2);

  EXPECT_EQ((int)pq.size(), 1);
  EXPECT_FALSE(pq.empty());

  p = pq.getMinimal();
  EXPECT_EQ(p->_key, "b");
  b = pq.popMinimal(k, v);
  EXPECT_TRUE(b);
  EXPECT_EQ(k, "b");
  EXPECT_EQ(v->_key, "b");
  EXPECT_EQ((int)v->_weight, 2);

  EXPECT_EQ((int)pq.size(), 0);
  EXPECT_TRUE(pq.empty());

  p = pq.getMinimal();
  EXPECT_EQ(p, nullptr);
  b = pq.popMinimal(k, v);
  EXPECT_FALSE(b);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test filling in random weight order, and later lowering some weight
////////////////////////////////////////////////////////////////////////////////

TEST(CPriorityQueueTest, tst_heap_case_with_lowering) {
  arangodb::graph::ShortestPathPriorityQueue<std::string, MyValue, unsigned int> pq;

  EXPECT_EQ(0, (int)pq.size());
  EXPECT_TRUE(pq.empty());

  bool b;
  MyValue* v = nullptr;

  b = pq.insert("a", std::make_unique<MyValue>("a", 4));
  EXPECT_TRUE(b);
  b = pq.insert("b", std::make_unique<MyValue>("b", 2));
  EXPECT_TRUE(b);
  b = pq.insert("c", std::make_unique<MyValue>("c", 3));
  EXPECT_TRUE(b);
  b = pq.insert("d", std::make_unique<MyValue>("d", 3));
  EXPECT_TRUE(b);
  b = pq.insert("c", std::make_unique<MyValue>("c", 5));
  EXPECT_FALSE(b);
  delete v;

  EXPECT_EQ(4, (int)pq.size());
  EXPECT_FALSE(pq.empty());

  pq.lowerWeight("a", 1);  // This moves "a" before all others

  MyValue const* p;

  p = pq.find("a");
  EXPECT_EQ((int)p->_weight, 1);
  p = pq.find("b");
  EXPECT_EQ((int)p->_weight, 2);
  p = pq.find("c");
  EXPECT_EQ((int)p->_weight, 3);
  p = pq.find("d");
  EXPECT_EQ((int)p->_weight, 3);
  p = pq.find("abc");
  EXPECT_EQ(p, nullptr);

  std::string k;

  p = pq.getMinimal();
  EXPECT_EQ(p->_key, "a");
  b = pq.popMinimal(k, v);
  EXPECT_TRUE(b);
  EXPECT_EQ(k, "a");
  EXPECT_EQ(v->_key, "a");
  EXPECT_EQ((int)v->_weight, 1);

  p = pq.getMinimal();
  EXPECT_EQ(p->_key, "b");
  b = pq.popMinimal(k, v);
  EXPECT_TRUE(b);
  EXPECT_EQ(k, "b");
  EXPECT_EQ(v->_key, "b");
  EXPECT_EQ((int)v->_weight, 2);

  p = pq.getMinimal();
  EXPECT_EQ(p->_key, "c");
  b = pq.popMinimal(k, v);
  EXPECT_TRUE(b);
  EXPECT_EQ(k, "c");
  EXPECT_EQ(v->_key, "c");
  EXPECT_EQ((int)v->_weight, 3);

  EXPECT_EQ((int)pq.size(), 1);
  EXPECT_FALSE(pq.empty());

  p = pq.getMinimal();
  EXPECT_EQ(p->_key, "d");
  b = pq.popMinimal(k, v);
  EXPECT_TRUE(b);
  EXPECT_EQ(k, "d");
  EXPECT_EQ(v->_key, "d");
  EXPECT_EQ((int)v->_weight, 3);

  EXPECT_EQ((int)pq.size(), 0);
  EXPECT_TRUE(pq.empty());

  p = pq.getMinimal();
  EXPECT_EQ(p, nullptr);
  b = pq.popMinimal(k, v);
  EXPECT_FALSE(b);
}
