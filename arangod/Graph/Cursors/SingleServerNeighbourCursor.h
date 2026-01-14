////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Basics/ResourceUsage.h"
#include "Graph/Cursors/SingleServerEdgeCursor.h"

namespace arangodb::aql {
class TraversalStats;
}
namespace arangodb::graph {

class RefactoredTraverserCache;
struct SingleServerBaseProviderOptions;

/**
   Provides neighbours of a given step in batches

   Light wrapper around SingleServerEdgeCursor, which adjusts to the
   NeighbourCursor API.
 */
template<class Step>
struct SingleServerNeighbourCursor {
  SingleServerNeighbourCursor(Step const& step, size_t position, aql::Ast* ast,
                              SingleServerProvider<Step>& provider,
                              SingleServerBaseProviderOptions& opts,
                              transaction::Methods* trx,
                              ResourceMonitor& resourceMonitor,
                              std::shared_ptr<aql::TraversalStats> stats,
                              RefactoredTraverserCache& traverserCache,
                              uint64_t batchSize);
  SingleServerNeighbourCursor(SingleServerNeighbourCursor const&) = delete;
  SingleServerNeighbourCursor(SingleServerNeighbourCursor&&) = default;
  SingleServerNeighbourCursor& operator=(SingleServerNeighbourCursor const&) =
      delete;

  /**
     Gives the next _batchSize neighbours for _currentStep
   */
  auto next() -> std::vector<Step>;

  auto hasDepthSpecificLookup(uint64_t depth) const noexcept -> bool;

  /**
     Is true if now all neighbours of _currentStep have been provided yet via
     next
   */
  auto hasMore() -> bool;
  auto markForDeletion() -> void { _deletable = true; }

 public:
  template<typename S, typename Inspector>
  friend auto inspect(Inspector& f, SingleServerNeighbourCursor<S>& x);
  bool _deletable = false;

 private:
  std::unique_ptr<SingleServerEdgeCursor<Step>> _cursor;
  Step _step;
  size_t _position;
  const uint64_t _batchSize;
  ResourceMonitor& _resourceMonitor;
  std::shared_ptr<aql::TraversalStats> _stats;
  SingleServerProvider<Step>& _provider;
  RefactoredTraverserCache& _traverserCache;
  SingleServerBaseProviderOptions& _opts;
};
template<typename Step, typename Inspector>
auto inspect(Inspector& f, SingleServerNeighbourCursor<Step>& x) {
  return f.object(x).fields(f.field("step", x._step));
}

}  // namespace arangodb::graph
