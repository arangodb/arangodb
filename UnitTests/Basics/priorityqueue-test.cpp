////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for PriorityQueue
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

#define BOOST_TEST_INCLUDED
#include <boost/test/unit_test.hpp>

#include "Basics/ShortestPathFinder.h"
#include "Basics/voc-errors.h"

using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct CPriorityQueueSetup {
  CPriorityQueueSetup () {
    BOOST_TEST_MESSAGE("setup PriorityQueue");
  }

  ~CPriorityQueueSetup () {
    BOOST_TEST_MESSAGE("tear-down PriorityQueue");
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

struct MyValue {
  std::string _key;
  unsigned int _weight;
  unsigned int weight() const {
    return _weight;
  }
  std::string const& getKey() const {
    return _key;
  }
  void setWeight(unsigned int const w) {
    _weight = w;
  }
  MyValue (std::string k, unsigned int w) : _key(k), _weight(w) {
  }
};

BOOST_FIXTURE_TEST_SUITE(CPriorityQueueTest, CPriorityQueueSetup)

////////////////////////////////////////////////////////////////////////////////
/// @brief test filling in ascending weight order
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_deque_case) {
  arangodb::basics::PriorityQueue<std::string, MyValue, unsigned int> pq;
 
  BOOST_CHECK_EQUAL(0, (int) pq.size());
  BOOST_CHECK_EQUAL(true, pq.empty());

  bool b;
  MyValue* v;

  b = pq.insert("a", new MyValue("a", 1));
  BOOST_CHECK_EQUAL(b, true);
  b = pq.insert("b", new MyValue("b", 2));
  BOOST_CHECK_EQUAL(b, true);
  b = pq.insert("c", new MyValue("c", 2));
  BOOST_CHECK_EQUAL(b, true);
  b = pq.insert("d", new MyValue("d", 4));
  BOOST_CHECK_EQUAL(b, true);
  v = new MyValue("c", 5);
  b = pq.insert("c", v);
  BOOST_CHECK_EQUAL(b, false);
  delete v;

  BOOST_CHECK_EQUAL(4, (int) pq.size());
  BOOST_CHECK_EQUAL(false, pq.empty());

  MyValue const* p;

  p = pq.find("a");
  BOOST_CHECK_EQUAL((int) p->_weight, 1);
  p = pq.find("b");
  BOOST_CHECK_EQUAL((int) p->_weight, 2);
  p = pq.find("c");
  BOOST_CHECK_EQUAL((int) p->_weight, 2);
  p = pq.find("d");
  BOOST_CHECK_EQUAL((int) p->_weight, 4);
  p = pq.find("abc");
  BOOST_CHECK(p == nullptr);

  std::string k;

  p = pq.getMinimal();
  BOOST_CHECK_EQUAL(p->_key, "a");
  b = pq.popMinimal(k, v);
  BOOST_CHECK_EQUAL(b, true);
  BOOST_CHECK_EQUAL(k, "a");
  BOOST_CHECK_EQUAL(v->_key, "a");
  BOOST_CHECK_EQUAL((int) v->_weight, 1);
  delete v;

  p = pq.getMinimal();
  BOOST_CHECK_EQUAL(p->_key, "b");
  b = pq.popMinimal(k, v);
  BOOST_CHECK_EQUAL(b, true);
  BOOST_CHECK_EQUAL(k, "b");
  BOOST_CHECK_EQUAL(v->_key, "b");
  BOOST_CHECK_EQUAL((int) v->_weight, 2);
  delete v;

  p = pq.getMinimal();
  BOOST_CHECK_EQUAL(p->_key, "c");
  b = pq.popMinimal(k, v);
  BOOST_CHECK_EQUAL(b, true);
  BOOST_CHECK_EQUAL(k, "c");
  BOOST_CHECK_EQUAL(v->_key, "c");
  BOOST_CHECK_EQUAL((int) v->_weight, 2);
  delete v;

  BOOST_CHECK_EQUAL((int) pq.size(), 1);
  BOOST_CHECK_EQUAL(pq.empty(), false);

  p = pq.getMinimal();
  BOOST_CHECK_EQUAL(p->_key, "d");
  b = pq.popMinimal(k, v);
  BOOST_CHECK_EQUAL(b, true);
  BOOST_CHECK_EQUAL(k, "d");
  BOOST_CHECK_EQUAL(v->_key, "d");
  BOOST_CHECK_EQUAL((int) v->_weight, 4);
  delete v;

  BOOST_CHECK_EQUAL((int) pq.size(), 0);
  BOOST_CHECK_EQUAL(pq.empty(), true);

  p = pq.getMinimal();
  BOOST_CHECK(p == nullptr);
  b = pq.popMinimal(k, v);
  BOOST_CHECK_EQUAL(b, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test filling in random weight order
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_heap_case) {
  arangodb::basics::PriorityQueue<std::string, MyValue, unsigned int> pq;
 
  BOOST_CHECK_EQUAL(0, (int) pq.size());
  BOOST_CHECK_EQUAL(true, pq.empty());

  bool b;
  MyValue* v;

  b = pq.insert("a", new MyValue("a", 4));
  BOOST_CHECK_EQUAL(b, true);
  b = pq.insert("b", new MyValue("b", 1));
  BOOST_CHECK_EQUAL(b, true);
  b = pq.insert("c", new MyValue("c", 2));
  BOOST_CHECK_EQUAL(b, true);
  b = pq.insert("d", new MyValue("d", 2));
  BOOST_CHECK_EQUAL(b, true);
  v = new MyValue("c", 5);
  b = pq.insert("c", v);
  BOOST_CHECK_EQUAL(b, false);
  delete v;

  BOOST_CHECK_EQUAL(4, (int) pq.size());
  BOOST_CHECK_EQUAL(false, pq.empty());

  MyValue const* p;

  p = pq.find("a");
  BOOST_CHECK_EQUAL((int) p->_weight, 4);
  p = pq.find("b");
  BOOST_CHECK_EQUAL((int) p->_weight, 1);
  p = pq.find("c");
  BOOST_CHECK_EQUAL((int) p->_weight, 2);
  p = pq.find("d");
  BOOST_CHECK_EQUAL((int) p->_weight, 2);
  p = pq.find("abc");
  BOOST_CHECK(p == nullptr);

  std::string k;

  p = pq.getMinimal();
  BOOST_CHECK_EQUAL(p->_key, "b");
  b = pq.popMinimal(k, v);
  BOOST_CHECK_EQUAL(b, true);
  BOOST_CHECK_EQUAL(k, "b");
  BOOST_CHECK_EQUAL(v->_key, "b");
  BOOST_CHECK_EQUAL((int) v->_weight, 1);
  delete v;

  p = pq.getMinimal();
  BOOST_CHECK_EQUAL(p->_key, "d");
  b = pq.popMinimal(k, v);
  BOOST_CHECK_EQUAL(b, true);
  BOOST_CHECK_EQUAL(k, "d");
  BOOST_CHECK_EQUAL(v->_key, "d");
  BOOST_CHECK_EQUAL((int) v->_weight, 2);
  delete v;

  p = pq.getMinimal();
  BOOST_CHECK_EQUAL(p->_key, "c");
  b = pq.popMinimal(k, v);
  BOOST_CHECK_EQUAL(b, true);
  BOOST_CHECK_EQUAL(k, "c");
  BOOST_CHECK_EQUAL(v->_key, "c");
  BOOST_CHECK_EQUAL((int) v->_weight, 2);
  delete v;

  BOOST_CHECK_EQUAL((int) pq.size(), 1);
  BOOST_CHECK_EQUAL(pq.empty(), false);

  p = pq.getMinimal();
  BOOST_CHECK_EQUAL(p->_key, "a");
  b = pq.popMinimal(k, v);
  BOOST_CHECK_EQUAL(b, true);
  BOOST_CHECK_EQUAL(k, "a");
  BOOST_CHECK_EQUAL(v->_key, "a");
  BOOST_CHECK_EQUAL((int) v->_weight, 4);
  delete v;

  BOOST_CHECK_EQUAL((int) pq.size(), 0);
  BOOST_CHECK_EQUAL(pq.empty(), true);

  p = pq.getMinimal();
  BOOST_CHECK(p == nullptr);
  b = pq.popMinimal(k, v);
  BOOST_CHECK_EQUAL(b, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test filling in ascending weight order, but then doing lowerWeight
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_deque_case_with_lowering) {
  arangodb::basics::PriorityQueue<std::string, MyValue, unsigned int> pq;
 
  BOOST_CHECK_EQUAL(0, (int) pq.size());
  BOOST_CHECK_EQUAL(true, pq.empty());

  bool b;
  MyValue* v;

  b = pq.insert("a", new MyValue("a", 1));
  BOOST_CHECK_EQUAL(b, true);
  b = pq.insert("b", new MyValue("b", 2));
  BOOST_CHECK_EQUAL(b, true);
  b = pq.insert("c", new MyValue("c", 2));
  BOOST_CHECK_EQUAL(b, true);
  b = pq.insert("d", new MyValue("d", 4));
  BOOST_CHECK_EQUAL(b, true);
  v = new MyValue("c", 5);
  b = pq.insert("c", v);
  BOOST_CHECK_EQUAL(b, false);
  delete v;

  BOOST_CHECK_EQUAL(4, (int) pq.size());
  BOOST_CHECK_EQUAL(false, pq.empty());

  pq.lowerWeight("d", 1);   // This moves "d" before "b" and "c"

  MyValue const* p;

  p = pq.find("a");
  BOOST_CHECK_EQUAL((int) p->_weight, 1);
  p = pq.find("b");
  BOOST_CHECK_EQUAL((int) p->_weight, 2);
  p = pq.find("c");
  BOOST_CHECK_EQUAL((int) p->_weight, 2);
  p = pq.find("d");
  BOOST_CHECK_EQUAL((int) p->_weight, 1);
  p = pq.find("abc");
  BOOST_CHECK(p == nullptr);

  std::string k;

  p = pq.getMinimal();
  BOOST_CHECK_EQUAL(p->_key, "a");
  b = pq.popMinimal(k, v);
  BOOST_CHECK_EQUAL(b, true);
  BOOST_CHECK_EQUAL(k, "a");
  BOOST_CHECK_EQUAL(v->_key, "a");
  BOOST_CHECK_EQUAL((int) v->_weight, 1);
  delete v;

  p = pq.getMinimal();
  BOOST_CHECK_EQUAL(p->_key, "d");
  b = pq.popMinimal(k, v);
  BOOST_CHECK_EQUAL(b, true);
  BOOST_CHECK_EQUAL(k, "d");
  BOOST_CHECK_EQUAL(v->_key, "d");
  BOOST_CHECK_EQUAL((int) v->_weight, 1);
  delete v;

  p = pq.getMinimal();
  BOOST_CHECK_EQUAL(p->_key, "c");
  b = pq.popMinimal(k, v);
  BOOST_CHECK_EQUAL(b, true);
  BOOST_CHECK_EQUAL(k, "c");
  BOOST_CHECK_EQUAL(v->_key, "c");
  BOOST_CHECK_EQUAL((int) v->_weight, 2);
  delete v;

  BOOST_CHECK_EQUAL((int) pq.size(), 1);
  BOOST_CHECK_EQUAL(pq.empty(), false);

  p = pq.getMinimal();
  BOOST_CHECK_EQUAL(p->_key, "b");
  b = pq.popMinimal(k, v);
  BOOST_CHECK_EQUAL(b, true);
  BOOST_CHECK_EQUAL(k, "b");
  BOOST_CHECK_EQUAL(v->_key, "b");
  BOOST_CHECK_EQUAL((int) v->_weight, 2);
  delete v;

  BOOST_CHECK_EQUAL((int) pq.size(), 0);
  BOOST_CHECK_EQUAL(pq.empty(), true);

  p = pq.getMinimal();
  BOOST_CHECK(p == nullptr);
  b = pq.popMinimal(k, v);
  BOOST_CHECK_EQUAL(b, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test filling in random weight order, and later lowering some weight
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_heap_case_with_lowering) {
  arangodb::basics::PriorityQueue<std::string, MyValue, unsigned int> pq;
 
  BOOST_CHECK_EQUAL(0, (int) pq.size());
  BOOST_CHECK_EQUAL(true, pq.empty());

  bool b;
  MyValue* v;

  b = pq.insert("a", new MyValue("a", 4));
  BOOST_CHECK_EQUAL(b, true);
  b = pq.insert("b", new MyValue("b", 2));
  BOOST_CHECK_EQUAL(b, true);
  b = pq.insert("c", new MyValue("c", 3));
  BOOST_CHECK_EQUAL(b, true);
  b = pq.insert("d", new MyValue("d", 3));
  BOOST_CHECK_EQUAL(b, true);
  v = new MyValue("c", 5);
  b = pq.insert("c", v);
  BOOST_CHECK_EQUAL(b, false);
  delete v;

  BOOST_CHECK_EQUAL(4, (int) pq.size());
  BOOST_CHECK_EQUAL(false, pq.empty());

  pq.lowerWeight("a", 1);   // This moves "a" before all others

  MyValue const* p;

  p = pq.find("a");
  BOOST_CHECK_EQUAL((int) p->_weight, 1);
  p = pq.find("b");
  BOOST_CHECK_EQUAL((int) p->_weight, 2);
  p = pq.find("c");
  BOOST_CHECK_EQUAL((int) p->_weight, 3);
  p = pq.find("d");
  BOOST_CHECK_EQUAL((int) p->_weight, 3);
  p = pq.find("abc");
  BOOST_CHECK(p == nullptr);

  std::string k;

  p = pq.getMinimal();
  BOOST_CHECK_EQUAL(p->_key, "a");
  b = pq.popMinimal(k, v);
  BOOST_CHECK_EQUAL(b, true);
  BOOST_CHECK_EQUAL(k, "a");
  BOOST_CHECK_EQUAL(v->_key, "a");
  BOOST_CHECK_EQUAL((int) v->_weight, 1);
  delete v;

  p = pq.getMinimal();
  BOOST_CHECK_EQUAL(p->_key, "b");
  b = pq.popMinimal(k, v);
  BOOST_CHECK_EQUAL(b, true);
  BOOST_CHECK_EQUAL(k, "b");
  BOOST_CHECK_EQUAL(v->_key, "b");
  BOOST_CHECK_EQUAL((int) v->_weight, 2);
  delete v;

  p = pq.getMinimal();
  BOOST_CHECK_EQUAL(p->_key, "c");
  b = pq.popMinimal(k, v);
  BOOST_CHECK_EQUAL(b, true);
  BOOST_CHECK_EQUAL(k, "c");
  BOOST_CHECK_EQUAL(v->_key, "c");
  BOOST_CHECK_EQUAL((int) v->_weight, 3);
  delete v;

  BOOST_CHECK_EQUAL((int) pq.size(), 1);
  BOOST_CHECK_EQUAL(pq.empty(), false);

  p = pq.getMinimal();
  BOOST_CHECK_EQUAL(p->_key, "d");
  b = pq.popMinimal(k, v);
  BOOST_CHECK_EQUAL(b, true);
  BOOST_CHECK_EQUAL(k, "d");
  BOOST_CHECK_EQUAL(v->_key, "d");
  BOOST_CHECK_EQUAL((int) v->_weight, 3);
  delete v;

  BOOST_CHECK_EQUAL((int) pq.size(), 0);
  BOOST_CHECK_EQUAL(pq.empty(), true);

  p = pq.getMinimal();
  BOOST_CHECK(p == nullptr);
  b = pq.popMinimal(k, v);
  BOOST_CHECK_EQUAL(b, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE_END ()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
