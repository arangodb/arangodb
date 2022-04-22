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
#include <memory>
#include <chrono>
#include <vector>
#include <tuple>
#include <optional>
#include <unordered_set>
#include <deque>
#include <iostream>

#include <boost/container_hash/hash_fwd.hpp>

namespace arangodb::test::model_checker {

enum class CheckResult {
  kContinue,
  kPrune,
  kTerminate,
};

struct Stats {
  std::size_t uniqueStates;
  std::size_t eliminatedStates;
  std::size_t discoveredStates;
};

auto operator<<(std::ostream& os, Stats const& stats) -> std::ostream&;

template<typename StateType, typename TransitionType,
         typename StateHash = boost::hash<StateType>,
         typename StateCompare = std::equal_to<>>
struct SimulationEngine {
  using State = StateType;
  using Transition = TransitionType;

  virtual ~SimulationEngine() = default;

  template<typename Driver, typename Observer>
  void run(Driver& driver, Observer& observer, State initialState) {
    {
      auto step = registerFingerprint(std::move(initialState));
      activeSteps.push_back(std::move(step.second));
    }
    observer.initTick();
    while (not activeSteps.empty()) {
      auto nextStep = std::move(activeSteps.front());
      activeSteps.pop_front();
      auto newStates = driver.expand(nextStep->state);
      if (newStates.empty()) {
        observer.finalStep(*nextStep);
      }
      for (auto const& [transition, state] : newStates) {
        stats.discoveredStates += 1;
        // fingerprint this state
        auto [wasNewState, step] = registerFingerprint(std::move(state));
        // add previous step information
        step->registerPreviousStep(nextStep, std::move(transition));
        step->depth = nextStep->depth + 1;
        // TODO loop detection?
        if (!wasNewState) {
          stats.eliminatedStates += 1;
          continue;  // ignore
        }

        stats.uniqueStates += 1;

        // run the check function for this step
        auto checkResult = observer.check(*step);
        if (checkResult == CheckResult::kTerminate) {
          return;
        } else if (checkResult == CheckResult::kPrune) {
          continue;
        }

        // put step into active steps
        activeSteps.push_back(step);
      }

      observer.tick();
    }
  }

  auto statistics() const noexcept -> Stats { return stats; }

  void printAllStates(std::ostream& os) {
    std::size_t idx = 0;
    os << "digraph foobar {" << std::endl;
    for (auto const& s : fingerprints) {
      os << "v" << s->_uniqueId;
      if (s->_uniqueId == 0) {
        os << "[label=\"initial\"]";
      }
      os << ";" << std::endl;
    }

    for (auto const& s : fingerprints) {
      for (auto const& [p, action] : s->_previous) {
        os << "v" << p->_uniqueId << " -> v" << s->_uniqueId << "[label=\""
           << action->toString() << "\"];" << std::endl;
      }
    }

    os << "}" << std::endl;
  }

  struct Step {
    std::size_t depth{0}, uniqueId{0};
    State state;
    std::vector<std::pair<Transition, std::shared_ptr<Step const>>> parents;
    std::optional<std::size_t> hash;

    void registerPreviousStep(std::shared_ptr<Step const> step,
                              Transition transition) {
      parents.emplace_back(std::move(transition), std::move(step));
    }

    void printTrace(std::ostream& os) const noexcept {}

    explicit Step(State state, std::size_t uniqueId)
        : uniqueId(uniqueId), state(std::move(state)) {}
  };

 private:
  struct StepFingerprintHash {
    auto operator()(std::shared_ptr<Step> const& s) const -> std::size_t {
      if (!s->hash) {
        s->hash.emplace(StateHash{}(s->state));
      }
      return *s->hash;
    }
  };

  struct StepFingerprintCompare {
    auto operator()(std::shared_ptr<Step const> const& lhs,
                    std::shared_ptr<Step const> const& rhs) const -> bool {
      return StateCompare{}(lhs->state, rhs->state);
    }
  };

  auto registerFingerprint(State state)
      -> std::pair<bool, std::shared_ptr<Step>> {
    auto step = std::make_shared<Step>(std::move(state), ++nextUniqueId);
    auto [iter, inserted] = fingerprints.emplace(step);
    return {inserted, *iter};
  }

  Stats stats{};
  std::size_t nextUniqueId{0};
  std::deque<std::shared_ptr<Step const>> activeSteps;
  std::unordered_set<std::shared_ptr<Step>, StepFingerprintHash,
                     StepFingerprintCompare>
      fingerprints;
};

}  // namespace arangodb::test::model_checker
