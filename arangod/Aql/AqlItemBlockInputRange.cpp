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

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>
#include <algorithm>
#include <numeric>

using namespace arangodb;
using namespace arangodb::aql;

AqlItemBlockInputRange::AqlItemBlockInputRange(ExecutorState state, std::size_t skipped)
    : _finalState{state}, _skipped{skipped} {
  TRI_ASSERT(!hasDataRow());
}

AqlItemBlockInputRange::AqlItemBlockInputRange(ExecutorState state, std::size_t skipped,
                                               arangodb::aql::SharedAqlItemBlockPtr const& block,
                                               std::size_t index)
    : _block{block}, _rowIndex{index}, _finalState{state}, _skipped{skipped} {
  TRI_ASSERT(index <= _block->size());
}

AqlItemBlockInputRange::AqlItemBlockInputRange(ExecutorState state, std::size_t skipped,
                                               arangodb::aql::SharedAqlItemBlockPtr&& block,
                                               std::size_t index) noexcept
    : _block{std::move(block)}, _rowIndex{index}, _finalState{state}, _skipped{skipped} {
  TRI_ASSERT(index <= _block->size());
}

SharedAqlItemBlockPtr AqlItemBlockInputRange::getBlock() const noexcept {
  return _block;
}

bool AqlItemBlockInputRange::hasDataRow() const noexcept {
  return isIndexValid(_rowIndex) && !isShadowRowAtIndex(_rowIndex);
}

// TODO: Implement peekDataRow (without state). e.g. IResearchViewExecutor does not need the state!
std::pair<ExecutorState, InputAqlItemRow> AqlItemBlockInputRange::peekDataRow() const {
  if (hasDataRow()) {
    return std::make_pair(nextState<LookAhead::NEXT, RowType::DATA>(),
                          InputAqlItemRow{_block, _rowIndex});
  }
  return std::make_pair(nextState<LookAhead::NOW, RowType::DATA>(),
                        InputAqlItemRow{CreateInvalidInputRowHint{}});
}

std::pair<ExecutorState, InputAqlItemRow> AqlItemBlockInputRange::nextDataRow() {
  // this is an optimized version that intentionally does not call peekDataRow()
  // in order to save a few if conditions
  if (hasDataRow()) {
    TRI_ASSERT(_block != nullptr);
    auto state = nextState<LookAhead::NEXT, RowType::DATA>();
    return std::make_pair(state, InputAqlItemRow{_block, _rowIndex++});
  }
  return std::make_pair(nextState<LookAhead::NOW, RowType::DATA>(),
                        InputAqlItemRow{CreateInvalidInputRowHint{}});
}

/// @brief: this is a performance-optimized version of nextDataRow() that must only
/// be used if it is sure that there is another data row
std::pair<ExecutorState, InputAqlItemRow> AqlItemBlockInputRange::nextDataRow(HasDataRow /*tag unused*/) {
  TRI_ASSERT(_block != nullptr);
  TRI_ASSERT(hasDataRow());
  // must calculate nextState() before the increase of _rowIndex here.
  auto state = nextState<LookAhead::NEXT, RowType::DATA>();
  return std::make_pair(state, InputAqlItemRow{_block, _rowIndex++});
}

ExecutorState AqlItemBlockInputRange::upstreamState() const noexcept {
  return nextState<LookAhead::NOW, RowType::DATA>();
}

bool AqlItemBlockInputRange::upstreamHasMore() const noexcept {
  return upstreamState() == ExecutorState::HASMORE;
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

arangodb::aql::ShadowAqlItemRow AqlItemBlockInputRange::peekShadowRow() const {
  if (hasShadowRow()) {
    return ShadowAqlItemRow{_block, _rowIndex};
  }
  return ShadowAqlItemRow{CreateInvalidShadowRowHint{}};
}

std::pair<ExecutorState, ShadowAqlItemRow> AqlItemBlockInputRange::nextShadowRow() {
  if (hasShadowRow()) {
    ShadowAqlItemRow row{_block, _rowIndex};
    // Advance the current row.
    _rowIndex++;
    return std::make_pair(nextState<LookAhead::NOW, RowType::SHADOW>(), std::move(row));
  }
  return std::make_pair(nextState<LookAhead::NOW, RowType::SHADOW>(),
                        ShadowAqlItemRow{CreateInvalidShadowRowHint{}});
}

size_t AqlItemBlockInputRange::skipAllRemainingDataRows() {
  while (hasDataRow()) {
    ++_rowIndex;
  }
  return 0;
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

  if constexpr (RowType::DATA == type) {
    // We Return HASMORE, if the next row is a data row
    if (!isShadowRowAtIndex(testRowIndex)) {
      return ExecutorState::HASMORE;
    }
    return ExecutorState::DONE;
  } else {
    static_assert(RowType::SHADOW == type);
    // We checked the case of index invalid above
    // hence we have something now.
    // On the ShadowRow state we only need to return DONE
    // if there is absolutely nothing left.
    // So We have something => Return HASMORE;
    return ExecutorState::HASMORE;
  }
}

auto AqlItemBlockInputRange::skip(std::size_t const toSkip) noexcept -> std::size_t {
  auto const skipCount = std::min(_skipped, toSkip);
  _skipped -= skipCount;
  return skipCount;
}

auto AqlItemBlockInputRange::skippedInFlight() const noexcept -> std::size_t {
  return _skipped;
}

auto AqlItemBlockInputRange::skipAll() noexcept -> std::size_t {
  auto const skipped = _skipped;
  _skipped = 0;
  return skipped;
}

auto AqlItemBlockInputRange::countDataRows() const noexcept -> std::size_t {
  if (_block == nullptr) {
    return 0;
  }
  auto const& block = getBlock();
  auto total = block->size();
  if (_rowIndex >= total) {
    return 0;
  }
  return total - _rowIndex - countShadowRows();
}

auto AqlItemBlockInputRange::countShadowRows() const noexcept -> std::size_t {
  if (_block == nullptr) {
    return 0;
  }
  auto [shadowRowsBegin, shadowRowsEnd] = getBlock()->getShadowRowIndexesFrom(0);
  return std::count_if(std::lower_bound(shadowRowsBegin, shadowRowsEnd, _rowIndex), shadowRowsEnd,
                       [&](auto r) -> bool { return r >= _rowIndex; });
}

[[nodiscard]] auto AqlItemBlockInputRange::finalState() const noexcept -> ExecutorState {
  return _finalState;
}
