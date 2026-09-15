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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Inspection/InspectorBase.h"

#include <type_traits>
#include <utility>

namespace arangodb {

struct InspectAgencyContext {};
struct InspectUserContext {};

// Loading data the database itself wrote: an on-disk marker or a plan entry.
// That data is already committed, so it must never be rejected.
struct InspectInternalContext {};

namespace detail {
// Inspector::Context does not exist when no context was passed, so it cannot
// be named directly. Inspector::hasContext tells us whether it is there.
template<class Inspector, bool = Inspector::hasContext>
struct ContextOf {
  using type = inspection::NoContext;
};

template<class Inspector>
struct ContextOf<Inspector, true> {
  using type = typename Inspector::Context;
};
}  // namespace detail

template<class Inspector>
inline constexpr bool isInternalContext =
    std::is_same_v<typename detail::ContextOf<Inspector>::type,
                   InspectInternalContext>;

template<class Inspector>
inline constexpr bool isAgencyContext =
    std::is_same_v<typename detail::ContextOf<Inspector>::type,
                   InspectAgencyContext>;

// Applies `invariant` to `field` only when the value come from user input
template<class Inspector, class Field, class Invariant>
auto userInvariant(Inspector&, Field&& field, Invariant&& invariant) {
  if constexpr (isInternalContext<Inspector>) {
    return std::forward<Field>(field);
  } else {
    return std::forward<Field>(field).invariant(
        std::forward<Invariant>(invariant));
  }
}

}  // namespace arangodb
