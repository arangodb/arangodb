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

#include "Basics/VelocyPackHelper.h"
#include "Graph/ShortestPathFinder.h"

namespace arangodb {

namespace aql {
class ShortestPathBlock;
}

namespace velocypack {
class Slice;
}

namespace graph {

class ConstantWeightShortestPathFinder : public ShortestPathFinder {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief callback to find neighbors
  //////////////////////////////////////////////////////////////////////////////

  typedef std::function<void(
      arangodb::velocypack::Slice& V,
      std::vector<arangodb::velocypack::Slice>& edges,
      std::vector<arangodb::velocypack::Slice>& neighbors)>
      ExpanderFunction;

 private:
  struct PathSnippet {
    arangodb::velocypack::Slice const _pred;
    arangodb::velocypack::Slice const _path;

    PathSnippet(arangodb::velocypack::Slice& pred,
                arangodb::velocypack::Slice& path)
        : _pred(pred), _path(path) {}
  };

  std::unordered_map<arangodb::velocypack::Slice, PathSnippet*,
                     arangodb::basics::VelocyPackHelper::VPackStringHash,
                     arangodb::basics::VelocyPackHelper::VPackStringEqual>
      _leftFound;
  std::deque<arangodb::velocypack::Slice> _leftClosure;

  std::unordered_map<arangodb::velocypack::Slice, PathSnippet*,
                     arangodb::basics::VelocyPackHelper::VPackStringHash,
                     arangodb::basics::VelocyPackHelper::VPackStringEqual>
      _rightFound;
  std::deque<arangodb::velocypack::Slice> _rightClosure;

  // TODO Remove Me!
  arangodb::aql::ShortestPathBlock* _block;

 public:
  ConstantWeightShortestPathFinder(arangodb::aql::ShortestPathBlock* block);

  ~ConstantWeightShortestPathFinder();

  bool shortestPath(arangodb::velocypack::Slice const& start,
                    arangodb::velocypack::Slice const& end,
                    arangodb::traverser::ShortestPath& result,
                    std::function<void()> const& callback) override;

 private:
  void expandVertex(bool backward, arangodb::velocypack::Slice& vertex,
                    std::vector<arangodb::velocypack::Slice>& edges,
                    std::vector<arangodb::velocypack::Slice>& neighbors);

  void expandVertexCluster(bool backward, arangodb::velocypack::Slice& vertex,
                           std::vector<arangodb::velocypack::Slice>& edges,
                           std::vector<arangodb::velocypack::Slice>& neighbors);

  void clearVisited();
};

}  // namespace graph
}  // namespace arangodb
#endif
