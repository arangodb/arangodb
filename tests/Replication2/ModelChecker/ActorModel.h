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

template<typename State, typename Transition, typename... Actors>
struct GlobalActorState {
  State state;
  std::tuple<typename Actors::InternalState...> actors;

  friend auto hash_value(GlobalActorState const& g) noexcept -> std::size_t {
    std::size_t seed = 0;
    boost::hash_combine(seed, g.state);
    boost::hash_combine(seed, g.actors);
    return seed;
  }

  friend auto operator==(GlobalActorState const&,
                         GlobalActorState const&) noexcept -> bool = default;

  friend auto operator<<(std::ostream& os, GlobalActorState const& s)
      -> std::ostream& {
    return s.printActorStates(std::index_sequence_for<Actors...>{}, os);
  }

 private:
  template<std::size_t... Idxs>
  auto printActorStates(std::index_sequence<Idxs...>, std::ostream& os) const
      -> std::ostream& {
    os << "[" << state;
    return ((os << ", {" << std::get<Idxs>(actors) << "}"), ...) << "]";
  }
};

template<typename... Actors>
struct ActorDriver {
  template<typename State, typename Transition, typename... InternalStates>
  auto expand(GlobalActorState<State, Transition, InternalStates...> const&
                  globalState) {
    auto result = std::vector<std::pair<
        Transition, GlobalActorState<State, Transition, InternalStates...>>>{};
    expandEach(std::index_sequence_for<InternalStates...>{}, globalState,
               result);
    return result;
  }

  template<std::size_t... Idxs, typename State, typename Vector>
  auto expandEach(std::index_sequence<Idxs...>, State const& state, Vector& v) {
    (expandIdx<Idxs>(state, v), ...);
  }

  template<std::size_t Idx, typename State, typename Vector>
  auto expandIdx(State const& globalState, Vector& v) {
    auto& actor = std::get<Idx>(actors);
    auto const& internalState = std::get<Idx>(globalState.actors);
    auto result = actor.expand(globalState.state, internalState);
    v.reserve(result.size());

    for (auto& [transition, state, internal] : result) {
      auto newActors = globalState.actors;
      std::get<Idx>(newActors) = internal;
      v.emplace_back(std::move(transition),
                     State{std::move(state), std::move(newActors)});
    }
  }

  template<typename Transition, typename State>
  auto initialState(State state)
      -> GlobalActorState<State, Transition, Actors...> {
    return {std::move(state), {}};
  }

  explicit ActorDriver(Actors&&... as) : actors(std::forward<Actors>(as)...) {}

  std::tuple<Actors...> actors;
};

template<typename State, typename Transition>
struct ActorEngine {
  template<typename... Actors, typename Observer>
  static auto run(ActorDriver<Actors...>& driver, Observer&& observer,
                  State initState) {
    using GlobalState = GlobalActorState<State, Transition, Actors...>;
    using BaseEngine = SimulationEngine<GlobalState, Transition>;

    return BaseEngine::run(
        driver, std::forward<Observer>(observer),
        driver.template initialState<Transition>(std::move(initState)));
  }
};

}  // namespace arangodb::test::model_checker
