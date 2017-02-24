////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for arangodb::cache::State
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017 triagens GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
/// @author Copyright 2017, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#define BOOST_TEST_INCLUDED
#include <boost/test/unit_test.hpp>

#include "Cache/State.h"

#include <stdint.h>

using namespace arangodb::cache;

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct CCacheStateSetup {
  CCacheStateSetup() { BOOST_TEST_MESSAGE("setup State"); }

  ~CCacheStateSetup() { BOOST_TEST_MESSAGE("tear-down State"); }
};
// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE(CCacheStateTest, CCacheStateSetup)

////////////////////////////////////////////////////////////////////////////////
/// @brief test lock methods
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(tst_lock) {
  State state;
  bool success;

  uint32_t outsideState = 0;

  auto cb1 = [&outsideState]() -> void { outsideState = 1; };

  auto cb2 = [&outsideState]() -> void { outsideState = 2; };

  // check lock without contention
  BOOST_CHECK(!state.isLocked());
  success = state.lock(-1, cb1);
  BOOST_CHECK(success);
  BOOST_CHECK(state.isLocked());
  BOOST_CHECK_EQUAL(1UL, outsideState);

  // check lock with contention
  success = state.lock(10LL, cb2);
  BOOST_CHECK(!success);
  BOOST_CHECK(state.isLocked());
  BOOST_CHECK_EQUAL(1UL, outsideState);

  // check unlock
  state.unlock();
  BOOST_CHECK(!state.isLocked());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test methods for non-lock flags
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(tst_flags) {
  State state;
  bool success;

  success = state.lock();
  BOOST_CHECK(success);
  BOOST_CHECK(!state.isSet(State::Flag::migrated));
  state.unlock();

  success = state.lock();
  BOOST_CHECK(success);
  BOOST_CHECK(!state.isSet(State::Flag::migrated));
  state.toggleFlag(State::Flag::migrated);
  BOOST_CHECK(state.isSet(State::Flag::migrated));
  state.unlock();

  success = state.lock();
  BOOST_CHECK(success);
  BOOST_CHECK(state.isSet(State::Flag::migrated));
  state.unlock();

  success = state.lock();
  BOOST_CHECK(success);
  BOOST_CHECK(state.isSet(State::Flag::migrated));
  state.toggleFlag(State::Flag::migrated);
  BOOST_CHECK(!state.isSet(State::Flag::migrated));
  state.unlock();

  success = state.lock();
  BOOST_CHECK(success);
  BOOST_CHECK(!state.isSet(State::Flag::migrated));
  state.unlock();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE_END()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|//
// --SECTION--\\|/// @\\}\\)"
// End:
