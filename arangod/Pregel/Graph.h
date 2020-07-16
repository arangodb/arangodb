////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_PREGEL_GRAPH_STRUCTURE_H
#define ARANGODB_PREGEL_GRAPH_STRUCTURE_H 1

#include <velocypack/StringRef.h>

#include <cstdint>
#include <functional>
#include <string>

namespace arangodb {
namespace pregel {

typedef uint16_t PregelShard;
const PregelShard InvalidPregelShard = -1;

struct PregelID {
  std::string key; 
  PregelShard shard;

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

  bool isValid() const {
    return shard != InvalidPregelShard && !key.empty();
  }
};

template <typename V, typename E>
class GraphStore;

// header entry for the edge file
template <typename E>
// cppcheck-suppress noConstructor
class Edge {
  template <typename V, typename E2>
  friend class GraphStore;

  static_assert(sizeof(std::string) > 2, "");

  // these members are initialized by the GrapStore
  char* _toKey;             // uint64_t
  uint16_t _toKeyLength;    // uint16_t
  PregelShard _targetShard; // uint16_t

  E _data;

 public:

  velocypack::StringRef toKey() const { return velocypack::StringRef(_toKey, _toKeyLength); }
  E& data() {
    return _data;  
  }
  // PregelShard sourceShard() const { return _sourceShard; }
  PregelShard targetShard() const { return _targetShard; }
};

template <typename V, typename E>
// cppcheck-suppress noConstructor
class Vertex {
  friend class GraphStore<V,E>;
  
  const char* _key;
  
  // these members are initialized by the GrapStore
  Edge<E>* _edges;
  size_t _edgeCount;
  
  uint16_t _keyLength;
  PregelShard _shard;
  
  V _data; // variable byte size
  
  bool _active = true;

 public:
  
  Edge<E>* getEdges() const { return _edges; }
  size_t getEdgeCount() const { return _edgeCount; }
  
  bool active() const { return _active; }
  void setActive(bool bb) { _active = bb; }

  PregelShard shard() const { return _shard; }
  velocypack::StringRef key() const { return velocypack::StringRef(_key, _keyLength); };
  V const& data() const& { return _data; }
  V& data() & { return _data; }
  
  PregelID pregelId() const { return PregelID(_shard, std::string(_key, _keyLength)); }
};

}  // namespace pregel
}  // namespace arangodb

namespace std {
template <>
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

#endif
