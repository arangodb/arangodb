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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "ConstantWeightShortestPathFinder.h"

#include "Basics/ResourceUsage.h"
#include "Graph/EdgeCursor.h"
#include "Graph/EdgeDocumentToken.h"
#include "Graph/ShortestPathOptions.h"
#include "Graph/ShortestPathResult.h"
#include "Graph/TraverserCache.h"
#include "Transaction/Helpers.h"

#include <velocypack/Slice.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::graph;

ConstantWeightShortestPathFinder::PathSnippet::PathSnippet() noexcept
    : _pred(), 
      _path() {}

ConstantWeightShortestPathFinder::PathSnippet::PathSnippet(arangodb::velocypack::StringRef pred,
                                                           EdgeDocumentToken&& path) noexcept
    : _pred(pred), 
      _path(std::move(path)) {}

ConstantWeightShortestPathFinder::ConstantWeightShortestPathFinder(ShortestPathOptions& options)
    : ShortestPathFinder(options),
      _resourceMonitor(options.resourceMonitor()) {
  // cppcheck-suppress *
  _forwardCursor = _options.buildCursor(false);
  // cppcheck-suppress *
  _backwardCursor = _options.buildCursor(true);
}

ConstantWeightShortestPathFinder::~ConstantWeightShortestPathFinder() {
  clearVisited();
}

void ConstantWeightShortestPathFinder::clear() {
  clearVisited();
  options().cache()->clear();
}

bool ConstantWeightShortestPathFinder::shortestPath(
    arangodb::velocypack::Slice const& s, arangodb::velocypack::Slice const& e,
    arangodb::graph::ShortestPathResult& result) {
  result.clear();
  TRI_ASSERT(s.isString());
  TRI_ASSERT(e.isString());
  arangodb::velocypack::StringRef start(s);
  arangodb::velocypack::StringRef end(e);
  // Init
  if (start == end) {
    result._vertices.emplace_back(start);
    _options.fetchVerticesCoordinator(result._vertices);
    return true;
  }
  
  clearVisited();
  _leftFound.try_emplace(start, PathSnippet());
  try {
    _rightFound.try_emplace(end, PathSnippet());
  } catch (...) {
    // leave it in clean state
    _leftFound.erase(start);
    throw;
  }
  
  // memory usage for the initial start vertices
  _resourceMonitor.increaseMemoryUsage(2 * pathSnippetMemoryUsage());

  _leftClosure.clear();
  _rightClosure.clear();
  _leftClosure.emplace_back(start);
  _rightClosure.emplace_back(end);

  TRI_IF_FAILURE("TraversalOOMInitialize") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  arangodb::velocypack::StringRef n;
  while (!_leftClosure.empty() && !_rightClosure.empty()) {
    options().isQueryKilledCallback();

    if (_leftClosure.size() < _rightClosure.size()) {
      if (expandClosure(_leftClosure, _leftFound, _rightFound, false, n)) {
        fillResult(n, result);
        return true;
      }
    } else {
      if (expandClosure(_rightClosure, _rightFound, _leftFound, true, n)) {
        fillResult(n, result);
        return true;
      }
    }
  }
  return false;
}

bool ConstantWeightShortestPathFinder::expandClosure(
    Closure& sourceClosure, Snippets& sourceSnippets, Snippets const& targetSnippets,
    bool isBackward, arangodb::velocypack::StringRef& result) {
  _nextClosure.clear();
  for (auto const& v : sourceClosure) {
    // populates _neighbors
    expandVertex(isBackward, v);

    for (auto& n : _neighbors) {
      ResourceUsageScope guard(_resourceMonitor, pathSnippetMemoryUsage());

      // create the PathSnippet if it does not yet exist
      if (sourceSnippets.try_emplace(n.vertex, v, std::move(n.edge)).second) {
        // new PathSnippet created. now sourceSnippets is responsible for memory usage tracking
        guard.steal();
        
        auto targetFoundIt = targetSnippets.find(n.vertex);
        if (targetFoundIt != targetSnippets.end()) {
          result = n.vertex;
          return true;
        }
        _nextClosure.emplace_back(n.vertex);
      }
    }
  }
  _neighbors.clear();
  sourceClosure.swap(_nextClosure);
  _nextClosure.clear();
  return false;
}

void ConstantWeightShortestPathFinder::fillResult(arangodb::velocypack::StringRef n,
                                                  arangodb::graph::ShortestPathResult& result) {
  ResourceUsageScope guard(_resourceMonitor);

  result._vertices.emplace_back(n);
  auto it = _leftFound.find(n);
  TRI_ASSERT(it != _leftFound.end());
  arangodb::velocypack::StringRef next;
  while (it != _leftFound.end() && !it->second.empty()) {
    guard.increase(arangodb::graph::ShortestPathResult::resultItemMemoryUsage());

    next = it->second._pred;
    result._vertices.push_front(next);
    result._edges.push_front(std::move(it->second._path));
    it = _leftFound.find(next);
  }

  it = _rightFound.find(n);
  TRI_ASSERT(it != _rightFound.end());
  while (it != _rightFound.end() && !it->second.empty()) {
    guard.increase(arangodb::graph::ShortestPathResult::resultItemMemoryUsage());

    next = it->second._pred;
    result._vertices.emplace_back(next);
    result._edges.emplace_back(std::move(it->second._path));
    it = _rightFound.find(next);
  }

  TRI_IF_FAILURE("TraversalOOMPath") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  _options.fetchVerticesCoordinator(result._vertices);
  clearVisited();
  
  // we intentionally don't commit the memory usage to the _resourceMonitor here.
  // we do this later at the call site if the result will be used for longer.
}

void ConstantWeightShortestPathFinder::expandVertex(bool backward,
                                                    arangodb::velocypack::StringRef vertex) {
  EdgeCursor* cursor = backward ? _backwardCursor.get() : _forwardCursor.get();
  cursor->rearm(vertex, 0);
 
  // we are tracking the memory usage for neighbors temporarily here (only inside this function)
  ResourceUsageScope guard(_resourceMonitor);

  _neighbors.clear();

  cursor->readAll([&](EdgeDocumentToken&& eid, VPackSlice edge, size_t /*cursorIdx*/) -> void {
    if (edge.isString()) {
      if (edge.compareString(vertex.data(), vertex.length()) != 0) {
        guard.increase(Neighbor::itemMemoryUsage());

        arangodb::velocypack::StringRef id =
            _options.cache()->persistString(arangodb::velocypack::StringRef(edge));

        if (_neighbors.capacity() == 0) {
          // avoid a few reallocations for the first members
          _neighbors.reserve(8);
        }
        _neighbors.emplace_back(id, std::move(eid));
      }
    } else {
      arangodb::velocypack::StringRef other(
          transaction::helpers::extractFromFromDocument(edge));
      if (other == vertex) {
        other = arangodb::velocypack::StringRef(
            transaction::helpers::extractToFromDocument(edge));
      }
      if (other != vertex) {
        guard.increase(Neighbor::itemMemoryUsage());

        arangodb::velocypack::StringRef id = _options.cache()->persistString(other);
        
        if (_neighbors.capacity() == 0) {
          // avoid a few reallocations for the first members
          _neighbors.reserve(8);
        }
        _neighbors.emplace_back(id, std::move(eid));
      }
    }
  });
  
  // we don't commit the memory usage to the _resourceMonitor here because
  // _neighbors is recycled over and over
}

void ConstantWeightShortestPathFinder::clearVisited() {
  size_t totalMemoryUsage = (_leftFound.size() + _rightFound.size()) * pathSnippetMemoryUsage();
  _resourceMonitor.decreaseMemoryUsage(totalMemoryUsage);

  _leftFound.clear();
  _rightFound.clear();
}

size_t ConstantWeightShortestPathFinder::pathSnippetMemoryUsage() const noexcept {
  return 16 /*arbitrary overhead*/ + 
         sizeof(arangodb::velocypack::StringRef) + 
         sizeof(PathSnippet);
}
