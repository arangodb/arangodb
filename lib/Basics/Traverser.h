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

#include <deque>
#include <stack>
#include <thread>

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
};

template <typename edgeIdentifier, typename vertexIdentifier, typename edgeItem>
struct EdgeGetter {
  EdgeGetter() = default;
  virtual ~EdgeGetter() = default;
  virtual void getEdge(vertexIdentifier const&, std::vector<edgeIdentifier>&,
                       edgeItem*&, size_t&) = 0;

  virtual void getAllEdges(vertexIdentifier const&,
                           std::vector<edgeIdentifier>&, size_t) = 0;
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

  virtual void prune()  = 0;
};

template <typename edgeIdentifier, typename vertexIdentifier, typename edgeItem>
class DepthFirstEnumerator : public PathEnumerator<edgeIdentifier, vertexIdentifier, edgeItem> {
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

  const EnumeratedPath<edgeIdentifier, vertexIdentifier>& next() override {
    if (this->_isFirst) {
      this->_isFirst = false;
      return this->_enumeratedPath;
    }
    if (this->_enumeratedPath.edges.size() == this->_maxDepth) {
      // we have reached the maximal search depth.
      // We can prune this path and go to the next.
      prune();
    }

    // Avoid tail recusion. May crash on high search depth
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
class BreadthFirstEnumerator : public PathEnumerator<edgeIdentifier, vertexIdentifier, edgeItem> {
 private:

  //////////////////////////////////////////////////////////////////////////////
  /// @brief One entry in the schreier vector 
  //////////////////////////////////////////////////////////////////////////////

  struct PathStep {
    size_t sourceIdx;
    edgeIdentifier edge;
    vertexIdentifier vertex;

   private:
    PathStep(){};

   public:
    PathStep(vertexIdentifier const& vertex) : sourceIdx(0), vertex(vertex){};

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
    vertexIdentifier vertex;

   private:
    NextStep(){};

   public:
    NextStep(size_t sourceIdx, vertexIdentifier const& vertex)
        : sourceIdx(sourceIdx), vertex(vertex) {}
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

   std::vector<NextStep*> _nextDepth;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Vector storing the position at current search depth
  //////////////////////////////////////////////////////////////////////////////

   std::vector<NextStep*> _toSearch;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Vector storing the position at current search depth
  //////////////////////////////////////////////////////////////////////////////

   std::vector<edgeIdentifier> _tmpEdges;

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
    auto step = std::make_unique<PathStep>(startVertex);
    _schreier.emplace_back(step.get());
    step.release();

    auto next = std::make_unique<NextStep>(0, startVertex);
    _toSearch.emplace_back(next.get());
    next.release();
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
    for (auto& it : _toSearch) {
      delete it;
    }
    for (auto& it : _nextDepth) {
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
        for (auto& it : _toSearch) {
          delete it;
        }
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
      auto next = _toSearch[_toSearchPos++];
      TRI_ASSERT(next != nullptr);
      this->_edgeGetter->getAllEdges(next->vertex, _tmpEdges, _currentDepth);
      if (!_tmpEdges.empty()) {
        bool didInsert = false;
        for (auto const& e : _tmpEdges) {
          vertexIdentifier v;
          bool valid =
              this->_vertexGetter->getVertex(e, next->vertex, _currentDepth, v);
          if (valid) {
            auto step = std::make_unique<PathStep>(next->sourceIdx, e, v);
            _schreier.emplace_back(step.get());
            step.release();
            if (_currentDepth < this->_maxDepth) {
              auto nextSearch = std::make_unique<NextStep>(_schreierIndex, v);
              _nextDepth.emplace_back(nextSearch.get());
              nextSearch.release();
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

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Build the enumerated path for the given index in the schreier
  ///        vector.
  //////////////////////////////////////////////////////////////////////////////

  void computeEnumeratedPath(size_t index) {
    TRI_ASSERT(index < _schreier.size());
    std::deque<edgeIdentifier> edges;
    std::deque<vertexIdentifier> vertices;
    PathStep* current = nullptr;
    while (index != 0) {
      current = _schreier[index];
      vertices.push_front(current->vertex);
      edges.push_front(current->edge);
      index = current->sourceIdx;
    }
    current = _schreier[0];
    vertices.push_front(current->vertex);

    // Computed path. Insert it into the path enumerator.
    this->_enumeratedPath.edges.clear();
    this->_enumeratedPath.vertices.clear();
    std::copy(vertices.begin(), vertices.end(), std::back_inserter(this->_enumeratedPath.vertices));
    std::copy(edges.begin(), edges.end(), std::back_inserter(this->_enumeratedPath.edges));
  }
};


}
}

#endif
