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

#ifndef ARANGODB_PREGEL_GRAPH_STORE_H
#define ARANGODB_PREGEL_GRAPH_STORE_H 1

#include <cstdint>
#include <cstdio>
#include <set>
#include "Cluster/ClusterInfo.h"
#include "VocBase/voc-types.h"
#include "Pregel/GraphFormat.h"
#include "Pregel/Iterators.h"

struct TRI_vocbase_t;

namespace arangodb {
class Transaction;
class LogicalCollection;
namespace pregel {

typedef uint16_t prgl_shard_t;
  
/// @brief header entry for the edge file
template <typename E>
class Edge {
  prgl_shard_t _sourceShard;
  prgl_shard_t _targetShard;
  std::string _toKey;
  E _data;
  
 public:
  // EdgeEntry() : _nextEntryOffset(0), _dataSize(0), _vertexIDSize(0) {}
  Edge(prgl_shard_t source,
       prgl_shard_t target,
       std::string const& key)
  : _sourceShard(source),
    _targetShard(target),
    _toKey(key) {}

  // size_t getSize() { return sizeof(EdgeEntry) + _vertexIDSize + _dataSize; }
  std::string const& toKey() const { return _toKey; }
  // size_t getDataSize() { return _dataSize; }
  inline E* data() {
    return &_data;  // static_cast<E>(this + sizeof(EdgeEntry) + _vertexIDSize);
  }
  inline prgl_shard_t sourceShard() const {
    return _sourceShard;
  }
  inline prgl_shard_t targetShard() const {
    return _targetShard;
  }
};

class VertexEntry {
  template <typename V, typename E>
  friend class GraphStore;

  const prgl_shard_t _shard;// TODO optimize and remove
  const std::string _key;
  size_t _vertexDataOffset;  // size_t vertexID length
  size_t _edgeDataOffset;
  size_t _edgeCount;
  bool _active = true;

 public:
  VertexEntry(prgl_shard_t shard, std::string const& key)
      : _shard(shard),
        _key(key),
        _vertexDataOffset(0),
        _edgeDataOffset(0),
        _edgeCount(0),
        _active(true) {}  //_vertexIDSize(0)

  inline size_t getVertexDataOffset() const { return _vertexDataOffset; }
  inline size_t getEdgeDataOffset() const { return _edgeDataOffset; }
  // inline size_t getSize() { return sizeof(VertexEntry) + _vertexIDSize; }
  inline size_t getSize() { return sizeof(VertexEntry); }
  inline bool active() const { return _active; }
  inline void setActive(bool bb) { _active = bb; }
  
  inline prgl_shard_t shard() const {return _shard;}
  inline std::string const& key() const { return _key; };
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
  
class WorkerState;
template <typename V, typename E>
struct GraphFormat;

////////////////////////////////////////////////////////////////////////////////
/// @brief carry graph data for a worker job
////////////////////////////////////////////////////////////////////////////////
template <typename V, typename E>
class GraphStore {
  
  VocbaseGuard _vocbaseGuard;
  const std::unique_ptr<GraphFormat<V, E>> _graphFormat;
  Transaction *_transaction;// temporary transaction
  
  // int _indexFd, _vertexFd, _edgeFd;
  // void *_indexMapping, *_vertexMapping, *_edgeMapping;
  // size_t _indexSize, _vertexSize, _edgeSize;
  // std::map<std::string, std::string> _shardsPlanIdMap;

  // only for demo, move to memory
  std::vector<VertexEntry> _index;
  std::vector<V> _vertexData;
  std::vector<Edge<E>> _edges;

  std::set<ShardID> _loadedShards;
  size_t _localVerticeCount;
  size_t _localEdgeCount;
  
  void _cleanupTransactions();
  void _loadVertices(WorkerState const& state,
                     ShardID const& vertexShard,
                     ShardID const& edgeShard);
  void _loadEdges(WorkerState const& state,
                  ShardID const& shard,
                  VertexEntry& vertexEntry,
                  std::string const& documentID);

 public:
  GraphStore(TRI_vocbase_t* vocbase, WorkerState const& state,
             GraphFormat<V, E>* graphFormat);
  ~GraphStore();

  void loadShards(WorkerState const& state);
  inline size_t vertexCount() {
    return _index.size();
  }
  RangeIterator<VertexEntry> vertexIterator();
  RangeIterator<VertexEntry> vertexIterator(size_t start, size_t count);
  RangeIterator<Edge<E>> edgeIterator(VertexEntry const* entry);

  void* mutableVertexData(VertexEntry const* entry);
  V copyVertexData(VertexEntry const* entry);
  void replaceVertexData(VertexEntry const* entry, void* data, size_t size);
  
  /// Write results to database
  void storeResults(WorkerState const& state);
};
}
}
#endif
