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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Assertions/Assert.h"

#include <string>
#include <memory>
#include <experimental/vector>
#include <experimental/string>

#include "Edge.h"
#include "VertexID.h"

namespace arangodb::pregel {

template<typename V, typename E>
struct Vertex {
  using allocator_type = std::experimental::pmr::polymorphic_allocator<Edge<E>>;

  Vertex() noexcept : _key(), _shard(), _edges(), _active(true) {}
  Vertex(allocator_type const& alloc) noexcept : _key(alloc), _shard(), _edges(alloc), _active(true) {}
  Vertex(Vertex const&) = delete;
  Vertex(Vertex&&) = default;
  Vertex(allocator_type const& alloc, Vertex&& other) {}

  auto operator=(Vertex const& other) -> Vertex& = delete;
  auto operator=(Vertex&& other) -> Vertex& = default;

  std::experimental::pmr::vector<Edge<E>>& getEdges() noexcept { return _edges; }
  std::experimental::pmr::vector<Edge<E>>& getEdges() const noexcept { return _edges; }

  size_t addEdge(Edge<E>&& edge) noexcept {
    // must only be called during initial vertex creation
    // cppcheck-suppress ignoredReturnValue
    TRI_ASSERT(active());
    _edges.emplace_back(std::move(edge));
    return _edges.size();
  }

  // returns the number of associated edges
  [[nodiscard]] size_t getEdgeCount() const noexcept { return _edges.size(); }

  void setActive(bool bb) noexcept { _active = bb; }

  [[nodiscard]] bool active() const noexcept { return _active; }

  void setShard(PregelShard shard) noexcept { _shard = shard; }
  [[nodiscard]] PregelShard shard() const noexcept { return _shard; }

  // TODO: maybe we should hence use the construtor to set them
  //       at creation time and not any time later
  void setKey(char const* key, size_t keyLength) noexcept {
    _key = std::string(key, keyLength);
  }

  [[nodiscard]] std::string_view key() const { return std::string_view(_key); }
  V const& data() const& { return _data; }
  V& data() & { return _data; }

  [[nodiscard]] VertexID pregelId() const { return VertexID{_shard, _key}; }

  std::experimental::pmr::string _key;
  PregelShard _shard;
  std::experimental::pmr::vector<Edge<E>> _edges;
//  std::vector<Edge<E>> _edges;
  bool _active;
  V _data;
};

template<typename V, typename E, typename Inspector>
auto inspect(Inspector& f, Vertex<V, E>& v) {
  return f.object(v).fields(f.field("key", v._key), f.field("shard", v._shard),
                            f.field("active", v._active),
                            f.field("edges", v._edges),
                            f.field("data", v._data));
}

}  // namespace arangodb::pregel
