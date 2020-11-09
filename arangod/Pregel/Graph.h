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
  std::string key; // std::string 24
  PregelShard shard; // uint16_t

  PregelID() : key(""), shard(InvalidPregelShard) {}
  PregelID(PregelShard s, std::string const& k) : key(k), shard(s) {}
  // PregelID(PregelShard s, std::string const& k) : shard(s),
  // key(std::stoull(k)) {}

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

  // these members are initialized by the GraphStore
  char* _toKey;             // uint64_t
  uint16_t _toKeyLength;    // uint16_t
  PregelShard _targetShard; // uint16_t

  E _data;

 public:

  velocypack::StringRef toKey() const { return velocypack::StringRef(_toKey, _toKeyLength); }
  E& data() {
    return _data;
  }
  PregelShard targetShard() const { return _targetShard; }
};

template <typename V, typename E>
// cppcheck-suppress noConstructor
class Vertex {
  friend class GraphStore<V,E>;
  
  char const* _key; // uint64_t
  
  // these members are initialized by the GraphStore
  Edge<E>* _edges; // uint64_t
  size_t _edgeCount; // uint64_t
  
  uint16_t _keyLength; // uint16_t
  PregelShard _shard; // uint16_t

  bool _active;
  
  V _data; // variable byte size

 public:

  Vertex() noexcept 
    : _key(nullptr), 
      _edges(nullptr), 
      _edgeCount(0), 
      _keyLength(0), 
      _shard(InvalidPregelShard),
      _active(true) {}

  // note: the destructor for this type is never called,
  // so it must not allocate any memory or take ownership
  // of anything
  
  Edge<E>* getEdges() const { return _edges; }
  size_t getEdgeCount() const { return _edgeCount; }
  
  void setActive(bool bb) { _active = bb; }
  bool active() const { return _active; }

  void setShard(PregelShard shard) { _shard = shard; }
  PregelShard shard() const { return _shard; }

  void setKey(char const* key, uint16_t keyLength) {
    _key = key;
    _keyLength = keyLength;
  }

  velocypack::StringRef key() const { return velocypack::StringRef(_key, _keyLength); }
  V const& data() const& { return _data; }
  V& data() & { return _data; }
  
  PregelID pregelId() const { return PregelID(_shard, std::string(_key, _keyLength)); }
};

// unused right now
/*class LinkedListIterator {
 private:
  intptr_t _begin, _end, _current;

  VertexIterator(const VertexIterator&) = delete;
  VertexIterator& operator=(const FileInfo&) = delete;

 public:
  typedef VertexIterator iterator;
  typedef const VertexIterator const_iterator;

  VertexIterator(intptr_t beginPtr, intptr_t endPtr)
      : _begin(beginPtr), _end(endPtr), _current(beginPtr) {}

  iterator begin() { return VertexIterator(_begin, _end); }
  const_iterator begin() const { return VertexIterator(_begin, _end); }
  iterator end() {
    auto it = VertexIterator(_begin, _end);
    it._current = it._end;
    return it;
  }
  const_iterator end() const {
    auto it = VertexIterator(_begin, _end);
    it._current = it._end;
    return it;
  }

  // prefix ++
  VertexIterator& operator++() {
    VertexEntry* entry = (VertexEntry*)_current;
    _current += entry->getSize();
    return *this;
  }

  // postfix ++
  VertexIterator& operator++(int) {
    VertexEntry* entry = (VertexEntry*)_current;
    _current += entry->getSize();
    return *this;
  }

  VertexEntry* operator*() const {
    return _current != _end ? (VertexEntry*)_current : nullptr;
  }

  bool operator!=(VertexIterator const& other) const {
    return _current != other._current;
  }
};*/
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
