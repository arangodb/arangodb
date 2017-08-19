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

    // check lock without contention
    REQUIRE(!state.isLocked());
    success = state.writeLock(10LL); // writer gets lock
    REQUIRE(success);
    REQUIRE(state.isLocked());
    REQUIRE(state.isWriteLocked());

    // check lock with "contention"
    success = state.writeLock(10LL); // another writer can't steal lock
    REQUIRE(!success);
    REQUIRE(state.isLocked());
    REQUIRE(state.isWriteLocked());

    // check lock with "contention"
    success = state.readLock(10LL); // writer blocks reader
    REQUIRE(!success);
    REQUIRE(state.isLocked());
    REQUIRE(state.isWriteLocked());

    // check unlock
    state.writeUnlock(); // writer releases
    REQUIRE(!state.isLocked());
    REQUIRE(!state.isWriteLocked());

    // check read locks
    success = state.readLock(10LL);
    REQUIRE(success);
    REQUIRE(state.isLocked());
    REQUIRE(!state.isWriteLocked());

    success = state.readLock(10LL); // another reader should be fine
    REQUIRE(success);
    REQUIRE(state.isLocked());
    REQUIRE(!state.isWriteLocked());

    success = state.writeLock(10LL); // but a writer is not okay
    REQUIRE(!success);
    REQUIRE(state.isLocked());
    REQUIRE(!state.isWriteLocked());

    state.readUnlock(); // reader1 releases
    REQUIRE(state.isLocked());
    state.readUnlock(); // reader2 releases
    REQUIRE(!state.isLocked());
  }

  SECTION("test methods for non-lock flags") {
    State state;
    bool success;

    success = state.readLock(10LL);
    REQUIRE(success);
    REQUIRE(!state.isSet(State::Flag::migrated));
    state.readUnlock();

    success = state.writeLock(10LL);
    REQUIRE(success);
    REQUIRE(!state.isSet(State::Flag::migrated));
    state.toggleFlag(State::Flag::migrated);
    REQUIRE(state.isSet(State::Flag::migrated));
    state.writeUnlock();

    success = state.readLock(10LL);
    REQUIRE(success);
    REQUIRE(state.isSet(State::Flag::migrated));
    state.readUnlock();

    success = state.writeLock(10LL);
    REQUIRE(success);
    REQUIRE(state.isSet(State::Flag::migrated));
    state.toggleFlag(State::Flag::migrated);
    REQUIRE(!state.isSet(State::Flag::migrated));
    state.writeUnlock();

    success = state.readLock(10LL);
    REQUIRE(success);
    REQUIRE(!state.isSet(State::Flag::migrated));
    state.readUnlock();
  }
}
