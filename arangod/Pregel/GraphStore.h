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
#include "Cluster/ClusterInfo.h"
#include "VocBase/voc-types.h"
#include "GraphFormat.h"

struct TRI_vocbase_t;

namespace arangodb {
class Transaction;
class LogicalCollection;
namespace pregel {

/// @brief header entry for the edge file
template <typename E>
struct EdgeEntry {

 private:
  // size_t _nextEntryOffset;
  // size_t _dataSize;
  std::string _toVertexID;
  TRI_voc_cid_t sourceShardID;
  TRI_voc_cid_t targetShardID;
  E _data;
  // size_t _vertexIDSize;
  // char _vertexID[1];

 public:
  // EdgeEntry() : _nextEntryOffset(0), _dataSize(0), _vertexIDSize(0) {}
  EdgeEntry(std::string const& vid, E const& data)
      : _toVertexID(vid), _data(data) {}
  EdgeEntry(std::string const& vid) : _toVertexID(vid) {}
  ~EdgeEntry() {}

  // size_t getSize() { return sizeof(EdgeEntry) + _vertexIDSize + _dataSize; }
  std::string const& toVertexID() { return _toVertexID; }
  // size_t getDataSize() { return _dataSize; }
  E* getData() {
    return &_data;  // static_cast<E>(this + sizeof(EdgeEntry) + _vertexIDSize);
  }
};

template <typename T>
class RangeIterator {
 private:
  // void *_begin, *_end, *_current;
  std::vector<T>& _vector;
  size_t _begin, _end, _current;

 public:
  typedef RangeIterator<T> iterator;
  typedef const RangeIterator<T> const_iterator;

  RangeIterator(std::vector<T>& v, size_t begin, size_t end)
      : _vector(v), _begin(begin), _end(end), _current(begin) {}

  iterator begin() { return RangeIterator(_vector, _begin, _end); }
  const_iterator begin() const { return RangeIterator(_vector, _begin, _end); }
  iterator end() {
    auto it = RangeIterator(_vector, _begin, _end);
    it._current = it._end;
    return it;
  }
  const_iterator end() const {
    auto it = RangeIterator(_vector, _begin, _end);
    it._current = it._end;
    return it;
  }

  // prefix ++
  RangeIterator& operator++() {
    _current++;
    return *this;
  }

  // postfix ++
  RangeIterator<T>& operator++(int) {
    RangeIterator<T> result(*this);
    ++(*this);
    return result;
  }

  T* operator*() const {
    T* el = _vector.data();
    return _current != _end ? el + _current : nullptr;
  }

  bool operator!=(RangeIterator<T> const& other) const {
    return _current != other._current;
  }

  size_t size() const { return _end - _begin; }

  /*EdgeIterator(void* beginPtr, void* endPtr)
      : _begin(beginPtr), _end(endPtr), _current(_begin) {}
  iterator begin() { return EdgeIterator(_begin, _end); }
  const_iterator begin() const { return EdgeIterator(_begin, _end); }
  iterator end() {
    auto it = EdgeIterator(_begin, _end);
    it._current = it._end;
    return it;
  }
  const_iterator end() const {
    auto it = EdgeIterator(_begin, _end);
    it._current = it._end;
    return it;
  }

  // prefix ++
  EdgeIterator<E>& operator++() {
    EdgeEntry<E>* entry = static_cast<EdgeEntry<E>>(_current);
    _current += entry->getSize();
    return *this;
  }

  EdgeEntry<E>* operator*() const {
    return _current != _end ? static_cast<EdgeEntry<E>>(_current) : nullptr;
  }*/
};

struct VertexEntry {
  template <typename V, typename E>
  friend class GraphStore;

 private:
  const std::string _vertexID;
  size_t _vertexDataOffset;  // size_t vertexID length
  size_t _edgeDataOffset;
  size_t _edgeCount;
  bool _active = true;

 public:
  VertexEntry(std::string const& vid)
      : _vertexID(vid),
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
  inline std::string const& vertexID() const { return _vertexID; };
  /*std::string const& vertexID() const {
    return std::string(_vertexID, _vertexIDSize);
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

////////////////////////////////////////////////////////////////////////////////
/// @brief carry graph data for a worker job
////////////////////////////////////////////////////////////////////////////////
template <typename V, typename E>
class GraphStore {
 private:
  // int _indexFd, _vertexFd, _edgeFd;
  // void *_indexMapping, *_vertexMapping, *_edgeMapping;
  // size_t _indexSize, _vertexSize, _edgeSize;
  // std::map<std::string, std::string> _shardsPlanIdMap;

  // only for demo, move to memory
  std::vector<VertexEntry> _index;
  std::vector<V> _vertexData;
  std::vector<EdgeEntry<E>> _edges;

  size_t _localVerticeCount;
  size_t _localEdgeCount;

  VocbaseGuard _vocbaseGuard;
  const WorkerState* _workerState;
  const std::unique_ptr<GraphFormat<V, E>> _graphFormat;

  Transaction *_transaction;
  //SingleCollectionTransaction* readTransaction(ShardID const& shard);
  //SingleCollectionTransaction* writeTransaction(ShardID const& shard);
  void cleanupTransactions();
  
  void loadVertices(ShardID const& vertexShard, ShardID const& edgeShard);
  void loadEdges(ShardID const& edgeShard, VertexEntry& entry);

 public:
  GraphStore(TRI_vocbase_t* vocbase, const WorkerState* state,
             GraphFormat<V, E>* graphFormat);
  ~GraphStore();

  inline size_t vertexCount() {
    return _index.size();
  }
  RangeIterator<VertexEntry> vertexIterator(size_t start, size_t count);
  RangeIterator<EdgeEntry<E>> edgeIterator(VertexEntry const* entry);

  void* mutableVertexData(VertexEntry const* entry);
  V copyVertexData(VertexEntry const* entry);
  void replaceVertexData(VertexEntry const* entry, void* data, size_t size);

  void storeResults();
};
}
}
#endif
