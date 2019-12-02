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

#include "Logger/LogMacros.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>
#include <numeric>

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
  TRI_ASSERT(index <= endIndex);
  TRI_ASSERT(endIndex <= block->size());
}

AqlItemBlockInputRange::AqlItemBlockInputRange(ExecutorState state,
                                               SharedAqlItemBlockPtr&& block,
                                               std::size_t index, std::size_t endIndex) noexcept
    : _block{std::move(block)}, _rowIndex{index}, _endIndex(endIndex), _finalState{state} {
  TRI_ASSERT(index <= endIndex);
  TRI_ASSERT(endIndex <= block->size());
}

std::pair<ExecutorState, InputAqlItemRow> AqlItemBlockInputRange::peek() {
  if (indexIsValid()) {
    return std::make_pair(nextState<LookAhead::NEXT, RowType::DATA>(),
                          InputAqlItemRow{_block, _rowIndex});
  }
  return std::make_pair(nextState<LookAhead::NOW, RowType::DATA>(),
                        InputAqlItemRow{CreateInvalidInputRowHint{}});
}

std::pair<ExecutorState, InputAqlItemRow> AqlItemBlockInputRange::next() {
  auto res = peek();
  if (res.second) {
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
  return nextState<LookAhead::NOW, RowType::DATA>();
}

bool AqlItemBlockInputRange::hasShadowRow() const noexcept {
  return isIndexValid(_rowIndex) && isShadowRowAtIndex(_rowIndex);
}

bool AqlItemBlockInputRange::isIndexValid(std::size_t index) const noexcept {
  return _block != nullptr && index < _block->size();
}

bool AqlItemBlockInputRange::isShadowRowAtIndex(std::size_t index) const noexcept {
  TRI_ASSERT(isIndexValid(index));
  return _block->isShadowRow(index);
}

std::pair<ExecutorState, ShadowAqlItemRow> AqlItemBlockInputRange::peekShadowRow() {
  if (hasShadowRow()) {
    return std::make_pair(nextState<LookAhead::NEXT, RowType::SHADOW>(),
                          ShadowAqlItemRow{_block, _rowIndex});
  }
  return std::make_pair(nextState<LookAhead::NOW, RowType::SHADOW>(),
                        ShadowAqlItemRow{CreateInvalidShadowRowHint{}});
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

template <AqlItemBlockInputRange::LookAhead doPeek, AqlItemBlockInputRange::RowType type>
ExecutorState AqlItemBlockInputRange::nextState() const noexcept {
  size_t testRowIndex = _rowIndex;
  if constexpr (LookAhead::NEXT == doPeek) {
    // Look ahead one
    testRowIndex++;
  }
  if (!isIndexValid(testRowIndex)) {
    return _finalState;
  }

  bool isShadowRow = isShadowRowAtIndex(testRowIndex);

  if constexpr (RowType::DATA == type) {
    // We Return HASMORE, if the next row is a data row
    if (!isShadowRow) {
      return ExecutorState::HASMORE;
    }
    return ExecutorState::DONE;
  } else {
    TRI_ASSERT(RowType::SHADOW == type);
    // We Return HASMORE, if the next shadow row is NOT relevant.
    // So we can directly fetch the next shadow row without informing
    // the executor about an empty subquery.
    if (isShadowRow) {
      ShadowAqlItemRow nextRow{_block, testRowIndex};
      if (!nextRow.isRelevant()) {
        return ExecutorState::HASMORE;
      }
    }
    return ExecutorState::DONE;
  }
}