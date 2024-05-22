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
/// @author Tobias Gödderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "SingleRowFetcher.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/DependencyProxy.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/SkipResult.h"

using namespace arangodb;
using namespace arangodb::aql;

template<BlockPassthrough passBlocksThrough>
SingleRowFetcher<passBlocksThrough>::SingleRowFetcher(
    DependencyProxy<passBlocksThrough>& executionBlock)
    : _dependencyProxy(&executionBlock) {}

template<BlockPassthrough passBlocksThrough>
SingleRowFetcher<passBlocksThrough>::SingleRowFetcher()
    : _dependencyProxy(nullptr) {}

template<BlockPassthrough passBlocksThrough>
std::tuple<ExecutionState, SkipResult, AqlItemBlockInputRange>
SingleRowFetcher<passBlocksThrough>::execute(AqlCallStack const& stack) {
  auto [state, skipped, block] = _dependencyProxy->execute(stack);
  if (state == ExecutionState::WAITING) {
    // On waiting we have nothing to return
    return {state, SkipResult{},
            AqlItemBlockInputRange{MainQueryState::HASMORE}};
  }
  if (block == nullptr) {
    if (state == ExecutionState::HASMORE) {
      return {state, skipped,
              AqlItemBlockInputRange{MainQueryState::HASMORE,
                                     skipped.getSkipCount()}};
    }
    return {
        state, skipped,
        AqlItemBlockInputRange{MainQueryState::DONE, skipped.getSkipCount()}};
  }

  auto [start, end] = block->getRelevantRange();
  if (state == ExecutionState::HASMORE) {
    TRI_ASSERT(block != nullptr);
    return {
        state, skipped,
        AqlItemBlockInputRange{MainQueryState::HASMORE, skipped.getSkipCount(),
                               std::move(block), start}};
  }
  return {state, skipped,
          AqlItemBlockInputRange{MainQueryState::DONE, skipped.getSkipCount(),
                                 std::move(block), start}};
}

template<BlockPassthrough blockPassthrough>
void SingleRowFetcher<blockPassthrough>::setDistributeId(
    std::string const& id) {
  _dependencyProxy->setDistributeId(id);
}

template class ::arangodb::aql::SingleRowFetcher<BlockPassthrough::Disable>;
template class ::arangodb::aql::SingleRowFetcher<BlockPassthrough::Enable>;
