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

#include "Futures/Promise.h"

#include "catch.hpp"

using namespace arangodb::futures;

namespace {
  auto makeValid() {
    auto valid = Promise<int>();
    REQUIRE(valid.valid());
    return valid;
  }
  auto makeInvalid() {
    auto invalid = Promise<int>::makeEmpty();
    REQUIRE_FALSE(invalid.valid());
    return invalid;
  }
  template <typename T>
  constexpr typename std::decay<T>::type copy(T&& value) noexcept(noexcept(typename std::decay<T>::type(std::forward<T>(value)))) {
    return std::forward<T>(value);
  }
  
  typedef std::domain_error eggs_t;
  static eggs_t eggs("eggs");  
} // namespace


// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Promise", "[futures][Promise]") {
  
SECTION("makeEmpty") {
  auto p = Promise<int>::makeEmpty();
  REQUIRE(p.isFulfilled());
}

SECTION("special") {
  REQUIRE_FALSE(std::is_copy_constructible<Promise<int>>::value);
  REQUIRE_FALSE(std::is_copy_assignable<Promise<int>>::value);
  REQUIRE(std::is_move_constructible<Promise<int>>::value);
  REQUIRE(std::is_move_assignable<Promise<int>>::value);
}
  
SECTION("getFuture") {
  Promise<int> p;
  Future<int> f = p.getFuture();
  REQUIRE_FALSE(f.isReady());
}
  
SECTION("setValueUnit") {
  Promise<Unit> p;
  p.setValue();
}
  
SECTION("ctorPostconditionValid") {
  // Ctors/factories that promise valid -- postcondition: valid()
  
#define DOIT(CREATION_EXPR)    \
do {                         \
auto p1 = (CREATION_EXPR); \
REQUIRE(p1.valid());   \
auto p2 = std::move(p1);   \
REQUIRE_FALSE(p1.valid());  \
REQUIRE(p2.valid());   \
} while (false)
  
  DOIT(makeValid());
  DOIT(Promise<int>());
  DOIT(Promise<int>{});
  DOIT(Promise<Unit>());
  DOIT(Promise<Unit>{});
  
#undef DOIT
}
  
SECTION("ctorPostconditionInvalid") {
    // Ctors/factories that promise invalid -- postcondition: !valid()
    
#define DOIT(CREATION_EXPR)    \
do {                         \
auto p1 = (CREATION_EXPR); \
REQUIRE_FALSE(p1.valid());  \
auto p2 = std::move(p1);   \
REQUIRE_FALSE(p1.valid());  \
REQUIRE_FALSE(p2.valid());  \
} while (false)
    
    DOIT(makeInvalid());
    DOIT(Promise<int>::makeEmpty());
    
#undef DOIT
  }
  
SECTION("lacksPreconditionValid") {
    // Ops that don't throw PromiseInvalid if !valid() --
    // without precondition: valid()
    
#define DOIT(STMT)         \
do {                     \
auto p = makeValid();  \
{ STMT; }              \
copy(std::move(p));    \
STMT; \
} while (false)
    
    // misc methods that don't require isValid()
    DOIT(p.valid());
    DOIT(p.isFulfilled());
    
    // move-ctor - move-copy to local, copy(), pass-by-move-value
    DOIT(auto other = std::move(p));
    DOIT(copy(std::move(p)));
    DOIT(([](auto) {})(std::move(p)));
    
    // move-assignment into either {valid | invalid}
    DOIT({
      auto other = makeValid();
      other = std::move(p);
    });
    DOIT({
      auto other = makeInvalid();
      other = std::move(p);
    });
    
#undef DOIT
  }
  
SECTION("hasPreconditionValid") {
    // Ops that require validity; precondition: valid();
    // throw PromiseInvalid if !valid()
    
#define DOIT(STMT)                      \
do {                                  \
auto p = makeValid();               \
STMT;              \
::copy(std::move(p));                 \
REQUIRE_THROWS(STMT); \
} while (false)
    
    auto const except = std::logic_error("foo");
    auto const ewrap = std::make_exception_ptr(except);
    
    DOIT(p.getFuture());
    DOIT(p.setException(except));
    DOIT(p.setException(ewrap));
    //DOIT(p.setInterruptHandler([](auto&) {}));
    DOIT(p.setValue(42));
    DOIT(p.setTry(Try<int>(42)));
    DOIT(p.setTry(Try<int>(ewrap)));
    DOIT(p.setWith([] { return 42; }));
    
#undef DOIT
  }
  
SECTION("hasPostconditionValid") {
    // Ops that preserve validity -- postcondition: valid()
    
#define DOIT(STMT)          \
do {                      \
auto p = makeValid();   \
STMT;  \
REQUIRE(p.valid()); \
} while (false)
    
    auto const swallow = [](auto) {};
    
    DOIT(swallow(p.valid())); // p.valid() itself preserves validity
    DOIT(swallow(p.isFulfilled()));
    
#undef DOIT
  }
  
 SECTION("hasPostconditionInvalid") {
    // Ops that consume *this -- postcondition: !valid()
    
#define DOIT(CTOR, STMT)     \
do {                       \
auto p = (CTOR);         \
STMT;   \
REQUIRE_FALSE(p.valid()); \
} while (false)
    
    // move-ctor of {valid|invalid}
    DOIT(makeValid(), { auto other{std::move(p)}; });
    DOIT(makeInvalid(), { auto other{std::move(p)}; });
    
    // move-assignment of {valid|invalid} into {valid|invalid}
    DOIT(makeValid(), {
      auto other = makeValid();
      other = std::move(p);
    });
    DOIT(makeValid(), {
      auto other = makeInvalid();
      other = std::move(p);
    });
    DOIT(makeInvalid(), {
      auto other = makeValid();
      other = std::move(p);
    });
    DOIT(makeInvalid(), {
      auto other = makeInvalid();
      other = std::move(p);
    });
    
    // pass-by-value of {valid|invalid}
    DOIT(makeValid(), {
      auto const byval = [](auto) {};
      byval(std::move(p));
    });
    DOIT(makeInvalid(), {
      auto const byval = [](auto) {};
      byval(std::move(p));
    });
    
#undef DOIT
  }
  
SECTION("setValue") {
    Promise<int> fund;
    auto ffund = fund.getFuture();
    fund.setValue(42);
    REQUIRE(42 == ffund.get());
    
    struct Foo {
      std::string name;
      int value;
    };
    
    Promise<Foo> pod;
    auto fpod = pod.getFuture();
    Foo f = {"the answer", 42};
    pod.setValue(f);
    Foo f2 = fpod.get();
    REQUIRE(f.name == f2.name);
    REQUIRE(f.value == f2.value);
    
    pod = Promise<Foo>();
    fpod = pod.getFuture();
    pod.setValue(std::move(f2));
    Foo f3 = fpod.get();
    REQUIRE(f.name == f3.name);
    REQUIRE(f.value == f3.value);
    
    Promise<std::unique_ptr<int>> mov;
    auto fmov = mov.getFuture();
    mov.setValue(std::make_unique<int>(42));
    std::unique_ptr<int> ptr = std::move(fmov).get();
    REQUIRE(42 == *ptr);
    
    Promise<Unit> v;
    auto fv = v.getFuture();
    v.setValue();
    REQUIRE(fv.isReady());
  }
  
SECTION("setException") {
    {
      Promise<int> p;
      auto f = p.getFuture();
      p.setException(eggs);
      REQUIRE_THROWS_AS(f.get(), eggs_t);
    }
    {
      Promise<int> p;
      auto f = p.getFuture();
      p.setException(std::make_exception_ptr(eggs));
      REQUIRE_THROWS_AS(f.get(), eggs_t);
    }
  }
  
SECTION("setWith") {
    {
      Promise<int> p;
      auto f = p.getFuture();
      p.setWith([] { return 42; });
      REQUIRE(42 == f.get());
    }
    {
      Promise<int> p;
      auto f = p.getFuture();
      p.setWith([]() -> int { throw eggs; });
      REQUIRE_THROWS_AS(f.get(), eggs_t);
    }
  }
  
SECTION("isFulfilled") {
    Promise<int> p;
    
    REQUIRE_FALSE(p.isFulfilled());
    p.setValue(42);
    REQUIRE(p.isFulfilled());
  }
  
SECTION("isFulfilledWithFuture") {
    Promise<int> p;
    auto f = p.getFuture(); // so core_ will become null
    
    REQUIRE_FALSE(p.isFulfilled());
    p.setValue(42); // after here
    REQUIRE(p.isFulfilled());
  }
  
SECTION("brokenOnDelete") {
    auto p = std::make_unique<Promise<int>>();
    auto f = p->getFuture();
    
    REQUIRE_FALSE(f.isReady());
    
    p.reset();
    
    REQUIRE(f.isReady());
    
    auto t = f.getTry();
  
    REQUIRE(t.hasException());
    REQUIRE_THROWS_AS(t.throwIfFailed(), FutureException);
    //REQUIRE(t.hasException<BrokenPromise>());
  }
  
/*SECTION("brokenPromiseHasTypeInfo") {
    auto pInt = std::make_unique<Promise<int>>();
    auto fInt = pInt->getFuture();
    
    auto pFloat = std::make_unique<Promise<float>>();
    auto fFloat = pFloat->getFuture();
    
    pInt.reset();
    pFloat.reset();
  
  try {
    auto whatInt = fInt.getTry().exception().what();
  } catch(e) {
    
  }
    auto whatFloat = fFloat.getTry().exception().what();
    
    REQUIRE(whatInt != whatFloat);
  }*/
}
