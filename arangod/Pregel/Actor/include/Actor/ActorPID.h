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
/// @author Julia Volmer
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cstddef>
#include <string>
#include "Inspection/Format.h"

namespace arangodb::pregel::actor {
struct ActorID {
  size_t id;

  auto operator<=>(ActorID const& other) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, ActorID& x) {
  if constexpr (Inspector::isLoading) {
    auto v = size_t{0};
    auto res = f.apply(v);
    if (res.ok()) {
      x = ActorID{.id = v};
    }
    return res;
  } else {
    return f.apply(x.id);
  }
}
}  // namespace arangodb::pregel::actor

template<>
struct fmt::formatter<arangodb::pregel::actor::ActorID>
    : arangodb::inspection::inspection_formatter {};

namespace std {
template<>
struct hash<arangodb::pregel::actor::ActorID> {
  size_t operator()(arangodb::pregel::actor::ActorID const& x) const noexcept {
    return std::hash<size_t>()(x.id);
  };
};
}  // namespace std

namespace arangodb::pregel::actor {

// TODO: at some point this needs to be ArangoDB's ServerID or compatible
using ServerID = std::string;
using DatabaseName = std::string;

struct ActorPID {
  ServerID server;
  DatabaseName database;
  ActorID id;
};
template<typename Inspector>
auto inspect(Inspector& f, ActorPID& x) {
  return f.object(x).fields(f.field("server", x.server),
                            f.field("database", x.database),
                            f.field("id", x.id));
}

}  // namespace arangodb::pregel::actor

template<>
struct fmt::formatter<arangodb::pregel::actor::ActorPID>
    : arangodb::inspection::inspection_formatter {};
