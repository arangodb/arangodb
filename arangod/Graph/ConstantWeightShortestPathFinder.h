////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_GRAPH_CONSTANT_WEIGHT_SHORTEST_PATH_FINDER_H
#define ARANGODB_GRAPH_CONSTANT_WEIGHT_SHORTEST_PATH_FINDER_H 1

#include "Basics/VelocyPackHelper.h"
#include "Graph/EdgeDocumentToken.h"
#include "Graph/ShortestPathFinder.h"

#include <velocypack/StringRef.h>
#include <deque>
#include <memory>

namespace arangodb {

namespace velocypack {
class Slice;
}

namespace graph {
class EdgeCursor;
struct ShortestPathOptions;

class ConstantWeightShortestPathFinder : public ShortestPathFinder {
 private:
  struct PathSnippet {
    arangodb::velocypack::StringRef const _pred;
    graph::EdgeDocumentToken _path;

    PathSnippet(arangodb::velocypack::StringRef& pred, graph::EdgeDocumentToken&& path);
  };

  typedef std::deque<arangodb::velocypack::StringRef> Closure;
  typedef std::unordered_map<arangodb::velocypack::StringRef, PathSnippet*> Snippets;

 public:
  explicit ConstantWeightShortestPathFinder(ShortestPathOptions& options);

  ~ConstantWeightShortestPathFinder();

  bool shortestPath(arangodb::velocypack::Slice const& start,
                    arangodb::velocypack::Slice const& end,
                    arangodb::graph::ShortestPathResult& result) override;

  void clear() override;

 private:
  void expandVertex(bool backward, arangodb::velocypack::StringRef vertex);

  void clearVisited();

  bool expandClosure(Closure& sourceClosure, Snippets& sourceSnippets, Snippets& targetSnippets,
                     bool direction, arangodb::velocypack::StringRef& result);

  void fillResult(arangodb::velocypack::StringRef& n,
                  arangodb::graph::ShortestPathResult& result);

 private:
  Snippets _leftFound;
  Closure _leftClosure;

  Snippets _rightFound;
  Closure _rightClosure;

  Closure _nextClosure;

  std::vector<arangodb::velocypack::StringRef> _neighbors;
  std::vector<graph::EdgeDocumentToken> _edges;

  std::unique_ptr<EdgeCursor> _forwardCursor;
  std::unique_ptr<EdgeCursor> _backwardCursor;
};

}  // namespace graph
}  // namespace arangodb
#endif
