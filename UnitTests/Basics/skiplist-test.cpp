////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for SkipList
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#define BOOST_TEST_INCLUDED
#include <boost/test/unit_test.hpp>

#include "Basics/SkipList.h"
#include "Basics/voc-errors.h"
#include "Random/RandomGenerator.h"

#include <vector>

static bool Initialized = false;

using namespace std;

static int CmpElmElm (void const* left,
                      void const* right,
                      arangodb::basics::SkipListCmpType cmptype) {
  auto l = *(static_cast<int const*>(left));
  auto r = *(static_cast<int const*>(right));

  if (l != r) {
    return l < r ? -1 : 1;
  }
  return 0;
}

static int CmpKeyElm (void const* left,
                      void const* right) {
  auto l = *(static_cast<int const*>(left));
  auto r = *(static_cast<int const*>(right));

  if (l != r) {
    return l < r ? -1 : 1;
  }
  return 0;
}

static void FreeElm (void* e) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct CSkipListSetup {
  CSkipListSetup () {
    BOOST_TEST_MESSAGE("setup SkipList");
    if (!Initialized) {
      Initialized = true;
      arangodb::RandomGenerator::initialize(arangodb::RandomGenerator::RandomType::MERSENNE);
    }

  }

  ~CSkipListSetup () {
    BOOST_TEST_MESSAGE("tear-down SkipList");
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE(CSkipListTest, CSkipListSetup)

////////////////////////////////////////////////////////////////////////////////
/// @brief test filling in forward order
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_unique_forward) {
  arangodb::basics::SkipList<void, void> skiplist(CmpElmElm, CmpKeyElm, FreeElm, true, false);
  
  // check start node
  BOOST_CHECK_EQUAL((void*) 0, skiplist.startNode()->nextNode());
  BOOST_CHECK_EQUAL((void*) 0, skiplist.startNode()->prevNode());

  // check end node
  BOOST_CHECK_EQUAL((void*) 0, skiplist.endNode());
  
  BOOST_CHECK_EQUAL(0, (int) skiplist.getNrUsed());

  // insert 100 values
  std::vector<int*> values; 
  for (int i = 0; i < 100; ++i) {
    values.push_back(new int(i));
  }
  
  for (int i = 0; i < 100; ++i) {
    skiplist.insert(values[i]);
  }

  // now check consistency
  BOOST_CHECK_EQUAL(100, (int) skiplist.getNrUsed());

  // check start node
  BOOST_CHECK_EQUAL((void*) 0, skiplist.startNode()->prevNode());
  BOOST_CHECK_EQUAL(values[0], skiplist.startNode()->nextNode()->document());

  // check end node
  BOOST_CHECK_EQUAL((void*) 0, skiplist.endNode());

  arangodb::basics::SkipListNode<void, void>* current;

  // do a forward iteration
  current = skiplist.startNode()->nextNode();
  for (int i = 0; i < 100; ++i) {
    // compare value
    BOOST_CHECK_EQUAL((void*) values[i], current->document());

    // compare prev and next node
    if (i > 0) {
      BOOST_CHECK_EQUAL(values[i - 1], current->prevNode()->document());
    }
    if (i < 99) {
      BOOST_CHECK_EQUAL(values[i + 1], current->nextNode()->document());
    }
    current = current->nextNode();
  }
  
  // do a backward iteration
  current = skiplist.lookup(values[99]);
  for (int i = 99; i >= 0; --i) {
    // compare value
    BOOST_CHECK_EQUAL(values[i], current->document());

    // compare prev and next node
    if (i > 0) {
      BOOST_CHECK_EQUAL(values[i - 1], current->prevNode()->document());
    }
    if (i < 99) {
      BOOST_CHECK_EQUAL(values[i + 1], current->nextNode()->document());
    }
    current = current->prevNode();
  }

  for (int i = 0; i < 100; ++i) {
    BOOST_CHECK_EQUAL(values[i], skiplist.lookup(values[i])->document());
  }
    
  // clean up
  for (auto i : values) {
    delete i;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test filling in reverse order
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_unique_reverse) {
  arangodb::basics::SkipList<void, void> skiplist(CmpElmElm, CmpKeyElm, FreeElm, true, false);
  
  // check start node
  BOOST_CHECK_EQUAL((void*) 0, skiplist.startNode()->nextNode());
  BOOST_CHECK_EQUAL((void*) 0, skiplist.startNode()->prevNode());

  // check end node
  BOOST_CHECK_EQUAL((void*) 0, skiplist.endNode());
  
  BOOST_CHECK_EQUAL(0, (int) skiplist.getNrUsed());

  std::vector<int*> values; 
  for (int i = 0; i < 100; ++i) {
    values.push_back(new int(i));
  }
  
  // insert 100 values in reverse order
  for (int i = 99; i >= 0; --i) {
    skiplist.insert(values[i]);
  }

  // now check consistency
  BOOST_CHECK_EQUAL(100, (int) skiplist.getNrUsed());

  // check start node
  BOOST_CHECK_EQUAL((void*) 0, skiplist.startNode()->prevNode());
  BOOST_CHECK_EQUAL(values[0], skiplist.startNode()->nextNode()->document());

  // check end node
  BOOST_CHECK_EQUAL((void*) 0, skiplist.endNode());

  arangodb::basics::SkipListNode<void, void>* current;

  // do a forward iteration
  current = skiplist.startNode()->nextNode();
  for (int i = 0; i < 100; ++i) {
    // compare value
    BOOST_CHECK_EQUAL((void*) values[i], current->document());

    // compare prev and next node
    if (i > 0) {
      BOOST_CHECK_EQUAL(values[i - 1], current->prevNode()->document());
    }
    if (i < 99) {
      BOOST_CHECK_EQUAL(values[i + 1], current->nextNode()->document());
    }
    current = current->nextNode();
  }

  // do a backward iteration
  current = skiplist.lookup(values[99]);
  for (int i = 99; i >= 0; --i) {
    // compare value
    BOOST_CHECK_EQUAL(values[i], current->document());

    // compare prev and next node
    if (i > 0) {
      BOOST_CHECK_EQUAL(values[i - 1], current->prevNode()->document());
    }
    if (i < 99) {
      BOOST_CHECK_EQUAL(values[i + 1], current->nextNode()->document());
    }
    current = current->prevNode();
  }

  for (int i = 0; i < 100; ++i) {
    BOOST_CHECK_EQUAL(values[i], skiplist.lookup(values[i])->document());
  }
 
  // clean up
  for (auto i : values) {
    delete i;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test lookup
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_unique_lookup) {
  arangodb::basics::SkipList<void, void> skiplist(CmpElmElm, CmpKeyElm, FreeElm, true, false);
  
  std::vector<int*> values; 
  for (int i = 0; i < 100; ++i) {
    values.push_back(new int(i));
  }
  
  for (int i = 0; i < 100; ++i) {
    skiplist.insert(values[i]);
  }

  // lookup existing values
  BOOST_CHECK_EQUAL(values[0], skiplist.lookup(values[0])->document());
  BOOST_CHECK_EQUAL(values[3], skiplist.lookup(values[3])->document());
  BOOST_CHECK_EQUAL(values[17], skiplist.lookup(values[17])->document());
  BOOST_CHECK_EQUAL(values[99], skiplist.lookup(values[99])->document());
  
  // lookup non-existing values
  int value;

  value = -1;
  BOOST_CHECK_EQUAL((void*) 0, skiplist.lookup(&value));

  value = 100;
  BOOST_CHECK_EQUAL((void*) 0, skiplist.lookup(&value));
  
  value = 101;
  BOOST_CHECK_EQUAL((void*) 0, skiplist.lookup(&value));

  value = 1000;
  BOOST_CHECK_EQUAL((void*) 0, skiplist.lookup(&value));

  // clean up
  for (auto i : values) {
    delete i;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test removal
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_unique_remove) {
  arangodb::basics::SkipList<void, void> skiplist(CmpElmElm, CmpKeyElm, FreeElm, true, false);
  
  std::vector<int*> values; 
  for (int i = 0; i < 100; ++i) {
    values.push_back(new int(i));
  }
  
  for (int i = 0; i < 100; ++i) {
    skiplist.insert(values[i]);
  }

  // remove some values, including start and end nodes
  BOOST_CHECK_EQUAL(0, skiplist.remove(values[7]));
  BOOST_CHECK_EQUAL(0, skiplist.remove(values[12]));
  BOOST_CHECK_EQUAL(0, skiplist.remove(values[23]));
  BOOST_CHECK_EQUAL(0, skiplist.remove(values[99]));
  BOOST_CHECK_EQUAL(0, skiplist.remove(values[98]));
  BOOST_CHECK_EQUAL(0, skiplist.remove(values[0]));
  BOOST_CHECK_EQUAL(0, skiplist.remove(values[1]));

  // remove non-existing and already removed values
  int value;
  
  value = -1;
  BOOST_CHECK_EQUAL(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, skiplist.remove(&value));
  value = 0;
  BOOST_CHECK_EQUAL(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, skiplist.remove(&value));
  value = 12;
  BOOST_CHECK_EQUAL(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, skiplist.remove(&value));
  value = 99;
  BOOST_CHECK_EQUAL(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, skiplist.remove(&value));
  value = 101;
  BOOST_CHECK_EQUAL(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, skiplist.remove(&value));
  value = 1000;
  BOOST_CHECK_EQUAL(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, skiplist.remove(&value));

  // check start node
  BOOST_CHECK_EQUAL(values[2], skiplist.startNode()->nextNode()->document());
  BOOST_CHECK_EQUAL((void*) 0, skiplist.startNode()->prevNode());

  // check end node
  BOOST_CHECK_EQUAL((void*) 0, skiplist.endNode());
  
  BOOST_CHECK_EQUAL(93, (int) skiplist.getNrUsed());


  // lookup existing values
  BOOST_CHECK_EQUAL(values[2], skiplist.lookup(values[2])->document());
  BOOST_CHECK_EQUAL(skiplist.startNode(), skiplist.lookup(values[2])->prevNode());
  BOOST_CHECK_EQUAL(values[3], skiplist.lookup(values[2])->nextNode()->document());

  BOOST_CHECK_EQUAL(values[3], skiplist.lookup(values[3])->document());
  BOOST_CHECK_EQUAL(values[2], skiplist.lookup(values[3])->prevNode()->document());
  BOOST_CHECK_EQUAL(values[4], skiplist.lookup(values[3])->nextNode()->document());

  BOOST_CHECK_EQUAL(values[6], skiplist.lookup(values[6])->document());
  BOOST_CHECK_EQUAL(values[5], skiplist.lookup(values[6])->prevNode()->document());
  BOOST_CHECK_EQUAL(values[8], skiplist.lookup(values[6])->nextNode()->document());

  BOOST_CHECK_EQUAL(values[8], skiplist.lookup(values[8])->document());
  BOOST_CHECK_EQUAL(values[6], skiplist.lookup(values[8])->prevNode()->document());
  BOOST_CHECK_EQUAL(values[9], skiplist.lookup(values[8])->nextNode()->document());

  BOOST_CHECK_EQUAL(values[11], skiplist.lookup(values[11])->document());
  BOOST_CHECK_EQUAL(values[10], skiplist.lookup(values[11])->prevNode()->document());
  BOOST_CHECK_EQUAL(values[13], skiplist.lookup(values[11])->nextNode()->document());

  BOOST_CHECK_EQUAL(values[13], skiplist.lookup(values[13])->document());
  BOOST_CHECK_EQUAL(values[11], skiplist.lookup(values[13])->prevNode()->document());
  BOOST_CHECK_EQUAL(values[14], skiplist.lookup(values[13])->nextNode()->document());

  BOOST_CHECK_EQUAL(values[22], skiplist.lookup(values[22])->document());
  BOOST_CHECK_EQUAL(values[24], skiplist.lookup(values[24])->document());

  BOOST_CHECK_EQUAL(values[97], skiplist.lookup(values[97])->document());
  BOOST_CHECK_EQUAL(values[96], skiplist.lookup(values[97])->prevNode()->document());
  BOOST_CHECK_EQUAL((void*) 0, skiplist.lookup(values[97])->nextNode());
  
  // lookup non-existing values
  value = 0;
  BOOST_CHECK_EQUAL((void*) 0, skiplist.lookup(&value));
  value = 1;
  BOOST_CHECK_EQUAL((void*) 0, skiplist.lookup(&value));
  value = 7;
  BOOST_CHECK_EQUAL((void*) 0, skiplist.lookup(&value));
  value = 12;
  BOOST_CHECK_EQUAL((void*) 0, skiplist.lookup(&value));
  value = 23;
  BOOST_CHECK_EQUAL((void*) 0, skiplist.lookup(&value));
  value = 98;
  BOOST_CHECK_EQUAL((void*) 0, skiplist.lookup(&value));
  value = 99;
  BOOST_CHECK_EQUAL((void*) 0, skiplist.lookup(&value));
  
  // clean up
  for (auto i : values) {
    delete i;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test removal
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_unique_remove_all) {
  arangodb::basics::SkipList<void, void> skiplist(CmpElmElm, CmpKeyElm, FreeElm, true, false);
  
  std::vector<int*> values; 
  for (int i = 0; i < 100; ++i) {
    values.push_back(new int(i));
  }
  
  for (int i = 0; i < 100; ++i) {
    skiplist.insert(values[i]);
  }
  
  for (int i = 0; i < 100; ++i) {
    BOOST_CHECK_EQUAL(0, skiplist.remove(values[i]));
  }
  
  // try removing again
  for (int i = 0; i < 100; ++i) {
    BOOST_CHECK_EQUAL(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, skiplist.remove(values[i]));
  }
  
  // check start node
  BOOST_CHECK_EQUAL((void*) 0, skiplist.startNode()->nextNode());
  BOOST_CHECK_EQUAL((void*) 0, skiplist.startNode()->prevNode());

  // check end node
  BOOST_CHECK_EQUAL((void*) 0, skiplist.endNode());
  
  BOOST_CHECK_EQUAL(0, (int) skiplist.getNrUsed());

  // lookup non-existing values
  BOOST_CHECK_EQUAL((void*) 0, skiplist.lookup(values[0]));
  BOOST_CHECK_EQUAL((void*) 0, skiplist.lookup(values[12]));
  BOOST_CHECK_EQUAL((void*) 0, skiplist.lookup(values[99]));
  
  // clean up
  for (auto i : values) {
    delete i;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE_END ()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
