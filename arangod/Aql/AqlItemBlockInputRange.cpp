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
  auto res = peekDataRow();
  if (res.second) {
    ++_rowIndex;
  }
  return res;
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

std::pair<ExecutorState, ShadowAqlItemRow> AqlItemBlockInputRange::peekShadowRowAndState() const {
  if (hasShadowRow()) {
    return std::make_pair(nextState<LookAhead::NEXT, RowType::SHADOW>(),
                          ShadowAqlItemRow{_block, _rowIndex});
  }
  return std::make_pair(nextState<LookAhead::NOW, RowType::SHADOW>(),
                        ShadowAqlItemRow{CreateInvalidShadowRowHint{}});
}

std::pair<ExecutorState, ShadowAqlItemRow> AqlItemBlockInputRange::nextShadowRow() {
  auto res = peekShadowRowAndState();
  if (res.second.isInitialized()) {
    // Advance the current row.
    _rowIndex++;
  }
  return res;
}

size_t AqlItemBlockInputRange::skipAllRemainingDataRows() {
  ExecutorState state;
  InputAqlItemRow row{CreateInvalidInputRowHint{}};

  while (hasDataRow()) {
    std::tie(state, row) = nextDataRow();
    TRI_ASSERT(row.isInitialized());
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

  bool isShadowRow = isShadowRowAtIndex(testRowIndex);

  if constexpr (RowType::DATA == type) {
    // We Return HASMORE, if the next row is a data row
    if (!isShadowRow) {
      return ExecutorState::HASMORE;
    }
    return ExecutorState::DONE;
  } else {
    static_assert(RowType::SHADOW == type);
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
  return block->size() - block->getShadowRowIndexes().size();
}

auto AqlItemBlockInputRange::countShadowRows() const noexcept -> std::size_t {
  if (_block == nullptr) {
    return 0;
  }
  auto const& block = getBlock();
  return block->getShadowRowIndexes().size();
}

[[nodiscard]] auto AqlItemBlockInputRange::finalState() const noexcept -> ExecutorState {
  return _finalState;
}
