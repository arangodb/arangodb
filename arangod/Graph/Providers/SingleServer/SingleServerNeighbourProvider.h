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

#include <s2/base/integral_types.h>
#include "Aql/TraversalStats.h"
#include "Basics/ResourceUsage.h"
#include "Graph/Cursors/SingleServerEdgeCursor.h"
#include "VocBase/Identifiers/LocalDocumentId.h"
#include "Graph/Providers/SingleServer/NeighbourCache.h"

namespace arangodb::graph {

struct ExpansionInfo;
struct NeighbourCache;

struct SingleServerBaseProviderOptions;

/**
   Provides neighbours of a given step in batches

   Before doing anything with this provider, you need to specify the step for
   which vertex the provider should get the neighbours: do this via rearm.

   Read neighbours are saved in a cache. If the neighbours of a step are
   requested for more than one time, the cached neighbours are used instead of
   reading them from disk again.
 */
template<class Step>
struct SingleServerNeighbourProvider {
  SingleServerNeighbourProvider(SingleServerBaseProviderOptions& opts,
                                transaction::Methods* trx,
                                ResourceMonitor& resourceMonitor,
                                uint64_t batchSize, bool useCache = true);
  SingleServerNeighbourProvider(SingleServerNeighbourProvider const&) = delete;
  SingleServerNeighbourProvider(SingleServerNeighbourProvider&&) = default;
  SingleServerNeighbourProvider& operator=(
      SingleServerNeighbourProvider const&) = delete;
  ~SingleServerNeighbourProvider() { clear(); }

  /**
     Gives the next _batchSize neighbours for _currentStep
   */
  auto next(SingleServerProvider<Step>& provider, aql::TraversalStats& stats)
      -> std::shared_ptr<std::vector<ExpansionInfo>>;

  /**
     (Re)defines the step for which vertex the provider should
     provide neighbours
   */
  auto rearm(Step const& step, aql::TraversalStats& stats) -> void;

  /**
     Clears cache and statistics variables
   */
  auto clear() -> void;

  auto prepareIndexExpressions(aql::Ast* ast) -> void;
  auto hasDepthSpecificLookup(uint64_t depth) const noexcept -> bool;

  /**
     Is true if now all neighbours of _currentStep have been provided yet via
     next
   */
  auto hasMore(uint64_t depth) -> bool;

 private:
  std::unique_ptr<SingleServerEdgeCursor<Step>> _cursor;

  std::optional<Step> _currentStep;
  // if we can use the cache for the current step, this iterator is set and we
  // iterate over this iterator the get the batches via next
  std::optional<NeighbourIterator> _currentStepNeighbourCacheIterator;

  // statistics variables
  size_t _rearmed = 0;
  size_t _readSomething = 0;

  std::optional<NeighbourCache> _neighbourCache;
  const uint64_t _batchSize;
  ResourceMonitor& _resourceMonitor;
};

}  // namespace arangodb::graph
