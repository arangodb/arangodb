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

#ifndef ARANGODB_GRAPH_NEIGHBORSENUMERATOR_H
#define ARANGODB_GRAPH_NEIGHBORSENUMERATOR_H 1

#include "Basics/Common.h"
#include "Graph/PathEnumerator.h"

#include <velocypack/Slice.h>

namespace arangodb {
namespace graph {
// @brief Enumerator optimized for neighbors. Does not allow edge access

class NeighborsEnumerator final : public arangodb::traverser::PathEnumerator {
  std::unordered_set<arangodb::StringRef> _allFound;
  std::unordered_set<arangodb::StringRef> _currentDepth;
  std::unordered_set<arangodb::StringRef> _lastDepth;
  std::unordered_set<arangodb::StringRef>::iterator _iterator;

  uint64_t _searchDepth;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Vector storing the position at current search depth
  //////////////////////////////////////////////////////////////////////////////

  std::unordered_set<arangodb::velocypack::Slice> _tmpEdges;

 public:
  NeighborsEnumerator(arangodb::traverser::Traverser* traverser,
                      arangodb::velocypack::Slice const& startVertex,
                      arangodb::traverser::TraverserOptions* opts);

  ~NeighborsEnumerator() {}

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Get the next Path element from the traversal.
  //////////////////////////////////////////////////////////////////////////////

  bool next() override;

  aql::AqlValue lastVertexToAqlValue() override;

  aql::AqlValue lastEdgeToAqlValue() override;

  aql::AqlValue pathToAqlValue(arangodb::velocypack::Builder& result) override;
};

}  // namespace graph
}  // namespace arangodb

#endif
