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

#include "Actor/ActorID.h"

namespace arangodb::actor {

struct LocalActorPID {
  ActorID id;
  bool operator==(const LocalActorPID&) const = default;

  template<typename Inspector>
  friend inline auto inspect(Inspector& f, LocalActorPID& x) {
    return f.object(x).fields(f.field("id", x.id));
  }
};
}  // namespace arangodb::actor

// TODO - remove in favor of inspection::json()
template<>
struct fmt::formatter<arangodb::actor::LocalActorPID>
    : arangodb::inspection::inspection_formatter {};

namespace std {
template<>
struct hash<arangodb::actor::LocalActorPID> {
  size_t operator()(arangodb::actor::LocalActorPID const& x) const noexcept {
    return std::hash<arangodb::actor::ActorID>()(x.id);
  };
};
}  // namespace std
