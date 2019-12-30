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
#include "Futures/Utilities.h"

#include "gtest/gtest.h"

#include <condition_variable>
#include <mutex>

using namespace arangodb::futures;

namespace {
  auto makeValid() {
    auto valid = makeFuture<int>(42);
    EXPECT_TRUE(valid.valid());
    return valid;
  }
  auto makeInvalid() {
    auto invalid = Future<int>::makeEmpty();
    EXPECT_FALSE(invalid.valid());
    return invalid;
  }

  template <typename T>
  constexpr typename std::decay<T>::type copy(T&& value) noexcept(noexcept(typename std::decay<T>::type(std::forward<T>(value)))) {
    return std::forward<T>(value);
  }

  int onThenHelperAddOne(int i) {
    return i + 1;
  }

  int onThenHelperAddFive(int i) {
    return i + 5;
  }

  Future<int> onThenHelperAddFutureFive(int i) {
    return makeFuture(i + 5);
  }

  typedef std::domain_error eggs_t;
  static eggs_t eggs("eggs");

  Future<int> onErrorHelperEggs(const eggs_t&) {
    return makeFuture(10);
  }
  Future<int> onErrorHelperGeneric(const std::exception&) {
    return makeFuture(20);
  }
} // namespace



// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST(FutureTest, basic) {
  auto f = Future<int>::makeEmpty();
  EXPECT_ANY_THROW(f.isReady());
}

TEST(FutureTest, default_ctor) {
  Future<Unit> abc{};
}

TEST(FutureTest, requires_only_move_ctor) {
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
    ASSERT_TRUE(f.valid());
    ASSERT_TRUE(f.isReady());
    auto v = std::move(f).get();
    ASSERT_TRUE(v.id_ == 42);
  }
  {
    auto f = makeFuture<MoveCtorOnly>(MoveCtorOnly(42));
    ASSERT_TRUE(f.valid());
    ASSERT_TRUE(f.isReady());
    auto v = std::move(f).get();
    ASSERT_TRUE(v.id_ == 42);
  }
  {
    auto f = makeFuture<MoveCtorOnly>(MoveCtorOnly(42));
    ASSERT_TRUE(f.valid());
    ASSERT_TRUE(f.isReady());
    auto v = std::move(f).get(std::chrono::milliseconds(10));
    ASSERT_TRUE(v.id_ == 42);
  }
  {
    auto f = makeFuture<MoveCtorOnly>(MoveCtorOnly(42));
    ASSERT_TRUE(f.valid());
    ASSERT_TRUE(f.isReady());
    auto v = std::move(f).get(std::chrono::milliseconds(10));
    ASSERT_TRUE(v.id_ == 42);
  }
}

TEST(FutureTest, ctor_post_condition) {
  auto const except = std::logic_error("foo");
  auto const ewrap = std::make_exception_ptr(std::logic_error("foo"));

#define DOIT(CREATION_EXPR)    \
do {                         \
auto f1 = (CREATION_EXPR);   \
ASSERT_TRUE(f1.valid());         \
auto f2 = std::move(f1);     \
ASSERT_FALSE(f1.valid());  \
ASSERT_TRUE(f2.valid());        \
} while (false)

  DOIT(makeValid());
  DOIT(Future<int>(42));
  DOIT(Future<int>{42});
  // I did not include Follys Unit type
  //DOIT(Future<Unit>());
  //DOIT(Future<Unit>{});
  DOIT(makeFuture());
  //DOIT(makeFuture(Unit{}));
  //DOIT(makeFuture<Unit>(Unit{}));
  DOIT(makeFuture(42));
  DOIT(makeFuture<int>(42));
  DOIT(makeFuture<int>(except));
  DOIT(makeFuture<int>(ewrap));
  DOIT(makeFuture(Try<int>(42)));
  DOIT(makeFuture<int>(Try<int>(42)));
  DOIT(makeFuture<int>(Try<int>(ewrap)));
#undef DOIT
}

TEST(FutureTest, ctor_post_condition_invalid) {
#define DOIT(CREATION_EXPR)    \
do {                         \
auto f1 = (CREATION_EXPR); \
ASSERT_FALSE(f1.valid());  \
auto f2 = std::move(f1);   \
ASSERT_FALSE(f1.valid());  \
ASSERT_FALSE(f2.valid());  \
} while (false)

  DOIT(makeInvalid());
  DOIT(Future<int>::makeEmpty());

#undef DOIT
}

TEST(FutureTest, lacksPreconditionValid) {
    // Ops that don't throw FutureInvalid if !valid() --
    // without precondition: valid()

#define DOIT(STMT)         \
do {                     \
auto f = makeValid();  \
{ STMT; }              \
::copy(std::move(f));    \
STMT; \
} while (false)

    // .valid() itself
    DOIT(f.valid());

    // move-ctor - move-copy to local, copy(), pass-by-move-value
    DOIT(auto other = std::move(f));
    DOIT(copy(std::move(f)));
    DOIT(([](auto) {})(std::move(f)));

    // move-assignment into either {valid | invalid}
    DOIT({
      auto other = makeValid();
      other = std::move(f);
    });
    DOIT({
      auto other = makeInvalid();
      other = std::move(f);
    });

#undef DOIT
  }

TEST(FutureTest, hasPreconditionValid) {
    // Ops that require validity; precondition: valid();
    // throw FutureInvalid if !valid()

#define DOIT(STMT)                     \
do {                                 \
auto f = makeValid();              \
STMT;             \
::copy(std::move(f));                \
EXPECT_ANY_THROW(STMT); \
} while (false)

    DOIT(f.isReady());
    DOIT(f.result());
    DOIT(std::move(f).get());
    DOIT(std::move(f).get(std::chrono::milliseconds(10)));
    DOIT(f.result());
    DOIT(f.hasValue());
    DOIT(f.hasException());
    DOIT(f.get());
    //DOIT(std::move(f).then());
    DOIT(std::move(f).thenValue([](int&&) noexcept -> void {}));
    DOIT(std::move(f).thenValue([](auto&&) noexcept -> void {}));

#undef DOIT
}

TEST(FutureTest, hasPostconditionValid) {
    // Ops that preserve validity -- postcondition: valid()

#define DOIT(STMT)          \
do {                      \
auto f = makeValid();   \
EXPECT_NO_THROW(STMT);  \
ASSERT_TRUE(f.valid()); \
} while (false)

    auto const swallow = [](auto) {};

    DOIT(swallow(f.valid())); // f.valid() itself preserves validity
    DOIT(swallow(f.isReady()));
    DOIT(swallow(f.hasValue()));
    DOIT(swallow(f.hasException()));
    DOIT(swallow(f.get()));
    DOIT(swallow(f.getTry()));
    //DOIT(swallow(f.poll()));
    //DOIT(f.raise(std::logic_error("foo")));
    //DOIT(f.cancel());
    DOIT(swallow(f.getTry()));
    DOIT(f.wait());
    //DOIT(std::move(f.wait()));

#undef DOIT
  }

TEST(FutureTest, lacksPostconditionValid) {
    // Ops that consume *this -- postcondition: !valid()

#define DOIT(CTOR, STMT)     \
do {                       \
auto f = (CTOR);         \
STMT;   \
ASSERT_FALSE(f.valid()); \
} while (false)

    // move-ctor of {valid|invalid}
    DOIT(makeValid(), { auto other{std::move(f)}; });
    DOIT(makeInvalid(), { auto other{std::move(f)}; });

    // move-assignment of {valid|invalid} into {valid|invalid}
    DOIT(makeValid(), {
      auto other = makeValid();
      other = std::move(f);
    });
    DOIT(makeValid(), {
      auto other = makeInvalid();
      other = std::move(f);
    });
    DOIT(makeInvalid(), {
      auto other = makeValid();
      other = std::move(f);
    });
    DOIT(makeInvalid(), {
      auto other = makeInvalid();
      other = std::move(f);
    });

    // pass-by-value of {valid|invalid}
    DOIT(makeValid(), {
      auto const byval = [](auto) {};
      byval(std::move(f));
    });
    DOIT(makeInvalid(), {
      auto const byval = [](auto) {};
      byval(std::move(f));
    });

    // other consuming ops
    auto const swallow = [](auto) {};
    //DOIT(makeValid(), swallow(std::move(f).wait()));
    //DOIT(makeValid(), swallow(std::move(f.wait())));
    DOIT(makeValid(), swallow(std::move(f).get()));
    DOIT(makeValid(), swallow(std::move(f).get(std::chrono::milliseconds(10))));
    //DOIT(makeValid(), swallow(std::move(f).semi()));

#undef DOIT
}

TEST(FutureTest, thenError) {
    bool theFlag = false;
    auto flag = [&] { theFlag = true; };
#define EXPECT_FLAG()     \
do {              \
f.wait();         \
ASSERT_TRUE(theFlag); \
theFlag = false;      \
} while (0);

#define EXPECT_NO_FLAG()   \
do {                     \
ASSERT_FALSE(theFlag); \
theFlag = false;       \
} while (0);

    // By reference
    {
      auto f = makeFuture()
      .thenValue([](Unit){ throw std::logic_error("abc"); })
      .thenError<std::logic_error>([&](const std::logic_error& /* e */) noexcept { flag(); });
      EXPECT_FLAG();
      EXPECT_NO_THROW(f.get());
    }

    // By auto reference
    {
      auto f = makeFuture()
      .thenValue([](Unit){ throw eggs; })
      .thenError<eggs_t>([&](auto const& /* e */) noexcept { flag(); });
      EXPECT_FLAG();
      EXPECT_NO_THROW(f.get());
    }

    {
      auto f =
      makeFuture()
      .thenValue([](Unit){ throw eggs; })
      .thenError<eggs_t>([&](auto const& /* e */) {
        flag();
        return makeFuture();
      });
      EXPECT_FLAG();
      EXPECT_NO_THROW(f.get());
    }

    // By value
    {
      auto f = makeFuture()
      .thenValue([](Unit){ throw eggs; })
      .thenError<eggs_t>([&](eggs_t /* e */) noexcept {
        flag();
      });
      EXPECT_FLAG();
      EXPECT_NO_THROW(f.get());
    }

    {
      auto f = makeFuture()
      .thenValue([](Unit) { throw eggs; }).thenError<eggs_t>([&](eggs_t /* e */) {
        flag();
        return makeFuture();
      });
      EXPECT_FLAG();
      EXPECT_NO_THROW(f.get());
    }

//    // Polymorphic
//    {
//      auto f = makeFuture()
//      .thenValue([] { throw eggs; })
//      .thenError([&](std::exception& /* e */) { flag(); });
//      EXPECT_FLAG();
//      EXPECT_NO_THROW(f.value());
//    }
//
//    {
//      auto f = makeFuture()
//      .thenValue([] { throw eggs; })
//      .thenError([&](std::exception& /* e */) {
//        flag();
//        return makeFuture();
//      });
//      EXPECT_FLAG();
//      EXPECT_NO_THROW(f.value());
//    }
//
    // Non-exceptions
    {
      auto f = makeFuture().thenValue([](Unit){ throw - 1; }).thenError<int>([&](int /* e */) noexcept {
        flag();
      });
      EXPECT_FLAG();
      EXPECT_NO_THROW(f.get());
    }

    {
      auto f = makeFuture().thenValue([](Unit){ throw - 1; }).thenError<int>([&](int /* e */) {
        flag();
        return makeFuture();
      });
      EXPECT_FLAG();
      EXPECT_NO_THROW(f.get());
    }

    // Mutable lambda
    {
      auto f = makeFuture()
      .thenValue([](Unit){ throw eggs; })
      .thenError<eggs_t&>([&](eggs_t& /* e */) mutable noexcept { flag(); });
      EXPECT_FLAG();
      EXPECT_NO_THROW(f.get());
    }

    {
      auto f = makeFuture()
      .thenValue([](Unit){ throw eggs; })
      .thenError<eggs_t&>([&](eggs_t& /* e */) mutable {
        flag();
        return makeFuture();
      });
      EXPECT_FLAG();
      EXPECT_NO_THROW(f.get());
    }

    // Function pointer
    {
      auto f = makeFuture()
      .thenValue([](Unit) -> int { throw eggs; })
      .thenError<const eggs_t&>(onErrorHelperEggs)
      .thenError<std::exception const&>(onErrorHelperGeneric);
      ASSERT_TRUE(10 == f.get());
    }
    {
      auto f = makeFuture()
      .thenValue([](Unit) -> int { throw std::runtime_error("test"); })
      .thenError<const eggs_t&>(onErrorHelperEggs)
      .thenError<std::exception>(onErrorHelperGeneric);
      ASSERT_TRUE(20 == f.get());
    }
    {
      auto f = makeFuture()
      .thenValue([](Unit) -> int { throw std::runtime_error("test"); })
      .thenError<const eggs_t&>(onErrorHelperEggs);
      EXPECT_THROW(f.get(), std::runtime_error);
    }

    // No throw
    {
      auto f = makeFuture().thenValue([](Unit) noexcept { return 42; }).thenError<eggs_t&>([&](eggs_t& /* e */) noexcept {
        flag();
        return -1;
      });
      EXPECT_NO_FLAG();
      ASSERT_TRUE(42 == f.get());
    }

    {
      auto f = makeFuture().thenValue([](Unit) noexcept { return 42; }).thenError<eggs_t&>([&](eggs_t& /* e */) {
        flag();
        return makeFuture<int>(-1);
      });
      EXPECT_NO_FLAG();
      ASSERT_TRUE(42 == f.get());
    }

    // Catch different exception
    {
      auto f = makeFuture()
      .thenValue([](Unit) { throw eggs; })
      .thenError<std::runtime_error&>([&](std::runtime_error& /* e */) noexcept { flag(); });
      EXPECT_NO_FLAG();
      EXPECT_THROW(f.get(), eggs_t);
    }

    {
      auto f = makeFuture()
      .thenValue([](Unit) { throw eggs; })
      .thenError<std::runtime_error&>([&](std::runtime_error& /* e */) {
        flag();
        return makeFuture();
      });
      EXPECT_NO_FLAG();
      EXPECT_THROW(f.get(), eggs_t);
    }

    // Returned value propagates
    {
      auto f = makeFuture()
      .thenValue([](Unit) -> int { throw eggs; })
      .thenError<eggs_t&>([&](eggs_t& /* e */) noexcept { return 42; });
      ASSERT_TRUE(42 == f.get());
    }

    // Returned future propagates
    {
      auto f = makeFuture()
      .thenValue([](Unit) -> int { throw eggs; })
      .thenError<eggs_t&>([&](eggs_t& /* e */) { return makeFuture<int>(42); });
      ASSERT_TRUE(42 == f.get());
    }

    // Throw in callback
    {
      auto f = makeFuture()
      .thenValue([](Unit) -> int { throw eggs; })
      .thenError<eggs_t&>([&](eggs_t& e) -> int { throw e; });
      EXPECT_THROW(f.get(), eggs_t);
    }

    {
      auto f = makeFuture()
      .thenValue([](Unit) -> int { throw eggs; })
      .thenError<eggs_t&>([&](eggs_t& e) -> Future<int> { throw e; });
      EXPECT_THROW(f.get(), eggs_t);
    }

//    // exception_wrapper, return Future<T>
//    {
//      auto f = makeFuture()
//      .thenValue([] { throw eggs; })
//      .onError([&](exception_wrapper /* e */) {
//        flag();
//        return makeFuture();
//      });
//      EXPECT_FLAG();
//      EXPECT_NO_THROW(f.value());
//    }
//
//    // exception_wrapper, return Future<T> but throw
//    {
//      auto f = makeFuture()
//      .thenValue([]() -> int { throw eggs; })
//      .onError([&](exception_wrapper /* e */) -> Future<int> {
//        flag();
//        throw eggs;
//      });
//      EXPECT_FLAG();
//      EXPECT_THROW(f.value(), eggs_t);
//    }
//
//    // exception_wrapper, return T
//    {
//      auto f = makeFuture()
//      .thenValue([]() -> int { throw eggs; })
//      .onError([&](exception_wrapper /* e */) {
//        flag();
//        return -1;
//      });
//      EXPECT_FLAG();
//      ASSERT_TRUE(-1 == f.value());
//    }
//
//    // exception_wrapper, return T but throw
//    {
//      auto f = makeFuture()
//      .then([]() -> int { throw eggs; })
//      .onError([&](exception_wrapper /* e */) -> int {
//        flag();
//        throw eggs;
//      });
//      EXPECT_FLAG();
//      EXPECT_THROW(f.value(), eggs_t);
//    }
//
//    // const exception_wrapper&
//    {
//      auto f = makeFuture()
//      .then([] { throw eggs; })
//      .onError([&](const exception_wrapper& /* e */) {
//        flag();
//        return makeFuture();
//      });
//      EXPECT_FLAG();
//      EXPECT_NO_THROW(f.value());
//    }
#undef EXPECT_FLAG
#undef EXPECT_NO_FLAG
  }

TEST(FutureTest, special) {
  ASSERT_FALSE(std::is_copy_constructible<Future<int>>::value);
  ASSERT_FALSE(std::is_copy_assignable<Future<int>>::value);
  ASSERT_TRUE(std::is_move_constructible<Future<int>>::value);
  ASSERT_TRUE(std::is_move_assignable<Future<int>>::value);
}

TEST(FutureTest, then) {
  auto f =
  makeFuture<std::string>("0")
  .thenValue([](std::string) { return makeFuture<std::string>("1"); })
  .then([](Try<std::string>&& t) { return makeFuture(t.get() + ";2"); })
  .then([](const Try<std::string>&& t) {
    return makeFuture(t.get() + ";3");
  })
  .then([](const Try<std::string>& t) {
    return makeFuture(t.get() + ";4");
  })
  .then([](Try<std::string> t) { return makeFuture(t.get() + ";5"); })
  .then([](const Try<std::string> t) {
    return makeFuture(t.get() + ";6");
  })
  .thenValue([](std::string&& s) { return makeFuture(s + ";7"); })
  .thenValue([](const std::string&& s) { return makeFuture(s + ";8"); })
  .thenValue([](const std::string& s) { return makeFuture(s + ";9"); })
  .thenValue([](std::string s) { return makeFuture(s + ";10"); })
  .thenValue([](const std::string s) { return makeFuture(s + ";11"); });
  std::string value = f.get();
  ASSERT_TRUE(value == "1;2;3;4;5;6;7;8;9;10;11");
}

TEST(FutureTest, then_static_functions) {
  auto f = makeFuture<int>(10).thenValue(onThenHelperAddFive);
  ASSERT_TRUE(f.get() == 15);

  auto f2 = makeFuture<int>(15).thenValue(onThenHelperAddFutureFive);
  ASSERT_TRUE(f2.get() == 20);
}

TEST(FutureTest, get) {
  auto f = makeFuture(std::make_unique<int>(42));
  auto up = std::move(f).get();
  ASSERT_TRUE(42 == *up);

  EXPECT_THROW(makeFuture<int>(eggs).get(), eggs_t);
}

TEST(FutureTest, isReady) {
  Promise<int> p;
  auto f = p.getFuture();
  ASSERT_FALSE(f.isReady());
  p.setValue(42);
  ASSERT_TRUE(f.isReady());
}

TEST(FutureTest, futureNotReady) {
  Promise<int> p;
  Future<int> f = p.getFuture();
  EXPECT_THROW(f.result().get(), FutureException);
}

TEST(FutureTest, makeFuture) {
  ASSERT_TRUE(makeFuture<int>(eggs).getTry().hasException());
  ASSERT_FALSE(makeFuture(42).getTry().hasException());
}

TEST(FutureTest, hasValue) {
  ASSERT_TRUE(makeFuture(42).getTry().hasValue());
  ASSERT_FALSE(makeFuture<int>(eggs).getTry().hasValue());
}

TEST(FutureTest, makeFuture2) {
  //EXPECT_TYPE(makeFuture(42), Future<int>);
  ASSERT_TRUE(42 == makeFuture(42).get());

  //EXPECT_TYPE(makeFuture<float>(42), Future<float>);
  ASSERT_TRUE(42 == makeFuture<float>(42).get());

  auto fun = [] { return 42; };
  //EXPECT_TYPE(makeFutureWith(fun), Future<int>);
  ASSERT_TRUE(42 == makeFutureWith(fun).get());

  auto funf = [] { return makeFuture<int>(43); };
  //EXPECT_TYPE(makeFutureWith(funf), Future<int>);
  ASSERT_TRUE(43 == makeFutureWith(funf).get());

  auto failfun = []() -> int { throw eggs; };
  //EXPECT_TYPE(makeFutureWith(failfun), Future<int>);
  EXPECT_NO_THROW(makeFutureWith(failfun));
  EXPECT_THROW(makeFutureWith(failfun).get(), eggs_t);

  auto failfunf = []() -> Future<int> { throw eggs; };
  //EXPECT_TYPE(makeFutureWith(failfunf), Future<int>);
  EXPECT_NO_THROW(makeFutureWith(failfunf));
  EXPECT_THROW(makeFutureWith(failfunf).get(), eggs_t);

  //EXPECT_TYPE(makeFuture(), Future<Unit>);
}

TEST(FutureTest, finish) {
  auto x = std::make_shared<int>(0);

  Promise<int> p;
  auto f = p.getFuture().then([x](Try<int>&& t) { *x = t.get(); });

  // The callback hasn't executed
  ASSERT_TRUE(0 == *x);

  // The callback has a reference to x
  ASSERT_TRUE(2 == x.use_count());

  p.setValue(42);
  f.wait();

  // the callback has executed
  ASSERT_EQ(42, *x);

  std::this_thread::yield();

  // the callback has been destructed
  // and has released its reference to x
  ASSERT_EQ(1, x.use_count());
}

TEST(FutureTest, detachRace) {
  // This test is designed to detect a race that was in Core::detachOne()
  // where detached_ was incremented and then tested, and that
  // allowed a race where both Promise and Future would think they were the
  // second and both try to delete. This showed up at scale but was very
  // difficult to reliably repro in a test. As it is, this only fails about
  // once in every 1,000 executions. Doing this 1,000 times is going to make a
  // slow test so I won't do that but if it ever fails, take it seriously, and
  // run the test binary with "--gtest_repeat=10000 --gtest_filter=*detachRace"
  // (Don't forget to enable ASAN)
  auto p = std::make_unique<Promise<bool>>();
  auto f = std::make_unique<Future<bool>>(p->getFuture());
  //folly::Baton<> baton;
  std::mutex m;
  std::condition_variable condition;

  std::unique_lock<std::mutex> guard(m);
  std::thread t1([&]{
    std::unique_lock<std::mutex> lock(m);
    condition.notify_one();
    lock.unlock();
    p.reset();
  });
  condition.wait(guard);
  f.reset();
  t1.join();
}

  // Test of handling of a circular dependency. It's never recommended
  // to have one because of possible memory leaks. Here we test that
  // we can handle freeing of the Future while it is running.
TEST(FutureTest, CircularDependencySharedPtrSelfReset) {
  Promise<int64_t> promise;
  auto ptr = std::make_shared<Future<int64_t>>(promise.getFuture());

  std::move(*ptr).then([ptr](Try<int64_t>&& /* uid */) mutable {
    ASSERT_TRUE(1 == ptr.use_count());

    // Leaving no references to ourselves.
    ptr.reset();
    ASSERT_TRUE(0 == ptr.use_count());
  });

  ASSERT_TRUE(2 == ptr.use_count());

  ptr.reset();

  promise.setValue(1);
}

TEST(FutureTest, Constructor) {
  auto f1 = []() -> Future<int> { return Future<int>(3); }();
  ASSERT_TRUE(f1.get() == 3);
  auto f2 = []() -> Future<Unit> { return Future<Unit>(); }();
  EXPECT_NO_THROW(f2.getTry());
}

TEST(FutureTest, ImplicitConstructor) {
  auto f1 = []() -> Future<int> { return 3; }();
  ASSERT_TRUE(f1.get() == 3);
  // Unfortunately, the C++ standard does not allow the
  // following implicit conversion to work:
  //auto f2 = []() -> Future<Unit> { }();
}

TEST(FutureTest, InPlaceConstructor) {
  auto f = Future<std::pair<int, double>>(std::in_place, 5, 3.2);
  ASSERT_TRUE(5 == f.get().first);
}

TEST(FutureTest, makeFutureNoThrow) {
  makeFuture().get();
}

TEST(FutureTest, invokeCallbackReturningValueAsRvalue) {
  struct Foo {
    int operator()(int x) & noexcept {
      return x + 1;
    }
    int operator()(int x) const& noexcept {
      return x + 2;
    }
    int operator()(int x) && noexcept {
      return x + 3;
    }
  };

  Foo foo;
  Foo const cfoo;

  // The continuation will be forward-constructed - copied if given as & and
  // moved if given as && - everywhere construction is required.
  // The continuation will be invoked with the same cvref as it is passed.
  ASSERT_TRUE(101 == makeFuture<int>(100).thenValue(foo).get());
  ASSERT_TRUE(202 == makeFuture<int>(200).thenValue(cfoo).get());
  ASSERT_TRUE(303 == makeFuture<int>(300).thenValue(Foo()).get());
}

TEST(FutureTest, invokeCallbackReturningFutureAsRvalue) {
  struct Foo {
    Future<int> operator()(int x) & {
      return x + 1;
    }
    Future<int> operator()(int x) const& {
      return x + 2;
    }
    Future<int> operator()(int x) && {
      return x + 3;
    }
  };

  Foo foo;
  Foo const cfoo;

  // The continuation will be forward-constructed - copied if given as & and
  // moved if given as && - everywhere construction is required.
  // The continuation will be invoked with the same cvref as it is passed.
  ASSERT_TRUE(101 == makeFuture<int>(100).thenValue(foo).get());
  ASSERT_TRUE(202 == makeFuture<int>(200).thenValue(cfoo).get());
  ASSERT_TRUE(303 == makeFuture<int>(300).thenValue(Foo()).get());
}

TEST(FutureTest, basic_example) {
  Promise<int> p;
  Future<int> f = p.getFuture();
  auto f2 = std::move(f).thenValue(onThenHelperAddOne);
  p.setValue(42);
  ASSERT_TRUE(f2.get() == 43);
}

TEST(FutureTest, basic_example_fpointer) {
  Promise<int> p;
  Future<int> f = p.getFuture();
  auto f2 = std::move(f).thenValue(&onThenHelperAddOne);
  p.setValue(42);
  ASSERT_TRUE(f2.get() == 43);
}
