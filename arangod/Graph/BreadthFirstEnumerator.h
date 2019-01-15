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

#ifndef ARANGODB_GRAPH_BREADTHFIRSTENUMERATOR_H
#define ARANGODB_GRAPH_BREADTHFIRSTENUMERATOR_H 1

#include "Basics/Common.h"
#include "Graph/PathEnumerator.h"

namespace arangodb {

namespace traverser {
class Traverser;
struct TraverserOptions;
}  // namespace traverser

namespace graph {

class BreadthFirstEnumerator final : public arangodb::traverser::PathEnumerator {
 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief One entry in the schreier vector
  //////////////////////////////////////////////////////////////////////////////

  struct PathStep {
    size_t sourceIdx;
    graph::EdgeDocumentToken edge;
    arangodb::StringRef const vertex;

   public:
    explicit PathStep(arangodb::StringRef const vertex);

    PathStep(size_t sourceIdx, graph::EdgeDocumentToken&& edge,
             arangodb::StringRef const vertex);

    ~PathStep();

    PathStep(PathStep& other);
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Struct to hold all information required to get the list of
  ///        connected edges
  //////////////////////////////////////////////////////////////////////////////

  struct NextStep {
    size_t sourceIdx;

   private:
    NextStep() = delete;

   public:
    explicit NextStep(size_t sourceIdx) : sourceIdx(sourceIdx) {}
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief schreier vector to store the visited vertices
  //////////////////////////////////////////////////////////////////////////////

  std::vector<std::unique_ptr<PathStep>> _schreier;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Next free index in schreier vector.
  //////////////////////////////////////////////////////////////////////////////

  size_t _schreierIndex;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Position of the last returned value in the schreier vector
  //////////////////////////////////////////////////////////////////////////////

  size_t _lastReturned;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Vector to store where to continue search on next depth
  //////////////////////////////////////////////////////////////////////////////

  std::vector<NextStep> _nextDepth;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Vector storing the position at current search depth
  //////////////////////////////////////////////////////////////////////////////

  std::vector<NextStep> _toSearch;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Vector storing the position at current search depth
  //////////////////////////////////////////////////////////////////////////////

  std::unordered_set<arangodb::velocypack::Slice> _tmpEdges;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Marker for the search depth. Used to abort searching.
  //////////////////////////////////////////////////////////////////////////////

  uint64_t _currentDepth;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief position in _toSearch. If this is >= _toSearch.size() we are done
  ///        with this depth.
  //////////////////////////////////////////////////////////////////////////////

  size_t _toSearchPos;

 public:
  BreadthFirstEnumerator(arangodb::traverser::Traverser* traverser,
                         arangodb::velocypack::Slice startVertex,
                         arangodb::traverser::TraverserOptions* opts);

  ~BreadthFirstEnumerator();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Get the next Path element from the traversal.
  //////////////////////////////////////////////////////////////////////////////

  bool next() override;

  aql::AqlValue lastVertexToAqlValue() override;

  aql::AqlValue lastEdgeToAqlValue() override;

  aql::AqlValue pathToAqlValue(arangodb::velocypack::Builder& result) override;

 private:
  inline size_t getDepth(size_t index) const {
    size_t depth = 0;
    while (index != 0) {
      ++depth;
      index = _schreier[index]->sourceIdx;
    }
    return depth;
  }

  /**
   * @brief Helper function to validate if the path contains the given
   *        vertex.
   *
   * @param index The index of the path to search for
   * @param vertex The vertex that should be checked against.
   *
   * @return true if the vertex is already in the path
   */
  bool pathContainsVertex(size_t index, StringRef vertex) const;

  /**
   * @brief Helper function to validate if the path contains the given
   *        edge.
   *
   * @param index The index of the path to search for
   * @param edge The edge that should be checked against.
   *
   * @return true if the edge is already in the path
   */
  bool pathContainsEdge(size_t index, graph::EdgeDocumentToken const& edge) const;
};
}  // namespace graph
}  // namespace arangodb

#endif
