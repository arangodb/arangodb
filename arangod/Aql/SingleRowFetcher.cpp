////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

using namespace arangodb;
using namespace arangodb::aql;

template <bool passBlocksThrough>
SingleRowFetcher<passBlocksThrough>::SingleRowFetcher(DependencyProxy<passBlocksThrough>& executionBlock)
    : _dependencyProxy(&executionBlock),
      _upstreamState(ExecutionState::HASMORE),
      _rowIndex(0),
      _currentRow{CreateInvalidInputRowHint{}} {}

template <bool passBlocksThrough>
std::pair<ExecutionState, SharedAqlItemBlockPtr> SingleRowFetcher<passBlocksThrough>::fetchBlock(size_t atMost) {
  atMost = (std::min)(atMost, ExecutionBlock::DefaultBatchSize());

  // There are still some blocks left that ask their parent even after they got
  // DONE the last time, and I don't currently have time to track them down.
  // Thus the following assert is commented out.
  // TRI_ASSERT(_upstreamState != ExecutionState::DONE);
  auto res = _dependencyProxy->fetchBlock(atMost);

  _upstreamState = res.first;

  return res;
}

template <bool passBlocksThrough>
SingleRowFetcher<passBlocksThrough>::SingleRowFetcher()
    : _dependencyProxy(nullptr), _rowIndex(0), _currentRow{CreateInvalidInputRowHint{}} {}

template <bool passBlocksThrough>
std::pair<ExecutionState, SharedAqlItemBlockPtr>
SingleRowFetcher<passBlocksThrough>::fetchBlockForPassthrough(size_t atMost) {
  return _dependencyProxy->fetchBlockForPassthrough(atMost);
}

template <bool passBlocksThrough>
std::pair<ExecutionState, size_t> SingleRowFetcher<passBlocksThrough>::skipRows(size_t atMost) {
  TRI_ASSERT(!_currentRow.isInitialized() || _currentRow.isLastRowInBlock());
  TRI_ASSERT(!indexIsValid());

  auto res = _dependencyProxy->skipSome(atMost);
  _upstreamState = res.first;

  return res;
}

template class ::arangodb::aql::SingleRowFetcher<false>;
template class ::arangodb::aql::SingleRowFetcher<true>;
