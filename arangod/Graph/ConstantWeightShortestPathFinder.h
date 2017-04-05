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

#ifndef ARANGODB_GRAPH_CONSTANT_WEIGHT_SHORTEST_PATH_FINDER_H
#define ARANGODB_GRAPH_CONSTANT_WEIGHT_SHORTEST_PATH_FINDER_H 1

#include "Graph/ShortestPathFinder.h"

namespace arangodb {
namespace graph {

template <typename VertexId, typename EdgeId, typename HashFuncType, typename EqualFuncType, typename Path>
class ConstDistanceFinder : public PathFinder<VertexId, Path> {
 public:

  //////////////////////////////////////////////////////////////////////////////
  /// @brief callback to find neighbors
  //////////////////////////////////////////////////////////////////////////////

  typedef std::function<void(VertexId& V, std::vector<EdgeId>& edges,
                             std::vector<VertexId>& neighbors)>
      ExpanderFunction;

 private:
  struct PathSnippet {
    VertexId const _pred;
    EdgeId const _path;

    PathSnippet(VertexId& pred, EdgeId& path) : _pred(pred), _path(path) {}
  };

  std::unordered_map<VertexId, PathSnippet*, HashFuncType, EqualFuncType> _leftFound;
  std::deque<VertexId> _leftClosure;

  std::unordered_map<VertexId, PathSnippet*, HashFuncType, EqualFuncType> _rightFound;
  std::deque<VertexId> _rightClosure;

  ExpanderFunction _leftNeighborExpander;
  ExpanderFunction _rightNeighborExpander;

 public:
  ConstDistanceFinder(ExpanderFunction left, ExpanderFunction right)
      : _leftNeighborExpander(left), _rightNeighborExpander(right) {}

  ~ConstDistanceFinder() {
    clearVisited();
  }

  bool shortestPath(VertexId const& start, VertexId const& end, 
                    Path& result, std::function<void()> const& callback) override {
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

    std::vector<EdgeId> edges;
    std::vector<VertexId> neighbors;
    std::deque<VertexId> nextClosure;
    while (!_leftClosure.empty() && !_rightClosure.empty()) {
      callback();

      edges.clear();
      neighbors.clear();
      nextClosure.clear();
      if (_leftClosure.size() < _rightClosure.size()) {
        for (VertexId& v : _leftClosure) {
          _leftNeighborExpander(v, edges, neighbors);
          size_t const neighborsSize = neighbors.size();
          TRI_ASSERT(edges.size() == neighborsSize);
          for (size_t i = 0; i < neighborsSize; ++i) {
            VertexId const& n = neighbors[i];
            if (_leftFound.find(n) == _leftFound.end()) {
              auto leftFoundIt = _leftFound.emplace(n, new PathSnippet(v, edges[i])).first;
              auto rightFoundIt = _rightFound.find(n);
              if (rightFoundIt != _rightFound.end()) {
                result._vertices.emplace_back(n);
                auto it = leftFoundIt;
                VertexId next;
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
        for (VertexId& v : _rightClosure) {
          _rightNeighborExpander(v, edges, neighbors);
          size_t const neighborsSize = neighbors.size();
          TRI_ASSERT(edges.size() == neighborsSize);
          for (size_t i = 0; i < neighborsSize; ++i) {
            VertexId const& n = neighbors[i];
            if (_rightFound.find(n) == _rightFound.end()) {
              auto rightFoundIt = _rightFound.emplace(n, new PathSnippet(v, edges[i])).first;
              auto leftFoundIt = _leftFound.find(n);
              if (leftFoundIt != _leftFound.end()) {
                result._vertices.emplace_back(n);
                auto it = leftFoundIt;
                VertexId next;
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

 private:
  void clearVisited() {  
    for (auto& it : _leftFound) {
      delete it.second;
    }
    _leftFound.clear();

    for (auto& it : _rightFound) {
      delete it.second;
    }
    _rightFound.clear();
  }

};


}  // namespace graph
}  // namespace arangodb
#endif
