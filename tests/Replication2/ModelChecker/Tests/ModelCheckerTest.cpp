////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include <fmt/core.h>

#include "Replication2/ModelChecker/ModelChecker.h"
#include "Replication2/ModelChecker/ActorModel.h"
#include <boost/container_hash/hash.hpp>

using namespace arangodb::test;

struct ModelCheckerTest : ::testing::Test {};

namespace {
struct MyState {
  int x;

  friend auto hash_value(MyState const& state) -> std::size_t {
    return boost::hash_value(state.x);
  }

  friend auto operator==(MyState const&, MyState const&) noexcept
      -> bool = default;
  friend auto operator<<(std::ostream& os, MyState const& s) -> std::ostream& {
    return os << " x = " << s.x;
  }
};

struct MyTransition {
  int deltaX;

  friend auto operator<<(std::ostream& os, MyTransition const& s)
      -> std::ostream& {
    return os << " delta = " << s.deltaX;
  }
};

template<typename E>
auto max(E const& e) -> E {
  return e;
}
template<typename E, typename F, typename... Gs>
auto max(E const& e, F const& f, Gs const&... gs) -> E {
  return std::max(e, max(f, gs...));
}

}  // namespace

template<const char File[], std::size_t Line>
struct FileLineType {
  static inline constexpr auto filename = File;
  static inline constexpr std::size_t line = Line;

  static auto annotate(std::string_view message) -> std::string {
    return fmt::format("{}:{}: {}", filename, line, message);
  }
};

#define HERE                                 \
  decltype([] {                              \
    static constexpr char data[] = __FILE__; \
    return ::FileLineType<data, __LINE__>{}; \
  }())

namespace arangodb::test::model_checker {

template<typename F>
struct lambda_driver : private F {
  explicit lambda_driver(F&& f) : F(std::forward<F>(f)) {}
  template<typename S>
  auto expand(S const& state) {
    return F::operator()(state);
  }
};

template<typename Location, typename F>
struct gtest_predicate : private F {
  explicit gtest_predicate(Location, F&& f) : F(std::forward<F>(f)) {}
  template<typename S>
  auto check(S const& state) const {
    static_assert(std::is_invocable_r_v<void, F const&, S const&>);
    F::operator()(state);
    if (testing::Test::HasFailure()) {
      return CheckResult::withError(Location::annotate("GTest failed"));
    }
    return CheckResult::withOk();
  }

  template<typename S>
  auto operator()(S const& state) const {
    return isOk(check(state));
  }

  template<typename S>
  auto finalStep(S const& state) {
    return CheckResult::withOk();
  }

  friend auto hash_value(gtest_predicate const&) -> std::size_t { return 0; }
  friend auto operator==(gtest_predicate const&, gtest_predicate const&) {
    return true;
  }
};

template<typename Location, typename F>
struct bool_predicate : private F {
  explicit bool_predicate(Location, F&& f) : F(std::forward<F>(f)) {}
  template<typename S>
  auto check(S const& state) const {
    static_assert(std::is_invocable_r_v<bool, F const&, S const&>);
    if (!F::operator()(state)) {
      return CheckResult::withError(
          Location::annotate("predicate evaluated to false"));
    }
    return CheckResult::withOk();
  }

  template<typename S>
  auto operator()(S const& state) const {
    return isOk(check(state));
  }

  template<typename S>
  auto finalStep(S const& state) {
    return CheckResult::withOk();
  }

  friend auto hash_value(bool_predicate const&) -> std::size_t { return 0; }
  friend auto operator==(bool_predicate const&, bool_predicate const&) {
    return true;
  }
};

template<typename Location, typename F>
struct eventually : private F {
  explicit eventually(Location, F&& f) : F(std::forward<F>(f)) {}
  template<typename S>
  auto check(S const& state) {
    auto result = F::check(state);
    wasTrueOnce = wasTrueOnce || isOk(result);
    return CheckResult::withOk();
  }

  template<typename S>
  auto finalStep(S const& state) {
    std::cout << "wasTrueOnce: " << wasTrueOnce << std::endl;
    if (wasTrueOnce) {
      return CheckResult::withOk();
    } else {
      return CheckResult::withError(
          Location::annotate("Predicate was never fulfilled"));
    }
  }

  bool wasTrueOnce = false;

  friend auto hash_value(eventually const& e) -> std::size_t {
    return boost::hash_value(e.wasTrueOnce);
  }
  friend auto operator==(eventually const& lhs, eventually const& rhs) {
    return lhs.wasTrueOnce == rhs.wasTrueOnce;
  }
};

template<typename Location, typename F>
struct eventually_always : private F {
  explicit eventually_always(Location, F&& f) : F(std::forward<F>(f)) {}

  template<typename S>
  auto check(S const&) {
    return CheckResult::withOk();
  }

  template<typename S>
  auto finalStep(S const& state) {
    static_assert(std::is_invocable_r_v<bool, F const&, S const&>);
    if (F::operator()(state)) {
      return CheckResult::withOk();
    } else {
      return CheckResult::withError(Location::annotate(
          "Predicate did not evaluate to true on final state"));
    }
  }

  friend auto hash_value(eventually_always const&) -> std::size_t { return 0; }
  friend auto operator==(eventually_always const&, eventually_always const&) {
    return true;
  }
};

template<typename F>
struct always : F {
  explicit always(F&& f) : F(std::forward<F>(f)) {}
  template<typename S>
  auto check(S const& state) {
    if (F::operator()(state)) {
      return CheckResult::withOk();
    } else {
      return CheckResult::withError("Predicate was violated");
    }
  }

  template<typename S>
  auto finalStep(S const& state) {
    return CheckResult::withOk();
  }
  friend auto hash_value(always const&) -> std::size_t { return 0; }
  friend auto operator==(always const&, always const&) { return true; }
};

template<std::size_t Idx, typename O>
struct Tagged : O {
  friend auto hash_value(Tagged const& c) noexcept -> std::size_t {
    return hash_value(static_cast<O const&>(c));
  }
  friend auto operator<<(std::ostream& os, Tagged const& b) -> std::ostream& {
    return os << static_cast<O const&>(b);
  }
};

template<typename, typename...>
struct combined_base;
template<std::size_t... Idx, typename... Os>
struct combined_base<std::index_sequence<Idx...>, Os...> : Tagged<Idx, Os>... {
  auto finalStep(auto const& step) {
    // return max(Tagged<Idx, Os>::check(step)...);
  }
  auto check(auto const& step) -> model_checker::CheckResult {
    // return max(Tagged<Idx, Os>::check(step)...);
  }

  friend auto hash_value(combined_base const& c) noexcept -> std::size_t {
    std::size_t seed = 0;
    (boost::hash_combine(seed, static_cast<Tagged<Idx, Os> const&>(c)), ...);
    return seed;
  }
  friend auto operator==(combined_base const&, combined_base const&) noexcept
      -> bool = default;
};

template<typename... Os>
struct combined : combined_base<std::index_sequence_for<Os...>, Os...> {};
template<typename... Os>
combined(Os...) -> combined<Os...>;

}  // namespace arangodb::test::model_checker

#define MC_GTEST_PRED(name, pred)                   \
  model_checker::gtest_predicate {                  \
    HERE{}, [&](Engine::State const& name) { pred } \
  }
#define MC_BOOL_PRED(name, pred)                    \
  model_checker::bool_predicate {                   \
    HERE{}, [&](Engine::State const& name) { pred } \
  }
#define MC_EVENTUALLY(pred) \
  model_checker::eventually { HERE{}, pred }
#define MC_EVENTUALLY_ALWAYS(pred) \
  model_checker::eventually_always { HERE{}, pred }

TEST_F(ModelCheckerTest, simple_model_test) {
  using Engine = model_checker::SimulationEngine<MyState, MyTransition>;

  auto driver = model_checker::lambda_driver{[&](MyState const& state) {
    auto result = std::vector<std::pair<MyTransition, MyState>>{};
    if (state.x < 10) {
      result.emplace_back(MyTransition{.deltaX = 1}, MyState{.x = state.x + 1});
    }
    return result;
  }};

  auto test = MC_GTEST_PRED(state, {
    ASSERT_LE(state.x, 10);
    ASSERT_GE(state.x, 0);
  });

  auto result = Engine::run(driver, test, {.x = 0});
  EXPECT_FALSE(result.failed) << *result.failed;

  auto stats = result.stats;
  EXPECT_EQ(stats.eliminatedStates, 0);
  EXPECT_EQ(stats.discoveredStates, 10);
  EXPECT_EQ(stats.uniqueStates, 10);
  EXPECT_EQ(stats.finalStates, 1);
}

TEST_F(ModelCheckerTest, simple_model_test_eventually) {
  using Engine = model_checker::SimulationEngine<MyState, MyTransition>;
  auto driver = model_checker::lambda_driver{[&](MyState const& state) {
    auto result = std::vector<std::pair<MyTransition, MyState>>{};
    if (state.x < 10) {
      result.emplace_back(MyTransition{.deltaX = 1}, MyState{.x = state.x + 1});
    }
    return result;
  }};

  auto test = MC_EVENTUALLY(MC_BOOL_PRED(state, { return state.x == 5; }));

  auto result = Engine::run(driver, test, {.x = 0});
  EXPECT_FALSE(result.failed) << *result.failed;

  auto stats = result.stats;
  EXPECT_EQ(stats.eliminatedStates, 0);
  EXPECT_EQ(stats.discoveredStates, 10);
  EXPECT_EQ(stats.uniqueStates, 10);
  EXPECT_EQ(stats.finalStates, 1);
}

TEST_F(ModelCheckerTest, simple_model_test_eventually_always) {
  using Engine = model_checker::SimulationEngine<MyState, MyTransition>;
  auto driver = model_checker::lambda_driver{[&](MyState const& state) {
    auto result = std::vector<std::pair<MyTransition, MyState>>{};
    if (state.x < 10) {
      result.emplace_back(MyTransition{.deltaX = 1}, MyState{.x = state.x + 1});
    }
    return result;
  }};

  auto test =
      MC_EVENTUALLY_ALWAYS(MC_BOOL_PRED(state, { return state.x > 5; }));

  auto result = Engine::run(driver, test, {.x = 0});
  EXPECT_FALSE(result.failed) << *result.failed;

  auto stats = result.stats;
  EXPECT_EQ(stats.eliminatedStates, 0);
  EXPECT_EQ(stats.discoveredStates, 10);
  EXPECT_EQ(stats.uniqueStates, 10);
  EXPECT_EQ(stats.finalStates, 1);
}

TEST_F(ModelCheckerTest, simple_model_test_eventually_always_fail) {
  using Engine = model_checker::SimulationEngine<MyState, MyTransition>;
  auto driver = model_checker::lambda_driver{[&](MyState const& state) {
    auto result = std::vector<std::pair<MyTransition, MyState>>{};
    if (state.x < 10) {
      result.emplace_back(MyTransition{.deltaX = 1}, MyState{.x = state.x + 1});
    }
    return result;
  }};

  auto test =
      MC_EVENTUALLY_ALWAYS(MC_BOOL_PRED(state, { return state.x > 11; }));

  auto result = Engine::run(driver, test, {.x = 0});
  EXPECT_TRUE(result.failed);
}

using ActorEngine = model_checker::ActorEngine<MyState, MyTransition>;

TEST_F(ModelCheckerTest, simple_model_test_actor) {
  auto driver = ActorDriver{
      SupervisionDriver{},
      DBServerActor{"A"},
      DBServerActor{"B"},
      DBServerActor{"C"},
  };

  auto driver = ActorDriver.addActor<ActorType>("D").addActor<ActorType2>();

  auto driver = builder.make({.x = 0}, &gtestDriver);
  ActorEngine engine;
  Engine::run(driver);
  std::cout << engine.statistics() << std::endl;
}

/*
TEST_F(ModelCheckerTest, simple_model_test_with_loops) {
  using Engine = model_checker::SimulationEngine<MyState, MyTransition>;

  auto driver = model_checker::lambda_driver{[&](MyState const& state) {
    auto result = std::vector<std::pair<MyTransition, MyState>>{};
    if (state.x < 10) {
      result.emplace_back(MyTransition{.deltaX = 1}, MyState{.x = state.x + 1});
    }
    if (state.x > 0) {
      result.emplace_back(MyTransition{.deltaX = -1},
                          MyState{.x = state.x - 1});
    }
    return result;
  }};

  auto observer =
      model_checker::gtest_predicate{[&](Engine::State const& state) {
        ASSERT_LE(state.x, 10);
        ASSERT_GE(state.x, 0);
      }};

  auto result = Engine::run(driver, observer, {.x = 3});
  auto stats = result.stats;
  EXPECT_EQ(stats.eliminatedStates, 10);
  EXPECT_EQ(stats.discoveredStates, 20);
  EXPECT_EQ(stats.uniqueStates, 10);
  EXPECT_EQ(stats.finalStates, 0);
}*/

#if 0
namespace {
struct MyActor : model_checker::Actor<MyState, MyTransition> {
  auto clone() const -> std::shared_ptr<Actor> override {
    return std::make_shared<MyActor>(*this);
  }
  auto check(const MyState&) -> model_checker::CheckResult override {
    return model_checker::CheckResult::kContinue;
  }
  auto expand(const MyState& state)
      -> std::vector<std::pair<MyState, MyTransition>> override {
    auto result = std::vector<std::pair<MyState, MyTransition>>{};
    if (state.x < 10) {
      result.emplace_back(MyState{.x = state.x + 1}, MyTransition{.deltaX = 1});
    }
    return result;
  }
};
}  // namespace

#endif
