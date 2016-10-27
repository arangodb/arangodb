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
#include "GraphFormat.h"

struct TRI_vocbase_t;
namespace arangodb {
class SingleCollectionTransaction;
namespace pregel {

/// @brief header entry for the edge file
template <typename E>
struct EdgeEntry {;

 private:
  // size_t _nextEntryOffset;
  // size_t _dataSize;
  std::string _toVertexID;
  E _data;
  // size_t _vertexIDSize;
  // char _vertexID[1];

 public:
  // EdgeEntry() : _nextEntryOffset(0), _dataSize(0), _vertexIDSize(0) {}
    EdgeEntry(std::string const& vid, E const& data) : _toVertexID(vid), _data(data) {}
  ~EdgeEntry() {}

  // size_t getSize() { return sizeof(EdgeEntry) + _vertexIDSize + _dataSize; }
  std::string toVertexID() { return _toVertexID; }
  // size_t getDataSize() { return _dataSize; }
  E* getData() {
    return &_data;  // static_cast<E>(this + sizeof(EdgeEntry) + _vertexIDSize);
  }
};

template <typename E>
class EdgeIterator {
 private:
  // void *_begin, *_end, *_current;
  std::vector<EdgeEntry<E>>& _edges;
  size_t _begin, _end, _current;

 public:
  typedef EdgeIterator<E> iterator;
  typedef const EdgeIterator<E> const_iterator;

  EdgeIterator(std::vector<EdgeEntry<E>>& edges, size_t begin, size_t end)
      : _edges(edges), _begin(begin), _end(end), _current(0) {}

  iterator begin() { return EdgeIterator(_edges, _begin, _end); }
  const_iterator begin() const { return EdgeIterator(_edges, _begin, _end); }
  iterator end() {
    auto it = EdgeIterator(_edges, _begin, _end);
    it._current = it._end;
    return it;
  }
  const_iterator end() const {
    auto it = EdgeIterator(_edges, _begin, _end);
    it._current = it._end;
    return it;
  }

  // prefix ++
  EdgeIterator& operator++() {
    _current++;
    return *this;
  }

  // postfix ++
  EdgeIterator<E>& operator++(int) {
    EdgeIterator result(*this);
    ++(*this);
    return result;
  }

  EdgeEntry<E>* operator*() const {
      EdgeEntry<E>* el = _edges.data();
      if (_current != _end) return el + _current;
      else return nullptr;
  }

  bool operator!=(EdgeIterator const& other) const {
    return _current != other._current;
  }

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
  size_t _vertexDataOffset;
  size_t _edgeDataOffset;
  size_t _edgeCount;
  bool _active;
  std::string _vertexID;
  // size_t _vertexIDSize;
  // char _vertexID[1];

 public:
  VertexEntry()
      : _vertexDataOffset(0),
        _edgeDataOffset(0),
        _active(true) {}  //_vertexIDSize(0)

  inline size_t getVertexDataOffset() { return _vertexDataOffset; }
  inline size_t getEdgeDataOffset() { return _edgeDataOffset; }
  // inline size_t getSize() { return sizeof(VertexEntry) + _vertexIDSize; }
  inline size_t getSize() { return sizeof(VertexEntry); }
  inline bool active() { return _active; }
  inline void setActive(bool bb) { _active = bb; }
  std::string const& vertexID() {
    return _vertexID /* std::string(_vertexID, _vertexIDSize)*/;
  };
};

// unused right now
class VertexIterator {
 private:
  intptr_t _begin, _end, _current;

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
};

////////////////////////////////////////////////////////////////////////////////
/// @brief carry graph data for a worker job
////////////////////////////////////////////////////////////////////////////////
template <typename V, typename E>
class GraphStore {
 private:
  // int _indexFd, _vertexFd, _edgeFd;
  void *_indexMapping, *_vertexMapping, *_edgeMapping;
  size_t _indexSize, _vertexSize, _edgeSize;

  std::vector<SingleCollectionTransaction*> _readTrxList;
  void lookupVertices(ShardID const& vertexShard, TRI_vocbase_t* vocbase,
                      std::unique_ptr<GraphFormat<V, E>> const& graphFormat);
  void lookupEdges(std::string vertexCollectionName, ShardID const& edgeShardID,
                   TRI_vocbase_t* vocbase,
                   std::unique_ptr<GraphFormat<V, E>> const& graphFormat);
  void cleanupReadTransactions();

  // only for demo, move to memory
  std::vector<VertexEntry> _index;
  std::vector<V> _vertices;
  std::vector<EdgeEntry<E>> _edges;

 public:
    
  GraphStore(std::string const& vertexCollectionName,
             std::vector<ShardID> const& vertexShards,
             std::vector<ShardID> const& edgeShards,
             TRI_vocbase_t* vocbase,
             std::unique_ptr<GraphFormat<V, E>> const& graphFormat);
  ~GraphStore();

  typedef std::vector<VertexEntry> VertexIterator;

  VertexIterator vertexIterator();
  V* vertexData(VertexEntry const* entry);
  V vertexDataCopy(VertexEntry const* entry);
  void replaceVertexData(VertexEntry const* entry, V* data);
  EdgeIterator<E> edgeIterator(VertexEntry const* entry);
};
}
}
#endif
