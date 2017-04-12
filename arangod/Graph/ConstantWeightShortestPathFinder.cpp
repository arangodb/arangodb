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
#include "Cluster/ServerState.h"
#include "Utils/OperationCursor.h"
#include "VocBase/EdgeCollectionInfo.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/Traverser.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::graph;

ConstantWeightShortestPathFinder::ConstantWeightShortestPathFinder(
    arangodb::aql::ShortestPathBlock* block)
    : _block(block) {}

ConstantWeightShortestPathFinder::~ConstantWeightShortestPathFinder() {
  clearVisited();
}

bool ConstantWeightShortestPathFinder::shortestPath(
    arangodb::velocypack::Slice const& start,
    arangodb::velocypack::Slice const& end,
    arangodb::traverser::ShortestPath& result,
    std::function<void()> const& callback) {
  result.clear();
  // Init
  if (start == end) {
    result._vertices.emplace_back(start);
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

  std::vector<arangodb::velocypack::Slice> edges;
  std::vector<arangodb::velocypack::Slice> neighbors;
  std::deque<arangodb::velocypack::Slice> nextClosure;
  while (!_leftClosure.empty() && !_rightClosure.empty()) {
    callback();

    edges.clear();
    neighbors.clear();
    nextClosure.clear();
    if (_leftClosure.size() < _rightClosure.size()) {
      for (arangodb::velocypack::Slice& v : _leftClosure) {
        if (arangodb::ServerState::instance()->isCoordinator()) {
          expandVertexCluster(false, v, edges, neighbors);
        } else {
          expandVertex(false, v, edges, neighbors);
        }
        size_t const neighborsSize = neighbors.size();
        TRI_ASSERT(edges.size() == neighborsSize);
        for (size_t i = 0; i < neighborsSize; ++i) {
          arangodb::velocypack::Slice const& n = neighbors[i];
          if (_leftFound.find(n) == _leftFound.end()) {
            auto leftFoundIt =
                _leftFound.emplace(n, new PathSnippet(v, edges[i])).first;
            auto rightFoundIt = _rightFound.find(n);
            if (rightFoundIt != _rightFound.end()) {
              result._vertices.emplace_back(n);
              auto it = leftFoundIt;
              arangodb::velocypack::Slice next;
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
              return true;
            }
            nextClosure.emplace_back(n);
          }
        }
      }
      _leftClosure = std::move(nextClosure);
      nextClosure.clear();
    } else {
      for (arangodb::velocypack::Slice& v : _rightClosure) {
        if (arangodb::ServerState::instance()->isCoordinator()) {
          expandVertexCluster(true, v, edges, neighbors);
        } else {
          expandVertex(true, v, edges, neighbors);
        }
        size_t const neighborsSize = neighbors.size();
        TRI_ASSERT(edges.size() == neighborsSize);
        for (size_t i = 0; i < neighborsSize; ++i) {
          arangodb::velocypack::Slice const& n = neighbors[i];
          if (_rightFound.find(n) == _rightFound.end()) {
            auto rightFoundIt =
                _rightFound.emplace(n, new PathSnippet(v, edges[i])).first;
            auto leftFoundIt = _leftFound.find(n);
            if (leftFoundIt != _leftFound.end()) {
              result._vertices.emplace_back(n);
              auto it = leftFoundIt;
              arangodb::velocypack::Slice next;
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
              return true;
            }
            nextClosure.emplace_back(n);
          }
        }
      }
      _rightClosure = std::move(nextClosure);
      nextClosure.clear();
    }
  }
  return false;
}

void ConstantWeightShortestPathFinder::expandVertex(
    bool backward, VPackSlice& vertex, std::vector<VPackSlice>& edges,
    std::vector<VPackSlice>& neighbors) {
  TRI_ASSERT(vertex.isString());
  std::string id = vertex.copyString();
  ManagedDocumentResult* mmdr = _block->_mmdr.get();
  std::unique_ptr<arangodb::OperationCursor> edgeCursor;
  for (auto const& edgeCollection : _block->_collectionInfos) {
    TRI_ASSERT(edgeCollection != nullptr);
    if (backward) {
      edgeCursor = edgeCollection->getReverseEdges(id, mmdr);
    } else {
      edgeCursor = edgeCollection->getEdges(id, mmdr);
    }

    LogicalCollection* collection = edgeCursor->collection();
    auto cb = [&](DocumentIdentifierToken const& element) {
      if (collection->readDocument(_block->transaction(), element, *mmdr)) {
        VPackSlice edge(mmdr->vpack());
        VPackSlice from = transaction::helpers::extractFromFromDocument(edge);
        if (from == vertex) {
          VPackSlice to = transaction::helpers::extractToFromDocument(edge);
          if (to != vertex) {
            edges.emplace_back(edge);
            neighbors.emplace_back(to);
          }
        } else {
          edges.emplace_back(edge);
          neighbors.emplace_back(from);
        }
      }
    };
    while (edgeCursor->getMore(cb, 1000)) {
    }
  }
}

void ConstantWeightShortestPathFinder::expandVertexCluster(
    bool backward, VPackSlice& vertex, std::vector<VPackSlice>& resEdges,
    std::vector<VPackSlice>& neighbors) {
  int res = TRI_ERROR_NO_ERROR;
  for (auto const& edgeCollection : _block->_collectionInfos) {
    VPackBuilder result;
    TRI_ASSERT(edgeCollection != nullptr);
    if (backward) {
      res = edgeCollection->getReverseEdgesCoordinator(vertex, result);
    } else {
      res = edgeCollection->getEdgesCoordinator(vertex, result);
    }

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    VPackSlice edges = result.slice().get("edges");
    for (auto const& edge : VPackArrayIterator(edges)) {
      VPackSlice from = transaction::helpers::extractFromFromDocument(edge);
      if (from == vertex) {
        VPackSlice to = transaction::helpers::extractToFromDocument(edge);
        if (to != vertex) {
          resEdges.emplace_back(edge);
          neighbors.emplace_back(to);
        }
      } else {
        resEdges.emplace_back(edge);
        neighbors.emplace_back(from);
      }
    }
    // Make sure the data Slices are pointing to is not running out of scope.
    // This is not thread-safe!
    _block->_coordinatorCache.emplace_back(result.steal());
  }
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
