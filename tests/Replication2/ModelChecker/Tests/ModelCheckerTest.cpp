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

template<typename Engine>
struct GTestDriver {
  using State = typename Engine::State;
  using Transition = typename Engine::Transition;
  using Step = typename Engine::Step;

  explicit GTestDriver(State initState) : initState(initState) {}

  template<typename F>
  void onStep(F&& fn) {
    _stepFunction = std::forward<F>(fn);
  }

  auto init() const -> State { return initState; }

  auto expand(const State& state) const
      -> std::vector<std::pair<Transition, State>> {
    return _stepFunction(state);
  }

 private:
  State initState;
  std::function<std::vector<std::pair<Transition, State>>(State const&)>
      _stepFunction;
  std::function<void(State const&)> _validateFunction;
};

template<typename Engine>
struct GTestHelper {
  using Step = typename Engine::Step;
  using State = typename Engine::State;
  using Transition = typename Engine::Transition;

  template<typename F>
  struct Observer : F {
    explicit Observer(F f) : F(std::move(f)) {}
    void finalStep(Step const& step) {}
    void tick() {}
    void initTick() {}
    auto check(Step const& step) -> model_checker::CheckResult {
      test(step);
      if (testing::Test::HasFailure()) {
        dumpStepTrace(step);
        if (testing::Test::HasFatalFailure()) {
          return model_checker::CheckResult::kTerminate;
        }
        return model_checker::CheckResult::kPrune;
      }
      return model_checker::CheckResult::kContinue;
    }

    void test(Step const& step) { F::operator()(step); }

    static void dumpStepTrace(Step const& step) {
      if (!step.parents.empty()) {
        dumpStepTrace(*step.parents[0].second);
        std::cout << "Transition: " << step.parents[0].first << std::endl;
      }
      std::cout << "at step of depth " << step.depth << ": " << std::endl;
      std::cout << "State: " << step.state << std::endl;
    }
  };

  template<typename F>
  struct Driver : F {
    auto expand(State const& state) const
        -> std::vector<std::pair<Transition, State>> {
      static_assert(std::is_invocable_r_v<void, F const, State const&>);
      return F::operator()(state);
    }
  };
};

}  // namespace

using Engine = model_checker::SimulationEngine<MyState, MyTransition>;

TEST_F(ModelCheckerTest, simple_model_test) {
  Engine engine;

  auto driver = GTestHelper<Engine>::Driver{[&](MyState const& state) {
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

  auto observer = GTestHelper<Engine>::Observer{[&](Engine::Step const& step) {
    ASSERT_LE(step.state.x, 10);
    ASSERT_GE(step.state.x, 0);
  }};

  engine.run(driver, observer, {.x = 3});
  std::cout << engine.statistics() << std::endl;
}
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

using ActorEngine = model_checker::ActorEngine<MyState, MyTransition>;

TEST_F(ModelCheckerTest, simple_model_test_actor) {

  GTestDriver<ActorEngine> gtestDriver({});
  model_checker::ActorDriverBuilder<MyState, MyTransition> builder;
  builder.addObserver([&](MyState const& state) {
    ASSERT_LE(state.x, 10);
    ASSERT_GE(state.x, 0);
  });
  builder.addActor(std::make_unique<MyActor>());

  auto driver = builder.make({.x = 0}, &gtestDriver);
  ActorEngine engine;
  engine.run(driver);
  std::cout << engine.statistics() << std::endl;
}
#endif
