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
#pragma once
#include <vector>
#include <iostream>
#include "ModelChecker.h"

namespace arangodb::test::model_checker {

template<typename State, typename Transition>
struct Actor {
  struct InternalState {};

  virtual ~Actor() = default;
  virtual auto clone() const -> std::shared_ptr<Actor> = 0;
  virtual auto expand(State const&, InternalState const&)
      -> std::vector<std::tuple<State, Transition, InternalState>> = 0;
  virtual auto toString() -> std::string { return ""; }
};

template<typename State, typename Transition>
struct ActorModelState {
  State realState;
  std::vector<std::shared_ptr<Actor<State, Transition>>> actors;

  friend auto operator==(ActorModelState const& lhs,
                         ActorModelState const& rhs) noexcept -> bool {
    if (lhs.realState != rhs.realState) {
      return false;
    }
    /*for (std::size_t i = 0; i < lhs.actors.size(); i++) {
      if (*lhs.actors[i] != *rhs.actors[i]) {
        return false;
      }
    }*/
    return true;
  }

  friend auto hash_value(ActorModelState const& s) -> std::size_t {
    std::size_t seed = 0;
    boost::hash_combine(seed, s.realState);
    /*for (auto const& actor : s.actors) {
      boost::hash_combine(seed, *actor);
    }*/
    return seed;
  }

  friend auto operator<<(std::ostream& os, ActorModelState const& state)
      -> std::ostream& {
    os << "State: " << state.realState << std::endl;
    for (auto const& actor : state.actors) {
      os << "Actor: " << actor->toString() << std::endl;
    }
    return os;
  }
};

template<typename State, typename Transition>
using ActorEngine =
    SimulationEngine<ActorModelState<State, Transition>, Transition>;

template<typename State, typename Transition, typename NextDriver>
struct ActorDriver {
  using Engine = ActorEngine<State, Transition>;
  using Step = typename Engine::Step;
  using ActorType = Actor<State, Transition>;
  using StateType = ActorModelState<State, Transition>;

  auto init() const -> StateType { return {initialState, initialActors}; }

  auto check(Step const& step) const -> model_checker::CheckResult {
    CheckResult result = CheckResult::withOk();
    for (auto const& actor : step.state.actors) {
      result = std::max(result, actor->check(step.state.realState));
    }
    if constexpr (!std::is_void_v<NextDriver>) {
      if (_next != nullptr) {
        result = std::max(result, _next->check(step));
      }
    }

    return result;
  }

  using ExpandResult = std::vector<std::pair<Transition, StateType>>;

  auto expand(StateType const& state) const -> ExpandResult {
    auto result = ExpandResult{};

    for (std::size_t i = 0; i < state.actors.size(); i++) {
      expandActor(state, i, result);
    }

    return result;
  }

  static void expandActor(StateType const& inputState, std::size_t index,
                          ExpandResult& result) {
    // clone the actor
    auto outputActors = inputState.actors;
    outputActors[index] = outputActors[index]->clone();
    for (auto& [newState, transition] :
         outputActors[index]->expand(inputState.realState)) {
      result.emplace_back(std::pair<Transition, StateType>{
          std::move(transition), StateType{std::move(newState), outputActors}});
    }
  }

  State initialState;
  std::vector<std::shared_ptr<ActorType>> initialActors;
  NextDriver* _next = nullptr;
};

template<typename State, typename Transition>
struct ActorDriverBuilder {
  using ActorType = Actor<State, Transition>;

  template<typename F>
  struct ObserverActor final : F, ActorType {
    static_assert(std::is_copy_constructible_v<F>);
    ObserverActor(ObserverActor const&) = default;
    ObserverActor& operator=(ObserverActor const&) = default;
    explicit ObserverActor(F f) : F(std::move(f)) {}
    auto check(State const& s) -> CheckResult override {
      static_assert(std::is_invocable_r_v<void, F, State const&>);
      F::operator()(s);
      return CheckResult::withOk();
    }
    auto clone() const -> std::shared_ptr<ActorType> override {
      return std::make_shared<ObserverActor>(*this);
    }
    auto expand(const State& s)
        -> std::vector<std::pair<State, Transition>> override {
      return {};
    }
  };

  template<typename F>
  void addObserver(F&& fn) {
    actors.emplace_back(
        std::make_shared<ObserverActor<F>>(std::forward<F>(fn)));
  }

  void addActor(std::shared_ptr<ActorType> actor) {
    actors.emplace_back(std::move(actor));
  }

  template<typename NextLevel = void>
  auto make(State initialState, NextLevel* nextLevel = nullptr)
      -> ActorDriver<State, Transition, NextLevel> {
    return {initialState, std::move(actors), nextLevel};
  }

 private:
  std::vector<std::shared_ptr<ActorType>> actors;
};

#if 0
template<typename Actor, typename State>
struct ActorSimulationState {
  State realState;
  std::vector<Actor> actors;

  ActorSimulationState(State realState, std::vector<Actor> actors)
      : realState(realState), actors(actors) {}
  friend auto hash_value(ActorSimulationState const& s) -> std::size_t {
    std::size_t seed = 0;
    boost::hash_combine(seed, s.realState);
    boost::hash_combine(seed, s.actors);
    return seed;
  }

  friend auto operator<<(std::ostream& os, ActorSimulationState const& s)
      -> std::ostream& {
    std::cout << "State: " << s.realState << std::endl;
    for (auto const& actor : s.actors) {
      std::cout << "Actor: " << actor << std::endl;
    }
    return os;
  }
};

template<typename Actor, typename State, typename Transition>
struct ActorSimulation
    : Simulation<ActorSimulationState<Actor, State>, Transition> {
  using Step =
      typename Simulation<ActorSimulationState<Actor, State>, Transition>::Step;

 protected:
  ActorSimulation() {}
  auto check(Step const& step) const -> CheckResult override {
    CheckResult result = CheckResult::kContinue;
    for (auto const& actor : step.state.actors) {
      result = std::max(result, actor.check(step));
    }
    return result;
  }

  using ExpandResult =
      std::vector<std::pair<Transition, ActorSimulationState<Actor, State>>>;

  auto expand(ActorSimulationState<Actor, State> const& state) const
      -> ExpandResult override {
    auto result = ExpandResult{};

    for (std::size_t i = 0; i < state.actors.size(); i++) {
      expandActor(state, i, result);
    }

    return result;
  }

  static void expandActor(ActorSimulationState<Actor, State> const& inputState,
                          std::size_t index, ExpandResult& result) {
    // copy the actor
    auto outputActors = inputState.actors;
    for (auto& [transition, newState] :
         outputActors->expand(inputState.realState)) {
      result.emplace_back(std::move(transition),
                          ActorSimulationState<Actor, State>{
                              std::move(newState), outputActors});
    }
  }
};
#endif
}  // namespace arangodb::test::model_checker
