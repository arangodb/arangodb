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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <algorithm>
#include <cstddef>
#include "Basics/Common.h"
#include "Pregel/GraphStore/Graph.h"
#include "Pregel/GraphStore/Quiver.h"
#include "Pregel/Iterators.h"
#include "Pregel/Worker/WorkerConfig.h"
#include "Pregel/OutgoingCache.h"
#include "Pregel/WorkerContext.h"

namespace arangodb {
namespace pregel {

template<typename V, typename E, typename M>
class Worker;
class IAggregator;

template<typename V, typename E, typename M>
class VertexContext {
  friend class Worker<V, E, M>;

 public:
  uint64_t _gss = 0;
  uint64_t _lss = 0;
  WorkerContext* _context = nullptr;
  AggregatorHandler* _readAggregators = nullptr;
  AggregatorHandler* _writeAggregators = nullptr;
  Vertex<V, E>* _vertexEntry = nullptr;

  virtual ~VertexContext() = default;

  template<typename T>
  inline void aggregate(std::string const& name, T const& value) {
    T const* ptr = &value;
    _writeAggregators->aggregate(name, ptr);
  }

  template<typename T>
  inline const T& getAggregatedValueRef(std::string const& name) {
    auto ptr = _readAggregators->getAggregatedValue(name);
    TRI_ASSERT(ptr != nullptr);
    if (ptr == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "unexpected read aggregator reference.");
    }
    return *(T const*)ptr;
  }

  IAggregator const* getReadAggregator(std::string const& name) {
    return _readAggregators->getAggregator(name);
  }

  IAggregator* getWriteAggregator(std::string const& name) {
    return _writeAggregators->getAggregator(name);
  }

  inline WorkerContext const* context() const { return _context; }

  V* mutableVertexData() { return &(_vertexEntry->data()); }

  V const& vertexData() const { return _vertexEntry->data(); }

  size_t getEdgeCount() const { return _vertexEntry->getEdgeCount(); }

  std::vector<Edge<E>>& getEdges() const { return _vertexEntry->getEdges(); }

  void voteHalt() { _vertexEntry->setActive(false); }
  void voteActive() { _vertexEntry->setActive(true); }
  bool isActive() { return _vertexEntry->active(); }

  inline uint64_t phaseGlobalSuperstep() {
    return globalSuperstep() -
           getAggregatedValueRef<uint64_t>(Utils::phaseFirstStepKey);
  }
  inline uint64_t globalSuperstep() const { return _gss; }
  inline uint64_t localSuperstep() const { return _lss; }

  PregelShard shard() const { return _vertexEntry->shard(); }
  std::string_view key() const { return _vertexEntry->key(); }
  VertexID pregelId() const { return _vertexEntry->pregelId(); }
};

template<typename V, typename E, typename M>
class VertexComputation : public VertexContext<V, E, M> {
  friend class Worker<V, E, M>;

 public:
  OutCache<M>* _cache = nullptr;

  virtual ~VertexComputation() = default;

  void sendMessage(Edge<E> const& edge, M const& data) {
    _cache->appendMessage(edge.targetShard(), edge.toKey(), data);
  }

  void sendMessage(VertexID const& pid, M const& data) {
    _cache->appendMessage(pid.shard, std::string_view(pid.key), data);
  }

  /// Send message along outgoing edges to all reachable neighbours
  /// TODO Multi-receiver messages
  void sendMessageToAllNeighbours(M const& data) {
    for (auto& edge : this->getEdges()) {
      _cache->appendMessage(edge.targetShard(), edge.toKey(), data);
    }
  }

  virtual void compute(MessageIterator<M> const& messages) = 0;
};

template<typename V, typename E, typename M>
class VertexCompensation : public VertexContext<V, E, M> {
  friend class Worker<V, E, M>;

 public:
  virtual ~VertexCompensation() = default;
  virtual void compensate(bool inLostPartition) = 0;
};
}  // namespace pregel
}  // namespace arangodb
