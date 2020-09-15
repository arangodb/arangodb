////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

bool Traverser::VertexGetter::getVertex(VPackSlice edge,
                                        std::vector<arangodb::velocypack::StringRef>& result) {
  TRI_ASSERT(!result.empty());

  // getSingleVertex will populate s and register the underlying character data
  // if the vertex is found.
  arangodb::velocypack::StringRef s;
  if (!getSingleVertex(edge, result.back(), result.size(), s)) {
    return false;
  }

  result.emplace_back(s);
  return true;
}

bool Traverser::VertexGetter::getSingleVertex(arangodb::velocypack::Slice edge,
                                              arangodb::velocypack::StringRef cmp,
                                              uint64_t depth,
                                              arangodb::velocypack::StringRef& result) {
  VPackSlice resSlice = edge;
  if (!resSlice.isString()) {
    resSlice = transaction::helpers::extractFromFromDocument(edge);
    if (resSlice.compareString(cmp.data(), cmp.length()) == 0) {
      resSlice = transaction::helpers::extractToFromDocument(edge);
    }
  }
  TRI_ASSERT(resSlice.isString());

  arangodb::velocypack::StringRef s(resSlice);
  if (!_traverser->vertexMatchesConditions(s, depth)) {
    return false;
  }

  result = _traverser->traverserCache()->persistString(s);
  return true;
}

void Traverser::VertexGetter::reset(arangodb::velocypack::StringRef const&) {}

bool Traverser::VertexGetter::getVertex(arangodb::velocypack::StringRef vertex, size_t depth) {
  return _traverser->vertexMatchesConditions(vertex, depth);
}

bool Traverser::UniqueVertexGetter::getVertex(VPackSlice edge,
                                              std::vector<arangodb::velocypack::StringRef>& result) {
  TRI_ASSERT(!result.empty());

  // getSingleVertex will populate s and register the underlying character data
  // if the vertex is found.
  arangodb::velocypack::StringRef s;
  if (!getSingleVertex(edge, result.back(), result.size(), s)) {
    return false;
  }

  result.emplace_back(s);
  return true;
}

bool Traverser::UniqueVertexGetter::getVertex(arangodb::velocypack::StringRef vertex, size_t depth) {
  if (_returnedVertices.find(vertex) != _returnedVertices.end()) {
    // This vertex is not unique.
    _traverser->traverserCache()->increaseFilterCounter();
    return false;
  }

  if(!_traverser->vertexMatchesConditions(vertex, depth)) {
    return false;
  }

  _returnedVertices.emplace(vertex);
  return true;
}

bool Traverser::UniqueVertexGetter::getSingleVertex(arangodb::velocypack::Slice edge,
                                                    arangodb::velocypack::StringRef cmp,
                                                    uint64_t depth,
                                                    arangodb::velocypack::StringRef& result) {
  VPackSlice resSlice = edge;
  if (!resSlice.isString()) {
    resSlice = transaction::helpers::extractFromFromDocument(edge);
    if (resSlice.compareString(cmp.data(), cmp.length()) == 0) {
      resSlice = transaction::helpers::extractToFromDocument(edge);
    }
    TRI_ASSERT(resSlice.isString());
  }

  arangodb::velocypack::StringRef s(resSlice);

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

void Traverser::UniqueVertexGetter::reset(arangodb::velocypack::StringRef const& startVertex) {
  _returnedVertices.clear();
  // The startVertex always counts as visited!
  _returnedVertices.emplace(startVertex);
}

Traverser::Traverser(arangodb::traverser::TraverserOptions* opts)
    : _trx(opts->trx()),
      _done(true),
      _opts(opts) {
  if (opts->uniqueVertices == TraverserOptions::UniquenessLevel::GLOBAL) {
    _vertexGetter = std::make_unique<UniqueVertexGetter>(this);
  } else {
    _vertexGetter = std::make_unique<VertexGetter>(this);
  }
}

Traverser::~Traverser() = default;

bool arangodb::traverser::Traverser::edgeMatchesConditions(VPackSlice e,
                                                           arangodb::velocypack::StringRef vid,
                                                           uint64_t depth, size_t cursorId) {
  return _opts->evaluateEdgeExpression(e, vid, depth, cursorId);
}

bool arangodb::traverser::Traverser::vertexMatchesConditions(arangodb::velocypack::StringRef v,
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
