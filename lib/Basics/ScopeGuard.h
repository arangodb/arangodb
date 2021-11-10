////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <functional>
#include <type_traits>
#include <utility>

namespace arangodb {

namespace detail {
struct UniqueBool {
  UniqueBool() noexcept = default;
  explicit UniqueBool(bool v) : _value(v) {}
  UniqueBool(UniqueBool&& other) noexcept : _value(other.once()) {}
  UniqueBool& operator=(UniqueBool&& other) noexcept {
    _value = other.once();
    return *this;
  }

  void reset() noexcept { _value = false; }
  explicit operator bool() const noexcept { return _value; }
  [[nodiscard]] auto value() const noexcept -> bool { return _value; }
  auto once() noexcept -> bool {
    if (_value) {
      _value = false;
      return true;
    }
    return false;
  }

 private:
  bool _value = false;
};

template<bool isDeleted>
struct ConditionalDeletedMoveConstructor {
  ConditionalDeletedMoveConstructor() = default;
};
template<>
struct ConditionalDeletedMoveConstructor<true> {
  ConditionalDeletedMoveConstructor() = default;
  ConditionalDeletedMoveConstructor(
      ConditionalDeletedMoveConstructor&&) noexcept = delete;
};
}  // namespace detail

template<typename F, typename Func = std::decay_t<F>>
struct ScopeGuard : private Func,
                    private detail::UniqueBool,
                    private detail::ConditionalDeletedMoveConstructor<
                        !std::is_nothrow_move_constructible_v<Func>> {
  static_assert(std::is_nothrow_invocable_r_v<void, F>);

  [[nodiscard]] explicit ScopeGuard(F&& fn)
      : Func(std::forward<F>(fn)), UniqueBool(true) {}

  ScopeGuard(ScopeGuard&&) noexcept = default;
  ScopeGuard& operator=(ScopeGuard&&) noexcept = default;

  ~ScopeGuard() { fire(); }

  void fire() noexcept {
    if (UniqueBool::once()) {
      std::invoke(std::forward<F>(*this));
    }
  }

  void cancel() noexcept { UniqueBool::reset(); }
  [[nodiscard]] auto active() const noexcept -> bool {
    return UniqueBool::value();
  }
};

template<typename G>
ScopeGuard(G &&) -> ScopeGuard<G>;

// TODO can be deleted, because the deduction guide above allows to use
//      the constructor directly.
template<class T>
[[nodiscard]] ScopeGuard<T> scopeGuard(T&& f) {
  return ScopeGuard<T>(std::forward<T>(f));
}

[[nodiscard]] inline auto scopeGuard(void (*func)() noexcept) {
  return ScopeGuard([func]() noexcept { func(); });
}

}  // namespace arangodb
