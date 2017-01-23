////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "Traverser.h"
#include "Basics/VelocyPackHelper.h"
#include "Indexes/EdgeIndex.h"
#include "Utils/Transaction.h"
#include "Utils/TransactionContext.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/TraverserOptions.h"

#include <velocypack/Iterator.h> 
#include <velocypack/velocypack-aliases.h>

using Traverser = arangodb::traverser::Traverser;
/// @brief Class Shortest Path

/// @brief Clears the path
void arangodb::traverser::ShortestPath::clear() {
  _vertices.clear();
  _edges.clear();
}

void arangodb::traverser::ShortestPath::edgeToVelocyPack(Transaction*, ManagedDocumentResult* mmdr,
                                                         size_t position, VPackBuilder& builder) {
  TRI_ASSERT(position < length());
  if (position == 0) {
    builder.add(basics::VelocyPackHelper::NullValue());
  } else {
    TRI_ASSERT(position - 1 < _edges.size());
    builder.add(_edges[position - 1]);
  }
}

void arangodb::traverser::ShortestPath::vertexToVelocyPack(Transaction* trx, ManagedDocumentResult* mmdr, 
                                                           size_t position, VPackBuilder& builder) {
  TRI_ASSERT(position < length());
  VPackSlice v = _vertices[position];
  TRI_ASSERT(v.isString());
  std::string collection =  v.copyString();
  size_t p = collection.find("/");
  TRI_ASSERT(p != std::string::npos);

  TransactionBuilderLeaser searchBuilder(trx);
  searchBuilder->add(VPackValue(collection.substr(p + 1)));
  collection = collection.substr(0, p);

  int res =
      trx->documentFastPath(collection, mmdr, searchBuilder->slice(), builder, true);
  if (res != TRI_ERROR_NO_ERROR) {
    builder.clear(); // Just in case...
    builder.add(basics::VelocyPackHelper::NullValue());
  }
}

bool Traverser::VertexGetter::getVertex(
    VPackSlice edge, std::vector<VPackSlice>& result) {
  VPackSlice cmp = result.back();
  VPackSlice res = Transaction::extractFromFromDocument(edge);
  if (cmp == res) {
    res = Transaction::extractToFromDocument(edge);
  }

  if (!_traverser->vertexMatchesConditions(res, result.size())) {
    return false;
  }
  result.emplace_back(res);
  return true;
}

bool Traverser::VertexGetter::getSingleVertex(VPackSlice edge,
                                              VPackSlice cmp,
                                              size_t depth,
                                              VPackSlice& result) {
  VPackSlice from = Transaction::extractFromFromDocument(edge);
  if (from != cmp) {
    result = from;
  } else {
    result = Transaction::extractToFromDocument(edge);
  }
  return _traverser->vertexMatchesConditions(result, depth);
}

void Traverser::VertexGetter::reset(arangodb::velocypack::Slice) {
}

bool Traverser::UniqueVertexGetter::getVertex(
  VPackSlice edge, std::vector<VPackSlice>& result) {
  VPackSlice toAdd = Transaction::extractFromFromDocument(edge);
  VPackSlice cmp = result.back();

  if (toAdd == cmp) {
    toAdd = Transaction::extractToFromDocument(edge);
  }

  arangodb::basics::VPackHashedSlice hashed(toAdd);

  // First check if we visited it. If not, then mark
  if (_returnedVertices.find(hashed) != _returnedVertices.end()) {
    // This vertex is not unique.
    ++_traverser->_filteredPaths;
    return false;
  } else {
    _returnedVertices.emplace(hashed);
  }

  if (!_traverser->vertexMatchesConditions(toAdd, result.size())) {
    return false;
  }

  result.emplace_back(toAdd);
  return true;
}

bool Traverser::UniqueVertexGetter::getSingleVertex(
  VPackSlice edge, VPackSlice cmp, size_t depth, VPackSlice& result) {
  result = Transaction::extractFromFromDocument(edge);

  if (cmp == result) {
    result = Transaction::extractToFromDocument(edge);
  }
  
  arangodb::basics::VPackHashedSlice hashed(result);
  
  // First check if we visited it. If not, then mark
  if (_returnedVertices.find(hashed) != _returnedVertices.end()) {
    // This vertex is not unique.
    ++_traverser->_filteredPaths;
    return false;
  } else {
    _returnedVertices.emplace(hashed);
  }

  return _traverser->vertexMatchesConditions(result, depth);
}

void Traverser::UniqueVertexGetter::reset(VPackSlice startVertex) {
  _returnedVertices.clear();
  
  arangodb::basics::VPackHashedSlice hashed(startVertex);
  // The startVertex always counts as visited!
  _returnedVertices.emplace(hashed);
}

Traverser::Traverser(arangodb::traverser::TraverserOptions* opts, arangodb::Transaction* trx,
                     arangodb::ManagedDocumentResult* mmdr)
    : _trx(trx),
      _mmdr(mmdr),
      _startIdBuilder(trx),
      _readDocuments(0),
      _filteredPaths(0),
      _pruneNext(false),
      _done(true),
      _opts(opts),
      _canUseOptimizedNeighbors(false) {
  if (opts->uniqueVertices == TraverserOptions::UniquenessLevel::GLOBAL) {
    _vertexGetter = std::make_unique<UniqueVertexGetter>(this);
  } else {
    _vertexGetter = std::make_unique<VertexGetter>(this);
  }
}

bool arangodb::traverser::Traverser::edgeMatchesConditions(VPackSlice e,
                                                           VPackSlice vid,
                                                           size_t depth,
                                                           size_t cursorId) {
  if (!_opts->evaluateEdgeExpression(e, vid, depth, cursorId)) {
    ++_filteredPaths;
    return false;
  }
  return true;
}

bool arangodb::traverser::Traverser::vertexMatchesConditions(VPackSlice v, size_t depth) {
  TRI_ASSERT(v.isString());
  if (_opts->vertexHasFilter(depth)) {
    aql::AqlValue vertex = fetchVertexData(v);
    if (!_opts->evaluateVertexExpression(vertex.slice(), depth)) {
      ++_filteredPaths;
      return false;
    }
  }
  return true;
}

bool arangodb::traverser::Traverser::next() {
  TRI_ASSERT(!_done);
  bool res = _enumerator->next();
  if (!res) {
    _done = true;
  }
  return res;
}

arangodb::aql::AqlValue arangodb::traverser::Traverser::lastVertexToAqlValue() {
  return _enumerator->lastVertexToAqlValue();
}

arangodb::aql::AqlValue arangodb::traverser::Traverser::lastEdgeToAqlValue() {
  return _enumerator->lastEdgeToAqlValue();
}

arangodb::aql::AqlValue arangodb::traverser::Traverser::pathToAqlValue(
    VPackBuilder& builder) {
  return _enumerator->pathToAqlValue(builder);
}

void arangodb::traverser::Traverser::allowOptimizedNeighbors() {
  _canUseOptimizedNeighbors = true;
}
