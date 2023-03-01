////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include <cstdint>
#include <string>

#include "VertexID.h"

namespace arangodb::pregel {

template<typename V, typename E>
class GraphStore;

template<typename E>
struct Edge {
  template<typename V, typename E2>
  friend class GraphStore;

  Edge() = delete;
  Edge(VertexID id, E&& data) : _to{id}, _data(std::move(data)) {}
  Edge(Edge const&) = delete;
  Edge(Edge&&) = default;
  auto operator=(Edge const& other) -> Edge& = delete;
  auto operator=(Edge&& other) -> Edge& = default;

  [[nodiscard]] std::string_view toKey() const { return _to.key; }
  E& data() noexcept { return _data; }
  [[nodiscard]] PregelShard targetShard() const noexcept { return _to.shard; }

  VertexID _to;
  E _data;
};

template<typename E, typename Inspector>
auto inspect(Inspector& f, Edge<E>& e) {
  return f.object(e).fields(f.field("to", e._to), f.field("data", e._data));
}

}  // namespace arangodb::pregel
