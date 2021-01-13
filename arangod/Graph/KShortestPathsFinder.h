////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
#include "Containers/HashSet.h"
#include "Graph/EdgeDocumentToken.h"
#include "Graph/ShortestPathFinder.h"
#include "Graph/ShortestPathPriorityQueue.h"

#include <velocypack/StringRef.h>

#include <list>
#include <memory>
#include <optional>

namespace arangodb {

namespace velocypack {
class Slice;
}

namespace graph {
class EdgeCursor;
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

  typedef arangodb::containers::HashSet<VertexRef, std::hash<VertexRef>, std::equal_to<VertexRef>> VertexSet;
  typedef arangodb::containers::HashSet<Edge, std::hash<Edge>, std::equal_to<Edge>> EdgeSet;
  enum Direction { FORWARD, BACKWARD };

  // TODO: This could be merged with ShortestPathResult
  //       or become a class to pass around paths
  struct Path {
    std::deque<VertexRef> _vertices;
    std::deque<Edge> _edges;

    // weight of path to vertex
    // where _weights.front() == 0 and
    //       _weights.back() == _weight.
    std::deque<double> _weights;
    double _weight;

    // Where this path branched off the previous shortest path
    // This is an optimization because we only need to consider
    // spur paths from after the branch point
    size_t _branchpoint;

    void clear() {
      _vertices.clear();
      _edges.clear();
      _weights.clear();
      _weight = 0;
      _branchpoint = 0;
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
      TRI_ASSERT(!_weights.empty());

      double ew = _weights.back();
      double pw = p._weights.at(a);
      while (a < b) {
        _edges.emplace_back(p._edges.at(a));
        a++;
        _vertices.emplace_back(p._vertices.at(a));
        _weights.emplace_back(ew + (p._weights.at(a) - pw));
      }
      _weight = _weights.back();
    }
    // TODO: implement == for EdgeDocumentToken and VertexRef
    // so these things become less cluttery
    bool operator==(Path const& rhs) const {
      if (_edges.size() != rhs._edges.size() ||
          _vertices.size() != rhs._vertices.size()) {
        return false;
      }
      for (size_t i = 0; i < _vertices.size(); ++i) {
        if (!_vertices[i].equals(rhs._vertices[i])) {
          return false;
        }
      }
      for (size_t i = 0; i < _edges.size(); ++i) {
        if (!_edges[i].equals(rhs._edges[i])) {
          return false;
        }
      }
      return true;
    }
  };

  //
  // Datastructures required for Dijkstra
  //
  struct DijkstraInfo {
    VertexRef _vertex;
    Edge _edge;
    VertexRef _pred;
    double _weight;

    // If true, we know that the path leading from the center of the Ball
    // under consideration to this vertex following the _pred
    // is the shortest/lowest weight amongst all paths leading to this vertex.
    bool _done;

    // Interface needed for ShortestPathPriorityQueue
    double weight() const { return _weight; }
    VertexRef getKey() const { return _vertex; }
    void setWeight(double weight) { _weight = weight; }

    DijkstraInfo(VertexRef const& vertex, Edge const&& edge, VertexRef const& pred, double weight)
        : _vertex(vertex), _edge(std::move(edge)), _pred(pred), _weight(weight), _done(false) {}
    explicit DijkstraInfo(VertexRef const& vertex)
        : _vertex(vertex), _weight(0), _done(true) {}
  };

  typedef ShortestPathPriorityQueue<VertexRef, DijkstraInfo, double> Frontier;

  // Dijkstra is run using two Balls, one around the start vertex, one around
  // the end vertex
  struct Ball {
    Direction const _direction;
    VertexRef _center;
    Frontier _frontier;
    // The distance of the last node that has been fully expanded
    // from _center
    double _closest;

    explicit Ball(Direction direction) 
        : _direction(direction), _closest(0) {}
 
    void reset(VertexRef center) { 
      _center = center; 
      _frontier.clear();
      _frontier.insert(center, std::make_unique<DijkstraInfo>(center));
      _closest = 0;
    }

    VertexRef center() const { return _center; }
    
    bool done(std::optional<double> const& currentBest) {
      return _frontier.empty() || (currentBest.has_value() && currentBest.value() < _closest);
    }
  };

  //
  // Caching functionality
  //

  // one step in the neighbourhood of a vertex
  // used for readability to not pass anonymous
  // 3-tuples around
  struct Step {
    Edge _edge;
    VertexRef _vertex;
    double _weight;

    Step(Edge&& edge, VertexRef const& vertex, double weight)
        : _edge(std::move(edge)), _vertex(vertex), _weight(weight) {}
  };

  // A vertex that was discovered while computing
  // a shortest path. Used for caching neightbours
  // and path information
  struct FoundVertex {
    VertexRef _vertex;

    bool _hasCachedOutNeighbours;
    bool _hasCachedInNeighbours;
    std::vector<Step> _outNeighbours;
    std::vector<Step> _inNeighbours;

    explicit FoundVertex(VertexRef const& vertex)
        : _vertex(vertex), _hasCachedOutNeighbours(false), _hasCachedInNeighbours(false) {}
  };
  // Contains the vertices that were found while searching
  // for a shortest path between start and end together with
  // the number of paths leading to that vertex and information
  // how to trace paths from the vertex from start/to end.
  typedef std::unordered_map<VertexRef, FoundVertex> FoundVertexCache;

 public:
  explicit KShortestPathsFinder(ShortestPathOptions& options);
  ~KShortestPathsFinder();

  // reset the traverser; this is mainly needed because the traverser is
  // part of the KShortestPathsExecutorInfos, and hence not recreated when
  // a cursor is initialized.
  void clear() override;

  // This is here because we inherit from ShortestPathFinder (to get the destroyEngines function)
  // TODO: Remove
  bool shortestPath(arangodb::velocypack::Slice const& start,
                    arangodb::velocypack::Slice const& target,
                    arangodb::graph::ShortestPathResult& result) override {
    TRI_ASSERT(false);
    return false;
  }

  // initialize k Shortest Paths
  TEST_VIRTUAL bool startKShortestPathsTraversal(arangodb::velocypack::Slice const& start,
                                                 arangodb::velocypack::Slice const& end);

  // get the next available path as AQL value.
  TEST_VIRTUAL bool getNextPathAql(arangodb::velocypack::Builder& builder);
  // get the next available path as a ShortestPathResult
  // TODO: this is only here to not break catch-tests and needs a cleaner solution.
  //       probably by making ShortestPathResult versatile enough and using that
#ifdef ARANGODB_USE_GOOGLE_TESTS
  bool getNextPathShortestPathResult(ShortestPathResult& path);
#endif
  // get the next available path as a Path
  bool getNextPath(Path& path);
  TEST_VIRTUAL bool skipPath();
  TEST_VIRTUAL bool isDone() const { return _traversalDone; }

 private:
  // Compute the first shortest path
  bool computeShortestPath(VertexRef const& start, VertexRef const& end,
                           VertexSet const& forbiddenVertices,
                           EdgeSet const& forbiddenEdges, 
                           Path& result);
  bool computeNextShortestPath(Path& result);

  void reconstructPath(Ball const& left, Ball const& right,
                       VertexRef const& join, Path& result);

  // make sure the neighbourhood of vertex is in the cache, and return
  // that neighbourhood.
  void computeNeighbourhoodOfVertexCache(VertexRef vertex, Direction direction,
                                         std::vector<Step>*& steps);
  // get the neighbourhood of the vertex in the given direction
  void computeNeighbourhoodOfVertex(VertexRef vertex, Direction direction,
                                    std::vector<Step>& steps);

  void advanceFrontier(Ball& source, Ball const& target,
                       VertexSet const& forbiddenVertices,
                       EdgeSet const& forbiddenEdges,
                       VertexRef& join, std::optional<double>& currentBest);

 private:
  bool _traversalDone{true};

  VertexRef _start;
  VertexRef _end;
    
  Ball _left;
  Ball _right;

  FoundVertexCache _vertexCache;

  std::vector<Path> _shortestPaths;

  std::list<Path> _candidatePaths;

  std::unique_ptr<EdgeCursor> _forwardCursor;
  std::unique_ptr<EdgeCursor> _backwardCursor;

  // a temporary object that is reused for building results
  Path _tempPath;

  // a temporary object that is reused for building candidate results
  Path _candidate;
};

}  // namespace graph
}  // namespace arangodb
#endif
