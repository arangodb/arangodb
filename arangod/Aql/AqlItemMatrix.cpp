////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#include "AqlItemMatrix.h"

#include "Aql/InputAqlItemRow.h"
#include "Aql/SharedAqlItemBlockPtr.h"
#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"
#include "Containers/Enumerate.h"

#include <algorithm>

using namespace arangodb;
using namespace arangodb::aql;

static constexpr size_t InvalidRowIndex = std::numeric_limits<size_t>::max();

size_t AqlItemMatrix::numberOfBlocks() const noexcept { return _blocks.size(); }

std::pair<SharedAqlItemBlockPtr, size_t> AqlItemMatrix::getBlock(size_t index) const noexcept {
  TRI_ASSERT(index < numberOfBlocks());
  // The first block could contain a shadowRow
  // and the first unused data row, could be after the
  // shadowRow.
  // All other blocks start with the first row as data row
  return  {_blocks[index], index == 0 ? _startIndexInFirstBlock : 0};
}

InputAqlItemRow AqlItemMatrix::getRow(AqlItemMatrix::RowIndex index) const noexcept {
  auto const& [block, unused] = getBlock(index.first);
  TRI_ASSERT(index.second >= unused);
  return InputAqlItemRow{block, index.second};
}

std::vector<AqlItemMatrix::RowIndex> AqlItemMatrix::produceRowIndexes() const {
  std::vector<RowIndex> result;
  if (!blocksEmpty()) {
    result.reserve(size());
    for (auto const& [index, block] : enumerate(_blocks)) {
      // Default case, 0 -> end
      size_t startRow = 0;
      // We know block size is <= DefaultBatchSize (1000) so it should easily fit into 32bit...
      size_t endRow = block->numRows();

      if (index == 0) {
        startRow = _startIndexInFirstBlock;
      }
      if (index + 1 == _blocks.size() && _stopIndexInLastBlock != InvalidRowIndex) {
        endRow = _stopIndexInLastBlock;
      }
      for (; startRow < endRow; ++startRow) {
        TRI_ASSERT(!block->isShadowRow(startRow));
        result.emplace_back(index, static_cast<uint32_t>(startRow));
      }
    }
  }
  return result;
}

bool AqlItemMatrix::blocksEmpty() const noexcept { return _blocks.empty(); }

void AqlItemMatrix::clear() {
  _blocks.clear();
  _numDataRows = 0;
  _startIndexInFirstBlock = 0;
  _stopIndexInLastBlock = InvalidRowIndex;
}

RegisterCount AqlItemMatrix::getNumRegisters() const noexcept { return _nrRegs; }

uint64_t AqlItemMatrix::size() const noexcept { return _numDataRows; }

void AqlItemMatrix::addBlock(SharedAqlItemBlockPtr blockPtr) {
  // If we are stopped by shadow row, we first need to solve this blockage
  // by popShadowRow calls. In order to continue.
  // The schadowRow logic is only based on the last node.
  TRI_ASSERT(!stoppedOnShadowRow());

  TRI_ASSERT(blockPtr->numRegisters() == getNumRegisters());
  // Test if we have more than uint32_t many blocks
  if (ADB_UNLIKELY(_blocks.size() == std::numeric_limits<uint32_t>::max())) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_RESOURCE_LIMIT,
        "Reaching the limit of AqlItems to SORT, please consider using a "
        "limit after sorting.");
  }
  // Test if we have more than uint32_t many rows within a block
  if (ADB_UNLIKELY(blockPtr->numRows() > std::numeric_limits<uint32_t>::max())) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_RESOURCE_LIMIT,
        "Reaching the limit of AqlItems to SORT, please consider lowering "
        "the batch size.");
  }

  // ShadowRow handling
  if (blockPtr->hasShadowRows()) {
    auto [shadowRowsBegin, shadowRowsEnd] = blockPtr->getShadowRowIndexesFrom(0);
    TRI_ASSERT(shadowRowsBegin != shadowRowsEnd);
    // Let us stop on the first
    _stopIndexInLastBlock = *shadowRowsBegin;
    _numDataRows += _stopIndexInLastBlock;
  } else {
    _numDataRows += blockPtr->numRows();
  }

  // Move block into _blocks
  _blocks.emplace_back(std::move(blockPtr));
}

bool AqlItemMatrix::stoppedOnShadowRow() const noexcept {
  return _stopIndexInLastBlock != InvalidRowIndex;
}

ShadowAqlItemRow AqlItemMatrix::popShadowRow() {
  TRI_ASSERT(stoppedOnShadowRow());
  auto const& blockPtr = _blocks.back();
  // We need to return this shadow row
  ShadowAqlItemRow shadowRow{_blocks.back(), _stopIndexInLastBlock};

  // We need to move forward the next shadow row.
  auto [shadowRowsBegin, shadowRowsEnd] = blockPtr->getShadowRowIndexesFrom(_stopIndexInLastBlock);
  _startIndexInFirstBlock = _stopIndexInLastBlock + 1;

  shadowRowsBegin++;

  if (shadowRowsBegin != shadowRowsEnd) {
    _stopIndexInLastBlock = *shadowRowsBegin;
    TRI_ASSERT(stoppedOnShadowRow());
    // We move always forward
    TRI_ASSERT(_stopIndexInLastBlock >= _startIndexInFirstBlock);
    _numDataRows = _stopIndexInLastBlock - _startIndexInFirstBlock;
  } else {
    _stopIndexInLastBlock = InvalidRowIndex;
    TRI_ASSERT(!stoppedOnShadowRow());
    // _stopIndexInLastBlock a 0 based index. size is a counter.
    TRI_ASSERT(blockPtr->numRows() >= _startIndexInFirstBlock);
    _numDataRows = blockPtr->numRows() - _startIndexInFirstBlock;
  }
  if (_startIndexInFirstBlock >= _blocks.back()->numRows()) {
    // The last block is also fully used
    _blocks.clear();
    _startIndexInFirstBlock = 0;
  } else {
    // Remove all but the last block
    _blocks.erase(_blocks.begin(), _blocks.end() - 1);
    TRI_ASSERT(_blocks.size() == 1);
  }
  return shadowRow;
}

ShadowAqlItemRow AqlItemMatrix::peekShadowRow() const {
  TRI_ASSERT(stoppedOnShadowRow());
  return ShadowAqlItemRow{_blocks.back(), _stopIndexInLastBlock};
}

AqlItemMatrix::AqlItemMatrix(RegisterCount nrRegs)
    : _numDataRows(0), _nrRegs(nrRegs), _stopIndexInLastBlock(InvalidRowIndex) {}

[[nodiscard]] auto AqlItemMatrix::countDataRows() const noexcept -> std::size_t {
  size_t num = 0;
  for (auto const& block : _blocks) {
    // We only have valid blocks
    TRI_ASSERT(block != nullptr);
    num += block->numRows();
  }
  // Guard against underflow
  TRI_ASSERT(_startIndexInFirstBlock + countShadowRows() <= num);
  // We have counted overall amount.
  // Subtract all rows we have already delivered (_startIndex)
  // Subtract all shadow rows.
  return num - _startIndexInFirstBlock - countShadowRows();
}

[[nodiscard]] auto AqlItemMatrix::countShadowRows() const noexcept -> std::size_t {
  // We can only have shadow rows in the last block
  if (!stoppedOnShadowRow()) {
    return 0;
  }
  auto const& block = _blocks.back();
  auto [shadowRowsBegin, shadowRowsEnd] = block->getShadowRowIndexesFrom(_stopIndexInLastBlock);
  return std::count_if(shadowRowsBegin, shadowRowsEnd,
                       [&](auto r) -> bool { return r >= _stopIndexInLastBlock; });
}

[[nodiscard]] auto AqlItemMatrix::hasMoreAfterShadowRow() const noexcept -> bool {
  if (_blocks.empty()) {
    return false;
  }
  return _stopIndexInLastBlock + 1 < _blocks.back()->numRows();
}
