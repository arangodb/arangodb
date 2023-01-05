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
/// @author Valery Mironov
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Basics/debugging.h"

#include <memory>
#include <utility>
#include <type_traits>

namespace arangodb::basics {

template<typename To, typename From>
constexpr auto* downCast(From* from) noexcept {
  static_assert(!std::is_pointer_v<To>, "'To' shouldn't be pointer");
  static_assert(!std::is_reference_v<To>, "'To' shouldn't be reference");
  using CastTo =
      std::conditional_t<std::is_const_v<From>, std::add_const_t<To>, To>;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(from == nullptr || dynamic_cast<CastTo*>(from) != nullptr);
#endif
  return static_cast<CastTo*>(from);
}

template<typename To, typename From>
constexpr auto& downCast(From&& from) noexcept {
  return *downCast<To>(std::addressof(from));
}

template<typename To, typename From>
auto downCast(std::shared_ptr<From> from) noexcept {
  static_assert(!std::is_pointer_v<To>, "'To' shouldn't be pointer");
  static_assert(!std::is_reference_v<To>, "'To' shouldn't be reference");
  using CastTo =
      std::conditional_t<std::is_const_v<From>, std::add_const_t<To>, To>;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(from == nullptr ||
             std::dynamic_pointer_cast<CastTo>(from) != nullptr);
#endif
  return std::static_pointer_cast<CastTo>(std::move(from));
}

}  // namespace arangodb::basics
