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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "Traverser.h"
#include "Basics/VelocyPackHelper.h"
#include "Graph/TraverserCache.h"
#include "Graph/TraverserOptions.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/KeyGenerator.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::traverser;
using namespace arangodb::graph;

bool Traverser::VertexGetter::getVertex(
    VPackSlice edge, arangodb::traverser::EnumeratedPath& path) {
  // getSingleVertex will populate s and register the underlying character data
  // if the vertex is found.
  std::string_view s;
  if (!getSingleVertex(edge, path.lastVertex(), path.numVertices(), s)) {
    return false;
  }

  path.pushVertex(s);
  return true;
}

bool Traverser::VertexGetter::getSingleVertex(arangodb::velocypack::Slice edge,
                                              std::string_view cmp,
                                              uint64_t depth,
                                              std::string_view& result) {
  VPackSlice resSlice = edge;
  if (!resSlice.isString()) {
    resSlice = transaction::helpers::extractFromFromDocument(edge);
    if (resSlice.compareString(cmp.data(), cmp.length()) == 0) {
      resSlice = transaction::helpers::extractToFromDocument(edge);
    }
  }
  TRI_ASSERT(resSlice.isString());

  std::string_view s(resSlice.stringView());
  if (!_traverser->vertexMatchesConditions(s, depth)) {
    return false;
  }

  result = _traverser->traverserCache()->persistString(s);
  return true;
}

void Traverser::VertexGetter::reset(std::string_view) {}

// nothing to do
void Traverser::VertexGetter::clear() {}

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
bool Traverser::VertexGetter::pointsIntoTraverserCache() const noexcept {
  return false;
}
#endif

bool Traverser::VertexGetter::getVertex(std::string_view vertex, size_t depth) {
  return _traverser->vertexMatchesConditions(vertex, depth);
}

bool Traverser::UniqueVertexGetter::getVertex(
    VPackSlice edge, arangodb::traverser::EnumeratedPath& path) {
  // getSingleVertex will populate s and register the underlying character data
  // if the vertex is found.
  std::string_view s;
  if (!getSingleVertex(edge, path.lastVertex(), path.numVertices(), s)) {
    return false;
  }

  path.pushVertex(s);
  return true;
}

bool Traverser::UniqueVertexGetter::getVertex(std::string_view vertex,
                                              size_t depth) {
  if (_returnedVertices.find(vertex) != _returnedVertices.end()) {
    // This vertex is not unique.
    _traverser->traverserCache()->increaseFilterCounter();
    return false;
  }

  if (!_traverser->vertexMatchesConditions(vertex, depth)) {
    return false;
  }

  _returnedVertices.emplace(vertex);
  return true;
}

bool Traverser::UniqueVertexGetter::getSingleVertex(
    arangodb::velocypack::Slice edge, std::string_view cmp, uint64_t depth,
    std::string_view& result) {
  VPackSlice resSlice = edge;
  if (!resSlice.isString()) {
    resSlice = transaction::helpers::extractFromFromDocument(edge);
    if (resSlice.compareString(cmp.data(), cmp.length()) == 0) {
      resSlice = transaction::helpers::extractToFromDocument(edge);
    }
    TRI_ASSERT(resSlice.isString());
  }

  std::string_view s(resSlice.stringView());

  // First check if we visited it. If not, then mark
  if (_returnedVertices.find(s) != _returnedVertices.end()) {
    // This vertex is not unique.
    _traverser->traverserCache()->increaseFilterCounter();
    return false;
  }

  if (!_traverser->vertexMatchesConditions(s, depth)) {
    return false;
  }

  result = _traverser->traverserCache()->persistString(s);
  _returnedVertices.emplace(result);
  return true;
}

void Traverser::UniqueVertexGetter::clear() {
  // we must make sure that we clear _returnedVertices, not only for
  // correctness, but also because it may point into memory that is
  // going to be freed after this call.
  _returnedVertices.clear();
}

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
bool Traverser::UniqueVertexGetter::pointsIntoTraverserCache() const noexcept {
  return !_returnedVertices.empty();
}
#endif

void Traverser::UniqueVertexGetter::reset(std::string_view startVertex) {
  _returnedVertices.clear();
  // The startVertex always counts as visited!
  _returnedVertices.emplace(startVertex);
}

Traverser::Traverser(arangodb::traverser::TraverserOptions* opts)
    : _trx(opts->trx()), _done(true), _opts(opts) {
  if (opts->uniqueVertices == TraverserOptions::UniquenessLevel::GLOBAL) {
    _vertexGetter = std::make_unique<UniqueVertexGetter>(this);
  } else {
    _vertexGetter = std::make_unique<VertexGetter>(this);
  }
}

Traverser::~Traverser() = default;

bool arangodb::traverser::Traverser::edgeMatchesConditions(VPackSlice e,
                                                           std::string_view vid,
                                                           uint64_t depth,
                                                           size_t cursorId) {
  return _opts->evaluateEdgeExpression(e, vid, depth, cursorId);
}

bool arangodb::traverser::Traverser::vertexMatchesConditions(std::string_view v,
                                                             uint64_t depth) {
  if (_opts->vertexHasFilter(depth)) {
    // We always need to destroy this vertex
    aql::AqlValue vertex = fetchVertexData(v);
    aql::AqlValueGuard guard{vertex, true};
    if (!_opts->evaluateVertexExpression(vertex.slice(), depth)) {
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

TraverserCache* arangodb::traverser::Traverser::traverserCache() {
  return _opts->cache();
}

size_t arangodb::traverser::Traverser::getAndResetFilteredPaths() {
  return traverserCache()->getAndResetFilteredDocuments();
}

size_t arangodb::traverser::Traverser::getAndResetReadDocuments() {
  return traverserCache()->getAndResetInsertedDocuments();
}

size_t arangodb::traverser::Traverser::getAndResetHttpRequests() {
  return _enumerator->getAndResetHttpRequests();
}
