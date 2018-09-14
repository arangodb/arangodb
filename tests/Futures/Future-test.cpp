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

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/GreetingsPhase.h"
#include "Futures/Future.h"
#include "Futures/Utilities.h"
#include "RestServer/FileDescriptorsFeature.h"
#include "Scheduler/SchedulerFeature.h"
#include "Scheduler/Scheduler.h"

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
    REQUIRE_FALSE(invalid.valid());
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
  
  struct SchedulerTestSetup {
    arangodb::application_features::ApplicationServer server;
    
    SchedulerTestSetup() : server(nullptr, nullptr) {
      using namespace arangodb::application_features;
      std::vector<ApplicationFeature*> features;
      
      features.emplace_back(new GreetingsFeaturePhase(server, false));
      features.emplace_back(new arangodb::FileDescriptorsFeature(server));
      features.emplace_back(new arangodb::SchedulerFeature(server));
      
      for (auto& f : features) {
        ApplicationServer::server->addFeature(f);
      }
      ApplicationServer::server->setupDependencies(false);
      
      ApplicationServer::setStateUnsafe(ServerState::IN_WAIT);
      
      auto orderedFeatures = server.getOrderedFeatures();
      for (auto& f : orderedFeatures) {
        f->prepare();
      }
      
      for (auto& f : orderedFeatures) {
        f->start();
      }
      
    }
    
    ~SchedulerTestSetup() {
      using namespace arangodb::application_features;
      ApplicationServer::setStateUnsafe(ServerState::IN_STOP);
      
      auto orderedFeatures = server.getOrderedFeatures();
      for (auto& f : orderedFeatures) {
        f->beginShutdown();
      }
      for (auto& f : orderedFeatures) {
        f->stop();
      }
      for (auto& f : orderedFeatures) {
        f->unprepare();
      }
      
      arangodb::application_features::ApplicationServer::server = nullptr;
    }
    
    std::vector<std::unique_ptr<arangodb::application_features::ApplicationFeature*>> features;
  };
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
  Future<Unit> abc{};
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
  
SECTION("ctor post condition invalid") {
#define DOIT(CREATION_EXPR)    \
do {                         \
auto f1 = (CREATION_EXPR); \
REQUIRE_FALSE(f1.valid());  \
auto f2 = std::move(f1);   \
REQUIRE_FALSE(f1.valid());  \
REQUIRE_FALSE(f2.valid());  \
} while (false)
  
  DOIT(makeInvalid());
  DOIT(Future<int>::makeEmpty());
  
#undef DOIT
}
  
SECTION("lacksPreconditionValid") {
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
  
SECTION("hasPreconditionValid") {
    // Ops that require validity; precondition: valid();
    // throw FutureInvalid if !valid()
  
#define DOIT(STMT)                     \
do {                                 \
auto f = makeValid();              \
STMT;             \
::copy(std::move(f));                \
REQUIRE_THROWS(STMT); \
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
    DOIT(std::move(f).thenValue([](int&&) -> void {}));
    DOIT(std::move(f).thenValue([](auto&&) -> void {}));
  
#undef DOIT
}
  
SECTION("hasPostconditionValid") {
    // Ops that preserve validity -- postcondition: valid()
    
#define DOIT(STMT)          \
do {                      \
auto f = makeValid();   \
REQUIRE_NOTHROW(STMT);  \
REQUIRE(f.valid()); \
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
  
SECTION("hasPostconditionInvalid") {
    // Ops that consume *this -- postcondition: !valid()
    
#define DOIT(CTOR, STMT)     \
do {                       \
auto f = (CTOR);         \
STMT;   \
REQUIRE_FALSE(f.valid()); \
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
 
SECTION("thenError") {
    bool theFlag = false;
    auto flag = [&] { theFlag = true; };
#define EXPECT_FLAG()     \
do {              \
f.wait();         \
REQUIRE(theFlag); \
theFlag = false;      \
} while (0);
  
#define EXPECT_NO_FLAG()   \
do {                     \
REQUIRE_FALSE(theFlag); \
theFlag = false;       \
} while (0);
  
    // By reference
    {
      auto f = makeFuture()
      .thenValue([](Unit){ throw std::logic_error("abc"); })
      .thenError<std::logic_error>([&](const std::logic_error& /* e */) { flag(); });
      EXPECT_FLAG();
      REQUIRE_NOTHROW(f.get());
    }
  
    // By auto reference
    {
      auto f = makeFuture()
      .thenValue([](Unit){ throw eggs; })
      .thenError<eggs_t>([&](auto const& /* e */) { flag(); });
      EXPECT_FLAG();
      REQUIRE_NOTHROW(f.get());
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
      REQUIRE_NOTHROW(f.get());
    }

    // By value
    {
      auto f = makeFuture()
      .thenValue([](Unit){ throw eggs; })
      .thenError<eggs_t>([&](eggs_t /* e */) {
        flag();
      });
      EXPECT_FLAG();
      REQUIRE_NOTHROW(f.get());
    }

    {
      auto f = makeFuture()
      .thenValue([](Unit) { throw eggs; }).thenError<eggs_t>([&](eggs_t /* e */) {
        flag();
        return makeFuture();
      });
      EXPECT_FLAG();
      REQUIRE_NOTHROW(f.get());
    }

//    // Polymorphic
//    {
//      auto f = makeFuture()
//      .thenValue([] { throw eggs; })
//      .thenError([&](std::exception& /* e */) { flag(); });
//      EXPECT_FLAG();
//      REQUIRE_NOTHROW(f.value());
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
//      REQUIRE_NOTHROW(f.value());
//    }
//
    // Non-exceptions
    {
      auto f = makeFuture().thenValue([](Unit){ throw - 1; }).thenError<int>([&](int /* e */) {
        flag();
      });
      EXPECT_FLAG();
      REQUIRE_NOTHROW(f.get());
    }

    {
      auto f = makeFuture().thenValue([](Unit){ throw - 1; }).thenError<int>([&](int /* e */) {
        flag();
        return makeFuture();
      });
      EXPECT_FLAG();
      REQUIRE_NOTHROW(f.get());
    }

    // Mutable lambda
    {
      auto f = makeFuture()
      .thenValue([](Unit){ throw eggs; })
      .thenError<eggs_t&>([&](eggs_t& /* e */) mutable { flag(); });
      EXPECT_FLAG();
      REQUIRE_NOTHROW(f.get());
    }

    {
      auto f = makeFuture()
      .thenValue([](Unit){ throw eggs; })
      .thenError<eggs_t&>([&](eggs_t& /* e */) mutable {
        flag();
        return makeFuture();
      });
      EXPECT_FLAG();
      REQUIRE_NOTHROW(f.get());
    }

    // Function pointer
    {
      auto f = makeFuture()
      .thenValue([](Unit) -> int { throw eggs; })
      .thenError<const eggs_t&>(&onErrorHelperEggs)
      .thenError<std::exception const&>(&onErrorHelperGeneric);
      REQUIRE(10 == f.get());
    }
    {
      auto f = makeFuture()
      .thenValue([](Unit) -> int { throw std::runtime_error("test"); })
      .thenError<const eggs_t&>(&onErrorHelperEggs)
      .thenError<std::exception>(&onErrorHelperGeneric);
      REQUIRE(20 == f.get());
    }
    {
      auto f = makeFuture()
      .thenValue([](Unit) -> int { throw std::runtime_error("test"); })
      .thenError<const eggs_t&>(&onErrorHelperEggs);
      REQUIRE_THROWS_AS(f.get(), std::runtime_error);
    }

    // No throw
    {
      auto f = makeFuture().thenValue([](Unit) { return 42; }).thenError<eggs_t&>([&](eggs_t& /* e */) {
        flag();
        return -1;
      });
      EXPECT_NO_FLAG();
      REQUIRE(42 == f.get());
    }

    {
      auto f = makeFuture().thenValue([](Unit){ return 42; }).thenError<eggs_t&>([&](eggs_t& /* e */) {
        flag();
        return makeFuture<int>(-1);
      });
      EXPECT_NO_FLAG();
      REQUIRE(42 == f.get());
    }

    // Catch different exception
    {
      auto f = makeFuture()
      .thenValue([](Unit) { throw eggs; })
      .thenError<std::runtime_error&>([&](std::runtime_error& /* e */) { flag(); });
      EXPECT_NO_FLAG();
      REQUIRE_THROWS_AS(f.get(), eggs_t);
    }

    {
      auto f = makeFuture()
      .thenValue([](Unit) { throw eggs; })
      .thenError<std::runtime_error&>([&](std::runtime_error& /* e */) {
        flag();
        return makeFuture();
      });
      EXPECT_NO_FLAG();
      REQUIRE_THROWS_AS(f.get(), eggs_t);
    }

    // Returned value propagates
    {
      auto f = makeFuture()
      .thenValue([](Unit) -> int { throw eggs; })
      .thenError<eggs_t&>([&](eggs_t& /* e */) { return 42; });
      REQUIRE(42 == f.get());
    }

    // Returned future propagates
    {
      auto f = makeFuture()
      .thenValue([](Unit) -> int { throw eggs; })
      .thenError<eggs_t&>([&](eggs_t& /* e */) { return makeFuture<int>(42); });
      REQUIRE(42 == f.get());
    }

    // Throw in callback
    {
      auto f = makeFuture()
      .thenValue([](Unit) -> int { throw eggs; })
      .thenError<eggs_t&>([&](eggs_t& e) -> int { throw e; });
      REQUIRE_THROWS_AS(f.get(), eggs_t);
    }

    {
      auto f = makeFuture()
      .thenValue([](Unit) -> int { throw eggs; })
      .thenError<eggs_t&>([&](eggs_t& e) -> Future<int> { throw e; });
      REQUIRE_THROWS_AS(f.get(), eggs_t);
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
//      REQUIRE_NOTHROW(f.value());
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
//      REQUIRE_THROWS_AS(f.value(), eggs_t);
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
//      REQUIRE(-1 == f.value());
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
//      REQUIRE_THROWS_AS(f.value(), eggs_t);
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
//      REQUIRE_NOTHROW(f.value());
//    }
#undef EXPECT_FLAG
#undef EXPECT_NO_FLAG
  }
  
SECTION("special") {
  REQUIRE_FALSE(std::is_copy_constructible<Future<int>>::value);
  REQUIRE_FALSE(std::is_copy_assignable<Future<int>>::value);
  REQUIRE(std::is_move_constructible<Future<int>>::value);
  REQUIRE(std::is_move_assignable<Future<int>>::value);
}

SECTION("then") {
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
  REQUIRE(value == "1;2;3;4;5;6;7;8;9;10;11");
}
  
SECTION("then static functions") {
  auto f = makeFuture<int>(10).thenValue(&onThenHelperAddFive);
  REQUIRE(f.get() == 15);
  
  auto f2 = makeFuture<int>(15).thenValue(&onThenHelperAddFutureFive);
  REQUIRE(f2.get() == 20);
}
  
SECTION("get") {
  auto f = makeFuture(std::make_unique<int>(42));
  auto up = std::move(f).get();
  REQUIRE(42 == *up);
  
  REQUIRE_THROWS_AS(makeFuture<int>(eggs).get(), eggs_t);
}
  
SECTION("isReady") {
  Promise<int> p;
  auto f = p.getFuture();
  REQUIRE_FALSE(f.isReady());
  p.setValue(42);
  REQUIRE(f.isReady());
}
  
SECTION("futureNotReady") {
  Promise<int> p;
  Future<int> f = p.getFuture();
  REQUIRE_THROWS_AS(f.result().get(), FutureException);
}
  
SECTION("makeFuture") {
  REQUIRE(makeFuture<int>(eggs).getTry().hasException());
  REQUIRE_FALSE(makeFuture(42).getTry().hasException());
}
  
SECTION("hasValue") {
  REQUIRE(makeFuture(42).getTry().hasValue());
  REQUIRE_FALSE(makeFuture<int>(eggs).getTry().hasValue());
}
  
SECTION("makeFuture") {
  //EXPECT_TYPE(makeFuture(42), Future<int>);
  REQUIRE(42 == makeFuture(42).get());
  
  //EXPECT_TYPE(makeFuture<float>(42), Future<float>);
  REQUIRE(42 == makeFuture<float>(42).get());
  
  auto fun = [] { return 42; };
  //EXPECT_TYPE(makeFutureWith(fun), Future<int>);
  REQUIRE(42 == makeFutureWith(fun).get());
  
  auto funf = [] { return makeFuture<int>(43); };
  //EXPECT_TYPE(makeFutureWith(funf), Future<int>);
  REQUIRE(43 == makeFutureWith(funf).get());
  
  auto failfun = []() -> int { throw eggs; };
  //EXPECT_TYPE(makeFutureWith(failfun), Future<int>);
  REQUIRE_NOTHROW(makeFutureWith(failfun));
  REQUIRE_THROWS_AS(makeFutureWith(failfun).get(), eggs_t);
  
  auto failfunf = []() -> Future<int> { throw eggs; };
  //EXPECT_TYPE(makeFutureWith(failfunf), Future<int>);
  REQUIRE_NOTHROW(makeFutureWith(failfunf));
  REQUIRE_THROWS_AS(makeFutureWith(failfunf).get(), eggs_t);
  
  //EXPECT_TYPE(makeFuture(), Future<Unit>);
}
  
SECTION("finish") {
  auto x = std::make_shared<int>(0);
  
  Promise<int> p;
  auto f = p.getFuture().then([x](Try<int>&& t) { *x = t.get(); });
  
  // The callback hasn't executed
  REQUIRE(0 == *x);
  
  // The callback has a reference to x
  REQUIRE(2 == x.use_count());
  
  p.setValue(42);
  f.wait();
  
  // the callback has executed
  REQUIRE(42 == *x);
  
  std::this_thread::yield();
  
  // the callback has been destructed
  // and has released its reference to x
  REQUIRE(1 == x.use_count());
}
  
SECTION("detachRace") {
  // Task #5438209
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
    std::lock_guard<std::mutex> guard(m);
    condition.notify_one();
    p.reset();
  });
  condition.wait(guard);
  f.reset();
  t1.join();
}

  // Test of handling of a circular dependency. It's never recommended
  // to have one because of possible memory leaks. Here we test that
  // we can handle freeing of the Future while it is running.
SECTION("CircularDependencySharedPtrSelfReset") {
  Promise<int64_t> promise;
  auto ptr = std::make_shared<Future<int64_t>>(promise.getFuture());

  std::move(*ptr).then([ptr](Try<int64_t>&& /* uid */) mutable {
    REQUIRE(1 == ptr.use_count());

    // Leaving no references to ourselves.
    ptr.reset();
    REQUIRE(0 == ptr.use_count());
  });

  REQUIRE(2 == ptr.use_count());

  ptr.reset();

  promise.setValue(1);
}

SECTION("Constructor") {
  auto f1 = []() -> Future<int> { return Future<int>(3); }();
  REQUIRE(f1.get() == 3);
  auto f2 = []() -> Future<Unit> { return Future<Unit>(); }();
  REQUIRE_NOTHROW(f2.getTry());
}

SECTION("ImplicitConstructor") {
  auto f1 = []() -> Future<int> { return 3; }();
  REQUIRE(f1.get() == 3);
  // Unfortunately, the C++ standard does not allow the
  // following implicit conversion to work:
  //auto f2 = []() -> Future<Unit> { }();
}

SECTION("InPlaceConstructor") {
  auto f = Future<std::pair<int, double>>(in_place, 5, 3.2);
  REQUIRE(5 == f.get().first);
}

SECTION("makeFutureNoThrow") {
  makeFuture().get();
}

SECTION("invokeCallbackReturningValueAsRvalue") {
  struct Foo {
    int operator()(int x) & {
      return x + 1;
    }
    int operator()(int x) const& {
      return x + 2;
    }
    int operator()(int x) && {
      return x + 3;
    }
  };

  Foo foo;
  Foo const cfoo;

  // The continuation will be forward-constructed - copied if given as & and
  // moved if given as && - everywhere construction is required.
  // The continuation will be invoked with the same cvref as it is passed.
  REQUIRE(101 == makeFuture<int>(100).thenValue(foo).get());
  REQUIRE(202 == makeFuture<int>(200).thenValue(cfoo).get());
  REQUIRE(303 == makeFuture<int>(300).thenValue(Foo()).get());
}

SECTION("invokeCallbackReturningFutureAsRvalue") {
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
  REQUIRE(101 == makeFuture<int>(100).thenValue(foo).get());
  REQUIRE(202 == makeFuture<int>(200).thenValue(cfoo).get());
  REQUIRE(303 == makeFuture<int>(300).thenValue(Foo()).get());
}
  
SECTION("Basic Example") {
  Promise<int> p;
  Future<int> f = p.getFuture();
  auto f2 = std::move(f).thenValue(&onThenHelperAddOne);
  p.setValue(42);
  REQUIRE(f2.get() == 43);
}

}
