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
/// @author Julia Volmer
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>

#include "Actor/ActorID.h"

namespace arangodb::actor {

// TODO: at some point this needs to be ArangoDB's ServerID or compatible
using ServerID = std::string;
using DatabaseName = std::string;

struct DistributedActorPID {
  ServerID server;
  DatabaseName database;
  ActorID id;
  bool operator==(const DistributedActorPID&) const = default;

  template<typename Inspector>
  friend inline auto inspect(Inspector& f, DistributedActorPID& x) {
    return f.object(x).fields(f.field("server", x.server),
                              f.field("database", x.database),
                              f.field("id", x.id));
  }
};
}  // namespace arangodb::actor

// TODO - remove in favor of inspection::json()
template<>
struct fmt::formatter<arangodb::actor::DistributedActorPID>
    : arangodb::inspection::inspection_formatter {};

namespace std {
template<>
struct hash<arangodb::actor::DistributedActorPID> {
  size_t operator()(
      arangodb::actor::DistributedActorPID const& x) const noexcept {
    size_t hash_id = std::hash<arangodb::actor::ActorID>()(x.id);
    size_t hash_database = std::hash<std::string>()(x.database);
    size_t hash_server = std::hash<std::string>()(x.server);
    // TODO lookup if mixing hashings is appropriate
    return (hash_id ^ (hash_database << 1)) ^ (hash_server << 1);
  };
};
}  // namespace std
