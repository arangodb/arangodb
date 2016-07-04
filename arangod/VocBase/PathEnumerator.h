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

#ifndef ARANGODB_VOCBASE_PATHENUMERATOR_H
#define ARANGODB_VOCBASE_PATHENUMERATOR_H 1

#include "Basics/Common.h"
#include <stack>

namespace arangodb {
namespace aql {
struct AqlValue;
}

namespace velocypack {
class Builder;
}

namespace traverser {
class Traverser;
struct TraverserOptions;

struct EnumeratedPath {
  std::vector<std::string> edges;
  std::vector<std::string> vertices;
  EnumeratedPath() {}
};


class PathEnumerator {

 protected:

  //////////////////////////////////////////////////////////////////////////////
  /// @brief This is the class that knows the details on how to
  ///        load the data and how to return data in the expected format
  ///        NOTE: This class does not known the traverser.
  //////////////////////////////////////////////////////////////////////////////

   traverser::Traverser* _traverser;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Indicates if we issue next() the first time.
  ///        It shall return an empty path in this case.
  //////////////////////////////////////////////////////////////////////////////

  bool _isFirst;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Maximal path length which should be enumerated.
  //////////////////////////////////////////////////////////////////////////////

  TraverserOptions const* _opts; 

  //////////////////////////////////////////////////////////////////////////////
  /// @brief List of the last path is used to
  //////////////////////////////////////////////////////////////////////////////

  EnumeratedPath _enumeratedPath;

 public:
  PathEnumerator(Traverser* traverser, std::string const& startVertex,
                 TraverserOptions const* opts)
      : _traverser(traverser), _isFirst(true), _opts(opts) {
    _enumeratedPath.vertices.push_back(startVertex);
    TRI_ASSERT(_enumeratedPath.vertices.size() == 1);
  }

  virtual ~PathEnumerator() {}

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Compute the next Path element from the traversal.
  ///        Returns false if there is no next path element.
  ///        Only if this is true one can compute the AQL values.
  //////////////////////////////////////////////////////////////////////////////

  virtual bool next() = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Prunes the current path prefix, the next function should not return
  ///        any path having this prefix anymore.
  //////////////////////////////////////////////////////////////////////////////

  virtual void prune() = 0;

  virtual aql::AqlValue lastVertexToAqlValue() = 0;
  virtual aql::AqlValue lastEdgeToAqlValue() = 0;
  virtual aql::AqlValue pathToAqlValue(arangodb::velocypack::Builder&) = 0;
};

class DepthFirstEnumerator final : public PathEnumerator {
 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief The pointers returned for edge indexes on this path. Used to
  /// continue
  ///        the search on respective levels.
  //////////////////////////////////////////////////////////////////////////////

  std::stack<size_t*> _lastEdges;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief An internal index for the edge collection used at each depth level
  //////////////////////////////////////////////////////////////////////////////

  std::stack<size_t> _lastEdgesIdx;

 public:
  DepthFirstEnumerator(
      Traverser* traverser,
      std::string const& startVertex, TraverserOptions const* opts)
    : PathEnumerator(traverser, startVertex, opts) {
    _lastEdges.push(nullptr);
    _lastEdgesIdx.push(0);
    TRI_ASSERT(_lastEdges.size() == 1);
  }

  ~DepthFirstEnumerator() {}

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Get the next Path element from the traversal.
  //////////////////////////////////////////////////////////////////////////////

  bool next() override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Prunes the current path prefix, the next function should not return
  ///        any path having this prefix anymore.
  //////////////////////////////////////////////////////////////////////////////

  void prune() override;

  aql::AqlValue lastVertexToAqlValue() override;

  aql::AqlValue lastEdgeToAqlValue() override;

  aql::AqlValue pathToAqlValue(arangodb::velocypack::Builder& result) override;

};

class BreadthFirstEnumerator final : public PathEnumerator {
 private:

  //////////////////////////////////////////////////////////////////////////////
  /// @brief One entry in the schreier vector 
  //////////////////////////////////////////////////////////////////////////////

  struct PathStep {
    size_t sourceIdx;
    std::string edge;
    std::string vertex;

   private:
    PathStep() {}

   public:
    explicit PathStep(std::string const& vertex) : sourceIdx(0), vertex(vertex) {}

    PathStep(size_t sourceIdx, std::string const& edge,
             std::string const& vertex)
        : sourceIdx(sourceIdx), edge(edge), vertex(vertex) {}
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
    explicit NextStep(size_t sourceIdx)
        : sourceIdx(sourceIdx) {}
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief schreier vector to store the visited vertices
  //////////////////////////////////////////////////////////////////////////////
  
   std::vector<PathStep*> _schreier;

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

   std::unordered_set<std::string> _tmpEdges;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Marker for the search depth. Used to abort searching.
  //////////////////////////////////////////////////////////////////////////////

   size_t _currentDepth;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief position in _toSerach. If this is >= _toSearch.size() we are done
  ///        with this depth.
  //////////////////////////////////////////////////////////////////////////////

   size_t _toSearchPos;

 public:
  BreadthFirstEnumerator(Traverser* traverser,
                         std::string const& startVertex, TraverserOptions const* opts);

  ~BreadthFirstEnumerator() {
    for (auto& it : _schreier) {
      delete it;
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Get the next Path element from the traversal.
  //////////////////////////////////////////////////////////////////////////////

  bool next() override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Prunes the current path prefix, the next function should not return
  ///        any path having this prefix anymore.
  //////////////////////////////////////////////////////////////////////////////

  void prune() override;

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

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Build the enumerated path for the given index in the schreier
  ///        vector.
  //////////////////////////////////////////////////////////////////////////////

  void computeEnumeratedPath(size_t index);
};


} // namespace traverser
} // namespace arangodb

#endif
