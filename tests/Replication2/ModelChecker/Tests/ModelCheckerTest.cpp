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
#include "Replication2/ModelChecker/Predicates.h"
#include <boost/container_hash/hash.hpp>

using namespace arangodb::test;

template<typename Engine>
struct TypedModelCheckerTest : ::testing::Test,
                               model_checker::testing::TracedSeedGenerator {};
TYPED_TEST_CASE_P(TypedModelCheckerTest);

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

}  // namespace

TYPED_TEST_P(TypedModelCheckerTest, simple_model_test) {
  using Engine = model_checker::DFSEngine<MyState, MyTransition>;

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

  auto result = Engine::run(driver, test, {.x = 0},
                            {.iterations = 3, .seed = this->seed(ADB_HERE)});
  EXPECT_FALSE(result.failed) << *result.failed;

  auto stats = result.stats;
  EXPECT_EQ(stats.eliminatedStates, 0);
  EXPECT_EQ(stats.discoveredStates, 10);
  EXPECT_EQ(stats.uniqueStates, 10);
  EXPECT_EQ(stats.finalStates, 1);
}

TYPED_TEST_P(TypedModelCheckerTest, simple_model_test_eventually) {
  using Engine = TypeParam;
  auto driver = model_checker::lambda_driver{[&](MyState const& state) {
    auto result = std::vector<std::pair<MyTransition, MyState>>{};
    if (state.x < 10) {
      result.emplace_back(MyTransition{.deltaX = 1}, MyState{.x = state.x + 1});
    }
    return result;
  }};

  auto test = MC_EVENTUALLY(MC_BOOL_PRED(state, { return state.x == 5; }));

  auto result = Engine::run(driver, test, {.x = 0},
                            {.iterations = 3, .seed = this->seed(ADB_HERE)});
  EXPECT_FALSE(result.failed) << *result.failed;

  // RandomEngine doesn't have stats
  if constexpr (!std::is_same_v<Engine, model_checker::RandomEngine<
                                            MyState, MyTransition>>) {
    auto stats = result.stats;
    EXPECT_EQ(stats.eliminatedStates, 0);
    EXPECT_EQ(stats.discoveredStates, 10);
    EXPECT_EQ(stats.uniqueStates, 10);
    EXPECT_EQ(stats.finalStates, 1);
  }
}

TYPED_TEST_P(TypedModelCheckerTest, simple_model_test_eventually_always) {
  using Engine = TypeParam;
  auto driver = model_checker::lambda_driver{[&](MyState const& state) {
    auto result = std::vector<std::pair<MyTransition, MyState>>{};
    if (state.x < 10) {
      result.emplace_back(MyTransition{.deltaX = 1}, MyState{.x = state.x + 1});
    }
    return result;
  }};

  auto test =
      MC_EVENTUALLY_ALWAYS(MC_BOOL_PRED(state, { return state.x > 5; }));

  auto result = Engine::run(driver, test, {.x = 0},
                            {.iterations = 3, .seed = this->seed(ADB_HERE)});
  EXPECT_FALSE(result.failed) << *result.failed;

  // RandomEngine doesn't have stats
  if constexpr (!std::is_same_v<Engine, model_checker::RandomEngine<
                                            MyState, MyTransition>>) {
    auto stats = result.stats;
    EXPECT_EQ(stats.eliminatedStates, 0);
    EXPECT_EQ(stats.discoveredStates, 10);
    EXPECT_EQ(stats.uniqueStates, 10);
    EXPECT_EQ(stats.finalStates, 1);
  }
}

TYPED_TEST_P(TypedModelCheckerTest, simple_model_test_eventually_always_fail) {
  using Engine = TypeParam;
  auto driver = model_checker::lambda_driver{[&](MyState const& state) {
    auto result = std::vector<std::pair<MyTransition, MyState>>{};
    if (state.x < 10) {
      result.emplace_back(MyTransition{.deltaX = 1}, MyState{.x = state.x + 1});
    }
    return result;
  }};

  auto test =
      MC_EVENTUALLY_ALWAYS(MC_BOOL_PRED(state, { return state.x > 11; }));

  auto result = Engine::run(driver, test, {.x = 0},
                            {.iterations = 3, .seed = this->seed(ADB_HERE)});
  EXPECT_TRUE(result.failed);
}

TYPED_TEST_P(TypedModelCheckerTest, simple_model_test_cycle_detector) {
  using Engine = TypeParam;
  auto driver = model_checker::lambda_driver{[&](MyState const& state) {
    auto result = std::vector<std::pair<MyTransition, MyState>>{};
    if (state.x < 10) {
      result.emplace_back(MyTransition{.deltaX = 1},
                          MyState{.x = (state.x % 5) + 1});
    }
    return result;
  }};

  auto test =
      MC_EVENTUALLY_ALWAYS(MC_BOOL_PRED(state, { return state.x > 11; }));

  auto result = Engine::run(driver, test, {.x = 0},
                            {.iterations = 3, .seed = this->seed(ADB_HERE)});
  EXPECT_TRUE(result.failed);
}

REGISTER_TYPED_TEST_CASE_P(TypedModelCheckerTest, simple_model_test,
                           simple_model_test_eventually,
                           simple_model_test_eventually_always,
                           simple_model_test_eventually_always_fail,
                           simple_model_test_cycle_detector);

using EngineTypes =
    ::testing::Types<model_checker::DFSEngine<MyState, MyTransition>,
                     model_checker::RandomEngine<MyState, MyTransition>>;
INSTANTIATE_TYPED_TEST_CASE_P(ModelCheckerTestInstant, TypedModelCheckerTest,
                              EngineTypes);

struct EmptyInternalState {
  friend auto hash_value(EmptyInternalState const&) noexcept -> std::size_t {
    return 0;
  }
  friend auto operator==(EmptyInternalState const&,
                         EmptyInternalState const&) noexcept -> bool = default;

  friend auto operator<<(std::ostream& os, EmptyInternalState const&)
      -> std::ostream& {
    return os;
  }
};

namespace {
struct DecrementActor {
  struct InternalState {
    int tries = 3;

    friend auto hash_value(InternalState const& s) noexcept -> std::size_t {
      return s.tries;
    }
    friend auto operator==(InternalState const&, InternalState const&) noexcept
        -> bool = default;
    friend auto operator<<(std::ostream& os, InternalState const& s)
        -> std::ostream& {
      return os << "tries = " << s.tries;
    }
  };

  auto expand(MyState const& state, InternalState const& internalState)
      -> std::vector<std::tuple<MyTransition, MyState, InternalState>> {
    if (state.x > 0 and internalState.tries > 0) {
      return {{MyTransition{.deltaX = -1}, MyState{.x = 0},
               InternalState{.tries = internalState.tries - 1}}};
    }

    return {};
  }
};

struct IncrementActor {
  using InternalState = EmptyInternalState;
  auto expand(MyState const& state, InternalState const& internalState)
      -> std::vector<std::tuple<MyTransition, MyState, InternalState>> {
    if (state.x < 10) {
      return {{MyTransition{.deltaX = +1}, MyState{.x = state.x + 1},
               InternalState{}}};
    }

    return {};
  }
};
}  // namespace

struct ModelCheckerTest : ::testing::Test,
                          model_checker::testing::TracedSeedGenerator {};

TEST_F(ModelCheckerTest, simple_model_test_actor) {
  auto driver = model_checker::ActorDriver{
      DecrementActor{},
      IncrementActor{},
  };

  auto test = MC_EVENTUALLY_ALWAYS(
      MC_BOOL_PRED(global, { return global.state.x > 2; }));
  using Engine = model_checker::ActorEngine<model_checker::DFSEnumerator,
                                            MyState, MyTransition>;

  auto result = Engine::run(driver, test, {.x = 0},
                            {.iterations = 3, .seed = this->seed(ADB_HERE)});
  EXPECT_FALSE(result.failed) << *result.failed;
}
