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

#ifndef ARANGODB_BASICS_TRAVERSER_H
#define ARANGODB_BASICS_TRAVERSER_H 1

#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"

#include <stack>

namespace arangodb {
namespace basics {

template <typename edgeIdentifier, typename vertexIdentifier>
struct EnumeratedPath {
  std::vector<edgeIdentifier> edges;
  std::vector<vertexIdentifier> vertices;
  EnumeratedPath() {}
};

template <typename edgeIdentifier, typename vertexIdentifier>
struct VertexGetter {
  VertexGetter() = default;
  virtual ~VertexGetter() = default;
  virtual bool getVertex(edgeIdentifier const&, vertexIdentifier const&, size_t,
                         vertexIdentifier&) = 0;
  virtual void setStartVertex(std::string const&) {}
};

template <typename edgeIdentifier, typename vertexIdentifier, typename edgeItem>
struct EdgeGetter {
  EdgeGetter() = default;
  virtual ~EdgeGetter() = default;
  virtual void getEdge(vertexIdentifier const&, std::vector<edgeIdentifier>&,
                       edgeItem*&, size_t&) = 0;

  virtual void getAllEdges(vertexIdentifier const&,
                           std::unordered_set<edgeIdentifier>&, size_t) = 0;
};


template <typename edgeIdentifier, typename vertexIdentifier, typename edgeItem>
class PathEnumerator {

 protected:

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Function to get the next edge from index.
  //////////////////////////////////////////////////////////////////////////////

  EdgeGetter<edgeIdentifier, vertexIdentifier, edgeItem>* _edgeGetter;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Function to get the connected vertex from index.
  ///        Returns false if the vertex does not match the filter
  //////////////////////////////////////////////////////////////////////////////

  VertexGetter<edgeIdentifier, vertexIdentifier>* _vertexGetter;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Indicates if we issue next() the first time.
  ///        It shall return an empty path in this case.
  //////////////////////////////////////////////////////////////////////////////

  bool _isFirst;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Maximal path length which should be enumerated.
  //////////////////////////////////////////////////////////////////////////////

  size_t _maxDepth;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief List of the last path is used to
  //////////////////////////////////////////////////////////////////////////////

  EnumeratedPath<edgeIdentifier, vertexIdentifier> _enumeratedPath;

 public:
  PathEnumerator(
      EdgeGetter<edgeIdentifier, vertexIdentifier, edgeItem>* edgeGetter,
      VertexGetter<edgeIdentifier, vertexIdentifier>* vertexGetter,
      vertexIdentifier const& startVertex, size_t maxDepth)
      : _edgeGetter(edgeGetter), _vertexGetter(vertexGetter), _isFirst(true), _maxDepth(maxDepth) {
    _enumeratedPath.vertices.push_back(startVertex);
    TRI_ASSERT(_enumeratedPath.vertices.size() == 1);
  }

  virtual ~PathEnumerator() {}

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Get the next Path element from the traversal.
  //////////////////////////////////////////////////////////////////////////////

  virtual const EnumeratedPath<edgeIdentifier, vertexIdentifier>& next() = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Prunes the current path prefix, the next function should not return
  ///        any path having this prefix anymore.
  //////////////////////////////////////////////////////////////////////////////

  virtual void prune() = 0;
};

template <typename edgeIdentifier, typename vertexIdentifier, typename edgeItem>
class DepthFirstEnumerator final : public PathEnumerator<edgeIdentifier, vertexIdentifier, edgeItem> {
 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief The pointers returned for edge indexes on this path. Used to
  /// continue
  ///        the search on respective levels.
  //////////////////////////////////////////////////////////////////////////////

  std::stack<edgeItem*> _lastEdges;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief An internal index for the edge collection used at each depth level
  //////////////////////////////////////////////////////////////////////////////

  std::stack<size_t> _lastEdgesIdx;

 public:
  DepthFirstEnumerator(
      EdgeGetter<edgeIdentifier, vertexIdentifier, edgeItem>* edgeGetter,
      VertexGetter<edgeIdentifier, vertexIdentifier>* vertexGetter,
      vertexIdentifier const& startVertex, size_t maxDepth)
    : PathEnumerator<edgeIdentifier, vertexIdentifier, edgeItem>(edgeGetter, vertexGetter, startVertex, maxDepth) {
    _lastEdges.push(nullptr);
    _lastEdgesIdx.push(0);
    TRI_ASSERT(_lastEdges.size() == 1);
  }

  ~DepthFirstEnumerator() {}

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Get the next Path element from the traversal.
  //////////////////////////////////////////////////////////////////////////////

  EnumeratedPath<edgeIdentifier, vertexIdentifier> const& next() override {
    if (this->_isFirst) {
      this->_isFirst = false;
      return this->_enumeratedPath;
    }
    if (this->_enumeratedPath.edges.size() == this->_maxDepth) {
      // we have reached the maximal search depth.
      // We can prune this path and go to the next.
      prune();
    }

    // Avoid tail recursion. May crash on high search depth
    while (true) {
      if (_lastEdges.empty()) {
        this->_enumeratedPath.edges.clear();
        this->_enumeratedPath.vertices.clear();
        return this->_enumeratedPath;
      }
      this->_edgeGetter->getEdge(this->_enumeratedPath.vertices.back(),
                                 this->_enumeratedPath.edges, _lastEdges.top(),
                                 _lastEdgesIdx.top());
      if (_lastEdges.top() != nullptr) {
        // Could continue the path in the next depth.
        _lastEdges.push(nullptr);
        _lastEdgesIdx.push(0);
        vertexIdentifier v;
        bool isValid = this->_vertexGetter->getVertex(
            this->_enumeratedPath.edges.back(), this->_enumeratedPath.vertices.back(),
            this->_enumeratedPath.vertices.size(), v);
        this->_enumeratedPath.vertices.push_back(v);
        TRI_ASSERT(this->_enumeratedPath.vertices.size() ==
                   this->_enumeratedPath.edges.size() + 1);
        if (isValid) {
          return this->_enumeratedPath;
        }
      } else {
        if (this->_enumeratedPath.edges.empty()) {
          // We are done with enumerating paths
          this->_enumeratedPath.edges.clear();
          this->_enumeratedPath.vertices.clear();
          return this->_enumeratedPath;
        }
      }
      // This either modifies the stack or _lastEdges is empty.
      // This will return in next depth
      prune();
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Prunes the current path prefix, the next function should not return
  ///        any path having this prefix anymore.
  //////////////////////////////////////////////////////////////////////////////

  void prune() override {
    if (!_lastEdges.empty()) {
      _lastEdges.pop();
      _lastEdgesIdx.pop();
      if (!this->_enumeratedPath.edges.empty()) {
        this->_enumeratedPath.edges.pop_back();
        this->_enumeratedPath.vertices.pop_back();
      }
    }
  }
};

template <typename edgeIdentifier, typename vertexIdentifier, typename edgeItem>
class BreadthFirstEnumerator final : public PathEnumerator<edgeIdentifier, vertexIdentifier, edgeItem> {
 private:

  //////////////////////////////////////////////////////////////////////////////
  /// @brief One entry in the schreier vector 
  //////////////////////////////////////////////////////////////////////////////

  struct PathStep {
    size_t sourceIdx;
    edgeIdentifier edge;
    vertexIdentifier vertex;

   private:
    PathStep() {}

   public:
    explicit PathStep(vertexIdentifier const& vertex) : sourceIdx(0), vertex(vertex) {}

    PathStep(size_t sourceIdx, edgeIdentifier const& edge,
             vertexIdentifier const& vertex)
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

   std::unordered_set<edgeIdentifier> _tmpEdges;

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
  BreadthFirstEnumerator(
      EdgeGetter<edgeIdentifier, vertexIdentifier, edgeItem>* edgeGetter,
      VertexGetter<edgeIdentifier, vertexIdentifier>* vertexGetter,
      vertexIdentifier const& startVertex, size_t maxDepth)
      : PathEnumerator<edgeIdentifier, vertexIdentifier, edgeItem>(
            edgeGetter, vertexGetter, startVertex, maxDepth),
        _schreierIndex(1),
        _lastReturned(0),
        _currentDepth(0),
        _toSearchPos(0) {

    _schreier.reserve(32);
    auto step = std::make_unique<PathStep>(startVertex);
    _schreier.emplace_back(step.get());
    step.release();
    
    _toSearch.emplace_back(NextStep(0));

    if (this->_maxDepth > 0) {
      // We build the search values
      // only for one depth less
      this->_maxDepth--;
    }
  }

  ~BreadthFirstEnumerator() {
    for (auto& it : _schreier) {
      delete it;
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Get the next Path element from the traversal.
  //////////////////////////////////////////////////////////////////////////////

  EnumeratedPath<edgeIdentifier, vertexIdentifier> const& next() override {
    if (_lastReturned < _schreierIndex) {
      // We still have something on our stack.
      // Paths have been read but not returned.
      computeEnumeratedPath(_lastReturned++);
      return this->_enumeratedPath;
    }
    // Avoid large call stacks.
    // Loop will be left if we are either finished
    // with searching.
    // Or we found vertices in the next depth for
    // a vertex.
    while (true) {
      if (_toSearchPos >= _toSearch.size()) {
        // This depth is done. GoTo next
        if (_nextDepth.empty()) {
          // That's it. we are done.
          this->_enumeratedPath.edges.clear();
          this->_enumeratedPath.vertices.clear();
          return this->_enumeratedPath;
        }
        // Save copies:
        // We clear current
        // we swap current and next.
        // So now current is filled
        // and next is empty.
        _toSearch.clear();
        _toSearchPos = 0;
        _toSearch.swap(_nextDepth);
        _currentDepth++;
        TRI_ASSERT(_toSearchPos < _toSearch.size());
        TRI_ASSERT(_nextDepth.empty());
        TRI_ASSERT(_currentDepth <= this->_maxDepth);
      }
      // This access is always safe.
      // If not it should have bailed out before.
      TRI_ASSERT(_toSearchPos < _toSearch.size());

      _tmpEdges.clear();
      auto const nextIdx = _toSearch[_toSearchPos++].sourceIdx;
      auto const& nextVertex = _schreier[nextIdx]->vertex;

      this->_edgeGetter->getAllEdges(nextVertex, _tmpEdges, _currentDepth);
      if (!_tmpEdges.empty()) {
        bool didInsert = false;
        vertexIdentifier v;
        for (auto const& e : _tmpEdges) {
          bool valid =
              this->_vertexGetter->getVertex(e, nextVertex, _currentDepth, v);
          if (valid) {
            auto step = std::make_unique<PathStep>(nextIdx, e, v);
            _schreier.emplace_back(step.get());
            step.release();
            if (_currentDepth < this->_maxDepth) {
              _nextDepth.emplace_back(NextStep(_schreierIndex));
            }
            _schreierIndex++;
            didInsert = true;
          }
        }
        if (didInsert) {
          // We exit the loop here.
          // _schreierIndex is moved forward
          break;
        }
      }
      // Nothing found for this vertex.
      // _toSearchPos is increased so
      // we are not stuck in an endless loop
    }

    // _lastReturned points to the last used
    // entry. We compute the path to it
    // and increase the schreierIndex to point
    // to the next free position.
    computeEnumeratedPath(_lastReturned++);
    return this->_enumeratedPath;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Prunes the current path prefix, the next function should not return
  ///        any path having this prefix anymore.
  //////////////////////////////////////////////////////////////////////////////

  void prune() override {
    if (!_nextDepth.empty()) {
      _nextDepth.pop_back();
    }
  }

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

  void computeEnumeratedPath(size_t index) {
    TRI_ASSERT(index < _schreier.size());

    size_t depth = getDepth(index);
    this->_enumeratedPath.edges.clear();
    this->_enumeratedPath.vertices.clear();
    this->_enumeratedPath.edges.resize(depth);
    this->_enumeratedPath.vertices.resize(depth + 1);

    // Computed path. Insert it into the path enumerator.
    PathStep* current = nullptr;
    while (index != 0) {
      TRI_ASSERT(depth > 0);
      current = _schreier[index];
      this->_enumeratedPath.vertices[depth] = current->vertex;
      this->_enumeratedPath.edges[depth - 1] = current->edge;
      
      index = current->sourceIdx;
      --depth;
    }

    current = _schreier[0];
    this->_enumeratedPath.vertices[0] = current->vertex;
  }
};

}
}

#endif
