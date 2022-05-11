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
#include <utility>
#include <vector>
#include <tuple>
#include <optional>
#include <unordered_set>
#include <deque>
#include <iostream>
#include <variant>

#include <boost/container_hash/hash_fwd.hpp>

namespace arangodb::test::model_checker {

struct CheckError {
  explicit CheckError(std::string message) : message(std::move(message)) {}
  std::string message;

  friend auto operator<<(std::ostream& os, CheckError const& err)
      -> std::ostream& {
    return os << err.message << std::endl;
  }
};

struct CheckResult2 {
  struct Ok {};
  struct Prune {};
  struct Error : CheckError {
    using CheckError::CheckError;
  };

  static auto withOk() noexcept -> CheckResult2 {
    return CheckResult2{std::in_place_type<Ok>};
  }
  static auto withPrune() noexcept -> CheckResult2 {
    return CheckResult2{std::in_place_type<Prune>};
  }
  static auto withError(std::string message) noexcept -> CheckResult2 {
    return CheckResult2{std::in_place_type<Error>, std::move(message)};
  }

  friend auto isOk(CheckResult2 const& r) noexcept -> bool {
    return std::holds_alternative<Ok>(r._variant);
  }
  friend auto isPrune(CheckResult2 const& r) noexcept -> bool {
    return std::holds_alternative<Prune>(r._variant);
  }
  friend auto isError(CheckResult2 const& r) noexcept -> bool {
    return std::holds_alternative<Error>(r._variant);
  }

  auto asError() -> Error& { return std::get<Error>(_variant); }

 private:
  template<typename... Ts>
  explicit CheckResult2(Ts&&... ts) : _variant(std::forward<Ts>(ts)...) {}
  std::variant<Ok, Prune, Error> _variant;
};

using CheckResult = CheckResult2;

struct Stats {
  std::size_t uniqueStates;
  std::size_t eliminatedStates;
  std::size_t discoveredStates;
  std::size_t finalStates;
};

auto operator<<(std::ostream& os, Stats const& stats) -> std::ostream&;

template<typename StateType, typename TransitionType, typename Observer>
struct DFSEnumerator {
  struct SimulationState;

  using OutgoingVectorType =
      std::vector<std::pair<TransitionType, std::shared_ptr<SimulationState>>>;

  struct SimulationState {
    StateType state;
    Observer observer;
    OutgoingVectorType outgoing{};

    friend auto operator==(SimulationState const& lhs,
                           SimulationState const& rhs) noexcept -> bool {
      return std::tie(lhs.state, lhs.observer) ==
             std::tie(rhs.state, rhs.observer);
    }

    friend auto hash_value(SimulationState const& v) noexcept -> std::size_t {
      std::size_t seed = 0;
      boost::hash_combine(seed, v.state);
      boost::hash_combine(seed, v.observer);
      return seed;
    }
  };

  using PathVectorType =
      std::vector<std::pair<std::shared_ptr<SimulationState>, TransitionType>>;

  friend auto operator<<(std::ostream& os, PathVectorType const& path)
      -> std::ostream& {
    for (auto const& [v, t] : path) {
      os << "{" << v->state << "}"
         << " -[" << t << "]-> ";
    }
    return os;
  }

  struct StateVertex : SimulationState {
    std::size_t uniqueId{0};
    std::size_t searchIndex{0};
    std::size_t depth{0};

    StateVertex(StateType state, Observer observer)
        : SimulationState{std::move(state), std::move(observer)} {}

    auto isCompleted() const noexcept {
      return !isNewVertex() && searchIndex > this->outgoing.size();
    }
    auto isNewVertex() const noexcept { return uniqueId == 0; }
  };

  struct ObserverError {
    CheckError error;
    std::shared_ptr<StateVertex> badState;
    PathVectorType path;

    ObserverError(CheckError error, std::shared_ptr<StateVertex> badState,
                  PathVectorType path)
        : error(std::move(error)),
          badState(std::move(badState)),
          path(std::move(path)) {}

    friend auto operator<<(std::ostream& os, ObserverError const& oe)
        -> std::ostream& {
      return os << oe.error << ": " << oe.path << oe.badState->state;
    }
  };

  struct FingerprintHash {
    auto operator()(std::shared_ptr<StateVertex> const& s) const
        -> std::size_t {
      return hash_value(*s);
    }
  };

  struct FingerprintCompare {
    auto operator()(std::shared_ptr<StateVertex> const& lhs,
                    std::shared_ptr<StateVertex> const& rhs) const -> bool {
      return *lhs == *rhs;
    }
  };

  using FingerprintSet =
      std::unordered_set<std::shared_ptr<StateVertex>, FingerprintHash,
                         FingerprintCompare>;

  struct Result {
    FingerprintSet fingerprints;
    std::vector<std::shared_ptr<StateVertex const>> finalStates;
    std::optional<ObserverError> failed;
    std::optional<PathVectorType> cycle;
    Stats stats{};
  };

  template<typename Driver>
  static auto run(Driver& driver, Observer initialObserver,
                  StateType initialState) {
    std::size_t nextUniqueId{0};
    Result result;
    std::vector<std::shared_ptr<StateVertex>> path;
    std::shared_ptr<StateVertex> startVertex;

    auto const registerFingerprint = [&](StateType state, Observer observer)
        -> std::pair<bool, std::shared_ptr<StateVertex>> {
      auto step =
          std::make_shared<StateVertex>(std::move(state), std::move(observer));
      auto [iter, inserted] = result.fingerprints.emplace(step);
      return {inserted, *iter};
    };

    {
      auto [inserted, step] = registerFingerprint(std::move(initialState),
                                                  std::move(initialObserver));
      auto checkResult = step->observer.check(step->state);
      if (isPrune(checkResult)) {
        return result;
      } else if (isError(checkResult)) {
        result.failed.emplace(checkResult.asError(), step, PathVectorType{});
        return result;
      }
      path.push_back(step);
    }

    auto const buildPathVector = [&] {
      PathVectorType result;
      for (auto const& p : path) {
        result.emplace_back(p, p->outgoing[p->searchIndex - 1].first);
      }
      return result;
    };

    while (not path.empty()) {
      auto v = path.back();
      if (v->isCompleted()) {
        path.pop_back();
      } else if (v->isNewVertex()) {
        // expand the vertex
        v->uniqueId = ++nextUniqueId;
        auto exploredStates = driver.expand(v->state);
        v->outgoing.reserve(exploredStates.size());
        for (auto& [transition, state] : driver.expand(v->state)) {
          auto [inserted, step] =
              registerFingerprint(std::move(state), v->observer);
          result.stats.discoveredStates += 1;
          if (inserted) {
            result.stats.uniqueStates += 1;
            step->depth = v->depth + 1;
          } else {
            result.stats.eliminatedStates += 1;
          }
          auto checkResult = step->observer.check(step->state);
          if (isPrune(checkResult)) {
            continue;
          } else if (isError(checkResult)) {
            auto p = buildPathVector();
            result.failed.emplace(checkResult.asError(), step, p);
            return result;
          }
          v->outgoing.emplace_back(std::move(transition), step);
          if (!step->isNewVertex() && !step->isCompleted()) {
            v->searchIndex = v->outgoing.size();
            path.erase(path.begin(), std::find(path.begin(), path.end(), step));
            result.cycle.emplace(buildPathVector());
            result.failed.emplace(CheckError("cycle detected"), step,
                                  buildPathVector());
          }
        }
      } else if (v->outgoing.size() == v->searchIndex) {
        v->searchIndex += 1;  // make this vertex complete
        path.pop_back();
        if (v->outgoing.empty()) {
          result.stats.finalStates += 1;
          result.finalStates.emplace_back(v);
          if (auto checkResult = v->observer.finalStep(v->state);
              isError(checkResult)) {
            auto p = buildPathVector();
            result.failed.emplace(checkResult.asError(), v, p);
            return result;
          }
        }
      } else {
        path.push_back(std::static_pointer_cast<StateVertex>(
            v->outgoing[v->searchIndex++].second));
      }
    }

    return result;
  }
};

template<typename StateType, typename TransitionType>
struct DFSEngine {
  template<typename Driver, typename Observer>
  static auto run(Driver& driver, Observer initialObserver,
                  StateType initialState) {
    return DFSEnumerator<StateType, TransitionType, Observer>::run(
        driver, std::move(initialObserver), std::move(initialState));
  }
};

template<typename StateType, typename TransitionType,
         typename StateHash = boost::hash<StateType>,
         typename StateCompare = std::equal_to<>>
struct BFSEnumerator {
  using State = StateType;
  using Transition = TransitionType;

  virtual ~BFSEnumerator() = default;

  template<typename Observer>
  struct Step {
    std::size_t depth{0}, uniqueId{0};
    State state;
    Observer observer;
    std::vector<std::pair<Transition, std::shared_ptr<Step const>>> parents;
    std::optional<std::size_t> hash;

    void registerPreviousStep(std::shared_ptr<Step const> step,
                              Transition transition) {
      parents.emplace_back(std::move(transition), std::move(step));
    }

    void printTrace(std::ostream& os) const noexcept {
      if (!parents.empty()) {
        parents.front().second->printTrace(os);
        os << "- [" << parents.front().first << "] ->" << std::endl;
      }
      os << state << std::endl;
    }

    explicit Step(State state, std::size_t uniqueId, Observer observer)
        : uniqueId(uniqueId),
          state(std::move(state)),
          observer(std::move(observer)) {}

    friend auto operator<<(std::ostream& os, Step const& s) -> std::ostream& {
      s.printTrace(os);
      os << " (final state reached)" << std::endl;
      return os;
    }
  };

  struct StepFingerprintHash {
    template<typename Observer>
    auto operator()(std::shared_ptr<Step<Observer>> const& s) const
        -> std::size_t {
      if (!s->hash) {
        std::size_t seed = 0;
        boost::hash_combine(seed, s->observer);
        boost::hash_combine(seed, StateHash{}(s->state));
        s->hash.emplace(seed);
      }
      return *s->hash;
    }
  };

  template<typename Observer>
  struct StepFingerprintCompare {
    auto operator()(std::shared_ptr<Step<Observer> const> const& lhs,
                    std::shared_ptr<Step<Observer> const> const& rhs) const
        -> bool {
      return StateCompare{}(lhs->state, rhs->state) &&
             lhs->observer == rhs->observer;
    }
  };

  template<typename Observer>
  using FingerprintSet =
      std::unordered_set<std::shared_ptr<Step<Observer>>, StepFingerprintHash,
                         StepFingerprintCompare<Observer>>;

  template<typename Observer>
  struct Error {
    Error(std::shared_ptr<Step<Observer> const> state, CheckError error)
        : state(std::move(state)), error(std::move(error)) {}
    std::shared_ptr<Step<Observer> const> state;
    CheckError error;

    friend auto operator<<(std::ostream& os, Error const& err)
        -> std::ostream& {
      os << *err.state << std::endl;
      os << err.error << std::endl;
      return os;
    }
  };

  template<typename Observer>
  struct Result {
    Stats stats{};
    FingerprintSet<Observer> fingerprints;
    std::vector<std::shared_ptr<Step<Observer> const>> finalStates;
    std::optional<Error<Observer>> failed;

    void printAllStates(std::ostream& os) {
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
  };

  template<typename Driver, typename Observer>
  static auto run(Driver& driver, Observer initialObserver,
                  State initialState) {
    Result<Observer> result;

    std::size_t nextUniqueId{0};
    std::deque<std::shared_ptr<Step<Observer>>> activeSteps;

    auto const registerFingerprint = [&](State state, Observer observer)
        -> std::pair<bool, std::shared_ptr<Step<Observer>>> {
      auto step = std::make_shared<Step<Observer>>(
          std::move(state), ++nextUniqueId, std::move(observer));
      auto [iter, inserted] = result.fingerprints.emplace(step);
      return {inserted, *iter};
    };

    using clock = std::chrono::steady_clock;
    auto last = clock::now();

    {
      auto [inserted, step] = registerFingerprint(std::move(initialState),
                                                  std::move(initialObserver));
      auto checkResult = step->observer.check(step->state);
      if (isPrune(checkResult)) {
        return result;
      } else if (isError(checkResult)) {
        result.failed.emplace(step, checkResult.asError());
        return result;
      }
      activeSteps.push_back(std::move(step));
    }
    while (not activeSteps.empty()) {
      auto nextStep = std::move(activeSteps.front());
      activeSteps.pop_front();
      auto newStates = driver.expand(nextStep->state);
      if (newStates.empty()) {
        result.stats.finalStates += 1;
        result.finalStates.emplace_back(nextStep);
        if (auto checkResult = nextStep->observer.finalStep(nextStep->state);
            isError(checkResult)) {
          result.failed.emplace(nextStep, checkResult.asError());
          return result;
        }
      }
      for (auto const& [transition, state] : newStates) {
        result.stats.discoveredStates += 1;
        // fingerprint this state
        auto [wasNewState, step] =
            registerFingerprint(std::move(state), nextStep->observer);
        // add previous step information
        step->registerPreviousStep(nextStep, std::move(transition));
        // TODO loop detection?
        if (!wasNewState) {
          result.stats.eliminatedStates += 1;
          continue;  // ignore
        }
        step->depth = nextStep->depth + 1;

        result.stats.uniqueStates += 1;

        // run the check function for this step
        auto checkResult = step->observer.check(step->state);
        if (isPrune(checkResult)) {
          continue;
        } else if (isError(checkResult)) {
          result.failed.emplace(step, checkResult.asError());
          return result;
        }

        // put step into active steps
        activeSteps.push_back(step);
      }

      auto now = clock::now();
      if ((now - last) > std::chrono::seconds{5}) {
        std::cout << result.stats << " current depth = " << nextStep->depth
                  << std::endl;
        last = now;
      }
    }
    return result;
  }
};

}  // namespace arangodb::test::model_checker
