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

#ifndef ARANGODB_GRAPH_CONSTANT_WEIGHT_K_SHORTEST_PATHS_FINDER_H
#define ARANGODB_GRAPH_CONSTANT_WEIGHT_K_SHORTEST_PATHS_FINDER_H 1

#include "Aql/AqlValue.h"
#include "Basics/VelocyPackHelper.h"
#include "Graph/EdgeDocumentToken.h"
#include "Graph/ShortestPathFinder.h"
#include "Graph/ShortestPathPriorityQueue.h"

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
class KShortestPathsFinder : public ShortestPathFinder {
 private:
  // Mainly for readability
  typedef arangodb::velocypack::StringRef VertexRef;
  typedef arangodb::graph::EdgeDocumentToken Edge;
  enum Direction { FORWARD, BACKWARD };

  // TODO: This could be merged with ShortestPathResult
  struct Path {
    std::deque<VertexRef> _vertices;
    std::deque<Edge> _edges;

    // weight of path to vertex
    // where _weights.front() == 0 and
    //       _weights.back() == _weight.
    std::deque<double> _weights;
    double _weight;

    void clear() {
      _vertices.clear();
      _edges.clear();
      _weights.clear();
      _weight = 0;
    }
    size_t length() const { return _vertices.size(); }
    void append(Path const& p, size_t a, size_t b) {
      if (this->length() == 0) {
        _vertices.emplace_back(p._vertices.at(a));
        _weights.emplace_back(0);
      }
      // Only append paths where the first vertex of p
      // is the same as the last vertex of this.
      TRI_ASSERT((_vertices.back().equals(p._vertices.front())));

      double ew = _weights.back();
      double pw = p._weights.at(a);
      while (a < b) {
        _edges.emplace_back(p._edges.at(a));
        a++;
        _vertices.emplace_back(p._vertices.at(a));
        _weights.emplace_back(ew + (p._weights.at(a) - pw));
      }
      _weight = _weights.back();
    };
    // TODO: implement == for EdgeDocumentToken and VertexRef
    // so these things become less cluttery
    bool operator==(Path const& rhs) const {
      if (_edges.size() != rhs._edges.size() ||
          _vertices.size() != rhs._vertices.size()) {
        return false;
      }
      for (size_t i = 0; i < _vertices.size(); ++i) {
        if (!_vertices.at(i).equals(rhs._vertices.at(i))) {
          return false;
        }
      }
      for (size_t i = 0; i < _edges.size(); ++i) {
        if (!_edges.at(i).equals(rhs._edges.at(i))) {
          return false;
        }
      }
      return true;
    }
  };
  struct Step {
    Edge _edge;
    VertexRef _vertex;
    double _weight;

    Step(Edge const& edge, VertexRef const& vertex, double weight)
        : _edge(edge), _vertex(vertex), _weight(weight) {}
  };

  struct FoundVertex {
    VertexRef _vertex;
    VertexRef _pred;
    Edge _edge;
    double _weight;
    bool _done;

    explicit FoundVertex(VertexRef const& vertex)
        : _vertex(vertex), _weight(0), _done(false) {}
    FoundVertex(VertexRef const& vertex, VertexRef const& pred, Edge&& edge, double weight)
        : _vertex(vertex), _pred(pred), _edge(std::move(edge)), _weight(weight), _done(false) {}
    double weight() const { return _weight; }
    void setWeight(double weight) { _weight = weight; }
    VertexRef const& getKey() const { return _vertex; }
  };
  typedef ShortestPathPriorityQueue<VertexRef, FoundVertex, double> Frontier;

  // Contains the vertices that were found while searching
  // for a shortest path between start and end together with
  // the number of paths leading to that vertex and information
  // how to trace paths from the vertex from start/to end.
  typedef std::unordered_map<VertexRef, FoundVertex*> FoundVertices;

  struct Ball {
    VertexRef _centre;
    Direction _direction;
    Frontier _frontier;

    Ball(void) {}
    Ball(VertexRef const& centre, Direction direction)
        : _centre(centre), _direction(direction) {
      auto v = std::make_unique<FoundVertex>(centre);
      v->_done = true;
      _frontier.insert(centre, std::move(v));
    }
    ~Ball() {
      // TODO free all vertices
    }
  };

 public:
  explicit KShortestPathsFinder(ShortestPathOptions& options);
  ~KShortestPathsFinder();

  // This is here because we inherit from ShortestPathFinder (to get the destroyEngines function)
  // TODO: Remove
  bool shortestPath(arangodb::velocypack::Slice const& start,
                    arangodb::velocypack::Slice const& target,
                    arangodb::graph::ShortestPathResult& result) override {
    TRI_ASSERT(false);
    return false;
  }

  //
  bool startKShortestPathsTraversal(arangodb::velocypack::Slice const& start,
                                    arangodb::velocypack::Slice const& end);

  // get the next available path as AQL value.
  bool getNextPathAql(arangodb::velocypack::Builder& builder);
  // get the next available path as a ShortestPathResult
  // TODO: this is only here to not break catch-tests and needs a cleaner solution.
  //       probably by making ShortestPathResult versatile enough and using that
  bool getNextPathShortestPathResult(ShortestPathResult& path);
  // get the next available path as a Path
  bool getNextPath(Path& path);
  bool isPathAvailable(void) { return _pathAvailable; }

 private:
  bool computeShortestPath(VertexRef const& start, VertexRef const& end,
                           std::unordered_set<VertexRef> const& forbiddenVertices,
                           std::unordered_set<Edge> const& forbiddenEdges, Path& result);
  bool computeNextShortestPath(Path& result);

  void reconstructPath(Ball const& left, Ball const& right,
                       VertexRef const& join, Path& result);

  void computeNeighbourhoodOfVertex(VertexRef vertex, Direction direction,
                                    std::vector<Step>& steps);

  // returns the number of paths found
  // TODO: check why target can't be const
  bool advanceFrontier(Ball& source, Ball const& target,
                       std::unordered_set<VertexRef> const& forbiddenVertices,
                       std::unordered_set<Edge> const& forbiddenEdges, VertexRef& join);

 private:
  bool _pathAvailable;

  VertexRef _start;
  VertexRef _end;
  std::vector<Path> _shortestPaths;
  // TODO: This should be a priority queue
  std::vector<Path> _candidatePaths;
};

}  // namespace graph
}  // namespace arangodb
#endif
