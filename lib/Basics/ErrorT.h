////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <type_traits>
#include <variant>

namespace arangodb::errors {

template<typename Error, typename T>
struct ErrorT {
  using Contained = std::variant<Error, T>;

  template<typename... Args>
  [[nodiscard]] auto static error(Args&&... args) -> ErrorT<Error, T> {
    return ErrorT(
        Contained(std::in_place_index<0>, std::forward<Args>(args)...));
  }

  template<typename... Args>
  [[nodiscard]] auto static ok(Args&&... args) -> ErrorT<Error, T> {
    return ErrorT(
        Contained(std::in_place_index<1>, std::forward<Args>(args)...));
  }

  ErrorT() requires(std::is_nothrow_default_constructible_v<T>)
      : _contained{T{}} {}

  [[nodiscard]] auto ok() const noexcept -> bool {
    return _contained.index() == 1;
  }
  [[nodiscard]] auto error() const -> Error const& {
    return std::get<0>(_contained);
  }

  [[nodiscard]] auto get() const -> T const& { return std::get<1>(_contained); }
  [[nodiscard]] auto get() -> T& { return std::get<1>(_contained); }

  [[nodiscard]] explicit operator bool() const noexcept
      requires(!std::convertible_to<T, bool>) {
    return ok();
  }

  [[nodiscard]] auto operator->() -> T* { return &get(); }
  [[nodiscard]] auto operator->() const -> T const* { return &get(); }

  [[nodiscard]] auto operator*() & -> T& { return get(); }
  [[nodiscard]] auto operator*() const& -> T const& { return get(); }

  // cppcheck-suppress returnStdMoveLocal
  [[nodiscard]] auto operator*() && -> T&& { return std::move(get()); }

 private:
  ErrorT(Contained&& val) : _contained(std::move(val)) {}
  Contained _contained;
};

}  // namespace arangodb::errors
