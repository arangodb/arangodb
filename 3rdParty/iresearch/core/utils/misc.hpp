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

#ifndef IRESEARCH_MISC_H
#define IRESEARCH_MISC_H

#include <array>

#include "shared.hpp"

namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @brief Cross-platform 'COUNTOF' implementation
////////////////////////////////////////////////////////////////////////////////
#if __cplusplus >= 201103L || _MSC_VER >= 1900 || IRESEARCH_COMPILER_HAS_FEATURE(cxx_constexpr) // C++ 11 implementation
  namespace detail {
  template <typename T, std::size_t N>
  constexpr std::size_t countof(T const (&)[N]) noexcept { return N; }
  } // detail
  #define IRESEARCH_COUNTOF(x) ::iresearch::detail::countof(x)
#elif _MSC_VER // Visual C++ fallback
  #define IRESEARCH_COUNTOF(x) _countof(x)
#elif __cplusplus >= 199711L && \
      (defined(__clang__) \
       || (defined(__GNUC__) && ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4))) \
      ) // C++ 98 trick
  template <typename T, std::size_t N>
  char(&COUNTOF_ARRAY_ARGUMENT(T(&)[N]))[N];
  #define IRESEARCH_COUNTOF(x) sizeof(COUNTOF_ARRAY_ARGUMENT(x))
#else
  #define IRESEARCH_COUNTOF(x) sizeof(x) / sizeof(x[0])
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief convenient helper for simulating 'try/catch/finally' semantic
////////////////////////////////////////////////////////////////////////////////
template<typename Func>
class finally {
 public:
  // FIXME uncomment when no comments left:
  // "FIXME make me noexcept as I'm begin called from within ~finally()"
  //static_assert(std::is_nothrow_invocable_v<Func>);

  finally(Func&& func)
    : func_(std::forward<Func>(func)) {
  }
  ~finally() {
    func_();
  }

 private:
  Func func_;
}; // finally

template<typename Func>
finally<Func> make_finally(Func&& func) {
  return finally<Func>(std::forward<Func>(func));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convenient helper for simulating copy semantic for move-only types
///        e.g. lambda capture statement before c++14
////////////////////////////////////////////////////////////////////////////////
template<typename T>
class move_on_copy {
 public:
  move_on_copy(T&& value) noexcept : value_(std::forward<T>(value)) {}
  move_on_copy(const move_on_copy& rhs) noexcept : value_(std::move(rhs.value_)) {}

  T& value() noexcept { return value_; }
  const T& value() const noexcept { return value_; }

 private:
  move_on_copy& operator=(move_on_copy&&) = delete;
  move_on_copy& operator=(const move_on_copy&) = delete;

  mutable T value_;
}; // move_on_copy

template<typename T>
move_on_copy<T> make_move_on_copy(T&& value) noexcept {
  static_assert(std::is_rvalue_reference_v<decltype(value)>, "parameter must be an rvalue");
  return move_on_copy<T>(std::forward<T>(value));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convenient helper for caching function results
////////////////////////////////////////////////////////////////////////////////
template<
  typename Input,
  Input Size,
  typename Func,
  typename = typename std::enable_if_t<std::is_integral_v<Input>>
> class cached_func {
 public:
  using input_type = Input;
  using output_type = std::invoke_result_t<Func, Input>;

  constexpr explicit cached_func(size_t offset, Func&& func)
    : func_{std::forward<Func>(func)} {
    for (input_type i = offset; i < size(); ++i) {
      cache_[i] = func_(i);
    }
  }

  constexpr FORCE_INLINE output_type operator()(input_type value) const
      noexcept(std::is_nothrow_invocable_v<Func, Input>) {
    return value < size() ? cache_[value] : func_(value);
  }

  constexpr size_t size() const noexcept {
    return IRESEARCH_COUNTOF(cache_);
  }

 private:
  Func func_;
  output_type cache_[Size]{};

  static_assert(IRESEARCH_COUNTOF(decltype(cache_){}) == Size);
}; // cached_func

template<typename Input, size_t Size, typename Func>
constexpr cached_func<Input, Size, Func> cache_func(size_t offset, Func&& func) {
  return cached_func<Input, Size, Func>{ offset, std::forward<Func>(func) };
}

}

#endif
