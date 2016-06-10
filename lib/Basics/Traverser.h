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
class PathEnumerator {

 protected:

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Function to get the next edge from index.
  //////////////////////////////////////////////////////////////////////////////

  std::function<void(vertexIdentifier&, std::vector<edgeIdentifier>&,
                     edgeItem*&, size_t&, bool&)> _getEdge;

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
      std::function<void(vertexIdentifier const&, std::vector<edgeIdentifier>&,
                         edgeItem*&, size_t&, bool&)> getEdge,
      VertexGetter<edgeIdentifier, vertexIdentifier>* vertexGetter,
      vertexIdentifier const& startVertex, size_t maxDepth)
      : _getEdge(getEdge), _vertexGetter(vertexGetter), _isFirst(true), _maxDepth(maxDepth) {
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
  /// @brief The boolean value indicating the direction for 'any' search
  //////////////////////////////////////////////////////////////////////////////

  std::stack<bool> _lastEdgesDir;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief An internal index for the edge collection used at each depth level
  //////////////////////////////////////////////////////////////////////////////

  std::stack<size_t> _lastEdgesIdx;

 public:
  DepthFirstEnumerator(
      std::function<void(vertexIdentifier const&, std::vector<edgeIdentifier>&,
                         edgeItem*&, size_t&, bool&)> getEdge,
      VertexGetter<edgeIdentifier, vertexIdentifier>* vertexGetter,
      vertexIdentifier const& startVertex, size_t maxDepth)
    : PathEnumerator<edgeIdentifier, vertexIdentifier, edgeItem>(getEdge, vertexGetter, startVertex, maxDepth) {
    _lastEdges.push(nullptr);
    _lastEdgesDir.push(false);
    _lastEdgesIdx.push(0);
    TRI_ASSERT(_lastEdges.size() == 1);
    TRI_ASSERT(_lastEdgesDir.size() == 1);
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
      this->_getEdge(this->_enumeratedPath.vertices.back(), this->_enumeratedPath.edges,
               _lastEdges.top(), _lastEdgesIdx.top(), _lastEdgesDir.top());
      if (_lastEdges.top() != nullptr) {
        // Could continue the path in the next depth.
        _lastEdges.push(nullptr);
        _lastEdgesDir.push(false);
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
      _lastEdgesDir.pop();
      _lastEdgesIdx.pop();
      if (!this->_enumeratedPath.edges.empty()) {
        this->_enumeratedPath.edges.pop_back();
        this->_enumeratedPath.vertices.pop_back();
      }
    }
  }
};
}
}

#endif
