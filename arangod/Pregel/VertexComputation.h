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

#include <cstddef>
#include "Basics/Common.h"
#include "GraphStore.h"
#include "OutgoingCache.h"
#include "WorkerContext.h"

#ifndef ARANGODB_PREGEL_COMPUTATION_H
#define ARANGODB_PREGEL_COMPUTATION_H 1

namespace arangodb {
namespace pregel {

template <typename V, typename E, typename M>
class Worker;
class Aggregator;

template <typename V, typename E, typename M>
class VertexComputation {
  friend class Worker<V, E, M>;

  uint64_t _gss = 0;
  WorkerContext* _context;
  GraphStore<V, E>* _graphStore;
  OutgoingCache<M>* _outgoing;
  const AggregatorUsage* _conductorAggregators;
  AggregatorUsage* _workerAggregators;
  VertexEntry* _vertexEntry;

 public:
  template <typename T>
  inline void aggregate(std::string const& name, const T* valuePtr) {
    _workerAggregators->aggregate(name, valuePtr);
  }

  template <typename T>
  inline const T* getAggregatedValue(std::string const& name) {
    return (const T*)_conductorAggregators->getAggregatedValue(name);
  }

  inline WorkerContext const* context() { return _context; }

  template<typename T>
  T* mutableVertexData() {
    return (T*) _graphStore->mutableVertexData(_vertexEntry);
  }

  V vertexData() { return _graphStore->copyVertexData(_vertexEntry); }

  EdgeIterator<E> getEdges() { return _graphStore->edgeIterator(_vertexEntry); }

  /// store data, will potentially move the data around
  void setVertexData(void const* ptr, size_t size) {
    _graphStore->replaceVertexData(_vertexEntry, (void*)ptr, size);
  }

  void sendMessage(std::string const& toVertexID, M const& data) {
    _outgoing->sendMessageTo(toVertexID, data);
  }

  void voteHalt() { _vertexEntry->setActive(false); }

  inline uint64_t globalSuperstep() const { return _gss; }

  virtual void compute(std::string const& vertexID,
                       MessageIterator<M> const& messages) = 0;
};
}
}
#endif
