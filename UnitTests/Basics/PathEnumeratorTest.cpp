////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for PathEnumerator class
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#define BOOST_TEST_INCLUDED
#include <boost/test/unit_test.hpp>

#include "Basics/Traverser.h"

using namespace arangodb;
using namespace arangodb::basics;

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct PathEnumeratorSetup {
  PathEnumeratorSetup () {
    BOOST_TEST_MESSAGE("setup PathEnumerator");
  }

  ~PathEnumeratorSetup () {
    BOOST_TEST_MESSAGE("tear-down PathEnumerator");
  }
};

static int pointerFake = 0;
static int internalCounter = 0;
static int first = 0;
static int second = 0;
static int third = 0;
auto integerEdgeEnumerator = [] (int const& start, std::vector<int>& result, int*& next, size_t&, bool&) {
  if (result.size() >= 3) {
    next = nullptr;
  } else if (next == nullptr) {
    switch (result.size())
    {
      case 0:
        first = 1;
        result.push_back(first);
        break;
      case 1:
        second = 1;
        result.push_back(second);
        break;
      case 2:
        third = 1;
        result.push_back(third);
        break;
    }
    next = &pointerFake;
  } else {
    if (internalCounter == 3) {
      // Reseted the second layer 3 times
      next = nullptr;
    } else if (pointerFake == 3) {
      internalCounter++;
      pointerFake = 0;
      second = 0;
      next = nullptr;
    } else if (third >= 3) {
      pointerFake++;
      third = 0;
      next = nullptr;
    } else {
      switch (result.size())
      {
        case 0:
          first++;
          result.push_back(first);
          break;
        case 1:
          second++;
          result.push_back(second);
          break;
        case 2:
          third++;
          result.push_back(third);
          break;
      }
      next = &pointerFake;
    }
  }
};

auto integerVertexEnumerator = [] (int const& edge, int const& vertex, size_t depth, int& result) -> bool {
  result = 1;
  return true;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE (PathEnumeratorTest, PathEnumeratorSetup)

////////////////////////////////////////////////////////////////////////////////
/// @brief test_fullPathEnumeration
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (test_fullPathEnumerator) {
  int startVertex = 1;
  DepthFirstEnumerator<int, int, int> it(integerEdgeEnumerator, integerVertexEnumerator, startVertex);
  EnumeratedPath<int, int> path;
  for (int k = 1; k < 4; k++) {
    path = it.next();
    BOOST_CHECK_EQUAL(path.edges.size(), (size_t)1);
    BOOST_CHECK_EQUAL(path.vertices.size(), (size_t)2);
    BOOST_CHECK_EQUAL(path.edges[0], k);
    for (int i = 1; i < 4; i++) {
      path = it.next();
      BOOST_CHECK_EQUAL(path.edges.size(), (size_t)2);
      BOOST_CHECK_EQUAL(path.vertices.size(), (size_t)3);
      BOOST_CHECK_EQUAL(path.edges[0], k);
      BOOST_CHECK_EQUAL(path.edges[1], i);
      for (int j = 1; j < 4; j++) {
        path = it.next();
        BOOST_CHECK_EQUAL(path.edges.size(), (size_t)3);
        BOOST_CHECK_EQUAL(path.vertices.size(), (size_t)4);
        BOOST_CHECK_EQUAL(path.edges[0], k);
        BOOST_CHECK_EQUAL(path.edges[1], i);
        BOOST_CHECK_EQUAL(path.edges[2], j);
      }
    }
  }
  path = it.next();
  BOOST_CHECK_EQUAL(path.edges.size(), (size_t)0); 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE_END()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
