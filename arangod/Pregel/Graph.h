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

#include <cstdint>
#include <functional>
#include <string>

#include "Basics/StringRef.h"

namespace arangodb {
namespace pregel {

// typedef uint64_t PregelKey;
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

  bool operator<(const PregelID& rhs) const {
    return shard < rhs.shard || (shard == rhs.shard && key < rhs.key);
  }

  bool isValid() const {
    return shard != InvalidPregelShard && !key.empty();
  }
};

template <typename V, typename E>
class GraphStore;

/// @brief header entry for the edge file
template <typename E>
class Edge {
  template <typename V, typename E2>
  friend class GraphStore;

  static_assert(sizeof(std::string) > 2, "");
  char* _toKey;             // uint64_t
  size_t _toKeyLength;      // uint64_t
  // PregelShard _sourceShard;
  PregelShard _targetShard; // uint16_t

  E _data;

 public:
  // EdgeEntry() : _nextEntryOffset(0), _dataSize(0), _vertexIDSize(0) {}
//  Edge() : _targetShard(InvalidPregelShard), _data(0) {}
//  Edge(PregelShard target, PregelKey const& key)
//      : _targetShard(target), _toKey(key), _data(0) {}

  // size_t getSize() { return sizeof(EdgeEntry) + _vertexIDSize + _dataSize; }
  StringRef toKey() const { return StringRef(_toKey, _toKeyLength); }
  // size_t getDataSize() { return _dataSize; }
  E& data() {
    return _data;  // static_cast<E>(this + sizeof(EdgeEntry) + _vertexIDSize);
  }
  // PregelShard sourceShard() const { return _sourceShard; }
  PregelShard targetShard() const { return _targetShard; }
};

template <typename V>
class Vertex {
  template <typename V2, typename E>
  friend class GraphStore;
  
  const char* _key; // uint64_t
  size_t _keyLength; // uint64_t
  
  size_t _edgeBufferIdx; // uint64_t
  size_t _edgeOffset; // uint64_t
  size_t _edgeCount; // uint64_t
  
  PregelShard _shard; // uint16_t
  
  V _data; // variable byte size
  
  bool _active = true; // bool8_t

 public:
//  Vertex() : _shard(InvalidPregelShard) {}
//  Vertex(PregelShard shard, PregelKey const& key)
//    : _key(key), _shard(shard) {}

//  size_t getVertexDataOffset() const { return _vertexDataOffset; }
  
  size_t getEdgesBufferIdx() const { return _edgeBufferIdx; }
  size_t getEdgesOffset() const { return _edgeOffset; }
  size_t getEdgeCount() const { return _edgeCount; }
  
  // size_t getSize() { return sizeof(VertexEntry) + _vertexIDSize; }
  size_t getSize() const { return sizeof(Vertex); }
  bool active() const { return _active; }
  void setActive(bool bb) { _active = bb; }

  PregelShard shard() const { return _shard; }
  StringRef key() const { return StringRef(_key, _keyLength); };
  V const& data() const& { return _data; }
  V& data() & { return _data; }
  
  PregelID pregelId() const { return PregelID(_shard, std::string(_key, _keyLength)); }
  /*std::string const& key() const {
    return std::string(_key, _keySize);
  };*/
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
