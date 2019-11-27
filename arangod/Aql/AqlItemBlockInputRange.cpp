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
#include "Aql/ShadowAqlItemRow.h"

using namespace arangodb;
using namespace arangodb::aql;

AqlItemBlockInputRange::AqlItemBlockInputRange(ExecutorState state)
    : _block(nullptr), _rowIndex(0), _endIndex(0), _finalState(state) {
  TRI_ASSERT(!hasMore());
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
  if (indexIsValid()) {
    TRI_ASSERT(res.second);
    ++_rowIndex;
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

ExecutorState AqlItemBlockInputRange::shadowState() const noexcept {
  if (_block == nullptr) {
    return _finalState;
  }
  // We Return HASMORE, if the next shadow row is NOT relevant.
  // So we can directly fetch the next shadow row without informing
  // the executor about an empty subquery.
  size_t nextRowIndex = _rowIndex + 1;
  if (_block != nullptr && nextRowIndex < _block->size() && _block->isShadowRow(nextRowIndex)) {
    ShadowAqlItemRow nextRow{_block, nextRowIndex};
    if (!nextRow.isRelevant()) {
      return ExecutorState::HASMORE;
    }
  }
  return ExecutorState::DONE;
}

bool AqlItemBlockInputRange::hasShadowRow() const noexcept {
  if (_block == nullptr) {
    // No block => no ShadowRow
    return false;
  }

  if (hasMore()) {
    // As long as hasMore() is true, we still have DataRows and are not on a ShadowRow now.
    return false;
  }

  if (_rowIndex < _block->size()) {
    // We still have more rows here, get next ShadowRow
    TRI_ASSERT(_block->isShadowRow(_rowIndex));
    return true;
  }
  return false;
}

std::pair<ExecutorState, ShadowAqlItemRow> AqlItemBlockInputRange::peekShadowRow() {
  if (hasShadowRow()) {
    return std::make_pair(shadowState(), ShadowAqlItemRow{_block, _rowIndex});
  }
  return std::make_pair(shadowState(), ShadowAqlItemRow{CreateInvalidShadowRowHint{}});
}

std::pair<ExecutorState, ShadowAqlItemRow> AqlItemBlockInputRange::nextShadowRow() {
  auto res = peekShadowRow();
  if (hasShadowRow()) {
    auto const& shadowRowIndexes = _block->getShadowRowIndexes();
    auto it = std::find(shadowRowIndexes.begin(), shadowRowIndexes.end(), _rowIndex);
    // We have a shadow row in this index, so we cannot be at the end now.
    TRI_ASSERT(it != shadowRowIndexes.end());
    // Go to next ShadowRow.
    it++;
    if (it == shadowRowIndexes.end()) {
      // No more shadow row here.
      _endIndex = _block->size();
    } else {
      // Set endIndex to the next ShadowRowIndex.
      _endIndex = *it;
    }
    // Advance the current row.
    _rowIndex++;
  }
  return res;
}
