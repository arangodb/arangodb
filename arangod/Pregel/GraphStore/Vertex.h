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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Assertions/Assert.h"

#include "Edge.h"
#include "VertexID.h"

namespace arangodb::pregel {

template<typename V, typename E>
// cppcheck-suppress noConstructor
class Vertex {
  char const* _key;  // uint64_t

  // these members are initialized by the GraphStore
  Edge<E>* _edges;  // uint64_t

  // the number of edges per vertex is limited to 4G.
  // this should be more than enough
  uint32_t _edgeCount;  // uint32_t

  // combined uint16_t attribute fusing the active bit
  // and the length of the key in its other 15 bits. we do this
  // to save RAM/space (we may have _lots_ of vertices).
  // according to cppreference.com it is implementation-defined
  // if multiple variables in a bitfield are tightly packed or
  // not. in order to protect us from compilers that don't tightly
  // pack bitfield variables, we validate the size of the struct
  // via static_assert in the constructor of Vertex<V, E>.
  uint16_t _active : 1;      // uint16_t (shared with _keyLength)
  uint16_t _keyLength : 15;  // uint16_t (shared with _active)

  PregelShard _shard;  // uint16_t

  V _data;  // variable byte size

 public:
  Vertex() noexcept
      : _key(nullptr),
        _edges(nullptr),
        _edgeCount(0),
        _active(1),
        _keyLength(0),
        _shard(InvalidPregelShard) {
    TRI_ASSERT(keyLength() == 0);
    TRI_ASSERT(active());

    // make sure that Vertex has the smallest possible size, especially
    // that the bitfield for _acitve and _keyLength takes up only 16 bits in
    // total.
    static_assert(sizeof(Vertex<V, E>) ==
                      sizeof(char const*) + sizeof(Edge<E>*) +
                          sizeof(uint32_t) +
                          sizeof(uint16_t) +  // combined size of the bitfield
                          sizeof(PregelShard) + std::max<size_t>(8U, sizeof(V)),
                  "invalid size of Vertex");
  }

  // note: the destructor for this type is never called,
  // so it must not allocate any memory or take ownership
  // of anything

  Edge<E>* getEdges() const noexcept { return _edges; }

  // adds an edge for the vertex. returns the number of edges
  // after the addition. note that the caller must make sure that
  // we don't end up with more than 4GB edges per verte.
  size_t addEdge(Edge<E>* edge) noexcept {
    // must only be called during initial vertex creation
    TRI_ASSERT(active());
    TRI_ASSERT(_edgeCount < maxEdgeCount());

    if (_edges == nullptr) {
      _edges = edge;
    }
    return static_cast<size_t>(++_edgeCount);
  }

  // returns the number of associated edges
  [[nodiscard]] size_t getEdgeCount() const noexcept {
    return static_cast<size_t>(_edgeCount);
  }

  // maximum number of edges that can be added for each vertex
  static constexpr size_t maxEdgeCount() {
    return static_cast<size_t>(
        std::numeric_limits<decltype(_edgeCount)>::max());
  }

  void setActive(bool bb) noexcept {
    _active = bb ? 1 : 0;
    TRI_ASSERT((bb && active()) || (!bb && !active()));
  }

  [[nodiscard]] bool active() const noexcept { return _active == 1; }

  void setShard(PregelShard shard) noexcept { _shard = shard; }
  [[nodiscard]] PregelShard shard() const noexcept { return _shard; }

  void setKey(char const* key, uint16_t keyLength) noexcept {
    // must only be called during initial vertex creation
    TRI_ASSERT(active());
    TRI_ASSERT(this->keyLength() == 0);
    _key = key;
    _keyLength = keyLength;
    TRI_ASSERT(active());
    TRI_ASSERT(this->keyLength() == keyLength);
  }

  [[nodiscard]] uint16_t keyLength() const noexcept { return _keyLength; }

  [[nodiscard]] std::string_view key() const {
    return std::string_view(_key, keyLength());
  }
  V const& data() const& { return _data; }
  V& data() & { return _data; }

  [[nodiscard]] VertexID pregelId() const {
    return VertexID{_shard, std::string(_key, keyLength())};
  }
};

}  // namespace arangodb::pregel
