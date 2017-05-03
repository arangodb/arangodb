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

#ifndef ARANGODB_GRAPH_CONSTANT_WEIGHT_SHORTEST_PATH_FINDER_H
#define ARANGODB_GRAPH_CONSTANT_WEIGHT_SHORTEST_PATH_FINDER_H 1

#include "Basics/StringRef.h"
#include "Basics/VelocyPackHelper.h"
#include "Graph/ShortestPathFinder.h"

namespace arangodb {

class ManagedDocumentResult;
class StringRef;

namespace aql {
class ShortestPathBlock;
}

namespace velocypack {
class Slice;
}

namespace graph {

struct ShortestPathOptions;

class ConstantWeightShortestPathFinder : public ShortestPathFinder {

 private:
  struct PathSnippet {
    arangodb::StringRef const _pred;
    arangodb::StringRef const _path;

    PathSnippet(arangodb::StringRef& pred,
                arangodb::StringRef& path)
        : _pred(pred), _path(path) {}
  };

 public:
  explicit ConstantWeightShortestPathFinder(ShortestPathOptions* options);

  ~ConstantWeightShortestPathFinder();

  bool shortestPath(arangodb::velocypack::Slice const& start,
                    arangodb::velocypack::Slice const& end,
                    arangodb::graph::ShortestPathResult& result,
                    std::function<void()> const& callback) override;

 private:
  void expandVertex(bool backward, arangodb::StringRef vertex,
                    std::vector<arangodb::StringRef>& edges,
                    std::vector<arangodb::StringRef>& neighbors);

  void clearVisited();

 private:
  std::unordered_map<arangodb::StringRef, PathSnippet*> _leftFound;
  std::deque<arangodb::StringRef> _leftClosure;

  std::unordered_map<arangodb::StringRef, PathSnippet*> _rightFound;
  std::deque<arangodb::StringRef> _rightClosure;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief The options to modify this shortest path computation
  //////////////////////////////////////////////////////////////////////////////
  arangodb::graph::ShortestPathOptions* _options;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Reusable ManagedDocumentResult that temporarily takes
  ///        responsibility for one document.
  //////////////////////////////////////////////////////////////////////////////
  std::unique_ptr<ManagedDocumentResult> _mmdr;

};

}  // namespace graph
}  // namespace arangodb
#endif
