////////////////////////////////////////////////////////////////////////////////
/// @brief test case for Futures/Try object
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Futures/Future.h"

#include "catch.hpp"

using namespace arangodb::futures;

namespace {
  auto makeValid() {
    auto valid = makeFuture<int>(42);
    REQUIRE(valid.valid());
    return valid;
  }
  auto makeInvalid() {
    auto invalid = Future<int>::makeEmpty();
    REQUIRE(invalid.valid());
    return invalid;
  }
} // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Future Test", "[futures]") {

  
SECTION("Basic") {
  auto f = Future<int>::makeEmpty();
  REQUIRE_THROWS(f.isReady());
}
  
SECTION("Default ctor") {
  Future<void> abc{};
}
  
SECTION("requires only move ctor") {
  struct MoveCtorOnly {
    explicit MoveCtorOnly(int id) : id_(id) {}
    MoveCtorOnly(const MoveCtorOnly&) = delete;
    MoveCtorOnly(MoveCtorOnly&&) = default;
    void operator=(MoveCtorOnly const&) = delete;
    void operator=(MoveCtorOnly&&) = delete;
    int id_;
  };
  
  {
    auto f = makeFuture<MoveCtorOnly>(MoveCtorOnly(42));
    REQUIRE(f.valid());
    REQUIRE(f.isReady());
    auto v = std::move(f).get();
    REQUIRE(v.id_ == 42);
  }
  {
    auto f = makeFuture<MoveCtorOnly>(MoveCtorOnly(42));
    REQUIRE(f.valid());
    REQUIRE(f.isReady());
    auto v = std::move(f).get();
    REQUIRE(v.id_ == 42);
  }
  {
    auto f = makeFuture<MoveCtorOnly>(MoveCtorOnly(42));
    REQUIRE(f.valid());
    REQUIRE(f.isReady());
    auto v = std::move(f).get(std::chrono::milliseconds(10));
    REQUIRE(v.id_ == 42);
  }
  {
    auto f = makeFuture<MoveCtorOnly>(MoveCtorOnly(42));
    REQUIRE(f.valid());
    REQUIRE(f.isReady());
    auto v = std::move(f).get(std::chrono::milliseconds(10));
    REQUIRE(v.id_ == 42);
  }
}
  
SECTION("ctor post condition") {
  auto const except = std::logic_error("foo");
  auto const ewrap = std::make_exception_ptr(std::logic_error("foo"));
  
#define DOIT(CREATION_EXPR)    \
do {                         \
auto f1 = (CREATION_EXPR);   \
REQUIRE(f1.valid());         \
auto f2 = std::move(f1);     \
REQUIRE_FALSE(f1.valid());  \
REQUIRE(f2.valid());        \
} while (false)
  
  //DOIT(makeValid());
  
  auto f1 = (Future<int>(42));
  REQUIRE(f1.valid());
  auto f2 = std::move(f1);
  REQUIRE_FALSE(f1.valid());
  REQUIRE(f2.valid());
  
  /*DOIT(Future<int>(42));
  DOIT(Future<int>{42});
  //DOIT(Future<Unit>());
  //DOIT(Future<Unit>{});
  DOIT(makeFuture());
  DOIT(makeFuture(Unit{}));
  DOIT(makeFuture<Unit>(Unit{}));
  DOIT(makeFuture(42));
  DOIT(makeFuture<int>(42));
  DOIT(makeFuture<int>(except));
  DOIT(makeFuture<int>(ewrap));
  DOIT(makeFuture(Try<int>(42)));
  DOIT(makeFuture<int>(Try<int>(42)));
  DOIT(makeFuture<int>(Try<int>(ewrap)));*/
#undef DOIT
}
  
}
