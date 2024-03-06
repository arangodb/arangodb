////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string_view>

namespace arangodb::inspection {

namespace detail {
template<class T>
struct AlternativeType {
  using Type = T;
  static constexpr bool isInlineType = false;
  std::string_view const tag;
};

template<class T>
struct AlternativeInlineType {
  using Type = T;
  static constexpr bool isInlineType = true;
};
}  // namespace detail

template<class T>
detail::AlternativeType<T> type(std::string_view tag) {
  return detail::AlternativeType<T>{tag};
}

template<class T>
detail::AlternativeInlineType<T> inlineType() {
  return detail::AlternativeInlineType<T>{};
}

}  // namespace arangodb::inspection
