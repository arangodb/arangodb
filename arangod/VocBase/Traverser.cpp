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
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Transaction/Context.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/TraverserOptions.h"
#include "VocBase/TraverserCache.h"

#include <velocypack/Iterator.h> 
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::traverser;

/// @brief Class Shortest Path

/// @brief Clears the path
void arangodb::traverser::ShortestPath::clear() {
  _vertices.clear();
  _edges.clear();
}

void arangodb::traverser::ShortestPath::edgeToVelocyPack(transaction::Methods*, ManagedDocumentResult* mmdr,
                                                         size_t position, VPackBuilder& builder) {
  TRI_ASSERT(position < length());
  if (position == 0) {
    builder.add(basics::VelocyPackHelper::NullValue());
  } else {
    TRI_ASSERT(position - 1 < _edges.size());
    builder.add(_edges[position - 1]);
  }
}

void arangodb::traverser::ShortestPath::vertexToVelocyPack(transaction::Methods* trx, ManagedDocumentResult* mmdr, 
                                                           size_t position, VPackBuilder& builder) {
  TRI_ASSERT(position < length());
  VPackSlice v = _vertices[position];
  TRI_ASSERT(v.isString());
  std::string collection =  v.copyString();
  size_t p = collection.find("/");
  TRI_ASSERT(p != std::string::npos);

  transaction::BuilderLeaser searchBuilder(trx);
  searchBuilder->add(VPackValue(collection.substr(p + 1)));
  collection = collection.substr(0, p);

  int res =
      trx->documentFastPath(collection, mmdr, searchBuilder->slice(), builder, true);
  if (res != TRI_ERROR_NO_ERROR) {
    builder.clear(); // Just in case...
    builder.add(basics::VelocyPackHelper::NullValue());
  }
}

bool Traverser::VertexGetter::getVertex(VPackSlice edge, std::vector<StringRef>& result) {
  VPackSlice res = transaction::helpers::extractFromFromDocument(edge);
  if (result.back() == StringRef(res)) {
    res = transaction::helpers::extractToFromDocument(edge);
  }

  if (!_traverser->vertexMatchesConditions(res, result.size())) {
    return false;
  }
  result.emplace_back(_traverser->traverserCache()->persistString(StringRef(res)));
  return true;
}

bool Traverser::VertexGetter::getSingleVertex(arangodb::velocypack::Slice edge, StringRef cmp,
                                              uint64_t depth, StringRef& result) {
  VPackSlice resSlice;
  VPackSlice from = transaction::helpers::extractFromFromDocument(edge);
  if (from.compareString(cmp.data(), cmp.length()) != 0) {
    resSlice = from;
  } else {
    resSlice = transaction::helpers::extractToFromDocument(edge);
  }
  result = _traverser->traverserCache()->persistString(StringRef(resSlice));
  return _traverser->vertexMatchesConditions(resSlice, depth);
}

void Traverser::VertexGetter::reset(StringRef const&) {
}

bool Traverser::UniqueVertexGetter::getVertex(VPackSlice edge, std::vector<StringRef>& result) {
  VPackSlice toAdd = transaction::helpers::extractFromFromDocument(edge);
  StringRef const& cmp = result.back();
  TRI_ASSERT(toAdd.isString());
  if (cmp == StringRef(toAdd)) {
    toAdd = transaction::helpers::extractToFromDocument(edge);
  }
  StringRef toAddStr = _traverser->traverserCache()->persistString(StringRef(toAdd));
  // First check if we visited it. If not, then mark
  if (_returnedVertices.find(toAddStr) != _returnedVertices.end()) {
    // This vertex is not unique.
    ++_traverser->_filteredPaths;
    return false;
  } else {
    _returnedVertices.emplace(toAddStr);
  }

  if (!_traverser->vertexMatchesConditions(toAdd, result.size())) {
    return false;
  }

  result.emplace_back(toAddStr);
  return true;
}

bool Traverser::UniqueVertexGetter::getSingleVertex(arangodb::velocypack::Slice edge, StringRef cmp,
                                              uint64_t depth, StringRef& result) {
  VPackSlice resSlice = transaction::helpers::extractFromFromDocument(edge);
    
  if (resSlice.compareString(cmp.data(), cmp.length()) == 0) {
    resSlice = transaction::helpers::extractToFromDocument(edge);
  }
  TRI_ASSERT(resSlice.isString());
  
  result = _traverser->traverserCache()->persistString(StringRef(resSlice));
  // First check if we visited it. If not, then mark
  if (_returnedVertices.find(result) != _returnedVertices.end()) {
    // This vertex is not unique.
    ++_traverser->_filteredPaths;
    return false;
  } else {
    _returnedVertices.emplace(result);
  }
  return _traverser->vertexMatchesConditions(resSlice, depth);
}

void Traverser::UniqueVertexGetter::reset(arangodb::StringRef const& startVertex) {
  _returnedVertices.clear();
  // The startVertex always counts as visited!
  _returnedVertices.emplace(startVertex);
}

Traverser::Traverser(arangodb::traverser::TraverserOptions* opts,
                     transaction::Methods* trx,
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

Traverser::~Traverser() {}

bool arangodb::traverser::Traverser::edgeMatchesConditions(VPackSlice e,
                                                           StringRef vid,
                                                           uint64_t depth,
                                                           size_t cursorId) {
  if (!_opts->evaluateEdgeExpression(e, vid, depth, cursorId)) {
    ++_filteredPaths;
    return false;
  }
  return true;
}

bool arangodb::traverser::Traverser::vertexMatchesConditions(VPackSlice v, uint64_t depth) {
  TRI_ASSERT(v.isString());
  if (_opts->vertexHasFilter(depth)) {
    aql::AqlValue vertex = fetchVertexData(StringRef(v));
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

TraverserCache* arangodb::traverser::Traverser::traverserCache() {
  return _opts->cache();
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
