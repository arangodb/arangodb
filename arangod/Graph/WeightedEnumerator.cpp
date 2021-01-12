////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "WeightedEnumerator.h"

#include "Aql/PruneExpressionEvaluator.h"
#include "Graph/EdgeCursor.h"
#include "Graph/EdgeDocumentToken.h"
#include "Graph/Traverser.h"
#include "Graph/TraverserCache.h"
#include "Graph/TraverserOptions.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <utility>

using namespace arangodb;
using namespace arangodb::graph;
using namespace arangodb::traverser;

WeightedEnumerator::PathStep::PathStep(arangodb::velocypack::StringRef vertex)
    : fromIndex(0),
      fromEdgeToken(EdgeDocumentToken()),
      currentVertexId(std::move(vertex)),
      accumWeight(0.0) {}

WeightedEnumerator::PathStep::PathStep(size_t sourceIdx, EdgeDocumentToken&& edge,
                                       arangodb::velocypack::StringRef vertex, double weight)
    : fromIndex(sourceIdx),
      fromEdgeToken(edge),
      currentVertexId(std::move(vertex)),
      accumWeight(weight) {}

WeightedEnumerator::WeightedEnumerator(Traverser* traverser, TraverserOptions* opts)
    : PathEnumerator(traverser, opts), _schreierIndex(0), _lastReturned(0) {
  _schreier.reserve(32);
}

void WeightedEnumerator::setStartVertex(arangodb::velocypack::StringRef startVertex) {
  PathEnumerator::setStartVertex(startVertex);

  _schreier.clear();
  _schreierIndex = 0;
  _lastReturned = 0;
  _queue.clear();

  _schreier.emplace_back(startVertex);
}

bool WeightedEnumerator::expandEdge(NextEdge nextEdge) {
  VPackStringRef const toVertex = nextEdge.forwardVertexId;

  // we already have the toVertex, so we don't need to load the edge again
  // getSingleVertex does nothing but that and checking conditions
  // However, for global unique vertexes, we need the vertex getter.
  if (_traverser->getVertex(toVertex, nextEdge.depth)) {
    if (_opts->uniqueVertices == TraverserOptions::UniquenessLevel::PATH) {
      if (pathContainsVertex(nextEdge.fromIndex, toVertex)) {
        // This vertex is on the path.
        return false;
      }
    }
    _schreier.emplace_back(nextEdge.fromIndex, std::move(nextEdge.forwardEdgeToken),
                           toVertex, nextEdge.accumWeight);

    if (!shouldPrune()) {
      // expand all edges on this vertex
      expandVertex(_schreierIndex, nextEdge.depth);
    }
    _schreierIndex++;
    return true;
  }

  return false;
}

void WeightedEnumerator::expandVertex(size_t vertexIndex, size_t depth) {
  PathStep const& currentStep = _schreier[vertexIndex];
  VPackStringRef vertex = currentStep.currentVertexId;
  EdgeCursor* cursor = getCursor(vertex, depth);

  if (depth >= _opts->maxDepth) {
    return;
  }

  cursor->readAll([&](graph::EdgeDocumentToken&& eid, VPackSlice e, size_t cursorIdx) -> void {
    // transform edge if required
    if (e.isString()) {
      // this will result in a document request.
      // however, shortest path has to do it as well, so I think this is ok.
      e = _opts->cache()->lookupToken(eid);
      // keep will eventually translate the edge if there is condition
    }

    if (!keepEdge(eid, e, vertex, depth, cursorIdx)) {
      return;
    }

    if (_opts->uniqueEdges == TraverserOptions::UniquenessLevel::PATH) {
      if (pathContainsEdge(vertexIndex, eid)) {
        // This edge is on the path.
        return;
      }
    }

    double forwardWeight = currentStep.accumWeight + weightEdge(e);
    VPackStringRef toVertex = _opts->cache()->persistString(getToVertex(e, vertex));
    _queue.emplace(vertexIndex, forwardWeight, depth + 1, std::move(eid), toVertex);
  });

  incHttpRequests(cursor->httpRequests());
}

bool WeightedEnumerator::expand() {
  while (true) {
    if (_queue.empty()) {
      // That's it. we are done.
      return false;
    }

    // This access is always safe.
    // If not it should have bailed out before.
    TRI_ASSERT(!_queue.empty());
    NextEdge nextEdge = _queue.popTop();

    bool const shouldReturnPath = nextEdge.depth + 1 >= _opts->minDepth;
    bool const didInsert = expandEdge(std::move(nextEdge));

    if (!shouldReturnPath) {
      _lastReturned = _schreierIndex;
    } else if (didInsert) {
      // We exit the loop here.
      // _schreierIndex is moved forward
      return true;
    }
  }
}

bool WeightedEnumerator::next() {
  if (_isFirst) {
    _isFirst = false;

    if (!shouldPrune()) {
      expandVertex(0, 0);
    }
    // We have faked the 0 position in schreier for pruning
    _schreierIndex++;
    if (_opts->minDepth == 0) {
      return true;
    }
  }
  _lastReturned++;

  if (_lastReturned < _schreierIndex) {
    // We still have something on our stack.
    // Paths have been read but not returned.
    return true;
  }

  if (_opts->maxDepth == 0) {
    // Short circuit.
    // We cannot find any path of length 0 or less
    return false;
  }

  // otherwise expand "schreier" vector
  return expand();
}

arangodb::aql::AqlValue WeightedEnumerator::lastVertexToAqlValue() {
  return vertexToAqlValue(_lastReturned);
}

arangodb::aql::AqlValue WeightedEnumerator::lastEdgeToAqlValue() {
  return edgeToAqlValue(_lastReturned);
}

arangodb::aql::AqlValue WeightedEnumerator::pathToAqlValue(arangodb::velocypack::Builder& result) {
  return pathToIndexToAqlValue(result, _lastReturned);
}

arangodb::aql::AqlValue WeightedEnumerator::vertexToAqlValue(size_t index) {
  TRI_ASSERT(index < _schreier.size());
  return _traverser->fetchVertexData(_schreier[index].currentVertexId);
}

arangodb::aql::AqlValue WeightedEnumerator::edgeToAqlValue(size_t index) {
  TRI_ASSERT(index < _schreier.size());
  if (index == 0) {
    // This is the first Vertex. No Edge Pointing to it
    return arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull());
  }
  return _opts->cache()->fetchEdgeAqlResult(_schreier[index].fromEdgeToken);
}

VPackSlice WeightedEnumerator::pathToIndexToSlice(VPackBuilder& result, size_t index) {
  for (_tempPathHelper.clear(); index != 0; index = _schreier[index].fromIndex) {
    _tempPathHelper.emplace_back(index);
  }

  result.clear();
  {
    VPackObjectBuilder ob(&result);
    {  // edges
      VPackArrayBuilder ab(&result, StaticStrings::GraphQueryEdges);
      std::for_each(_tempPathHelper.rbegin(), _tempPathHelper.rend(), [&](size_t idx) {
        _opts->cache()->insertEdgeIntoResult(_schreier[idx].fromEdgeToken, result);
      });
    }
    {  // vertices
      VPackArrayBuilder ab(&result, StaticStrings::GraphQueryVertices);
      _traverser->addVertexToVelocyPack(_schreier[0].currentVertexId, result);
      std::for_each(_tempPathHelper.rbegin(), _tempPathHelper.rend(), [&](size_t idx) {
        _traverser->addVertexToVelocyPack(_schreier[idx].currentVertexId, result);
      });
    }
    {  // weights
      VPackArrayBuilder ab(&result, "weights");
      result.add(VPackValue(_schreier[0].accumWeight));
      std::for_each(_tempPathHelper.rbegin(), _tempPathHelper.rend(), [&](size_t idx) {
        result.add(VPackValue(_schreier[idx].accumWeight));
      });
    }
  }
  TRI_ASSERT(result.isClosed());
  return result.slice();
}

arangodb::aql::AqlValue WeightedEnumerator::pathToIndexToAqlValue(arangodb::velocypack::Builder& result,
                                                                  size_t index) {
  return arangodb::aql::AqlValue(pathToIndexToSlice(result, index));
}

bool WeightedEnumerator::pathContainsVertex(size_t index,
                                            arangodb::velocypack::StringRef vertex) const {
  while (true) {
    TRI_ASSERT(index < _schreier.size());
    auto const& step = _schreier[index];
    if (step.currentVertexId == vertex) {
      // We have the given vertex on this path
      return true;
    }
    if (index == 0) {
      // We have checked the complete path
      return false;
    }
    index = step.fromIndex;
  }
}

bool WeightedEnumerator::pathContainsEdge(size_t index,
                                          graph::EdgeDocumentToken const& edge) const {
  while (index != 0) {
    TRI_ASSERT(index < _schreier.size());
    auto const& step = _schreier[index];
    if (step.fromEdgeToken.equals(edge)) {
      // We have the given edge on this path
      return true;
    }
    index = step.fromIndex;
  }
  // We have checked the complete path
  return false;
}

bool WeightedEnumerator::shouldPrune() {
  if (!_opts->usesPrune()) {
    return false;
  }

  transaction::BuilderLeaser pathBuilder(_opts->trx());

  // evaluator->evaluate() might access these, so they have to live long enough.
  aql::AqlValue vertex, edge;
  aql::AqlValueGuard vertexGuard{vertex, true}, edgeGuard{edge, true};

  auto* evaluator = _opts->getPruneEvaluator();
  TRI_ASSERT(evaluator != nullptr);

  if (evaluator->needsVertex()) {
    // Note: vertexToAqlValue() copies the original vertex into the AqlValue.
    // This could be avoided with a function that just returns the slice,
    // as it will stay valid long enough.
    vertex = vertexToAqlValue(_schreierIndex);
    evaluator->injectVertex(vertex.slice());
  }
  if (evaluator->needsEdge()) {
    // Note: edgeToAqlValue() copies the original edge into the AqlValue.
    // This could be avoided with a function that just returns the slice,
    // as it will stay valid long enough.
    edge = edgeToAqlValue(_schreierIndex);
    evaluator->injectEdge(edge.slice());
  }
  if (evaluator->needsPath()) {
    VPackSlice path = pathToIndexToSlice(*pathBuilder.get(), _schreierIndex);
    evaluator->injectPath(path);
  }
  return evaluator->evaluate();
}

double WeightedEnumerator::weightEdge(VPackSlice edge) const {
  return _opts->weightEdge(edge);
}

velocypack::StringRef WeightedEnumerator::getToVertex(velocypack::Slice edge,
                                                      velocypack::StringRef from) {
  TRI_ASSERT(edge.isObject());
  VPackSlice resSlice = transaction::helpers::extractToFromDocument(edge);
  if (resSlice.isEqualString(from)) {
    resSlice = transaction::helpers::extractFromFromDocument(edge);
  }
  return resSlice.stringRef();
}
