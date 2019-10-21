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

#include "gtest/gtest.h"

#include "Basics/voc-errors.h"
#include "Containers/Skiplist.h"
#include "Random/RandomGenerator.h"

#include <vector>

static bool Initialized = false;

using namespace std;

static int CmpElmElm(void*, void const* left, void const* right,
                     arangodb::containers::SkiplistCmpType cmptype) {
  auto l = *(static_cast<int const*>(left));
  auto r = *(static_cast<int const*>(right));

  if (l != r) {
    return l < r ? -1 : 1;
  }
  return 0;
}

static int CmpKeyElm(void*, void const* left, void const* right) {
  auto l = *(static_cast<int const*>(left));
  auto r = *(static_cast<int const*>(right));

  if (l != r) {
    return l < r ? -1 : 1;
  }
  return 0;
}

static void FreeElm(void* e) {}

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class CSkiplistTest : public ::testing::Test {
 protected:
  CSkiplistTest() {
    if (!Initialized) {
      Initialized = true;
      arangodb::RandomGenerator::initialize(arangodb::RandomGenerator::RandomType::MERSENNE);
    }
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(CSkiplistTest, tst_unique_forward) {
  arangodb::containers::Skiplist<void, void> skiplist(CmpElmElm, CmpKeyElm,
                                                      FreeElm, true, false);

  // check start node
  EXPECT_EQ(nullptr, skiplist.startNode()->nextNode());
  EXPECT_EQ(nullptr, skiplist.startNode()->prevNode());

  // check end node
  EXPECT_EQ(nullptr, skiplist.endNode());

  EXPECT_EQ(0, skiplist.getNrUsed());

  // insert 100 values
  std::vector<int*> values;
  for (int i = 0; i < 100; ++i) {
    values.push_back(new int(i));
  }

  for (int i = 0; i < 100; ++i) {
    skiplist.insert(nullptr, values[i]);
  }

  // now check consistency
  EXPECT_EQ(100, skiplist.getNrUsed());

  // check start node
  EXPECT_EQ(nullptr, skiplist.startNode()->prevNode());
  EXPECT_EQ(values[0], skiplist.startNode()->nextNode()->document());

  // check end node
  EXPECT_EQ(nullptr, skiplist.endNode());

  arangodb::containers::SkiplistNode<void, void>* current;

  // do a forward iteration
  current = skiplist.startNode()->nextNode();
  for (int i = 0; i < 100; ++i) {
    // compare value
    EXPECT_EQ(values[i], current->document());

    // compare prev and next node
    if (i > 0) {
      EXPECT_EQ(values[i - 1], current->prevNode()->document());
    }
    if (i < 99) {
      EXPECT_EQ(values[i + 1], current->nextNode()->document());
    }
    current = current->nextNode();
  }

  // do a backward iteration
  current = skiplist.lookup(nullptr, values[99]);
  for (int i = 99; i >= 0; --i) {
    // compare value
    EXPECT_EQ(values[i], current->document());

    // compare prev and next node
    if (i > 0) {
      EXPECT_EQ(values[i - 1], current->prevNode()->document());
    }
    if (i < 99) {
      EXPECT_EQ(values[i + 1], current->nextNode()->document());
    }
    current = current->prevNode();
  }

  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(values[i], skiplist.lookup(nullptr, values[i])->document());
  }

  // clean up
  for (auto i : values) {
    delete i;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test filling in reverse order
////////////////////////////////////////////////////////////////////////////////

TEST_F(CSkiplistTest, tst_unique_reverse) {
  arangodb::containers::Skiplist<void, void> skiplist(CmpElmElm, CmpKeyElm,
                                                      FreeElm, true, false);

  // check start node
  EXPECT_EQ(nullptr, skiplist.startNode()->nextNode());
  EXPECT_EQ(nullptr, skiplist.startNode()->prevNode());

  // check end node
  EXPECT_EQ(nullptr, skiplist.endNode());

  EXPECT_EQ(0, skiplist.getNrUsed());

  std::vector<int*> values;
  for (int i = 0; i < 100; ++i) {
    values.push_back(new int(i));
  }

  // insert 100 values in reverse order
  for (int i = 99; i >= 0; --i) {
    skiplist.insert(nullptr, values[i]);
  }

  // now check consistency
  EXPECT_EQ(100, skiplist.getNrUsed());

  // check start node
  EXPECT_EQ(nullptr, skiplist.startNode()->prevNode());
  EXPECT_EQ(values[0], skiplist.startNode()->nextNode()->document());

  // check end node
  EXPECT_EQ(nullptr, skiplist.endNode());

  arangodb::containers::SkiplistNode<void, void>* current;

  // do a forward iteration
  current = skiplist.startNode()->nextNode();
  for (int i = 0; i < 100; ++i) {
    // compare value
    EXPECT_EQ(values[i], current->document());

    // compare prev and next node
    if (i > 0) {
      EXPECT_EQ(values[i - 1], current->prevNode()->document());
    }
    if (i < 99) {
      EXPECT_EQ(values[i + 1], current->nextNode()->document());
    }
    current = current->nextNode();
  }

  // do a backward iteration
  current = skiplist.lookup(nullptr, values[99]);
  for (int i = 99; i >= 0; --i) {
    // compare value
    EXPECT_EQ(values[i], current->document());

    // compare prev and next node
    if (i > 0) {
      EXPECT_EQ(values[i - 1], current->prevNode()->document());
    }
    if (i < 99) {
      EXPECT_EQ(values[i + 1], current->nextNode()->document());
    }
    current = current->prevNode();
  }

  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(values[i], skiplist.lookup(nullptr, values[i])->document());
  }

  // clean up
  for (auto i : values) {
    delete i;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test lookup
////////////////////////////////////////////////////////////////////////////////

TEST_F(CSkiplistTest, tst_unique_lookup) {
  arangodb::containers::Skiplist<void, void> skiplist(CmpElmElm, CmpKeyElm,
                                                      FreeElm, true, false);

  std::vector<int*> values;
  for (int i = 0; i < 100; ++i) {
    values.push_back(new int(i));
  }

  for (int i = 0; i < 100; ++i) {
    skiplist.insert(nullptr, values[i]);
  }

  // lookup existing values
  EXPECT_EQ(values[0], skiplist.lookup(nullptr, values[0])->document());
  EXPECT_EQ(values[3], skiplist.lookup(nullptr, values[3])->document());
  EXPECT_EQ(values[17], skiplist.lookup(nullptr, values[17])->document());
  EXPECT_EQ(values[99], skiplist.lookup(nullptr, values[99])->document());

  // lookup non-existing values
  int value;

  value = -1;
  EXPECT_EQ(nullptr, skiplist.lookup(nullptr, &value));

  value = 100;
  EXPECT_EQ(nullptr, skiplist.lookup(nullptr, &value));

  value = 101;
  EXPECT_EQ(nullptr, skiplist.lookup(nullptr, &value));

  value = 1000;
  EXPECT_EQ(nullptr, skiplist.lookup(nullptr, &value));

  // clean up
  for (auto i : values) {
    delete i;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test removal
////////////////////////////////////////////////////////////////////////////////

TEST_F(CSkiplistTest, tst_unique_remove) {
  arangodb::containers::Skiplist<void, void> skiplist(CmpElmElm, CmpKeyElm,
                                                      FreeElm, true, false);

  std::vector<int*> values;
  for (int i = 0; i < 100; ++i) {
    values.push_back(new int(i));
  }

  for (int i = 0; i < 100; ++i) {
    skiplist.insert(nullptr, values[i]);
  }

  // remove some values, including start and end nodes
  EXPECT_EQ(0, skiplist.remove(nullptr, values[7]));
  EXPECT_EQ(0, skiplist.remove(nullptr, values[12]));
  EXPECT_EQ(0, skiplist.remove(nullptr, values[23]));
  EXPECT_EQ(0, skiplist.remove(nullptr, values[99]));
  EXPECT_EQ(0, skiplist.remove(nullptr, values[98]));
  EXPECT_EQ(0, skiplist.remove(nullptr, values[0]));
  EXPECT_EQ(0, skiplist.remove(nullptr, values[1]));

  // remove non-existing and already removed values
  int value;

  value = -1;
  EXPECT_EQ(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, skiplist.remove(nullptr, &value));
  value = 0;
  EXPECT_EQ(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, skiplist.remove(nullptr, &value));
  value = 12;
  EXPECT_EQ(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, skiplist.remove(nullptr, &value));
  value = 99;
  EXPECT_EQ(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, skiplist.remove(nullptr, &value));
  value = 101;
  EXPECT_EQ(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, skiplist.remove(nullptr, &value));
  value = 1000;
  EXPECT_EQ(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, skiplist.remove(nullptr, &value));

  // check start node
  EXPECT_EQ(values[2], skiplist.startNode()->nextNode()->document());
  EXPECT_EQ(nullptr, skiplist.startNode()->prevNode());

  // check end node
  EXPECT_EQ(nullptr, skiplist.endNode());

  EXPECT_EQ(93, skiplist.getNrUsed());

  // lookup existing values
  EXPECT_EQ(values[2], skiplist.lookup(nullptr, values[2])->document());
  EXPECT_EQ(skiplist.startNode(), skiplist.lookup(nullptr, values[2])->prevNode());
  EXPECT_EQ(values[3], skiplist.lookup(nullptr, values[2])->nextNode()->document());

  EXPECT_EQ(values[3], skiplist.lookup(nullptr, values[3])->document());
  EXPECT_EQ(values[2], skiplist.lookup(nullptr, values[3])->prevNode()->document());
  EXPECT_EQ(values[4], skiplist.lookup(nullptr, values[3])->nextNode()->document());

  EXPECT_EQ(values[6], skiplist.lookup(nullptr, values[6])->document());
  EXPECT_EQ(values[5], skiplist.lookup(nullptr, values[6])->prevNode()->document());
  EXPECT_EQ(values[8], skiplist.lookup(nullptr, values[6])->nextNode()->document());

  EXPECT_EQ(values[8], skiplist.lookup(nullptr, values[8])->document());
  EXPECT_EQ(values[6], skiplist.lookup(nullptr, values[8])->prevNode()->document());
  EXPECT_EQ(values[9], skiplist.lookup(nullptr, values[8])->nextNode()->document());

  EXPECT_EQ(values[11], skiplist.lookup(nullptr, values[11])->document());
  EXPECT_EQ(values[10], skiplist.lookup(nullptr, values[11])->prevNode()->document());
  EXPECT_EQ(values[13], skiplist.lookup(nullptr, values[11])->nextNode()->document());

  EXPECT_EQ(values[13], skiplist.lookup(nullptr, values[13])->document());
  EXPECT_EQ(values[11], skiplist.lookup(nullptr, values[13])->prevNode()->document());
  EXPECT_EQ(values[14], skiplist.lookup(nullptr, values[13])->nextNode()->document());

  EXPECT_EQ(values[22], skiplist.lookup(nullptr, values[22])->document());
  EXPECT_EQ(values[24], skiplist.lookup(nullptr, values[24])->document());

  EXPECT_EQ(values[97], skiplist.lookup(nullptr, values[97])->document());
  EXPECT_EQ(values[96], skiplist.lookup(nullptr, values[97])->prevNode()->document());
  EXPECT_EQ(nullptr, skiplist.lookup(nullptr, values[97])->nextNode());

  // lookup non-existing values
  value = 0;
  EXPECT_EQ(nullptr, skiplist.lookup(nullptr, &value));
  value = 1;
  EXPECT_EQ(nullptr, skiplist.lookup(nullptr, &value));
  value = 7;
  EXPECT_EQ(nullptr, skiplist.lookup(nullptr, &value));
  value = 12;
  EXPECT_EQ(nullptr, skiplist.lookup(nullptr, &value));
  value = 23;
  EXPECT_EQ(nullptr, skiplist.lookup(nullptr, &value));
  value = 98;
  EXPECT_EQ(nullptr, skiplist.lookup(nullptr, &value));
  value = 99;
  EXPECT_EQ(nullptr, skiplist.lookup(nullptr, &value));

  // clean up
  for (auto i : values) {
    delete i;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test removal
////////////////////////////////////////////////////////////////////////////////

TEST_F(CSkiplistTest, tst_unique_remove_all) {
  arangodb::containers::Skiplist<void, void> skiplist(CmpElmElm, CmpKeyElm,
                                                      FreeElm, true, false);

  std::vector<int*> values;
  for (int i = 0; i < 100; ++i) {
    values.push_back(new int(i));
  }

  for (int i = 0; i < 100; ++i) {
    skiplist.insert(nullptr, values[i]);
  }

  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(0, skiplist.remove(nullptr, values[i]));
  }

  // try removing again
  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, skiplist.remove(nullptr, values[i]));
  }

  // check start node
  EXPECT_EQ(nullptr, skiplist.startNode()->nextNode());
  EXPECT_EQ(nullptr, skiplist.startNode()->prevNode());

  // check end node
  EXPECT_EQ(nullptr, skiplist.endNode());

  EXPECT_EQ(0, skiplist.getNrUsed());

  // lookup non-existing values
  EXPECT_EQ(nullptr, skiplist.lookup(nullptr, values[0]));
  EXPECT_EQ(nullptr, skiplist.lookup(nullptr, values[12]));
  EXPECT_EQ(nullptr, skiplist.lookup(nullptr, values[99]));

  // clean up
  for (auto i : values) {
    delete i;
  }
}
