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

#include "SingleServerTraverser.h"
#include "Utils/OperationCursor.h"
#include "Utils/Transaction.h"
#include "VocBase/MasterPointer.h"
#include "VocBase/SingleServerTraversalPath.h"

using namespace arangodb;
using namespace arangodb::traverser;

////////////////////////////////////////////////////////////////////////////////
/// @brief Get a document by it's ID. Also lazy locks the collection.
///        If DOCUMENT_NOT_FOUND this function will return normally
///        with a OperationResult.failed() == true.
///        On all other cases this function throws.
////////////////////////////////////////////////////////////////////////////////

static int FetchDocumentById(arangodb::Transaction* trx,
                             std::string const& id,
                             TRI_doc_mptr_t* mptr) {
  size_t pos = id.find('/');
  if (pos == std::string::npos) {
    TRI_ASSERT(false);
    return TRI_ERROR_INTERNAL;
  }
  if (id.find('/', pos + 1) != std::string::npos) {
    TRI_ASSERT(false);
    return TRI_ERROR_INTERNAL;
  }

  int res = trx->documentFastPathLocal(id.substr(0, pos), id.substr(pos + 1), mptr);

  if (res != TRI_ERROR_NO_ERROR && res != TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
    THROW_ARANGO_EXCEPTION(res);
  }
  return res;
}

SingleServerTraverser::SingleServerTraverser(
    TraverserOptions& opts, arangodb::Transaction* trx,
    std::unordered_map<size_t, std::vector<TraverserExpression*>> const*
        expressions)
    : Traverser(opts, expressions), _trx(trx) {

  _edgeGetter = std::make_unique<EdgeGetter>(this, opts, trx);
  if (opts.uniqueVertices == TraverserOptions::UniquenessLevel::GLOBAL) {
    _vertexGetter = std::make_unique<UniqueVertexGetter>(this);
  } else {
    _vertexGetter = std::make_unique<VertexGetter>(this);
  }
}

SingleServerTraverser::~SingleServerTraverser() {}

bool SingleServerTraverser::edgeMatchesConditions(VPackSlice e, size_t depth) {
  if (_hasEdgeConditions) {
    TRI_ASSERT(_expressions != nullptr);
    auto it = _expressions->find(depth);

    if (it != _expressions->end()) {
      for (auto const& exp : it->second) {
        TRI_ASSERT(exp != nullptr);

        if (exp->isEdgeAccess && !exp->matchesCheck(_trx, e)) {
          ++_filteredPaths;
          return false;
        }
      }
    }
  }
  return true;
}

bool SingleServerTraverser::vertexMatchesConditions(std::string const& v,
                                                    size_t depth) {
  if (_hasVertexConditions) {
    TRI_ASSERT(_expressions != nullptr);
    auto it = _expressions->find(depth);

    if (it != _expressions->end()) {
      bool fetchVertex = true;
      aql::AqlValue vertex;
      for (auto const& exp : it->second) {
        TRI_ASSERT(exp != nullptr);

        if (!exp->isEdgeAccess) {
          if (fetchVertex) {
            fetchVertex = false;
            vertex = fetchVertexData(v);
          }
          if (!exp->matchesCheck(_trx, vertex.slice())) {
            ++_filteredPaths;
            return false;
          }
        }
      }
    }
  }
  return true;
}

aql::AqlValue SingleServerTraverser::fetchVertexData(std::string const& id) {
  auto it = _vertices.find(id);

  if (it == _vertices.end()) {
    TRI_doc_mptr_t mptr;
    int res = FetchDocumentById(_trx, id, &mptr);
    ++_readDocuments;
    if (res != TRI_ERROR_NO_ERROR) {
      return aql::AqlValue(basics::VelocyPackHelper::NullValue());
    }

    uint8_t const* p = mptr.vpack();
    _vertices.emplace(id, p);
    return aql::AqlValue(p, aql::AqlValueFromMasterPointer());
  }

  return aql::AqlValue((*it).second, aql::AqlValueFromMasterPointer());
}

bool SingleServerTraverser::VertexGetter::getVertex(std::string const& edge,
                                                    std::string const& vertex,
                                                    size_t depth,
                                                    std::string& result) {
  auto it = _traverser->_edges.find(edge);
  TRI_ASSERT(it != _traverser->_edges.end());
  VPackSlice v((*it).second);
  // NOTE: We assume that we only have valid edges.
  VPackSlice from = Transaction::extractFromFromDocument(v);
  if (from.isEqualString(vertex)) {
    result = Transaction::extractToFromDocument(v).copyString();
  } else {
    result = from.copyString();
  }
  if (!_traverser->vertexMatchesConditions(result, depth)) {
    return false;
  }
  return true;
}

void SingleServerTraverser::VertexGetter::reset(std::string const&) {
}

bool SingleServerTraverser::UniqueVertexGetter::getVertex(
    std::string const& edge, std::string const& vertex, size_t depth,
    std::string& result) {
  
  auto it = _traverser->_edges.find(edge);
  TRI_ASSERT(it != _traverser->_edges.end());
  VPackSlice v((*it).second);
  // NOTE: We assume that we only have valid edges.
  VPackSlice from = Transaction::extractFromFromDocument(v);
  if (from.isEqualString(vertex)) {
    result = Transaction::extractToFromDocument(v).copyString();
  } else {
    result = from.copyString();
  }
  if (_returnedVertices.find(result) != _returnedVertices.end()) {
    return false;
  }
  if (!_traverser->vertexMatchesConditions(result, depth)) {
    return false;
  }
  _returnedVertices.emplace(result);
  return true;
}

void SingleServerTraverser::UniqueVertexGetter::reset(std::string const& startVertex) {
  _returnedVertices.clear();
  // The startVertex always counts as visited!
  _returnedVertices.emplace(startVertex);
}

void SingleServerTraverser::setStartVertex(std::string const& v) {
  _pruneNext = false;

  TRI_ASSERT(_expressions != nullptr);

  auto it = _expressions->find(0);

  if (it != _expressions->end()) {
    if (!it->second.empty()) {
      TRI_doc_mptr_t vertex;
      bool fetchVertex = true;
      for (auto const& exp : it->second) {
        TRI_ASSERT(exp != nullptr);

        if (!exp->isEdgeAccess) {
          if (fetchVertex) {
            fetchVertex = false;
            int result = FetchDocumentById(_trx, v, &vertex);
            ++_readDocuments;
            if (result != TRI_ERROR_NO_ERROR) {
              // Vertex does not exist
              _done = true;
              return;
            }

            _vertices.emplace(v, vertex.vpack());
          }
          if (!exp->matchesCheck(_trx, VPackSlice(vertex.vpack()))) {
            ++_filteredPaths;
            _done = true;
            return;
          }
        }
      }
    }
  }

  _vertexGetter->reset(v);
  if (_opts.useBreadthFirst) {
    _enumerator.reset(
        new basics::BreadthFirstEnumerator<std::string, std::string, size_t>(
            _edgeGetter.get(), _vertexGetter.get(), v, _opts.maxDepth));
  } else {
    _enumerator.reset(
        new basics::DepthFirstEnumerator<std::string, std::string, size_t>(
            _edgeGetter.get(), _vertexGetter.get(), v, _opts.maxDepth));
  }
  _done = false;
}

TraversalPath* SingleServerTraverser::next() {
  TRI_ASSERT(!_done);
  if (_pruneNext) {
    _pruneNext = false;
    _enumerator->prune();
  }
  TRI_ASSERT(!_pruneNext);
  basics::EnumeratedPath<std::string, std::string> const& path = _enumerator->next();
  if (path.vertices.empty()) {
    _done = true;
    // Done traversing
    return nullptr;
  }
  if (_opts.uniqueVertices == TraverserOptions::UniquenessLevel::PATH) {
    // it is sufficient to check if any of the vertices on the path is equal to the end.
    // Then we prune and any intermediate equality cannot happen.
    auto& last = path.vertices.back();
    auto found = std::find(path.vertices.begin(), path.vertices.end(), last);
    TRI_ASSERT(found != path.vertices.end()); // We have to find it once, it is at least the last!
    if ((++found) != path.vertices.end()) {
      // Test if we found the last element. That is ok.
      _pruneNext = true;
      return next();
    }
  }

  size_t countEdges = path.edges.size();
  if (_opts.useBreadthFirst &&
      _opts.uniqueVertices == TraverserOptions::UniquenessLevel::NONE &&
      _opts.uniqueEdges == TraverserOptions::UniquenessLevel::PATH) {
    // Only if we use breadth first
    // and vertex uniqueness is not guaranteed
    // We have to validate edges on path uniqueness.
    // Otherwise this situation cannot occur.
    // If two edges are identical than at least their start or end vertex
    // is on the path twice: A -> B <- A
    for (size_t i = 0; i < countEdges; ++i) {
      for (size_t j = i + 1; j < countEdges; ++j) {
        if (path.edges[i] == path.edges[j]) {
          // We found two idential edges. Prune.
          // Next
          _pruneNext = true;
          return next();
        }
      }
    }
  }

  if (countEdges < _opts.minDepth) {
    return next();
  }

  return new SingleServerTraversalPath(path, this);
}

bool SingleServerTraverser::EdgeGetter::nextCursor(std::string const& startVertex,
                                                   size_t& eColIdx,
                                                   size_t*& last) {
  std::string eColName;

  while (true) {
    arangodb::Transaction::IndexHandle indexHandle;
    if (last != nullptr) {
      // The cursor is empty clean up
      last = nullptr;
      TRI_ASSERT(!_posInCursor.empty());
      TRI_ASSERT(!_cursors.empty());
      TRI_ASSERT(!_results.empty());
      _posInCursor.pop();
      _cursors.pop();
      _results.pop();
    }
    if (!_opts.getCollectionAndSearchValue(eColIdx, startVertex, eColName, indexHandle,
                                           _builder)) {
      // If we get here there are no valid edges at all
      return false;
    }
    
    std::unique_ptr<OperationCursor> cursor = _trx->indexScan(
        eColName, arangodb::Transaction::CursorType::INDEX, indexHandle,
        _builder.slice(), 0, UINT64_MAX, Transaction::defaultBatchSize(), false);
    if (cursor->failed()) {
      // Some error, ignore and go to next
      eColIdx++;
      continue;
    }
    TRI_ASSERT(_posInCursor.size() == _cursors.size());
    _cursors.push(std::move(cursor));
    _results.emplace();
    return true;
  }
}

void SingleServerTraverser::EdgeGetter::nextEdge(
    std::string const& startVertex, size_t& eColIdx, size_t*& last,
    std::vector<std::string>& edges) {

  if (last == nullptr) {
    _posInCursor.push(0);
    last = &_posInCursor.top();
  } else {
    ++(*last);
  }

  while (true) {
    TRI_ASSERT(!_cursors.empty());
    auto& cursor = _cursors.top();
    TRI_ASSERT(!_results.empty());
    auto& mptrs = _results.top();

    // note: we need to check *first* that there is actually something in the mptrs list
    if (mptrs.empty() || mptrs.size() <= *last) {
      if (cursor->hasMore()) {
        // Fetch next and try again
        cursor->getMoreMptr(mptrs);
        TRI_ASSERT(last != nullptr);
        *last = 0;
        _traverser->_readDocuments += static_cast<size_t>(mptrs.size());
        continue;
      }
      eColIdx++;
      if (!nextCursor(startVertex, eColIdx, last)) {
        // No further edges.
        TRI_ASSERT(last == nullptr);
        TRI_ASSERT(_cursors.size() == _posInCursor.size());
        TRI_ASSERT(_cursors.size() == _results.size());
        return;
      }
      // There is a new Cursor on top of the stack, try it
      _posInCursor.push(0);
      last = &_posInCursor.top();
      continue;
    }

    VPackSlice edge(mptrs[*last]->vpack());
    std::string id = _trx->extractIdString(edge);
    if (!_traverser->edgeMatchesConditions(edge, edges.size())) {
      if (_opts.uniqueEdges == TraverserOptions::UniquenessLevel::GLOBAL) {
        // Insert a dummy to please the uniqueness
        _traverser->_edges.emplace(id, nullptr);
      }

      TRI_ASSERT(last != nullptr);
      (*last)++;
      continue;
    }
    if (_opts.uniqueEdges == TraverserOptions::UniquenessLevel::PATH) {
      // test if edge is already on this path
      auto found = std::find(edges.begin(), edges.end(), id);
      if (found != edges.end()) {
        // This edge is already on the path, next
        TRI_ASSERT(last != nullptr);
        (*last)++;
        continue;
      }
    } else if (_opts.uniqueEdges == TraverserOptions::UniquenessLevel::GLOBAL) {
      if (_traverser->_edges.find(id) != _traverser->_edges.end()) {
        // This edge is already on the path, next
        TRI_ASSERT(last != nullptr);
        (*last)++;
        continue;
      }
    }

    _traverser->_edges.emplace(id, edge.begin());
    edges.emplace_back(std::move(id));
    return;
  }
}

void SingleServerTraverser::EdgeGetter::getEdge(std::string const& startVertex,
                                                std::vector<std::string>& edges,
                                                size_t*& last,
                                                size_t& eColIdx) {
  if (last == nullptr) {
    eColIdx = 0;
    if (!nextCursor(startVertex, eColIdx, last)) {
      // We were not able to find any edge
      return;
    }
  }
  nextEdge(startVertex, eColIdx, last, edges);
}

void SingleServerTraverser::EdgeGetter::getAllEdges(
    std::string const& startVertex, std::unordered_set<std::string>& edges,
    size_t depth) {

  size_t idxId = 0;
  std::string eColName;
  arangodb::Transaction::IndexHandle indexHandle;
  std::vector<TRI_doc_mptr_t*> mptrs;

  // We iterate over all index ids. note idxId++
  while (_opts.getCollectionAndSearchValue(idxId++, startVertex, eColName,
                                           indexHandle, _builder)) {
    std::unique_ptr<OperationCursor> cursor = _trx->indexScan(
        eColName, arangodb::Transaction::CursorType::INDEX, indexHandle,
        _builder.slice(), 0, UINT64_MAX, Transaction::defaultBatchSize(), false);
    if (cursor->failed()) {
      // Some error, ignore and go to next
      continue;
    }
    mptrs.clear();
    while (cursor->hasMore()) {
      cursor->getMoreMptr(mptrs, UINT64_MAX);
      edges.reserve(mptrs.size());

      _traverser->_readDocuments += static_cast<size_t>(mptrs.size());
        
      std::string id;
      for (auto const& mptr : mptrs) {
        VPackSlice edge(mptr->vpack());
        id = _trx->extractIdString(edge);
        if (!_traverser->edgeMatchesConditions(edge, depth)) {
          if (_opts.uniqueEdges == TraverserOptions::UniquenessLevel::GLOBAL) {
            // Insert a dummy to please the uniqueness
            _traverser->_edges.emplace(std::move(id), nullptr);
          }
          continue;
        }
        if (_opts.uniqueEdges == TraverserOptions::UniquenessLevel::PATH) {
          // test if edge is already on this path
          auto found = edges.find(id);
          if (found != edges.end()) {
            // This edge is already on the path, next
            continue;
          }
        } else if (_opts.uniqueEdges == TraverserOptions::UniquenessLevel::GLOBAL) {
          if (_traverser->_edges.find(id) != _traverser->_edges.end()) {
            // This edge is already on the path, next
            continue;
          }
        }

        _traverser->_edges.emplace(id, edge.begin());
        edges.emplace(std::move(id));
      }
    }
  }
}

