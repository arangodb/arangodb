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
#include "IncomingCache.h"

#ifndef ARANGODB_PREGEL_COMPUTATION_H
#define ARANGODB_PREGEL_COMPUTATION_H 1

namespace arangodb {
namespace pregel {
/*
enum VertexActivationState {
  ACTIVE,
  STOPPED
};*/

template <typename V, typename E, typename M>
class OutgoingCache;
template <typename V, typename E, typename M>
class WorkerJob;

template <typename V, typename E, typename M>
class VertexComputation {
  friend class WorkerJob<V, E, M>;

 private:
  unsigned int _gss;
  OutgoingCache<V, E, M>* _outgoing;
  std::shared_ptr<GraphStore<V, E>> _graphStore;
  VertexEntry* _vertexEntry;

 protected:
  unsigned int getGlobalSuperstep() const { return _gss; }
  void sendMessage(std::string const& toValue, M const& data);
  EdgeIterator<E> getEdges();
  V* mutableVertexData();
  V vertexData();
  /// store data, will potentially move the data around
  void setVertexData(const V*, size_t size);
  void voteHalt();

 public:
  virtual void compute(std::string const& vertexID,
                       MessageIterator<M> const& messages) = 0;
};
}
}
#endif
