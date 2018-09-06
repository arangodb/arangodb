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
  
  struct FuturesTestSetup {
    arangodb::application_features::ApplicationServer server;
    
    FuturesTestSetup() : server(nullptr, nullptr) {
      std::vector<arangodb::application_features::ApplicationFeature*> features;
      
      features.emplace_back(new arangodb::application_features::GreetingsFeaturePhase(server, false));
      features.emplace_back(new arangodb::FileDescriptorsFeature(server));
      features.emplace_back(new arangodb::SchedulerFeature(server));
      
      for (auto& f : features) {
        arangodb::application_features::ApplicationServer::server->addFeature(f);
      }
      arangodb::application_features::ApplicationServer::server->setupDependencies(false);
      
      auto orderedFeatures = server.getOrderedFeatures();
      for (auto& f : orderedFeatures) {
        f->prepare();
      }
      for (auto& f : orderedFeatures) {
        f->start();
      }
      
      arangodb::application_features::ApplicationServer::setPreparedUnsafe()
    }
    
    ~FuturesTestSetup() {
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
  
  DOIT(makeValid());
  DOIT(Future<int>(42));
  DOIT(Future<int>{42});
  // I did not include Follys Unit type
  //DOIT(Future<Unit>());
  //DOIT(Future<Unit>{});
  //DOIT(makeFuture());
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
  
  FuturesTestSetup setup;
  
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
    DOIT(std::move(f).then([](auto&&) -> void {}));
    
#undef DOIT
  }
  
/*SECTION("hasPostconditionValid") {
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
    DOIT(swallow(f.value()));
    DOIT(swallow(f.getTry()));
    DOIT(swallow(f.poll()));
    DOIT(f.raise(std::logic_error("foo")));
    DOIT(f.cancel());
    DOIT(swallow(f.getTry()));
    DOIT(f.wait());
    DOIT(std::move(f.wait()));
    
#undef DOIT
  }
  
SECTION("hasPostconditionInvalid") {
    // Ops that consume *this -- postcondition: !valid()
    
#define DOIT(CTOR, STMT)     \
do {                       \
auto f = (CTOR);         \
REQUIRE_NOTHROW(STMT);   \
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
    DOIT(makeValid(), swallow(std::move(f).wait()));
    DOIT(makeValid(), swallow(std::move(f.wait())));
    DOIT(makeValid(), swallow(std::move(f).get()));
    DOIT(makeValid(), swallow(std::move(f).get(std::chrono::milliseconds(10))));
    DOIT(makeValid(), swallow(std::move(f).semi()));
    
#undef DOIT
}*/
  
}
