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

#include "catch.hpp"

#include "Cache/State.h"

#include <stdint.h>

using namespace arangodb::cache;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("CCacheStateTest", "[cache]") {

////////////////////////////////////////////////////////////////////////////////
/// @brief test lock methods
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_lock") {
  State state;
  bool success;

  uint32_t outsideState = 0;

  auto cb1 = [&outsideState]() -> void { outsideState = 1; };

  auto cb2 = [&outsideState]() -> void { outsideState = 2; };

  // check lock without contention
  CHECK(!state.isLocked());
  success = state.lock(-1, cb1);
  CHECK(success);
  CHECK(state.isLocked());
  CHECK(1UL == outsideState);

  // check lock with contention
  success = state.lock(10LL, cb2);
  CHECK(!success);
  CHECK(state.isLocked());
  CHECK(1UL == outsideState);

  // check unlock
  state.unlock();
  CHECK(!state.isLocked());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test methods for non-lock flags
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_flags") {
  State state;
  bool success;

  success = state.lock();
  CHECK(success);
  CHECK(!state.isSet(State::Flag::migrated));
  state.unlock();

  success = state.lock();
  CHECK(success);
  CHECK(!state.isSet(State::Flag::migrated));
  state.toggleFlag(State::Flag::migrated);
  CHECK(state.isSet(State::Flag::migrated));
  state.unlock();

  success = state.lock();
  CHECK(success);
  CHECK(state.isSet(State::Flag::migrated));
  state.unlock();

  success = state.lock();
  CHECK(success);
  CHECK(state.isSet(State::Flag::migrated));
  state.toggleFlag(State::Flag::migrated);
  CHECK(!state.isSet(State::Flag::migrated));
  state.unlock();

  success = state.lock();
  CHECK(success);
  CHECK(!state.isSet(State::Flag::migrated));
  state.unlock();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|//
// --SECTION--\\|/// @\\}\\)"
// End:
