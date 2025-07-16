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
#include "Graph/Cursors/RefactoredSingleServerEdgeCursor.h"
#include "VocBase/Identifiers/LocalDocumentId.h"
#include "Graph/Providers/SingleServer/NeighbourCache.h"

namespace arangodb::graph {

struct ExpansionInfo;
struct NeighbourCache;

struct SingleServerBaseProviderOptions;

template<class Step>
struct SingleServerNeighbourProvider {
  SingleServerNeighbourProvider(SingleServerBaseProviderOptions& opts,
                                transaction::Methods* trx,
                                ResourceMonitor& resourceMonitor);
  SingleServerNeighbourProvider(SingleServerNeighbourProvider const&) = delete;
  SingleServerNeighbourProvider(SingleServerNeighbourProvider&&) = default;
  SingleServerNeighbourProvider& operator=(
      SingleServerNeighbourProvider const&) = delete;
  ~SingleServerNeighbourProvider() { clear(); }
  auto next(SingleServerProvider<Step>& provider, aql::TraversalStats& stats)
      -> std::shared_ptr<std::vector<ExpansionInfo>>;
  auto rearm(Step const& step, aql::TraversalStats& stats) -> void;
  auto clear() -> void;
  auto prepareIndexExpressions(aql::Ast* ast) -> void;
  auto hasDepthSpecificLookup(uint64_t depth) const noexcept -> bool;
  auto hasMore(uint64_t depth) -> bool;

 private:
  std::unique_ptr<RefactoredSingleServerEdgeCursor<Step>> _cursor;

  std::optional<Step> _currentStep;
  std::optional<NeighbourIterator> _currentStepNeighbourCacheIterator;

  size_t _rearmed = 0;
  size_t _readSomething = 0;
  std::optional<NeighbourCache> _neighbourCache;
  ResourceMonitor& _resourceMonitor;
};

}  // namespace arangodb::graph
