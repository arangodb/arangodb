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

#ifndef ARANGODB_PREGEL_COMPUTATION_H
#define ARANGODB_PREGEL_COMPUTATION_H 1

#include <algorithm>
#include <cstddef>
#include "Basics/Common.h"
#include "Pregel/Graph.h"
#include "Pregel/GraphStore.h"
#include "Pregel/OutgoingCache.h"
#include "Pregel/WorkerConfig.h"
#include "Pregel/WorkerContext.h"

namespace arangodb {
namespace pregel {

template <typename V, typename E, typename M>
class Worker;
class IAggregator;

template <typename V, typename E, typename M>
class VertexContext {
  friend class Worker<V, E, M>;

  uint64_t _gss = 0;
  uint64_t _lss = 0;
  WorkerContext* _context = nullptr;
  GraphStore<V, E>* _graphStore = nullptr;
  AggregatorHandler* _readAggregators = nullptr;
  AggregatorHandler* _writeAggregators = nullptr;
  Vertex<V, E>* _vertexEntry = nullptr;

 public:
  virtual ~VertexContext() = default;

  template <typename T>
  inline void aggregate(std::string const& name, T const& value) {
    T const* ptr = &value;
    _writeAggregators->aggregate(name, ptr);
  }

  template <typename T>
  inline const T* getAggregatedValue(std::string const& name) {
    return (const T*)_readAggregators->getAggregatedValue(name);
  }

  IAggregator const* getReadAggregator(std::string const& name) {
    return _readAggregators->getAggregator(name);
  }

  IAggregator* getWriteAggregator(std::string const& name) {
    return _writeAggregators->getAggregator(name);
  }

  inline WorkerContext const* context() const { return _context; }

  V* mutableVertexData() {
    return &(_vertexEntry->data());
  }

  V vertexData() const { return _vertexEntry->data(); }

  size_t getEdgeCount() const { return _vertexEntry->getEdgeCount(); }

  RangeIterator<Edge<E>> getEdges() const {
    return _graphStore->edgeIterator(_vertexEntry);
  }

  void setVertexData(V const& val) {
    _graphStore->replaceVertexData(_vertexEntry, (void*)(&val), sizeof(V));
  }

  /// store data, will potentially move the data around
  void setVertexData(void const* ptr, size_t size) {
    _graphStore->replaceVertexData(_vertexEntry, (void*)ptr, size);
  }

  void voteHalt() { _vertexEntry->setActive(false); }
  void voteActive() { _vertexEntry->setActive(true); }
  bool isActive() { return _vertexEntry->active(); }

  inline uint64_t globalSuperstep() const { return _gss; }
  inline uint64_t localSuperstep() const { return _lss; }

  PregelShard shard() const { return _vertexEntry->shard(); }
  velocypack::StringRef key() const { return _vertexEntry->key(); }
  PregelID pregelId() const { return _vertexEntry->pregelId(); }
};

template <typename V, typename E, typename M>
class VertexComputation : public VertexContext<V, E, M> {
  friend class Worker<V, E, M>;
  OutCache<M>* _cache = nullptr;
  bool _enterNextGSS = false;

 public:
  virtual ~VertexComputation() = default;

  void sendMessage(Edge<E> const* edge, M const& data) {
    _cache->appendMessage(edge->targetShard(), edge->toKey(), data);
  }

  void sendMessage(PregelID const& pid, M const& data) {
    _cache->appendMessage(pid.shard, velocypack::StringRef(pid.key), data);
  }

  /// Send message along outgoing edges to all reachable neighbours
  /// TODO Multi-receiver messages
  void sendMessageToAllNeighbours(M const& data) {
    RangeIterator<Edge<E>> edges = this->getEdges();
    for (; edges.hasMore(); ++edges) {
      Edge<E> const* edge = *edges;
      _cache->appendMessage(edge->targetShard(), edge->toKey(), data);
    }
  }

  /// Causes messages to be available in GSS+1.
  /// Only valid in async mode, a no-op otherwise
  void enterNextGlobalSuperstep() {
    // _enterNextGSS is true when we are not in async mode
    // making this a no-op
    if (!_enterNextGSS) {
      _enterNextGSS = true;
      _cache->sendToNextGSS(true);
    }
  }

  virtual void compute(MessageIterator<M> const& messages) = 0;
};

template <typename V, typename E, typename M>
class VertexCompensation : public VertexContext<V, E, M> {
  friend class Worker<V, E, M>;

 public:
  virtual ~VertexCompensation() = default;
  virtual void compensate(bool inLostPartition) = 0;
};
}  // namespace pregel
}  // namespace arangodb
#endif
