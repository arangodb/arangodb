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
#include "Graph/ShortestPathResult.h"
#include "Transaction/Helpers.h"
#include "Utils/OperationCursor.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/TraverserCache.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::graph;

ConstantWeightShortestPathFinder::ConstantWeightShortestPathFinder(
    ShortestPathOptions* options)
    : _options(options),
      _mmdr(new ManagedDocumentResult{}) {}

ConstantWeightShortestPathFinder::~ConstantWeightShortestPathFinder() {
  clearVisited();
}

bool ConstantWeightShortestPathFinder::shortestPath(
    arangodb::velocypack::Slice const& s,
    arangodb::velocypack::Slice const& e,
    arangodb::graph::ShortestPathResult& result,
    std::function<void()> const& callback) {
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

  std::vector<arangodb::StringRef> edges;
  std::vector<arangodb::StringRef> neighbors;
  std::deque<arangodb::StringRef> nextClosure;

  while (!_leftClosure.empty() && !_rightClosure.empty()) {
    callback();

    edges.clear();
    neighbors.clear();
    nextClosure.clear();
    if (_leftClosure.size() < _rightClosure.size()) {
      for (auto& v : _leftClosure) {
        expandVertex(false, v, edges, neighbors);
        size_t const neighborsSize = neighbors.size();
        TRI_ASSERT(edges.size() == neighborsSize);
        for (size_t i = 0; i < neighborsSize; ++i) {
          auto const& n = neighbors[i];
          if (_leftFound.find(n) == _leftFound.end()) {
            auto leftFoundIt =
                _leftFound.emplace(n, new PathSnippet(v, edges[i])).first;
            auto rightFoundIt = _rightFound.find(n);
            if (rightFoundIt != _rightFound.end()) {
              result._vertices.emplace_back(n);
              auto it = leftFoundIt;
              arangodb::StringRef next;
              while (it != _leftFound.end() && it->second != nullptr) {
                next = it->second->_pred;
                result._vertices.push_front(next);
                result._edges.push_front(it->second->_path);
                it = _leftFound.find(next);
              }
              it = rightFoundIt;
              while (it != _rightFound.end() && it->second != nullptr) {
                next = it->second->_pred;
                result._vertices.emplace_back(next);
                result._edges.emplace_back(it->second->_path);
                it = _rightFound.find(next);
              }

              TRI_IF_FAILURE("TraversalOOMPath") {
                THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
              }
              _options->fetchVerticesCoordinator(result._vertices);
              return true;
            }
            nextClosure.emplace_back(n);
          }
        }
      }
      _leftClosure.swap(nextClosure);
      nextClosure.clear();
    } else {
      for (auto& v : _rightClosure) {
        expandVertex(true, v, edges, neighbors);
        size_t const neighborsSize = neighbors.size();
        TRI_ASSERT(edges.size() == neighborsSize);
        for (size_t i = 0; i < neighborsSize; ++i) {
          auto const& n = neighbors[i];
          if (_rightFound.find(n) == _rightFound.end()) {
            auto rightFoundIt =
                _rightFound.emplace(n, new PathSnippet(v, edges[i])).first;
            auto leftFoundIt = _leftFound.find(n);
            if (leftFoundIt != _leftFound.end()) {
              result._vertices.emplace_back(n);
              auto it = leftFoundIt;
              arangodb::StringRef next;
              while (it != _leftFound.end() && it->second != nullptr) {
                next = it->second->_pred;
                result._vertices.push_front(next);
                result._edges.push_front(it->second->_path);
                it = _leftFound.find(next);
              }
              it = rightFoundIt;
              while (it != _rightFound.end() && it->second != nullptr) {
                next = it->second->_pred;
                result._vertices.emplace_back(next);
                result._edges.emplace_back(it->second->_path);
                it = _rightFound.find(next);
              }

              TRI_IF_FAILURE("TraversalOOMPath") {
                THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
              }
              _options->fetchVerticesCoordinator(result._vertices);
              return true;
            }
            nextClosure.emplace_back(n);
          }
        }
      }
      _rightClosure.swap(nextClosure);
      nextClosure.clear();
    }
  }
  return false;
}

void ConstantWeightShortestPathFinder::expandVertex(
    bool backward, StringRef vertex, std::vector<StringRef>& edges,
    std::vector<StringRef>& neighbors) {
  std::unique_ptr<EdgeCursor> edgeCursor;
  if (backward) {
    edgeCursor.reset(_options->nextReverseCursor(_mmdr.get(), vertex));
  } else {
    edgeCursor.reset(_options->nextCursor(_mmdr.get(), vertex));
  }

  auto callback = [&] (arangodb::StringRef const& eid, VPackSlice edge, size_t cursorIdx) -> void {
    StringRef other(transaction::helpers::extractFromFromDocument(edge));
    if (other == vertex) {
      other = StringRef(transaction::helpers::extractToFromDocument(edge));
    }
    if (other != vertex) {
      StringRef id = _options->cache()->persistString(other);
      edges.emplace_back(eid);
      neighbors.emplace_back(id);
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
