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
#include "VocBase/TraverserOptions.h"
#include <velocypack/Slice.h>
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
  std::vector<arangodb::velocypack::Slice> edges;
  std::vector<arangodb::velocypack::Slice> vertices;
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

  /// @brief List which edges have been visited already.
  std::unordered_set<arangodb::velocypack::Slice> _returnedEdges;

 public:
  PathEnumerator(Traverser* traverser, arangodb::velocypack::Slice startVertex,
                 TraverserOptions const* opts)
      : _traverser(traverser), _isFirst(true), _opts(opts) {
    TRI_ASSERT(startVertex.isString());
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

  virtual aql::AqlValue lastVertexToAqlValue() = 0;
  virtual aql::AqlValue lastEdgeToAqlValue() = 0;
  virtual aql::AqlValue pathToAqlValue(arangodb::velocypack::Builder&) = 0;
};

class DepthFirstEnumerator final : public PathEnumerator {
 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief The stack of EdgeCursors to walk through.
  //////////////////////////////////////////////////////////////////////////////

  std::stack<std::unique_ptr<EdgeCursor>> _edgeCursors;

 public:
  DepthFirstEnumerator(Traverser* traverser,
                       arangodb::velocypack::Slice startVertex,
                       TraverserOptions const* opts)
      : PathEnumerator(traverser, startVertex, opts) {}

  ~DepthFirstEnumerator() {
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Get the next Path element from the traversal.
  //////////////////////////////////////////////////////////////////////////////

  bool next() override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Prunes the current path prefix, the next function should not return
  ///        any path having this prefix anymore.
  //////////////////////////////////////////////////////////////////////////////

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
    arangodb::velocypack::Slice edge;
    arangodb::velocypack::Slice vertex;

   private:
    PathStep() {}

   public:
    explicit PathStep(arangodb::velocypack::Slice vertex) : sourceIdx(0), vertex(vertex) {}

    PathStep(size_t sourceIdx, arangodb::velocypack::Slice edge,
             arangodb::velocypack::Slice vertex)
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

   std::unordered_set<arangodb::velocypack::Slice> _tmpEdges;

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
                         arangodb::velocypack::Slice startVertex,
                         TraverserOptions const* opts);

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
