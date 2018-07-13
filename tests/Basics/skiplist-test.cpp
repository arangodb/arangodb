////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for Skiplist
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

#include "catch.hpp"

#include "MMFiles/MMFilesSkiplist.h"
#include "Basics/voc-errors.h"
#include "Random/RandomGenerator.h"

#include <vector>

static bool Initialized = false;

using namespace std;

static int CmpElmElm (void*, void const* left,
                      void const* right,
                      arangodb::MMFilesSkiplistCmpType cmptype) {
  auto l = *(static_cast<int const*>(left));
  auto r = *(static_cast<int const*>(right));

  if (l != r) {
    return l < r ? -1 : 1;
  }
  return 0;
}

static int CmpKeyElm (void*, void const* left,
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

struct CSkiplistSetup {
  CSkiplistSetup () {
    if (!Initialized) {
      Initialized = true;
      arangodb::RandomGenerator::initialize(arangodb::RandomGenerator::RandomType::MERSENNE);
    }

  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("CSkiplistTest", "[cskiplist]") {
  CSkiplistSetup s;
////////////////////////////////////////////////////////////////////////////////
/// @brief test filling in forward order
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_unique_forward") {
  arangodb::MMFilesSkiplist<void, void> skiplist(CmpElmElm, CmpKeyElm, FreeElm, true, false);
  
  // check start node
  CHECK((void*) nullptr == skiplist.startNode()->nextNode());
  CHECK((void*) nullptr == skiplist.startNode()->prevNode());

  // check end node
  CHECK((void*) nullptr == skiplist.endNode());
  
  CHECK(0 == (int) skiplist.getNrUsed());

  // insert 100 values
  std::vector<int*> values; 
  for (int i = 0; i < 100; ++i) {
    values.push_back(new int(i));
  }
  
  for (int i = 0; i < 100; ++i) {
    skiplist.insert(nullptr, values[i]);
  }

  // now check consistency
  CHECK(100 == (int) skiplist.getNrUsed());

  // check start node
  CHECK((void*) nullptr == skiplist.startNode()->prevNode());
  CHECK(values[0] == skiplist.startNode()->nextNode()->document());

  // check end node
  CHECK((void*) nullptr == skiplist.endNode());

  arangodb::MMFilesSkiplistNode<void, void>* current;

  // do a forward iteration
  current = skiplist.startNode()->nextNode();
  for (int i = 0; i < 100; ++i) {
    // compare value
    CHECK((void*) values[i] == current->document());

    // compare prev and next node
    if (i > 0) {
      CHECK(values[i - 1] == current->prevNode()->document());
    }
    if (i < 99) {
      CHECK(values[i + 1] == current->nextNode()->document());
    }
    current = current->nextNode();
  }
  
  // do a backward iteration
  current = skiplist.lookup(nullptr, values[99]);
  for (int i = 99; i >= 0; --i) {
    // compare value
    CHECK(values[i] == current->document());

    // compare prev and next node
    if (i > 0) {
      CHECK(values[i - 1] == current->prevNode()->document());
    }
    if (i < 99) {
      CHECK(values[i + 1] == current->nextNode()->document());
    }
    current = current->prevNode();
  }

  for (int i = 0; i < 100; ++i) {
    CHECK(values[i] == skiplist.lookup(nullptr, values[i])->document());
  }
    
  // clean up
  for (auto i : values) {
    delete i;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test filling in reverse order
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_unique_reverse") {
  arangodb::MMFilesSkiplist<void, void> skiplist(CmpElmElm, CmpKeyElm, FreeElm, true, false);
  
  // check start node
  CHECK((void*) nullptr == skiplist.startNode()->nextNode());
  CHECK((void*) nullptr == skiplist.startNode()->prevNode());

  // check end node
  CHECK((void*) nullptr == skiplist.endNode());
  
  CHECK(0 == (int) skiplist.getNrUsed());

  std::vector<int*> values; 
  for (int i = 0; i < 100; ++i) {
    values.push_back(new int(i));
  }
  
  // insert 100 values in reverse order
  for (int i = 99; i >= 0; --i) {
    skiplist.insert(nullptr, values[i]);
  }

  // now check consistency
  CHECK(100 == (int) skiplist.getNrUsed());

  // check start node
  CHECK((void*) nullptr == skiplist.startNode()->prevNode());
  CHECK(values[0] == skiplist.startNode()->nextNode()->document());

  // check end node
  CHECK((void*) nullptr == skiplist.endNode());

  arangodb::MMFilesSkiplistNode<void, void>* current;

  // do a forward iteration
  current = skiplist.startNode()->nextNode();
  for (int i = 0; i < 100; ++i) {
    // compare value
    CHECK((void*) values[i] == current->document());

    // compare prev and next node
    if (i > 0) {
      CHECK(values[i - 1] == current->prevNode()->document());
    }
    if (i < 99) {
      CHECK(values[i + 1] == current->nextNode()->document());
    }
    current = current->nextNode();
  }

  // do a backward iteration
  current = skiplist.lookup(nullptr, values[99]);
  for (int i = 99; i >= 0; --i) {
    // compare value
    CHECK(values[i] == current->document());

    // compare prev and next node
    if (i > 0) {
      CHECK(values[i - 1] == current->prevNode()->document());
    }
    if (i < 99) {
      CHECK(values[i + 1] == current->nextNode()->document());
    }
    current = current->prevNode();
  }

  for (int i = 0; i < 100; ++i) {
    CHECK(values[i] == skiplist.lookup(nullptr, values[i])->document());
  }
 
  // clean up
  for (auto i : values) {
    delete i;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test lookup
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_unique_lookup") {
  arangodb::MMFilesSkiplist<void, void> skiplist(CmpElmElm, CmpKeyElm, FreeElm, true, false);
  
  std::vector<int*> values; 
  for (int i = 0; i < 100; ++i) {
    values.push_back(new int(i));
  }
  
  for (int i = 0; i < 100; ++i) {
    skiplist.insert(nullptr, values[i]);
  }

  // lookup existing values
  CHECK(values[0] == skiplist.lookup(nullptr, values[0])->document());
  CHECK(values[3] == skiplist.lookup(nullptr, values[3])->document());
  CHECK(values[17] == skiplist.lookup(nullptr, values[17])->document());
  CHECK(values[99] == skiplist.lookup(nullptr, values[99])->document());
  
  // lookup non-existing values
  int value;

  value = -1;
  CHECK((void*) nullptr == skiplist.lookup(nullptr, &value));

  value = 100;
  CHECK((void*) nullptr == skiplist.lookup(nullptr, &value));
  
  value = 101;
  CHECK((void*) nullptr == skiplist.lookup(nullptr, &value));

  value = 1000;
  CHECK((void*) nullptr == skiplist.lookup(nullptr, &value));

  // clean up
  for (auto i : values) {
    delete i;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test removal
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_unique_remove") {
  arangodb::MMFilesSkiplist<void, void> skiplist(CmpElmElm, CmpKeyElm, FreeElm, true, false);
  
  std::vector<int*> values; 
  for (int i = 0; i < 100; ++i) {
    values.push_back(new int(i));
  }
  
  for (int i = 0; i < 100; ++i) {
    skiplist.insert(nullptr, values[i]);
  }

  // remove some values, including start and end nodes
  CHECK(0 == skiplist.remove(nullptr, values[7]));
  CHECK(0 == skiplist.remove(nullptr, values[12]));
  CHECK(0 == skiplist.remove(nullptr, values[23]));
  CHECK(0 == skiplist.remove(nullptr, values[99]));
  CHECK(0 == skiplist.remove(nullptr, values[98]));
  CHECK(0 == skiplist.remove(nullptr, values[0]));
  CHECK(0 == skiplist.remove(nullptr, values[1]));

  // remove non-existing and already removed values
  int value;
  
  value = -1;
  CHECK(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND == skiplist.remove(nullptr, &value));
  value = 0;
  CHECK(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND == skiplist.remove(nullptr, &value));
  value = 12;
  CHECK(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND == skiplist.remove(nullptr, &value));
  value = 99;
  CHECK(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND == skiplist.remove(nullptr, &value));
  value = 101;
  CHECK(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND == skiplist.remove(nullptr, &value));
  value = 1000;
  CHECK(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND == skiplist.remove(nullptr, &value));

  // check start node
  CHECK(values[2] == skiplist.startNode()->nextNode()->document());
  CHECK((void*) nullptr == skiplist.startNode()->prevNode());

  // check end node
  CHECK((void*) nullptr == skiplist.endNode());
  
  CHECK(93 == (int) skiplist.getNrUsed());


  // lookup existing values
  CHECK(values[2] == skiplist.lookup(nullptr, values[2])->document());
  CHECK(skiplist.startNode() == skiplist.lookup(nullptr, values[2])->prevNode());
  CHECK(values[3] == skiplist.lookup(nullptr, values[2])->nextNode()->document());

  CHECK(values[3] == skiplist.lookup(nullptr, values[3])->document());
  CHECK(values[2] == skiplist.lookup(nullptr, values[3])->prevNode()->document());
  CHECK(values[4] == skiplist.lookup(nullptr, values[3])->nextNode()->document());

  CHECK(values[6] == skiplist.lookup(nullptr, values[6])->document());
  CHECK(values[5] == skiplist.lookup(nullptr, values[6])->prevNode()->document());
  CHECK(values[8] == skiplist.lookup(nullptr, values[6])->nextNode()->document());

  CHECK(values[8] == skiplist.lookup(nullptr, values[8])->document());
  CHECK(values[6] == skiplist.lookup(nullptr, values[8])->prevNode()->document());
  CHECK(values[9] == skiplist.lookup(nullptr, values[8])->nextNode()->document());

  CHECK(values[11] == skiplist.lookup(nullptr, values[11])->document());
  CHECK(values[10] == skiplist.lookup(nullptr, values[11])->prevNode()->document());
  CHECK(values[13] == skiplist.lookup(nullptr, values[11])->nextNode()->document());

  CHECK(values[13] == skiplist.lookup(nullptr, values[13])->document());
  CHECK(values[11] == skiplist.lookup(nullptr, values[13])->prevNode()->document());
  CHECK(values[14] == skiplist.lookup(nullptr, values[13])->nextNode()->document());

  CHECK(values[22] == skiplist.lookup(nullptr, values[22])->document());
  CHECK(values[24] == skiplist.lookup(nullptr, values[24])->document());

  CHECK(values[97] == skiplist.lookup(nullptr, values[97])->document());
  CHECK(values[96] == skiplist.lookup(nullptr, values[97])->prevNode()->document());
  CHECK((void*) nullptr == skiplist.lookup(nullptr, values[97])->nextNode());
  
  // lookup non-existing values
  value = 0;
  CHECK((void*) nullptr == skiplist.lookup(nullptr, &value));
  value = 1;
  CHECK((void*) nullptr == skiplist.lookup(nullptr, &value));
  value = 7;
  CHECK((void*) nullptr == skiplist.lookup(nullptr, &value));
  value = 12;
  CHECK((void*) nullptr == skiplist.lookup(nullptr, &value));
  value = 23;
  CHECK((void*) nullptr == skiplist.lookup(nullptr, &value));
  value = 98;
  CHECK((void*) nullptr == skiplist.lookup(nullptr, &value));
  value = 99;
  CHECK((void*) nullptr == skiplist.lookup(nullptr, &value));
  
  // clean up
  for (auto i : values) {
    delete i;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test removal
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_unique_remove_all") {
  arangodb::MMFilesSkiplist<void, void> skiplist(CmpElmElm, CmpKeyElm, FreeElm, true, false);
  
  std::vector<int*> values; 
  for (int i = 0; i < 100; ++i) {
    values.push_back(new int(i));
  }
  
  for (int i = 0; i < 100; ++i) {
    skiplist.insert(nullptr, values[i]);
  }
  
  for (int i = 0; i < 100; ++i) {
    CHECK(0 == skiplist.remove(nullptr, values[i]));
  }
  
  // try removing again
  for (int i = 0; i < 100; ++i) {
    CHECK(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND == skiplist.remove(nullptr, values[i]));
  }
  
  // check start node
  CHECK((void*) nullptr == skiplist.startNode()->nextNode());
  CHECK((void*) nullptr == skiplist.startNode()->prevNode());

  // check end node
  CHECK((void*) nullptr == skiplist.endNode());
  
  CHECK(0 == (int) skiplist.getNrUsed());

  // lookup non-existing values
  CHECK((void*) nullptr == skiplist.lookup(nullptr, values[0]));
  CHECK((void*) nullptr == skiplist.lookup(nullptr, values[12]));
  CHECK((void*) nullptr == skiplist.lookup(nullptr, values[99]));
  
  // clean up
  for (auto i : values) {
    delete i;
  }
}
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
