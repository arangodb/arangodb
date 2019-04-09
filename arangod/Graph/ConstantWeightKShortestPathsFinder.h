////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

// TODO: callback

#ifndef ARANGODB_GRAPH_CONSTANT_WEIGHT_K_SHORTEST_PATHS_FINDER_H
#define ARANGODB_GRAPH_CONSTANT_WEIGHT_K_SHORTEST_PATHS_FINDER_H 1

#include "Aql/AqlValue.h"
#include "Basics/VelocyPackHelper.h"
#include "Graph/EdgeDocumentToken.h"
#include "Graph/ShortestPathFinder.h"

#include <velocypack/StringRef.h>

namespace arangodb {

namespace velocypack {
class Slice;
}

namespace graph {

struct ShortestPathOptions;

// Inherit from ShortestPathfinder to get destroyEngines and not copy it
// again.
// TODO: Traverser.h has destroyEngines as well (the code for the two functions
//       is identical), refactor?
class ConstantWeightKShortestPathsFinder : public ShortestPathFinder {
 private:
  // Mainly for readability
  typedef arangodb::velocypack::StringRef VertexRef;
  typedef arangodb::graph::EdgeDocumentToken Edge;
  enum Direction { FORWARD, BACKWARD };

  struct Path {
    std::deque<VertexRef> _vertices;
    std::deque<Edge> _edges;

    void clear() {
      _vertices.clear();
      _edges.clear();
    };
    size_t length() const { return _vertices.size(); };
    void append(const Path& p, size_t a, size_t b) {
      if (a == b) {
        _vertices.emplace_back(p._vertices.at(a));
      } else if (this->length() == 0) {
        for (size_t i = a; i < b; ++i) {
          _vertices.emplace_back(p._vertices.at(i));
          _edges.emplace_back(p._edges.at(i));
        }
        _vertices.emplace_back(p._vertices.at(b));
      } else {
        if (this->_vertices.back().equals(p._vertices.front())) {
          _edges.emplace_back(p._edges.at(a));

          for (size_t i = a + 1; i < b; ++i) {
            _vertices.emplace_back(p._vertices.at(i));
            _edges.emplace_back(p._edges.at(i));
          }
          _vertices.emplace_back(p._vertices.at(b));
        } else {
        }
      }
    }
    void print(const std::string& pre) const {
      LOG_DEVEL << pre << " vertices " << _vertices.size();
      LOG_DEVEL << pre << " edges    " << _edges.size();
      for (auto& v : _vertices) {
        LOG_DEVEL << pre << "  v " << v.toString();
      }
    }
  };

  struct FoundVertex {
    bool _startOrEnd;
    VertexRef const _pred;
    Edge _edge;

    FoundVertex(VertexRef const& pred, Edge&& edge)
        : _startOrEnd(false), _pred(pred), _edge(std::move(edge)){};
    FoundVertex(bool startOrEnd)  // _npaths is 1 for start/end vertices
        : _startOrEnd(startOrEnd){};
  };
  typedef std::deque<VertexRef> Frontier;
  typedef std::deque<VertexRef> Trace;

  // Contains the vertices that were found while searching
  // for a shortest path between start and end together with
  // the number of paths leading to that vertex and information
  // how to trace paths from the vertex from start/to end.
  typedef std::unordered_map<VertexRef, FoundVertex> FoundVertices;

  struct Ball {
    VertexRef _centre;
    Direction _direction;
    FoundVertices _vertices;
    Frontier _frontier;
    Trace _trace;

    Ball(void){};
    Ball(const VertexRef& centre, Direction direction)
        : _centre(centre),
          _direction(direction),
          _vertices({{centre, FoundVertex(true)}}),
          _frontier({centre}){};

    void clear() {
      _vertices.clear();
      _frontier.clear();
      _trace.clear();
    };
    void setCentre(const VertexRef& centre) {
      _centre = centre;
      _frontier.emplace_back(centre);
      _vertices.emplace(centre, FoundVertex(true));
    }
    void setDirection(Direction direction) { _direction = direction; }
    void setup(const VertexRef& centre, Direction direction) {
      clear();
      setCentre(centre);
      setDirection(direction);
    }
  };

 public:
  explicit ConstantWeightKShortestPathsFinder(ShortestPathOptions& options);
  ~ConstantWeightKShortestPathsFinder();

  // This is here because we inherit from ShortestPathFinder (to get the destroyEngines function)
  // TODO: Remove
  bool shortestPath(arangodb::velocypack::Slice const& start,
                    arangodb::velocypack::Slice const& target,
                    arangodb::graph::ShortestPathResult& result,
                    std::function<void()> const& callback) {
    TRI_ASSERT(false);
    return false;
  };

  //
  bool startKShortestPathsTraversal(arangodb::velocypack::Slice const& start,
                                    arangodb::velocypack::Slice const& end);

  // get the next available path as AQL value.
  bool getNextPathAql(arangodb::velocypack::Builder& builder);
  // get the next available path as a ShortestPathResult
  bool getNextPath(arangodb::graph::ShortestPathResult& path);
  bool isPathAvailable(void) { return _pathAvailable; };

  void setCallback(std::function<void()> const& callback) { _callback = callback; };

 private:
  bool computeShortestPath(const VertexRef& start, const VertexRef& end,
                           const std::unordered_set<VertexRef>& forbiddenVertices,
                           const std::unordered_set<Edge>& forbiddenEdges, Path& result);
  bool computeNextShortestPath(Path& result);

  void reconstructPath(const Ball& left, const Ball& right,
                       const VertexRef& join, Path& result);

  void computeNeighbourhoodOfVertex(VertexRef vertex, Direction direction,
                                    std::vector<VertexRef>& neighbours,
                                    std::vector<Edge>& edges);

  // returns the number of paths found
  bool advanceFrontier(Ball& source, const Ball& target,
                       const std::unordered_set<VertexRef>& forbiddenVertices,
                       const std::unordered_set<Edge>& forbiddenEdges, VertexRef& join);

 private:
  std::function<void()> _callback;
  bool _pathAvailable;

  VertexRef _start;
  VertexRef _end;
  std::vector<Path> _shortestPaths;
};

}  // namespace graph
}  // namespace arangodb
#endif
