////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "AqlItemBlockInputRange.h"

using namespace arangodb;
using namespace arangodb::aql;

AqlItemBlockInputRange::AqlItemBlockInputRange()
    : _block(nullptr), _rowIndex(0), _endIndex(0), _finalState(ExecutorState::HASMORE) {
  TRI_ASSERT(!hasMore());
  TRI_ASSERT(state() == ExecutorState::HASMORE);
}

AqlItemBlockInputRange::AqlItemBlockInputRange(ExecutorState state,
                                               SharedAqlItemBlockPtr const& block,
                                               std::size_t index, std::size_t endIndex)
    : _block{block}, _rowIndex{index}, _endIndex(endIndex), _finalState{state} {
  TRI_ASSERT(index < endIndex);
  TRI_ASSERT(endIndex <= block->size());
}

AqlItemBlockInputRange::AqlItemBlockInputRange(ExecutorState state,
                                               SharedAqlItemBlockPtr&& block,
                                               std::size_t index, std::size_t endIndex) noexcept
    : _block{std::move(block)}, _rowIndex{index}, _endIndex(endIndex), _finalState{state} {
  TRI_ASSERT(index < endIndex);
  TRI_ASSERT(endIndex <= block->size());
}

std::pair<ExecutorState, InputAqlItemRow> AqlItemBlockInputRange::peek() {
  if (indexIsValid()) {
    return std::make_pair(state(), InputAqlItemRow{_block, _rowIndex});
  }
  return std::make_pair(state(), InputAqlItemRow{CreateInvalidInputRowHint{}});
}

std::pair<ExecutorState, InputAqlItemRow> AqlItemBlockInputRange::next() {
  auto res = peek();
  ++_rowIndex;
  if (!indexIsValid()) {
    _block = nullptr;
    _rowIndex = 0;
  }
  return res;
}

bool AqlItemBlockInputRange::indexIsValid() const noexcept {
  return _block != nullptr && _rowIndex < _endIndex;
}

bool AqlItemBlockInputRange::hasMore() const noexcept { return indexIsValid(); }

bool AqlItemBlockInputRange::hasMoreAfterThis() const noexcept {
  return indexIsValid() && _rowIndex + 1 < _endIndex;
}

ExecutorState AqlItemBlockInputRange::state() const noexcept {
  return hasMoreAfterThis() ? ExecutorState::HASMORE : _finalState;
}
