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

#include "utils/math_utils.hpp"
#include "utils/string.hpp"

namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @brief SFINAE
////////////////////////////////////////////////////////////////////////////////
#define DEFINE_HAS_MEMBER(member)                                            \
  template<typename T>                                                       \
  class has_member_##member {                                                \
   private:                                                                  \
    using yes_type = char;                                                   \
    using no_type = long;                                                    \
    using type = std::remove_reference_t<std::remove_cv_t<T>>;               \
    template<typename U> static yes_type test(decltype(&U::member));         \
    template<typename U> static no_type  test(...);                          \
   public:                                                                   \
    static constexpr bool value = sizeof(test<type>(0)) == sizeof(yes_type); \
  };                                                                         \
  template<typename T>                                                       \
  inline constexpr auto has_member_##member##_v = has_member_##member<T>::value

#define HAS_MEMBER(type, member) has_member_##member##_v<type>

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
/// @brief compile-time type identifier
////////////////////////////////////////////////////////////////////////////////
template<typename T>
constexpr string_ref ctti() noexcept {
  return { IRESEARCH_CURRENT_FUNCTION };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convenient helper for simulating 'try/catch/finally' semantic
////////////////////////////////////////////////////////////////////////////////
template<typename Func>
class finally {
 public:
  finally(const Func& func) : func_(func) { }
  finally(Func&& func) noexcept : func_(std::move(func)) { }
  ~finally() { func_(); }

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
  move_on_copy(T&& value) noexcept : value_(std::move(value)) {}
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
  static_assert(std::is_rvalue_reference<decltype(value)>::value, "parameter should be an rvalue");
  return move_on_copy<T>(std::move(value));
}

}

#endif
