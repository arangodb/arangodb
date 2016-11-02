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

#include "VertexComputation.h"
#include "GraphStore.h"
#include "OutgoingCache.h"

using namespace std;
using namespace arangodb;
using namespace arangodb::velocypack;
using namespace arangodb::pregel;

template <typename V, typename E, typename M>
void VertexComputation<V, E, M>::sendMessage(std::string const& toVertexID,
                                             M const& data) {
  _outgoing->sendMessageTo(toVertexID, data);
}

template <typename V, typename E, typename M>
EdgeIterator<E> VertexComputation<V, E, M>::VertexComputation::getEdges() {
  return _graphStore->edgeIterator(_vertexEntry);
}

template <typename V, typename E, typename M>
void* VertexComputation<V, E, M>::mutableVertexData() {
    return _graphStore->mutableVertexData(_vertexEntry);
}

template <typename V, typename E, typename M>
V VertexComputation<V, E, M>::vertexData() {
  return _graphStore->copyVertexData(_vertexEntry);
}

template <typename V, typename E, typename M>
void VertexComputation<V, E, M>::setVertexData(const V* ptr, size_t size) {
  _graphStore->replaceVertexData(_vertexEntry, (void*)ptr, size);
}

template <typename V, typename E, typename M>
void VertexComputation<V, E, M>::voteHalt() {
  _vertexEntry->setActive(false);
}

// template types to create
template class arangodb::pregel::VertexComputation<int64_t, int64_t, int64_t>;
