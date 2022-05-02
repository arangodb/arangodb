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
#include "ModelChecker.h"
#include <gtest/gtest.h>
#include <fmt/core.h>
#include <boost/container_hash/hash.hpp>

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

template<typename Location, typename F>
struct always : F {
  explicit always(Location, F&& f) : F(std::forward<F>(f)) {}
  template<typename S>
  auto check(S const& state) {
    if (F::operator()(state)) {
      return CheckResult::withOk();
    } else {
      return CheckResult::withError(
          Location::annotate("Predicate was violated"));
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
  friend auto operator==(Tagged const&, Tagged const&) noexcept
      -> bool = default;
};

template<typename, typename...>
struct combined_base;
template<std::size_t... Idx, typename... Os>
struct combined_base<std::index_sequence<Idx...>, Os...> : Tagged<Idx, Os>... {
  template<typename S>
  auto finalStep(S const& step) {
    return invokeAll<0>([&](auto& x) { return x.finalStep(step); });
  }
  template<typename S>
  auto check(S const& step) -> model_checker::CheckResult {
    return invokeAll<0>([&](auto& x) { return x.check(step); });
  }

  template<std::size_t I, typename F>
  auto invokeAll(F&& fn) {
    if constexpr (I >= sizeof...(Os)) {
      return CheckResult::withOk();
    } else {
      auto result = fn(get_ith<I>());
      if (isError(result)) {
        return result;
      } else {
        auto nextLayer = invokeAll<I + 1>(std::forward<F>(fn));
        if (!isOk(nextLayer)) {
          return nextLayer;
        } else {
          return result;
        }
      }
    }
  }

  template<std::size_t I>
  auto& get_ith() {
    using type = std::tuple_element_t<I, std::tuple<Os...>>;
    return static_cast<Tagged<I, type>&>(*this);
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

template<const char File[], std::size_t Line>
struct FileLineType {
  static inline constexpr auto filename = File;
  static inline constexpr std::size_t line = Line;

  static auto annotate(std::string_view message) -> std::string {
    return fmt::format("{}:{}: {}", filename, line, message);
  }
};

#define MC_HERE                              \
  ([] {                                      \
    static constexpr char data[] = __FILE__; \
    return ::FileLineType<data, __LINE__>{}; \
  }())

#define MC_GTEST_PRED(name, pred)           \
  model_checker::gtest_predicate {          \
    MC_HERE, [=](auto const& name) { pred } \
  }
#define MC_BOOL_PRED(name, pred)            \
  model_checker::bool_predicate {           \
    MC_HERE, [=](auto const& name) { pred } \
  }
#define MC_BOOL_PRED2(lambda) \
  model_checker::bool_predicate { MC_HERE, (lambda) }
#define MC_EVENTUALLY(pred) \
  model_checker::eventually { MC_HERE, pred }
#define MC_ALWAYS(pred) \
  model_checker::always { MC_HERE, pred }
#define MC_EVENTUALLY_ALWAYS(pred) \
  model_checker::eventually_always { MC_HERE, pred }
