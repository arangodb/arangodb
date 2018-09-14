////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_FUTURES_BACKPORTS_H
#define ARANGOD_FUTURES_BACKPORTS_H 1

namespace arangodb {
namespace futures {

/// Backports from C++17 of:
/// std::in_place_t
/// std::in_place
/// std::is_invocable
/// std::is_invocable_r
/// std::invoke

#if __cplusplus < 201703L
struct in_place_tag {};
using in_place_t = in_place_tag (&)(in_place_tag);
inline in_place_tag in_place(in_place_tag = {}) {
  return {};
}

template <typename F, typename... Args>
struct is_invocable :
std::is_constructible<
std::function<void(Args...)>,
std::reference_wrapper<typename std::remove_reference<F>::type>
>
{};

template <typename R, typename F, typename... Args>
struct is_invocable_r :
std::is_constructible<
std::function<R(Args ...)>,
std::reference_wrapper<typename std::remove_reference<F>::type>
>
{};
  
//  mimic: std::invoke, C++17
template <typename F, typename... Args>
constexpr auto invoke(F&& f, Args&&... args) noexcept(noexcept(static_cast<F&&>(f)(static_cast<Args&&>(args)...)))
-> decltype(static_cast<F&&>(f)(static_cast<Args&&>(args)...)) {
  return static_cast<F&&>(f)(static_cast<Args&&>(args)...);
}
template <typename M, typename C, typename... Args>
constexpr auto invoke(M(C::*d), Args&&... args)
-> decltype(std::mem_fn(d)(static_cast<Args&&>(args)...)) {
  return std::mem_fn(d)(static_cast<Args&&>(args)...);
}
  
#else
#include <type_traits>
#include <utility>
using in_place_t = std::in_place_t;
using in_place = std::in_place;
using is_invocable = std::is_invocable;
using is_invocable_r = std::is_invocable_r;
using invoke = std::invoke;
#endif

}}

#endif
