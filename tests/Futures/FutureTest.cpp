////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#include "gtest/gtest.h"
#include "yaclib/async/contract.hpp"
#include "yaclib/async/run.hpp"

#include <yaclib/async/make.hpp>
#include <yaclib/async/future.hpp>

#include <condition_variable>
#include <mutex>

namespace {
auto makeValid() {
  auto valid = yaclib::MakeFuture<int>(42);
  EXPECT_TRUE(valid.Valid());
  return valid;
}
auto makeInvalid() {
  yaclib::Future<int> invalid;
  EXPECT_FALSE(invalid.Valid());
  return invalid;
}

template<typename T>
constexpr typename std::decay<T>::type copy(T&& value) noexcept(
    noexcept(typename std::decay<T>::type(std::forward<T>(value)))) {
  return std::forward<T>(value);
}

int onThenHelperAddOne(int i) { return i + 1; }

int onThenHelperAddFive(int i) { return i + 5; }

yaclib::Future<int> onThenHelperAddFutureFive(int i) {
  return yaclib::MakeFuture(i + 5);
}

typedef std::domain_error eggs_t;
static eggs_t eggs("eggs");

yaclib::Future<int> onErrorHelperEggs(const eggs_t&) {
  return yaclib::MakeFuture(10);
}
yaclib::Future<int> onErrorHelperGeneric(const std::exception&) {
  return yaclib::MakeFuture(20);
}
}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST(FutureTest, basic) {
  yaclib::Future<int> f;
  EXPECT_FALSE(f.Valid());
}

TEST(FutureTest, default_ctor) {
  yaclib::Future<> abc;
  yaclib::Future<> abc2 = yaclib::MakeFuture();
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
    auto f = yaclib::MakeFuture<MoveCtorOnly>(MoveCtorOnly(42));
    ASSERT_TRUE(f.Valid());
    ASSERT_TRUE(f.Ready());
    auto v = std::move(f).Get().Ok();
    ASSERT_TRUE(v.id_ == 42);
  }
  {
    auto f = yaclib::MakeFuture<MoveCtorOnly>(MoveCtorOnly(42));
    ASSERT_TRUE(f.Valid());
    ASSERT_TRUE(f.Ready());
    auto v = std::move(f).Get().Ok();
    ASSERT_TRUE(v.id_ == 42);
  }
}

TEST(FutureTest, ctor_post_condition) {
  auto const except = std::logic_error("foo");
  auto const ewrap = std::make_exception_ptr(std::logic_error("foo"));

#define DOIT(CREATION_EXPR)    \
  do {                         \
    auto f1 = (CREATION_EXPR); \
    ASSERT_TRUE(f1.Valid());   \
    auto f2 = std::move(f1);   \
    ASSERT_FALSE(f1.Valid());  \
    ASSERT_TRUE(f2.Valid());   \
  } while (false)

  DOIT(makeValid());
  DOIT(yaclib::MakeFuture<int>(42));
  // I did not include Follys Unit type
  // DOIT(Future<Unit>());
  // DOIT(Future<Unit>{});
  DOIT(yaclib::MakeFuture());
  // DOIT(yaclib::MakeFuture(Unit{}));
  // DOIT(yaclib::MakeFuture<Unit>(Unit{}));
  DOIT(yaclib::MakeFuture(42));
  DOIT(yaclib::MakeFuture<int>(42));
  DOIT(yaclib::MakeFuture<int>(except));
  DOIT(yaclib::MakeFuture<int>(ewrap));
  DOIT(yaclib::MakeFuture<int>(yaclib::Result<int>(42)));
  DOIT(yaclib::MakeFuture<int>(yaclib::Result<int>(ewrap)));
#undef DOIT
}

TEST(FutureTest, ctor_post_condition_invalid) {
#define DOIT(CREATION_EXPR)    \
  do {                         \
    auto f1 = (CREATION_EXPR); \
    ASSERT_FALSE(f1.Valid());  \
    auto f2 = std::move(f1);   \
    ASSERT_FALSE(f1.Valid());  \
    ASSERT_FALSE(f2.Valid());  \
  } while (false)

  DOIT(makeInvalid());
  DOIT(yaclib::Future<int>{});

#undef DOIT
}

TEST(FutureTest, thenError) {
  bool theFlag = false;
  auto flag = [&] { theFlag = true; };
#define EXPECT_FLAG()     \
  do {                    \
    Wait(f);              \
    ASSERT_TRUE(theFlag); \
    theFlag = false;      \
  } while (0)

#define EXPECT_NO_FLAG()   \
  do {                     \
    ASSERT_FALSE(theFlag); \
    theFlag = false;       \
  } while (0)

  // By reference
  {
    auto f = yaclib::MakeFuture()
                 .ThenInline([]() { throw std::logic_error("abc"); })
                 .ThenInline([&](std::exception_ptr&&) noexcept { flag(); });
    EXPECT_FLAG();
    EXPECT_NO_THROW(std::move(f).Get().Ok());
  }

  // By auto reference
  {
    auto f = yaclib::MakeFuture()
                 .ThenInline([](yaclib::Unit) { throw eggs; })
                 .ThenInline([&](std::exception_ptr&&) noexcept { flag(); });
    EXPECT_FLAG();
    EXPECT_NO_THROW(std::move(f).Get().Ok());
  }

  {
    auto f = yaclib::MakeFuture()
                 .ThenInline([]() { throw eggs; })
                 .ThenInline([&](std::exception_ptr&&) {
                   flag();
                   return yaclib::MakeFuture();
                 });
    EXPECT_FLAG();
    EXPECT_NO_THROW(std::move(f).Get().Ok());
  }

  // By value
  {
    auto f = yaclib::MakeFuture()
                 .ThenInline([]() { throw eggs; })
                 .ThenInline([&](std::exception_ptr&&) noexcept { flag(); });
    EXPECT_FLAG();
    EXPECT_NO_THROW(std::move(f).Get().Ok());
  }

  {
    auto f = yaclib::MakeFuture()
                 .ThenInline([]() { throw eggs; })
                 .ThenInline([&](std::exception_ptr&&) {
                   flag();
                   return yaclib::MakeFuture();
                 });
    EXPECT_FLAG();
    EXPECT_NO_THROW(std::move(f).Get().Ok());
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
    auto f = yaclib::MakeFuture()
                 .ThenInline([]() { throw -1; })
                 .ThenInline([&](std::exception_ptr&&) noexcept { flag(); });
    EXPECT_FLAG();
    EXPECT_NO_THROW(std::move(f).Get().Ok());
  }

  {
    auto f = yaclib::MakeFuture()
                 .ThenInline([]() { throw -1; })
                 .ThenInline([&](std::exception_ptr&&) {
                   flag();
                   return yaclib::MakeFuture();
                 });
    EXPECT_FLAG();
    EXPECT_NO_THROW(std::move(f).Get().Ok());
  }

  // Mutable lambda
  {
    auto f =
        yaclib::MakeFuture()
            .ThenInline([]() { throw eggs; })
            .ThenInline([&](std::exception_ptr&&) mutable noexcept { flag(); });
    EXPECT_FLAG();
    EXPECT_NO_THROW(std::move(f).Get().Ok());
  }

  {
    auto f = yaclib::MakeFuture()
                 .ThenInline([]() { throw eggs; })
                 .ThenInline([&](std::exception_ptr&&) mutable {
                   flag();
                   return yaclib::MakeFuture();
                 });
    EXPECT_FLAG();
    EXPECT_NO_THROW(std::move(f).Get().Ok());
  }

  // Function pointer
  {
    auto f = yaclib::MakeFuture()
                 .ThenInline([]() -> int { throw eggs; })
                 .ThenInline([&](std::exception_ptr&& ptr) {
                   try {
                     std::rethrow_exception(std::move(ptr));
                   } catch (eggs_t const& e) {
                     onErrorHelperEggs(e);
                   } catch (std::exception const& e) {
                     onErrorHelperGeneric(e);
                   }
                   return 0;
                 });
    ASSERT_TRUE(10 == std::move(f).Get().Ok());
  }
  {
    auto f = yaclib::MakeFuture()
                 .ThenInline([]() -> int { throw std::runtime_error("test"); })
                 .ThenInline([&](std::exception_ptr&& ptr) {
                   try {
                     std::rethrow_exception(std::move(ptr));
                   } catch (eggs_t const& e) {
                     onErrorHelperEggs(e);
                   } catch (std::exception const& e) {
                     onErrorHelperGeneric(e);
                   }
                   return 0;
                 });
    ASSERT_TRUE(20 == std::move(f).Get().Ok());
  }
  {
    auto f = yaclib::MakeFuture()
                 .ThenInline([]() -> int { throw std::runtime_error("test"); })
                 .ThenInline([&](std::exception_ptr&& ptr) {
                   try {
                     std::rethrow_exception(std::move(ptr));
                   } catch (eggs_t const& e) {
                     onErrorHelperEggs(e);
                   }
                   return 0;
                 });
    EXPECT_THROW(std::move(f).Get().Ok(), std::runtime_error);
  }

  // No throw
  {
    auto f = yaclib::MakeFuture()
                 .ThenInline([]() noexcept { return 42; })
                 .ThenInline([&](std::exception_ptr&&) {
                   flag();
                   return -1;
                 });
    EXPECT_NO_FLAG();
    ASSERT_TRUE(42 == std::move(f).Get().Ok());
  }

  {
    auto f = yaclib::MakeFuture()
                 .ThenInline([]() noexcept { return 42; })
                 .ThenInline([&](std::exception_ptr&&) {
                   flag();
                   return yaclib::MakeFuture<int>(-1);
                 });
    EXPECT_NO_FLAG();
    ASSERT_TRUE(42 == std::move(f).Get().Ok());
  }

  // Catch different exception
  {
    auto f = yaclib::MakeFuture()
                 .ThenInline([]() { throw eggs; })
                 .ThenInline([&](std::exception_ptr&& ptr) {
                   try {
                     std::rethrow_exception(std::move(ptr));
                   } catch (std::runtime_error const& e) {
                     flag();
                   }
                 });
    EXPECT_NO_FLAG();
    EXPECT_THROW(std::move(f).Get().Ok(), eggs_t);
  }

  {
    auto f = yaclib::MakeFuture()
                 .ThenInline([]() { throw eggs; })
                 .ThenInline([&](std::exception_ptr&& ptr) {
                   try {
                     std::rethrow_exception(std::move(ptr));
                   } catch (std::runtime_error const& e) {
                     flag();
                   }
                   return yaclib::MakeFuture();
                 });
    EXPECT_NO_FLAG();
    EXPECT_THROW(std::move(f).Get().Ok(), eggs_t);
  }

  // Returned value propagates
  {
    auto f = yaclib::MakeFuture()
                 .ThenInline([]() -> int { throw eggs; })
                 .ThenInline([&](std::exception_ptr&&) noexcept { return 42; });
    ASSERT_TRUE(42 == std::move(f).Get().Ok());
  }

  // Returned future propagates
  {
    auto f = yaclib::MakeFuture()
                 .ThenInline([]() -> int { throw eggs; })
                 .ThenInline([&](std::exception_ptr&&) {
                   return yaclib::MakeFuture<int>(42);
                 });
    ASSERT_TRUE(42 == std::move(f).Get().Ok());
  }

  // Throw in callback
  {
    auto f = yaclib::MakeFuture()
                 .ThenInline([]() -> int { throw eggs; })
                 .ThenInline([&](std::exception_ptr&& ptr) {
                   std::rethrow_exception(std::move(ptr));
                   return 0;
                 });
    EXPECT_THROW(std::move(f).Get().Ok(), eggs_t);
  }

  {
    auto f = yaclib::MakeFuture()
                 .ThenInline([]() -> int { throw eggs; })
                 .ThenInline([&](std::exception_ptr&& ptr) {
                   std::rethrow_exception(std::move(ptr));
                   return yaclib::MakeFuture(0);
                 });

    EXPECT_THROW(std::move(f).Get().Ok(), eggs_t);
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
  ASSERT_FALSE(std::is_copy_constructible<yaclib::Future<int>>::value);
  ASSERT_FALSE(std::is_copy_assignable<yaclib::Future<int>>::value);
  ASSERT_TRUE(std::is_move_constructible<yaclib::Future<int>>::value);
  ASSERT_TRUE(std::is_move_assignable<yaclib::Future<int>>::value);
}

TEST(FutureTest, then) {
  auto f = yaclib::MakeFuture<std::string>("0")
               .ThenInline([](std::string) {
                 return yaclib::MakeFuture<std::string>("1");
               })
               .ThenInline([](yaclib::Result<std::string>&& t) {
                 return yaclib::MakeFuture(std::move(t).Ok() + ";2");
               })
               .ThenInline([](const yaclib::Result<std::string>&& t) {
                 return yaclib::MakeFuture(std::move(t).Ok() + ";3");
               })
               .ThenInline([](const yaclib::Result<std::string>& t) {
                 return yaclib::MakeFuture(std::move(t).Ok() + ";4");
               })
               .ThenInline([](yaclib::Result<std::string> t) {
                 return yaclib::MakeFuture(std::move(t).Ok() + ";5");
               })
               .ThenInline([](const yaclib::Result<std::string> t) {
                 return yaclib::MakeFuture(std::move(t).Ok() + ";6");
               })
               .ThenInline(
                   [](std::string&& s) { return yaclib::MakeFuture(s + ";7"); })
               .ThenInline([](const std::string&& s) {
                 return yaclib::MakeFuture(s + ";8");
               })
               .ThenInline([](const std::string& s) {
                 return yaclib::MakeFuture(s + ";9");
               })
               .ThenInline(
                   [](std::string s) { return yaclib::MakeFuture(s + ";10"); })
               .ThenInline([](const std::string s) {
                 return yaclib::MakeFuture(s + ";11");
               });
  std::string value = std::move(f).Get().Ok();
  ASSERT_TRUE(value == "1;2;3;4;5;6;7;8;9;10;11");
}

TEST(FutureTest, then_static_functions) {
  auto f = yaclib::MakeFuture<int>(10).ThenInline(onThenHelperAddFive);
  ASSERT_TRUE(std::move(f).Get().Ok() == 15);

  auto f2 = yaclib::MakeFuture<int>(15).ThenInline(onThenHelperAddFutureFive);
  ASSERT_TRUE(std::move(f2).Get().Ok() == 20);
}

TEST(FutureTest, get) {
  auto f = yaclib::MakeFuture(std::make_unique<int>(42));
  auto up = std::move(f).Get().Ok();
  ASSERT_TRUE(42 == *up);

  EXPECT_THROW(yaclib::MakeFuture<int>(eggs).Get().Ok(), eggs_t);
}

TEST(FutureTest, isReady) {
  auto [f, p] = yaclib::MakeContract<int>();
  ASSERT_FALSE(f.Ready());
  std::move(p).Set(42);
  ASSERT_TRUE(f.Ready());
}

TEST(FutureTest, MakeFuture) {
  ASSERT_TRUE(yaclib::MakeFuture<int>(eggs).Get().State() ==
              yaclib::ResultState::Exception);
  ASSERT_FALSE(yaclib::MakeFuture(42).Get().State() ==
               yaclib::ResultState::Exception);
}

TEST(FutureTest, hasValue) {
  ASSERT_TRUE(yaclib::MakeFuture(42).Get());
  ASSERT_FALSE(yaclib::MakeFuture<int>(eggs).Get());
}

TEST(FutureTest, makeFuture2) {
  // EXPECT_TYPE(yaclib::MakeFuture(42), Future<int>);
  ASSERT_TRUE(42 == yaclib::MakeFuture(42).Get().Ok());

  // EXPECT_TYPE(yaclib::MakeFuture<float>(42), Future<float>);
  ASSERT_TRUE(42 == yaclib::MakeFuture<float>(42).Get().Ok());

  auto fun = [] { return 42; };
  // EXPECT_TYPE(makeFutureWith(fun), Future<int>);
  ASSERT_TRUE(42 == yaclib::Run(yaclib::MakeInline(), fun).Get().Ok());

  auto funf = [] { return yaclib::MakeFuture<int>(43); };
  // EXPECT_TYPE(makeFutureWith(funf), Future<int>);
  ASSERT_TRUE(43 == yaclib::Run(yaclib::MakeInline(), funf).Get().Ok());

  auto failfun = []() -> int { throw eggs; };
  // EXPECT_TYPE(makeFutureWith(failfun), Future<int>);
  EXPECT_NO_THROW(yaclib::Run(yaclib::MakeInline(), failfun));
  EXPECT_THROW(yaclib::Run(yaclib::MakeInline(), failfun).Get().Ok(), eggs_t);

  auto failfunf = []() -> yaclib::Future<int> { throw eggs; };
  // EXPECT_TYPE(makeFutureWith(failfunf), Future<int>);
  EXPECT_NO_THROW(yaclib::Run(yaclib::MakeInline(), failfunf));
  EXPECT_THROW(yaclib::Run(yaclib::MakeInline(), failfunf).Get().Ok(), eggs_t);

  // EXPECT_TYPE(yaclib::MakeFuture(), Future<Unit>);
}

TEST(FutureTest, finish) {
  auto x = std::make_shared<int>(0);

  auto [future, p] = yaclib::MakeContract<int>();
  auto f = std::move(future).ThenInline([x](int t) { *x = t; });

  // The callback hasn't executed
  ASSERT_TRUE(0 == *x);

  // The callback has a reference to x
  ASSERT_TRUE(2 == x.use_count());

  std::move(p).Set(42);
  Wait(f);

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
  auto [future, promise] = yaclib::MakeContract<bool>();
  auto p = std::make_unique<yaclib::Promise<bool>>(std::move(promise));
  auto f = std::make_unique<yaclib::Future<bool>>(std::move(future));
  // folly::Baton<> baton;
  std::mutex m;
  std::condition_variable condition;

  std::unique_lock<std::mutex> guard(m);
  std::thread t1([&] {
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
  auto [f, promise] = yaclib::MakeContract<int64_t>();
  auto ptr = std::make_shared<yaclib::Future<int64_t>>(std::move(f));

  std::move(*ptr).DetachInline([ptr](auto&& /* uid */) mutable {
    ASSERT_TRUE(1 == ptr.use_count());

    // Leaving no references to ourselves.
    ptr.reset();
    ASSERT_TRUE(0 == ptr.use_count());
  });

  ASSERT_TRUE(2 == ptr.use_count());

  ptr.reset();

  std::move(promise).Set(1);
}

TEST(FutureTest, Constructor) {
  auto f1 = []() -> yaclib::Future<int> {
    return yaclib::MakeFuture<int>(3);
  }();
  ASSERT_TRUE(std::move(f1).Get().Ok() == 3);
  auto f2 = []() -> yaclib::Future<> { return yaclib::MakeFuture(); }();
  ASSERT_TRUE(std::move(f2).Get().Ok() == yaclib::Unit{});
}

TEST(FutureTest, ImplicitConstructor) {
  auto f1 = []() -> yaclib::Future<int> { return yaclib::MakeFuture(3); }();
  ASSERT_TRUE(std::move(f1).Get().Ok() == 3);
}

TEST(FutureTest, InPlaceConstructor) {
  auto f = yaclib::MakeFuture<std::pair<int, double>>(5, 3.2);
  ASSERT_TRUE(5 == std::move(f).Get().Ok().first);
}

TEST(FutureTest, makeFutureNoThrow) {
  std::ignore = yaclib::MakeFuture().Get().Ok();
}

TEST(FutureTest, invokeCallbackReturningValueAsRvalue) {
  struct Foo {
    int operator()(int x) & noexcept { return x + 1; }
    int operator()(int x) const& noexcept { return x + 2; }
    int operator()(int x) && noexcept { return x + 3; }
  };

  Foo foo;
  Foo const cfoo;

  // The continuation will be forward-constructed - copied if given as & and
  // moved if given as && - everywhere construction is required.
  // The continuation will be invoked with the same cvref as it is passed.
  ASSERT_TRUE(101 == yaclib::MakeFuture<int>(100).ThenInline(foo).Get().Ok());
  ASSERT_TRUE(202 == yaclib::MakeFuture<int>(200).ThenInline(cfoo).Get().Ok());
  ASSERT_TRUE(303 == yaclib::MakeFuture<int>(300).ThenInline(Foo()).Get().Ok());
}

TEST(FutureTest, invokeCallbackReturningFutureAsRvalue) {
  struct Foo {
    yaclib::Future<int> operator()(int x) & {
      return yaclib::MakeFuture(x + 1);
    }
    yaclib::Future<int> operator()(int x) const& {
      return yaclib::MakeFuture(x + 2);
    }
    yaclib::Future<int> operator()(int x) && {
      return yaclib::MakeFuture(x + 3);
    }
  };

  Foo foo;
  Foo const cfoo;

  // The continuation will be forward-constructed - copied if given as & and
  // moved if given as && - everywhere construction is required.
  // The continuation will be invoked with the same cvref as it is passed.
  ASSERT_TRUE(101 == yaclib::MakeFuture<int>(100).ThenInline(foo).Get().Ok());
  ASSERT_TRUE(202 == yaclib::MakeFuture<int>(200).ThenInline(cfoo).Get().Ok());
  ASSERT_TRUE(303 == yaclib::MakeFuture<int>(300).ThenInline(Foo()).Get().Ok());
}

TEST(FutureTest, basic_example) {
  auto [f, p] = yaclib::MakeContract<int>();
  auto f2 = std::move(f).ThenInline(onThenHelperAddOne);
  std::move(p).Set(42);
  ASSERT_TRUE(std::move(f2).Get().Ok() == 43);
}

TEST(FutureTest, basic_example_fpointer) {
  auto [f, p] = yaclib::MakeContract<int>();
  auto f2 = std::move(f).ThenInline(&onThenHelperAddOne);
  std::move(p).Set(42);
  ASSERT_TRUE(std::move(f2).Get().Ok() == 43);
}
