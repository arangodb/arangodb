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

#pragma once

#include "Basics/VelocyPackHelper.h"
#include "Containers/FlatHashMap.h"
#include "Graph/EdgeDocumentToken.h"
#include "Graph/ShortestPathFinder.h"

#include <deque>
#include <memory>

namespace arangodb {
struct ResourceMonitor;

namespace velocypack {
class Slice;
}

namespace graph {
class EdgeCursor;
struct ShortestPathOptions;

class ConstantWeightShortestPathFinder : public ShortestPathFinder {
 private:
  struct PathSnippet {
    PathSnippet() noexcept;
    PathSnippet(std::string_view pred,
                graph::EdgeDocumentToken&& path) noexcept;
    PathSnippet(PathSnippet&& other) noexcept = default;
    PathSnippet& operator=(PathSnippet&& other)
        ARANGODB_NOEXCEPT_ASSIGN_OP = default;

    bool empty() const noexcept { return _pred.empty(); }

    std::string_view _pred;
    graph::EdgeDocumentToken _path;
  };

  typedef std::vector<std::string_view> Closure;
  typedef containers::FlatHashMap<std::string_view, PathSnippet> Snippets;

 public:
  explicit ConstantWeightShortestPathFinder(ShortestPathOptions& options);

  ~ConstantWeightShortestPathFinder();

  bool shortestPath(arangodb::velocypack::Slice start,
                    arangodb::velocypack::Slice end,
                    arangodb::graph::ShortestPathResult& result) override;

  void clear() override;

 private:
  // side-effect: populates _neighbors
  void expandVertex(bool backward, std::string_view vertex);

  void clearVisited();

  bool expandClosure(Closure& sourceClosure, Snippets& sourceSnippets,
                     Snippets const& targetSnippets, bool direction,
                     std::string_view& result);

  void fillResult(std::string_view n,
                  arangodb::graph::ShortestPathResult& result);

  size_t pathSnippetMemoryUsage() const noexcept;

 private:
  arangodb::ResourceMonitor& _resourceMonitor;

  Snippets _leftFound;
  Closure _leftClosure;

  Snippets _rightFound;
  Closure _rightClosure;

  std::unique_ptr<EdgeCursor> _forwardCursor;
  std::unique_ptr<EdgeCursor> _backwardCursor;

  // temp values, only used inside expandClosure()
  Closure _nextClosure;

  struct Neighbor {
    std::string_view vertex;
    graph::EdgeDocumentToken edge;

    Neighbor(std::string_view v, graph::EdgeDocumentToken e) noexcept
        : vertex(v), edge(e) {}

    static constexpr size_t itemMemoryUsage() {
      return sizeof(decltype(vertex)) + sizeof(decltype(edge));
    }
  };

  std::vector<Neighbor> _neighbors;
};

}  // namespace graph
}  // namespace arangodb
