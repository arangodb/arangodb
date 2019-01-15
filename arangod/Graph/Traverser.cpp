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

bool Traverser::VertexGetter::getVertex(VPackSlice edge, std::vector<StringRef>& result) {
  VPackSlice res = edge;
  if (!res.isString()) {
    res = transaction::helpers::extractFromFromDocument(edge);
    if (result.back() == StringRef(res)) {
      res = transaction::helpers::extractToFromDocument(edge);
    }
  }
  TRI_ASSERT(res.isString());

  if (!_traverser->vertexMatchesConditions(StringRef(res), result.size())) {
    return false;
  }
  result.emplace_back(_traverser->traverserCache()->persistString(StringRef(res)));
  return true;
}

bool Traverser::VertexGetter::getSingleVertex(arangodb::velocypack::Slice edge, StringRef cmp,
                                              uint64_t depth, StringRef& result) {
  VPackSlice resSlice = edge;
  if (!resSlice.isString()) {
    VPackSlice from = transaction::helpers::extractFromFromDocument(edge);
    if (from.compareString(cmp.data(), cmp.length()) != 0) {
      resSlice = from;
    } else {
      resSlice = transaction::helpers::extractToFromDocument(edge);
    }
    TRI_ASSERT(resSlice.isString());
  }
  result = _traverser->traverserCache()->persistString(StringRef(resSlice));
  return _traverser->vertexMatchesConditions(result, depth);
}

void Traverser::VertexGetter::reset(StringRef const&) {}

bool Traverser::UniqueVertexGetter::getVertex(VPackSlice edge,
                                              std::vector<StringRef>& result) {
  VPackSlice toAdd = edge;
  if (!toAdd.isString()) {
    toAdd = transaction::helpers::extractFromFromDocument(edge);
    StringRef const& cmp = result.back();
    TRI_ASSERT(toAdd.isString());
    if (cmp == StringRef(toAdd)) {
      toAdd = transaction::helpers::extractToFromDocument(edge);
    }
    TRI_ASSERT(toAdd.isString());
  }

  StringRef toAddStr = _traverser->traverserCache()->persistString(StringRef(toAdd));
  // First check if we visited it. If not, then mark
  if (_returnedVertices.find(toAddStr) != _returnedVertices.end()) {
    // This vertex is not unique.
    _traverser->traverserCache()->increaseFilterCounter();
    return false;
  } else {
    if (!_traverser->vertexMatchesConditions(toAddStr, result.size())) {
      return false;
    }
    _returnedVertices.emplace(toAddStr);
  }

  result.emplace_back(toAddStr);
  return true;
}

bool Traverser::UniqueVertexGetter::getSingleVertex(arangodb::velocypack::Slice edge,
                                                    StringRef cmp, uint64_t depth,
                                                    StringRef& result) {
  VPackSlice resSlice = edge;
  if (!resSlice.isString()) {
    resSlice = transaction::helpers::extractFromFromDocument(edge);
    if (resSlice.compareString(cmp.data(), cmp.length()) == 0) {
      resSlice = transaction::helpers::extractToFromDocument(edge);
    }
    TRI_ASSERT(resSlice.isString());
  }

  result = _traverser->traverserCache()->persistString(StringRef(resSlice));
  // First check if we visited it. If not, then mark
  if (_returnedVertices.find(result) != _returnedVertices.end()) {
    // This vertex is not unique.
    _traverser->traverserCache()->increaseFilterCounter();
    return false;
  }

  if (!_traverser->vertexMatchesConditions(result, depth)) {
    return false;
  }

  _returnedVertices.emplace(result);
  return true;
}

void Traverser::UniqueVertexGetter::reset(arangodb::StringRef const& startVertex) {
  _returnedVertices.clear();
  // The startVertex always counts as visited!
  _returnedVertices.emplace(startVertex);
}

Traverser::Traverser(arangodb::traverser::TraverserOptions* opts,
                     transaction::Methods* trx, arangodb::ManagedDocumentResult* mmdr)
    : _trx(trx),
      _mmdr(mmdr),
      _startIdBuilder(trx),
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

bool arangodb::traverser::Traverser::edgeMatchesConditions(VPackSlice e, StringRef vid,
                                                           uint64_t depth, size_t cursorId) {
  if (!_opts->evaluateEdgeExpression(e, vid, depth, cursorId)) {
    return false;
  }
  return true;
}

bool arangodb::traverser::Traverser::vertexMatchesConditions(StringRef v, uint64_t depth) {
  if (_opts->vertexHasFilter(depth)) {
    // We always need to destroy this vertex
    aql::AqlValue vertex = fetchVertexData(v);
    if (!_opts->evaluateVertexExpression(vertex.slice(), depth)) {
      vertex.destroy();
      return false;
    }
    vertex.destroy();
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

size_t arangodb::traverser::Traverser::getAndResetReadDocuments() {
  return traverserCache()->getAndResetInsertedDocuments();
}

size_t arangodb::traverser::Traverser::getAndResetFilteredPaths() {
  return traverserCache()->getAndResetFilteredDocuments();
}

void arangodb::traverser::Traverser::allowOptimizedNeighbors() {
  _canUseOptimizedNeighbors = true;
}
