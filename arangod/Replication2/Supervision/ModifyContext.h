////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <optional>
#include <type_traits>
#include <tuple>

namespace arangodb::replication2 {
template<typename... Ts>
struct ModifyContext {
  explicit ModifyContext(std::optional<Ts>... values)
      : values(std::move(values)...) {}

  [[nodiscard]] auto hasModification() const noexcept -> bool {
    return (forType<Ts>().wasModified || ...);
  }

  template<typename... T, typename F>
  auto modify(F&& fn) {
    static_assert(std::is_invocable_v<F, T&...>);
    TRI_ASSERT((forType<T>().value.has_value() && ...))
        << "modifying action expects value to be present";
    ((forType<T>().wasModified = true), ...);
    return std::invoke(std::forward<F>(fn), (*forType<T>().value)...);
  }

  template<typename... T, typename F>
  auto modifyOrCreate(F&& fn) {
    static_assert(std::is_invocable_v<F, T&...>);
    (
        [&] {
          if (!forType<T>().value.has_value()) {
            static_assert(std::is_default_constructible_v<T>);
            forType<T>().value.emplace();
          }
        }(),
        ...);
    ((forType<T>().wasModified = true), ...);
    return std::invoke(std::forward<F>(fn), (*forType<T>().value)...);
  }

  template<typename T, typename... Args>
  void setValue(Args&&... args) {
    forType<T>().value.emplace(std::forward<Args>(args)...);
    forType<T>().wasModified = true;
  }

  template<typename T>
  auto getValue() const -> T const& {
    return forType<T>().value.value();
  }

  template<typename T>
  [[nodiscard]] auto hasModificationFor() const noexcept -> bool {
    return forType<T>().wasModified;
  }

  template<typename T>
  struct ModifySomeType {
    explicit ModifySomeType(std::optional<T> const& value) : value(value) {}
    std::optional<T> value;
    bool wasModified = false;
  };

 private:
  std::tuple<ModifySomeType<Ts>...> values;

  template<typename T>
  auto forType() -> ModifySomeType<T>& {
    return std::get<ModifySomeType<T>>(values);
  }
  template<typename T>
  auto forType() const -> ModifySomeType<T> const& {
    return std::get<ModifySomeType<T>>(values);
  }
};

}  // namespace arangodb::replication2
