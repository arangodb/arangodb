////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for ShortestPathPriorityQueue
///
/// @file
///
/// DISCLAIMER
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

#include "catch.hpp"

#include "Basics/voc-errors.h"
#include "Graph/ShortestPathPriorityQueue.h"

using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

struct MyValue {
  std::string _key;
  unsigned int _weight;
  unsigned int weight() const { return _weight; }
  std::string const& getKey() const { return _key; }
  void setWeight(unsigned int const w) { _weight = w; }
  MyValue(std::string k, unsigned int w) : _key(k), _weight(w) {}
};

TEST_CASE("CPriorityQueueTest", "[cpriorityqueue]") {
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief test filling in ascending weight order
  ////////////////////////////////////////////////////////////////////////////////

  SECTION("tst_deque_case") {
    arangodb::graph::ShortestPathPriorityQueue<std::string, MyValue, unsigned int> pq;

    CHECK(0 == (int)pq.size());
    CHECK(true == pq.empty());

    bool b;
    MyValue* v = nullptr;

    b = pq.insert("a", std::make_unique<MyValue>("a", 1));
    CHECK(b == true);
    b = pq.insert("b", std::make_unique<MyValue>("b", 2));
    CHECK(b == true);
    b = pq.insert("c", std::make_unique<MyValue>("c", 2));
    CHECK(b == true);
    b = pq.insert("d", std::make_unique<MyValue>("d", 4));
    CHECK(b == true);
    b = pq.insert("c", std::make_unique<MyValue>("c", 5));
    CHECK(b == false);

    CHECK(4 == (int)pq.size());
    CHECK(false == pq.empty());

    MyValue const* p;

    p = pq.find("a");
    CHECK((int)p->_weight == 1);
    p = pq.find("b");
    CHECK((int)p->_weight == 2);
    p = pq.find("c");
    CHECK((int)p->_weight == 2);
    p = pq.find("d");
    CHECK((int)p->_weight == 4);
    p = pq.find("abc");
    CHECK(p == nullptr);

    std::string k;

    p = pq.getMinimal();
    CHECK(p->_key == "a");
    b = pq.popMinimal(k, v);
    CHECK(b == true);
    CHECK(k == "a");
    REQUIRE(v != nullptr);
    CHECK(v->_key == "a");
    CHECK((int)v->_weight == 1);

    p = pq.getMinimal();
    CHECK(p->_key == "b");
    b = pq.popMinimal(k, v);
    CHECK(b == true);
    CHECK(k == "b");
    CHECK(v->_key == "b");
    CHECK((int)v->_weight == 2);

    p = pq.getMinimal();
    CHECK(p->_key == "c");
    b = pq.popMinimal(k, v);
    CHECK(b == true);
    CHECK(k == "c");
    CHECK(v->_key == "c");
    CHECK((int)v->_weight == 2);

    CHECK((int)pq.size() == 1);
    CHECK(pq.empty() == false);

    p = pq.getMinimal();
    CHECK(p->_key == "d");
    b = pq.popMinimal(k, v);
    CHECK(b == true);
    CHECK(k == "d");
    CHECK(v->_key == "d");
    CHECK((int)v->_weight == 4);

    CHECK((int)pq.size() == 0);
    CHECK(pq.empty() == true);

    p = pq.getMinimal();
    CHECK(p == nullptr);
    b = pq.popMinimal(k, v);
    CHECK(b == false);
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief test filling in random weight order
  ////////////////////////////////////////////////////////////////////////////////

  SECTION("tst_heap_case") {
    arangodb::graph::ShortestPathPriorityQueue<std::string, MyValue, unsigned int> pq;

    CHECK(0 == (int)pq.size());
    CHECK(true == pq.empty());

    bool b;
    MyValue* v = nullptr;

    b = pq.insert("a", std::make_unique<MyValue>("a", 4));
    CHECK(b == true);
    b = pq.insert("b", std::make_unique<MyValue>("b", 1));
    CHECK(b == true);
    b = pq.insert("c", std::make_unique<MyValue>("c", 2));
    CHECK(b == true);
    b = pq.insert("d", std::make_unique<MyValue>("d", 2));
    CHECK(b == true);
    b = pq.insert("c", std::make_unique<MyValue>("c", 5));
    CHECK(b == false);

    CHECK(4 == (int)pq.size());
    CHECK(false == pq.empty());

    MyValue const* p;

    p = pq.find("a");
    CHECK((int)p->_weight == 4);
    p = pq.find("b");
    CHECK((int)p->_weight == 1);
    p = pq.find("c");
    CHECK((int)p->_weight == 2);
    p = pq.find("d");
    CHECK((int)p->_weight == 2);
    p = pq.find("abc");
    CHECK(p == nullptr);

    std::string k;

    p = pq.getMinimal();
    CHECK(p->_key == "b");
    b = pq.popMinimal(k, v);
    CHECK(b == true);
    CHECK(k == "b");
    CHECK(v->_key == "b");
    CHECK((int)v->_weight == 1);

    p = pq.getMinimal();
    CHECK(p->_key == "d");
    b = pq.popMinimal(k, v);
    CHECK(b == true);
    CHECK(k == "d");
    CHECK(v->_key == "d");
    CHECK((int)v->_weight == 2);

    p = pq.getMinimal();
    CHECK(p->_key == "c");
    b = pq.popMinimal(k, v);
    CHECK(b == true);
    CHECK(k == "c");
    CHECK(v->_key == "c");
    CHECK((int)v->_weight == 2);

    CHECK((int)pq.size() == 1);
    CHECK(pq.empty() == false);

    p = pq.getMinimal();
    CHECK(p->_key == "a");
    b = pq.popMinimal(k, v);
    CHECK(b == true);
    CHECK(k == "a");
    CHECK(v->_key == "a");
    CHECK((int)v->_weight == 4);

    CHECK((int)pq.size() == 0);
    CHECK(pq.empty() == true);

    p = pq.getMinimal();
    CHECK(p == nullptr);
    b = pq.popMinimal(k, v);
    CHECK(b == false);
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief test filling in ascending weight order, but then doing lowerWeight
  ////////////////////////////////////////////////////////////////////////////////

  SECTION("tst_deque_case_with_lowering") {
    arangodb::graph::ShortestPathPriorityQueue<std::string, MyValue, unsigned int> pq;

    CHECK(0 == (int)pq.size());
    CHECK(true == pq.empty());

    bool b;
    MyValue* v = nullptr;

    b = pq.insert("a", std::make_unique<MyValue>("a", 1));
    CHECK(b == true);
    b = pq.insert("b", std::make_unique<MyValue>("b", 2));
    CHECK(b == true);
    b = pq.insert("c", std::make_unique<MyValue>("c", 2));
    CHECK(b == true);
    b = pq.insert("d", std::make_unique<MyValue>("d", 4));
    CHECK(b == true);
    b = pq.insert("c", std::make_unique<MyValue>("c", 5));
    CHECK(b == false);

    CHECK(4 == (int)pq.size());
    CHECK(false == pq.empty());

    pq.lowerWeight("d", 1);  // This moves "d" before "b" and "c"

    MyValue const* p;

    p = pq.find("a");
    CHECK((int)p->_weight == 1);
    p = pq.find("b");
    CHECK((int)p->_weight == 2);
    p = pq.find("c");
    CHECK((int)p->_weight == 2);
    p = pq.find("d");
    CHECK((int)p->_weight == 1);
    p = pq.find("abc");
    CHECK(p == nullptr);

    std::string k;

    p = pq.getMinimal();
    CHECK(p->_key == "a");
    b = pq.popMinimal(k, v);
    CHECK(b == true);
    CHECK(k == "a");
    CHECK(v->_key == "a");
    CHECK((int)v->_weight == 1);

    p = pq.getMinimal();
    CHECK(p->_key == "d");
    b = pq.popMinimal(k, v);
    CHECK(b == true);
    CHECK(k == "d");
    CHECK(v->_key == "d");
    CHECK((int)v->_weight == 1);

    p = pq.getMinimal();
    CHECK(p->_key == "c");
    b = pq.popMinimal(k, v);
    CHECK(b == true);
    CHECK(k == "c");
    CHECK(v->_key == "c");
    CHECK((int)v->_weight == 2);

    CHECK((int)pq.size() == 1);
    CHECK(pq.empty() == false);

    p = pq.getMinimal();
    CHECK(p->_key == "b");
    b = pq.popMinimal(k, v);
    CHECK(b == true);
    CHECK(k == "b");
    CHECK(v->_key == "b");
    CHECK((int)v->_weight == 2);

    CHECK((int)pq.size() == 0);
    CHECK(pq.empty() == true);

    p = pq.getMinimal();
    CHECK(p == nullptr);
    b = pq.popMinimal(k, v);
    CHECK(b == false);
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief test filling in random weight order, and later lowering some weight
  ////////////////////////////////////////////////////////////////////////////////

  SECTION("tst_heap_case_with_lowering") {
    arangodb::graph::ShortestPathPriorityQueue<std::string, MyValue, unsigned int> pq;

    CHECK(0 == (int)pq.size());
    CHECK(true == pq.empty());

    bool b;
    MyValue* v = nullptr;

    b = pq.insert("a", std::make_unique<MyValue>("a", 4));
    CHECK(b == true);
    b = pq.insert("b", std::make_unique<MyValue>("b", 2));
    CHECK(b == true);
    b = pq.insert("c", std::make_unique<MyValue>("c", 3));
    CHECK(b == true);
    b = pq.insert("d", std::make_unique<MyValue>("d", 3));
    CHECK(b == true);
    b = pq.insert("c", std::make_unique<MyValue>("c", 5));
    CHECK(b == false);
    delete v;

    CHECK(4 == (int)pq.size());
    CHECK(false == pq.empty());

    pq.lowerWeight("a", 1);  // This moves "a" before all others

    MyValue const* p;

    p = pq.find("a");
    CHECK((int)p->_weight == 1);
    p = pq.find("b");
    CHECK((int)p->_weight == 2);
    p = pq.find("c");
    CHECK((int)p->_weight == 3);
    p = pq.find("d");
    CHECK((int)p->_weight == 3);
    p = pq.find("abc");
    CHECK(p == nullptr);

    std::string k;

    p = pq.getMinimal();
    CHECK(p->_key == "a");
    b = pq.popMinimal(k, v);
    CHECK(b == true);
    CHECK(k == "a");
    CHECK(v->_key == "a");
    CHECK((int)v->_weight == 1);

    p = pq.getMinimal();
    CHECK(p->_key == "b");
    b = pq.popMinimal(k, v);
    CHECK(b == true);
    CHECK(k == "b");
    CHECK(v->_key == "b");
    CHECK((int)v->_weight == 2);

    p = pq.getMinimal();
    CHECK(p->_key == "c");
    b = pq.popMinimal(k, v);
    CHECK(b == true);
    CHECK(k == "c");
    CHECK(v->_key == "c");
    CHECK((int)v->_weight == 3);

    CHECK((int)pq.size() == 1);
    CHECK(pq.empty() == false);

    p = pq.getMinimal();
    CHECK(p->_key == "d");
    b = pq.popMinimal(k, v);
    CHECK(b == true);
    CHECK(k == "d");
    CHECK(v->_key == "d");
    CHECK((int)v->_weight == 3);

    CHECK((int)pq.size() == 0);
    CHECK(pq.empty() == true);

    p = pq.getMinimal();
    CHECK(p == nullptr);
    b = pq.popMinimal(k, v);
    CHECK(b == false);
  }
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|//
// --SECTION--\\|/// @\\}\\)" End:
