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
#include "VocBase/SingleServerTraversalPath.h"

using namespace arangodb::traverser;

////////////////////////////////////////////////////////////////////////////////
/// @brief Get a document by it's ID. Also lazy locks the collection.
///        If DOCUMENT_NOT_FOUND this function will return normally
///        with a OperationResult.failed() == true.
///        On all other cases this function throws.
////////////////////////////////////////////////////////////////////////////////

static int FetchDocumentById(arangodb::Transaction* trx,
                             std::string const& id,
                             VPackBuilder& builder,
                             VPackBuilder& result) {
  size_t pos = id.find('/');
  if (pos == std::string::npos) {
    TRI_ASSERT(false);
    return TRI_ERROR_INTERNAL;
  }
  if (id.find('/', pos + 1) != std::string::npos) {
    TRI_ASSERT(false);
    return TRI_ERROR_INTERNAL;
  }

  std::string col = id.substr(0, pos);
  trx->addCollectionAtRuntime(col);
  builder.clear();
  builder.openObject();
  builder.add(arangodb::StaticStrings::KeyString, VPackValue(id.substr(pos + 1)));
  builder.close();

  int res = trx->documentFastPath(col, builder.slice(), result);

  if (res != TRI_ERROR_NO_ERROR && res != TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
    THROW_ARANGO_EXCEPTION(res);
  }
  return res;
}



SingleServerTraverser::SingleServerTraverser(
    TraverserOptions& opts, Transaction* trx,
    std::unordered_map<size_t, std::vector<TraverserExpression*>> const*
        expressions)
    : Traverser(opts, expressions), _edgeGetter(this, opts, trx), _trx(trx) {
  if (opts.uniqueVertices == TraverserOptions::UniquenessLevel::GLOBAL) {
    _vertexGetter = std::make_unique<UniqueVertexGetter>(this);
  } else {
    _vertexGetter = std::make_unique<VertexGetter>(this);
  }
}

bool SingleServerTraverser::edgeMatchesConditions(VPackSlice e, size_t depth) {
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
  return true;
}

bool SingleServerTraverser::vertexMatchesConditions(std::string const& v,
                                                  size_t depth) {
  TRI_ASSERT(_expressions != nullptr);

  auto it = _expressions->find(depth);

  if (it != _expressions->end()) {
    bool fetchVertex = true;
    std::shared_ptr<VPackBuffer<uint8_t>> vertex;
    for (auto const& exp : it->second) {
      TRI_ASSERT(exp != nullptr);

      if (!exp->isEdgeAccess) {
        if (fetchVertex) {
          fetchVertex = false;
          vertex = fetchVertexData(v);
        }
        if (!exp->matchesCheck(_trx, VPackSlice(vertex->data()))) {
          ++_filteredPaths;
          return false;
        }
      }
    }
  }
  return true;
}

std::shared_ptr<VPackBuffer<uint8_t>> SingleServerTraverser::fetchVertexData(
    std::string const& id) {
  auto it = _vertices.find(id);
  if (it == _vertices.end()) {
    VPackBuilder tmp;
    int res = FetchDocumentById(_trx, id, _builder, tmp);
    ++_readDocuments;
    if (res != TRI_ERROR_NO_ERROR) {
      TRI_ASSERT(res == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
      tmp.add(VPackValue(VPackValueType::Null));
      return tmp.steal();
    }
    auto shared_buffer = tmp.steal();
    _vertices.emplace(id, shared_buffer);
    return shared_buffer;
  }
  return it->second;
}

bool SingleServerTraverser::VertexGetter::getVertex(std::string const& edge,
                                                  std::string const& vertex,
                                                  size_t depth,
                                                  std::string& result) {
  auto const& it = _traverser->_edges.find(edge);
  TRI_ASSERT(it != _traverser->_edges.end());
  VPackSlice v(it->second->data());
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

void SingleServerTraverser::VertexGetter::reset() {
}

bool SingleServerTraverser::UniqueVertexGetter::getVertex(
    std::string const& edge, std::string const& vertex, size_t depth,
    std::string& result) {
  auto const& it = _traverser->_edges.find(edge);
  TRI_ASSERT(it != _traverser->_edges.end());
  VPackSlice v(it->second->data());
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

void SingleServerTraverser::UniqueVertexGetter::reset() {
  _returnedVertices.clear();
}

void SingleServerTraverser::setStartVertex(std::string const& v) {
  _pruneNext = false;

  TRI_ASSERT(_expressions != nullptr);

  auto it = _expressions->find(0);

  if (it != _expressions->end()) {
    if (!it->second.empty()) {
      std::shared_ptr<VPackBuffer<uint8_t>> vertex;
      bool fetchVertex = true;
      for (auto const& exp : it->second) {
        TRI_ASSERT(exp != nullptr);

        if (!exp->isEdgeAccess) {
          if (fetchVertex) {
            fetchVertex = false;
            VPackBuilder tmp;
            int result = FetchDocumentById(_trx, v, _builder, tmp);
            ++_readDocuments;
            if (result != TRI_ERROR_NO_ERROR) {
              // Vertex does not exist
              _done = true;
              return;
            }
            vertex = tmp.steal();
            _vertices.emplace(v, vertex);
          }
          if (!exp->matchesCheck(_trx, VPackSlice(vertex->data()))) {
            ++_filteredPaths;
            _done = true;
            return;
          }
        }
      }
    }
  }

  _vertexGetter->reset();
  _enumerator.reset(new basics::PathEnumerator<std::string, std::string, VPackValueLength>(
      _edgeGetter, _vertexGetter.get(), v));
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
    auto last = path.vertices.back();
    auto found = std::find(path.vertices.begin(), path.vertices.end(), last);
    TRI_ASSERT(found != path.vertices.end()); // We have to find it once, it is at least the last!
    if ((++found) != path.vertices.end()) {
      // Test if we found the last element. That is ok.
      _pruneNext = true;
      return next();
    }
  }
  size_t countEdges = path.edges.size();

  auto p = std::make_unique<SingleServerTraversalPath>(path, this);
  if (countEdges >= _opts.maxDepth) {
    _pruneNext = true;
  }
  if (countEdges < _opts.minDepth) {
    return next();
  }
  return p.release();
}

bool SingleServerTraverser::EdgeGetter::nextCursor(std::string const& startVertex,
                                                 size_t& eColIdx,
                                                 VPackValueLength*& last) {
  while (true) {
    std::string eColName;
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
    std::shared_ptr<OperationCursor> cursor = _trx->indexScan(
        eColName, arangodb::Transaction::CursorType::INDEX, indexHandle,
        _builder.slice(), 0, UINT64_MAX, Transaction::defaultBatchSize(), false);
    if (cursor->failed()) {
      // Some error, ignore and go to next
      eColIdx++;
      continue;
    }
    TRI_ASSERT(_posInCursor.size() == _cursors.size());
    _cursors.push(cursor);
    _results.emplace(std::make_shared<OperationResult>(TRI_ERROR_NO_ERROR));
    return true;
  }
}

void SingleServerTraverser::EdgeGetter::nextEdge(
    std::string const& startVertex, size_t& eColIdx, VPackValueLength*& last,
    std::vector<std::string>& edges) {
  if (last == nullptr) {
    _posInCursor.push(0);
    last = &_posInCursor.top();
  } else {
    ++(*last);
  }
  while (true) {
    TRI_ASSERT(!_cursors.empty());
    auto cursor = _cursors.top();
    TRI_ASSERT(!_results.empty());
    auto opRes = _results.top();
    TRI_ASSERT(opRes != nullptr);
    // note: we need to check *first* that there is actually something in the buffer
    // before we access its internals. otherwise, the buffer contents are uninitialized
    // and non-deterministic behavior will be the consequence
    VPackSlice edge = opRes->slice();
    if (opRes->buffer->empty() || !edge.isArray() || edge.length() <= *last) {
      if (cursor->hasMore()) {
        // Fetch next and try again
        cursor->getMore(opRes);
        TRI_ASSERT(last != nullptr);
        *last = 0;
        edge = opRes->slice();
        TRI_ASSERT(edge.isArray());
        _traverser->_readDocuments += static_cast<size_t>(edge.length());
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
    edge = edge.at(*last);
    if (!_traverser->edgeMatchesConditions(edge, edges.size())) {
      TRI_ASSERT(last != nullptr);
      (*last)++;
      continue;
    }
    std::string id = _trx->extractIdString(edge);
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

    VPackBuilder tmpBuilder = VPackBuilder::clone(edge);
    _traverser->_edges.emplace(id, tmpBuilder.steal());

    edges.emplace_back(id);
    return;
  }
}

void SingleServerTraverser::EdgeGetter::operator()(
    std::string const& startVertex,
    std::vector<std::string>& edges, VPackValueLength*& last, size_t& eColIdx,
    bool& dir) {
  if (last == nullptr) {
    eColIdx = 0;
    if (!nextCursor(startVertex, eColIdx, last)) {
      // We were not able to find any edge
      return;
    }
  }
  nextEdge(startVertex, eColIdx, last, edges);
}
