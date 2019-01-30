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

#include "ConstantWeightShortestPathFinder.h"

#include "Aql/ShortestPathBlock.h"
#include "Basics/StringRef.h"
#include "Cluster/ServerState.h"
#include "Graph/EdgeCursor.h"
#include "Graph/EdgeDocumentToken.h"
#include "Graph/ShortestPathResult.h"
#include "Graph/TraverserCache.h"
#include "Transaction/Helpers.h"
#include "Utils/OperationCursor.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::graph;

ConstantWeightShortestPathFinder::PathSnippet::PathSnippet(StringRef& pred,
                                                           EdgeDocumentToken&& path)
    : _pred(pred), _path(std::move(path)) {}

ConstantWeightShortestPathFinder::ConstantWeightShortestPathFinder(ShortestPathOptions* options)
    : _options(options), _mmdr(new ManagedDocumentResult{}) {}

ConstantWeightShortestPathFinder::~ConstantWeightShortestPathFinder() {
  clearVisited();
}

bool ConstantWeightShortestPathFinder::shortestPath(
    arangodb::velocypack::Slice const& s, arangodb::velocypack::Slice const& e,
    arangodb::graph::ShortestPathResult& result, std::function<void()> const& callback) {
  result.clear();
  TRI_ASSERT(s.isString());
  TRI_ASSERT(e.isString());
  StringRef start(s);
  StringRef end(e);
  // Init
  if (start == end) {
    result._vertices.emplace_back(start);
    _options->fetchVerticesCoordinator(result._vertices);
    return true;
  }
  _leftClosure.clear();
  _rightClosure.clear();
  clearVisited();

  _leftFound.emplace(start, nullptr);
  _rightFound.emplace(end, nullptr);
  _leftClosure.emplace_back(start);
  _rightClosure.emplace_back(end);

  TRI_IF_FAILURE("TraversalOOMInitialize") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  StringRef n;
  while (!_leftClosure.empty() && !_rightClosure.empty()) {
    callback();

    if (_leftClosure.size() < _rightClosure.size()) {
      if (expandClosure(_leftClosure, _leftFound, _rightFound, false, n)) {
        fillResult(n, result);
        return true;
      }
    } else {
      if (expandClosure(_rightClosure, _rightFound, _leftFound, true, n)) {
        fillResult(n, result);
        return true;
      }
    }
  }
  return false;
}

bool ConstantWeightShortestPathFinder::expandClosure(Closure& sourceClosure,
                                                     Snippets& sourceSnippets,
                                                     Snippets& targetSnippets,
                                                     bool isBackward, StringRef& result) {
  _nextClosure.clear();
  for (auto& v : sourceClosure) {
    _edges.clear();
    _neighbors.clear();
    expandVertex(isBackward, v);
    size_t const neighborsSize = _neighbors.size();
    TRI_ASSERT(_edges.size() == neighborsSize);

    for (size_t i = 0; i < neighborsSize; ++i) {
      auto const& n = _neighbors[i];
      if (sourceSnippets.find(n) == sourceSnippets.end()) {
        // NOTE: _edges[i] stays intact after move
        // and is reset to a nullptr. So if we crash
        // here no mem-leaks. or undefined behavior
        // Just make sure _edges is not used after
        sourceSnippets.emplace(n, new PathSnippet(v, std::move(_edges[i])));
        auto targetFoundIt = targetSnippets.find(n);
        if (targetFoundIt != targetSnippets.end()) {
          result = n;
          return true;
        }
        _nextClosure.emplace_back(n);
      }
    }
  }
  _edges.clear();
  _neighbors.clear();
  sourceClosure.swap(_nextClosure);
  _nextClosure.clear();
  return false;
}

void ConstantWeightShortestPathFinder::fillResult(StringRef& n,
                                                  arangodb::graph::ShortestPathResult& result) {
  result._vertices.emplace_back(n);
  auto it = _leftFound.find(n);
  TRI_ASSERT(it != _leftFound.end());
  arangodb::StringRef next;
  while (it != _leftFound.end() && it->second != nullptr) {
    next = it->second->_pred;
    result._vertices.push_front(next);
    result._edges.push_front(std::move(it->second->_path));
    it = _leftFound.find(next);
  }
  it = _rightFound.find(n);
  TRI_ASSERT(it != _rightFound.end());
  while (it != _rightFound.end() && it->second != nullptr) {
    next = it->second->_pred;
    result._vertices.emplace_back(next);
    result._edges.emplace_back(std::move(it->second->_path));
    it = _rightFound.find(next);
  }

  TRI_IF_FAILURE("TraversalOOMPath") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  _options->fetchVerticesCoordinator(result._vertices);
  clearVisited();
}

void ConstantWeightShortestPathFinder::expandVertex(bool backward, StringRef vertex) {
  std::unique_ptr<EdgeCursor> edgeCursor;
  if (backward) {
    edgeCursor.reset(_options->nextReverseCursor(_mmdr.get(), vertex));
  } else {
    edgeCursor.reset(_options->nextCursor(_mmdr.get(), vertex));
  }

  auto callback = [&](EdgeDocumentToken&& eid, VPackSlice edge, size_t cursorIdx) -> void {
    if (edge.isString()) {
      if (edge.compareString(vertex.data(), vertex.length()) != 0) {
        StringRef id = _options->cache()->persistString(StringRef(edge));
        _edges.emplace_back(std::move(eid));
        _neighbors.emplace_back(id);
      }
    } else {
      StringRef other(transaction::helpers::extractFromFromDocument(edge));
      if (other == vertex) {
        other = StringRef(transaction::helpers::extractToFromDocument(edge));
      }
      if (other != vertex) {
        StringRef id = _options->cache()->persistString(other);
        _edges.emplace_back(std::move(eid));
        _neighbors.emplace_back(id);
      }
    }
  };
  edgeCursor->readAll(callback);
}

void ConstantWeightShortestPathFinder::clearVisited() {
  for (auto& it : _leftFound) {
    delete it.second;
  }
  _leftFound.clear();

  for (auto& it : _rightFound) {
    delete it.second;
  }
  _rightFound.clear();
}
