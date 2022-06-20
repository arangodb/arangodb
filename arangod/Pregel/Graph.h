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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/debugging.h"

#include <velocypack/Slice.h>

#include <cstdint>
#include <functional>
#include <string>

namespace arangodb {
namespace pregel {

typedef uint16_t PregelShard;
const PregelShard InvalidPregelShard = -1;

struct PregelID {
  std::string key;    // std::string 24
  PregelShard shard;  // uint16_t

  PregelID() : key(""), shard(InvalidPregelShard) {}
  PregelID(PregelShard s, std::string const& k) : key(k), shard(s) {}

  bool operator==(const PregelID& rhs) const {
    return shard == rhs.shard && key == rhs.key;
  }

  bool operator!=(const PregelID& rhs) const {
    return shard != rhs.shard || key != rhs.key;
  }

  bool operator<(const PregelID& rhs) const {
    return shard < rhs.shard || (shard == rhs.shard && key < rhs.key);
  }

  bool isValid() const { return shard != InvalidPregelShard && !key.empty(); }
};

template<typename V, typename E>
class GraphStore;

// header entry for the edge file
template<typename E>
// cppcheck-suppress noConstructor
class Edge {
  template<typename V, typename E2>
  friend class GraphStore;

  std::string _toKey;
  PregelShard _targetShard;

  E _data;

 public:
  void setToKey(std::string_view toKey) { _toKey = toKey; }
  std::string_view toKey() const { return std::string_view(_toKey); }
  void setTargetShard(PregelShard targetShard) { _targetShard = targetShard; }
  PregelShard targetShard() const noexcept { return _targetShard; }

  E const& data() const noexcept { return _data; }
  E& data_mut() noexcept { return _data; }
};

template<typename V, typename E>
// cppcheck-suppress noConstructor
class Vertex {
  std::string _key;

  std::vector<Edge<E>> _edges;

  // combined uint16_t attribute fusing the active bit
  // and the length of the key in its other 15 bits. we do this
  // to save RAM/space (we may have _lots_ of vertices).
  // according to cppreference.com it is implementation-defined
  // if multiple variables in a bitfield are tightly packed or
  // not. in order to protect us from compilers that don't tightly
  // pack bitfield variables, we validate the size of the struct
  // via static_assert in the constructor of Vertex<V, E>.
  uint16_t _active : 1;

  PregelShard _shard;

  V _data;

 public:
  Vertex() noexcept : _key(), _edges(), _active(1), _shard(InvalidPregelShard) {
    TRI_ASSERT(keyLength() == 0);
    TRI_ASSERT(active());
  }

  // note: the destructor for this type is never called,
  // so it must not allocate any memory or take ownership
  // of anything

  std::vector<Edge<E>> const& getEdges() const noexcept { return _edges; }

  // adds an edge for the vertex. returns the number of edges
  // after the addition. note that the caller must make sure that
  // we don't end up with more than 4GB edges per verte.
  size_t emplaceEdge(Edge<E>&& edge) noexcept {
    // must only be called during initial vertex creation
    TRI_ASSERT(active());
    _edges.emplace_back(std::move(edge));

    return _edges.size();
  }

  // returns the number of associated edges
  size_t getEdgeCount() const noexcept { return _edges.size(); }

  void setActive(bool bb) noexcept {
    _active = bb ? 1 : 0;
    TRI_ASSERT((bb && active()) || (!bb && !active()));
  }

  bool active() const noexcept { return _active == 1; }

  void setShard(PregelShard shard) noexcept { _shard = shard; }
  PregelShard shard() const noexcept { return _shard; }

  void setKey(std::string_view key) noexcept { _key = key; }
  void setKey(char const* key, uint16_t keyLength) noexcept {
    ADB_PROD_ASSERT(this->keyLength() == 0);
    _key = std::string(key, keyLength);
  }

  uint16_t keyLength() const noexcept { return _key.size(); }

  std::string_view key() const { return std::string_view(_key); }
  V const& data() const& { return _data; }
  V& data() & { return _data; }

  PregelID pregelId() const { return PregelID(_shard, _key); }
};

}  // namespace pregel
}  // namespace arangodb

namespace std {
template<>
struct hash<arangodb::pregel::PregelID> {
  std::size_t operator()(const arangodb::pregel::PregelID& k) const noexcept {
    using std::hash;
    using std::size_t;
    using std::string;

    // Compute individual hash values for first,
    // second and third and combine them using XOR
    // and bit shifting:
    size_t h1 = std::hash<std::string>()(k.key);
    size_t h2 = std::hash<size_t>()(k.shard);
    return h2 ^ (h1 << 1);
  }
};
}  // namespace std
