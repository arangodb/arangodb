////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <array>
#include <memory>
#include <type_traits>

#include "shared.hpp"
#include "utils/assert.hpp"

namespace irs {

// Convenient helper for simulating 'try/catch/finally' semantic
template<typename Func>
class [[nodiscard]] Finally {
 public:
  static_assert(std::is_nothrow_invocable_v<Func>);

  // If you need some of it, please use absl::Cleanup
  Finally(Finally&&) = delete;
  Finally(const Finally&) = delete;
  Finally& operator=(Finally&&) = delete;
  Finally& operator=(const Finally&) = delete;

  Finally(Func&& func) : func_{std::move(func)} {}

  ~Finally() noexcept { func_(); }

 private:
  IRS_NO_UNIQUE_ADDRESS Func func_;
};

// Convenient helper for caching function results
template<typename Input, Input Size, typename Func,
         typename = typename std::enable_if_t<std::is_integral_v<Input>>>
class CachedFunc {
 public:
  using input_type = Input;
  using output_type = std::invoke_result_t<Func, Input>;

  constexpr explicit CachedFunc(input_type offset, Func&& func)
    : func_{std::forward<Func>(func)} {
    for (; offset < Size; ++offset) {
      cache_[offset] = func_(offset);
    }
  }

  template<bool Checked>
  constexpr IRS_FORCE_INLINE output_type get(input_type value) const
    noexcept(std::is_nothrow_invocable_v<Func, Input>) {
    if constexpr (Checked) {
      return value < size() ? cache_[value] : func_(value);
    } else {
      IRS_ASSERT(value < cache_.size());
      return cache_[value];
    }
  }

  constexpr size_t size() const noexcept { return cache_.size(); }

 private:
  IRS_NO_UNIQUE_ADDRESS Func func_;
  std::array<output_type, Size> cache_{};
};

template<typename Input, size_t Size, typename Func>
constexpr CachedFunc<Input, Size, Func> cache_func(Input offset, Func&& func) {
  return CachedFunc<Input, Size, Func>{offset, std::forward<Func>(func)};
}

template<typename To, typename From>
constexpr auto* DownCast(From* from) noexcept {
  static_assert(!std::is_pointer_v<To>);
  static_assert(!std::is_reference_v<To>);
  using CastTo =
    std::conditional_t<std::is_const_v<From>, std::add_const_t<To>, To>;
  IRS_ASSERT(from == nullptr || dynamic_cast<CastTo*>(from) != nullptr);
  return static_cast<CastTo*>(from);
}

template<typename To, typename From>
constexpr auto& DownCast(From& from) noexcept {
  return *DownCast<To>(std::addressof(from));
}

// A convenient helper to use with std::visit(...)
template<typename... Visitors>
struct Visitor : Visitors... {
  template<typename... T>
  Visitor(T&&... visitors) : Visitors{std::forward<T>(visitors)}... {}

  using Visitors::operator()...;
};

template<typename... T>
Visitor(T...) -> Visitor<std::decay_t<T>...>;

template<typename Func>
auto ResolveBool(bool value, Func&& func) {
  if (value) {
    return std::forward<Func>(func).template operator()<true>();
  } else {
    return std::forward<Func>(func).template operator()<false>();
  }
}

}  // namespace irs
