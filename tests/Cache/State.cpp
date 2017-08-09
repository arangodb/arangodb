////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for arangodb::cache::State
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Cache/State.h"
#include "Basics/Common.h"

#include "catch.hpp"

#include <stdint.h>

using namespace arangodb::cache;

TEST_CASE("cache::State", "[cache]") {
  SECTION("test lock methods") {
    State state;
    bool success;

    uint32_t outsideState = 0;

    auto cb1 = [&outsideState]() -> void { outsideState = 1; };

    auto cb2 = [&outsideState]() -> void { outsideState = 2; };

    // check lock without contention
    REQUIRE(!state.isLocked());
    success = state.lock(false, 10LL, cb1); // writer gets lock
    REQUIRE(success);
    REQUIRE(state.isLocked());
    REQUIRE(state.isWriteLocked());
    REQUIRE(1UL == outsideState);

    // check lock with "contention"
    success = state.lock(false, 10LL, cb2); // another writer can't steal lock
    REQUIRE(!success);
    REQUIRE(state.isLocked());
    REQUIRE(state.isWriteLocked());
    REQUIRE(1UL == outsideState);

    // check lock with "contention"
    success = state.lock(true, 10LL, cb2); // writer blocks reader
    REQUIRE(!success);
    REQUIRE(state.isLocked());
    REQUIRE(state.isWriteLocked());
    REQUIRE(1UL == outsideState);

    // check unlock
    state.unlock(); // writer releases
    REQUIRE(!state.isLocked());
    REQUIRE(!state.isWriteLocked());

    // check read locks
    success = state.lock(true, 10LL, cb1);
    REQUIRE(success);
    REQUIRE(state.isLocked());
    REQUIRE(!state.isWriteLocked());

    success = state.lock(true, 10LL, cb2); // another reader should be fine
    REQUIRE(success);
    REQUIRE(state.isLocked());
    REQUIRE(!state.isWriteLocked());

    success = state.lock(false, 10LL, cb2); // but a writer is not okay
    REQUIRE(!success);
    REQUIRE(state.isLocked());
    REQUIRE(!state.isWriteLocked());

    state.unlock(); // reader1 releases
    REQUIRE(state.isLocked());
    state.unlock(); // reader2 releases
    REQUIRE(!state.isLocked());
  }

  SECTION("test read-lock overflow") {
    State state;
    bool success;

    for (size_t i = 0; i < 127; i++) {
      success = state.lock(true, 10LL);
      REQUIRE(success);
      REQUIRE(state.isLocked());
      REQUIRE(!state.isWriteLocked());
    }

    success = state.lock(true, 10LL);
    REQUIRE(!success);
    REQUIRE(state.isLocked());
    REQUIRE(!state.isWriteLocked());

    for (size_t i = 0; i < 126; i++) {
      state.unlock();
      REQUIRE(state.isLocked());
      REQUIRE(!state.isWriteLocked());
    }

    state.unlock();
    REQUIRE(!state.isLocked());
    REQUIRE(!state.isWriteLocked());
  }

  SECTION("test methods for non-lock flags") {
    State state;
    bool success;

    success = state.lock(true, 10LL);
    REQUIRE(success);
    REQUIRE(!state.isSet(State::Flag::migrated));
    state.unlock();

    success = state.lock(false, 10LL);
    REQUIRE(success);
    REQUIRE(!state.isSet(State::Flag::migrated));
    state.toggleFlag(State::Flag::migrated);
    REQUIRE(state.isSet(State::Flag::migrated));
    state.unlock();

    success = state.lock(true, 10LL);
    REQUIRE(success);
    REQUIRE(state.isSet(State::Flag::migrated));
    state.unlock();

    success = state.lock(false, 10LL);
    REQUIRE(success);
    REQUIRE(state.isSet(State::Flag::migrated));
    state.toggleFlag(State::Flag::migrated);
    REQUIRE(!state.isSet(State::Flag::migrated));
    state.unlock();

    success = state.lock(true, 10LL);
    REQUIRE(success);
    REQUIRE(!state.isSet(State::Flag::migrated));
    state.unlock();
  }
}
