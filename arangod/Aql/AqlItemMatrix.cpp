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

#include "AqlItemMatrix.h"

#include "Aql/InputAqlItemRow.h"
#include "Aql/SharedAqlItemBlockPtr.h"
#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"
#include "Containers/Enumerate.h"

#include "Logger/LogMacros.h"

using namespace arangodb;
using namespace arangodb::aql;

static constexpr size_t InvalidRowIndex = std::numeric_limits<size_t>::max();

size_t AqlItemMatrix::numberOfBlocks() const noexcept { return _blocks.size(); }

SharedAqlItemBlockPtr AqlItemMatrix::getBlock(size_t index) const noexcept {
  TRI_ASSERT(index < numberOfBlocks());
  return _blocks[index];
}

InputAqlItemRow AqlItemMatrix::getRow(AqlItemMatrix::RowIndex index) const noexcept {
  auto const& block = getBlock(index.first);
  return InputAqlItemRow{block, index.second};
}

std::vector<AqlItemMatrix::RowIndex> AqlItemMatrix::produceRowIndexes() const {
  std::vector<RowIndex> result;
  if (!empty()) {
    result.reserve(size());
    for (auto const& [index, block] : enumerate(_blocks)) {
      // Default case, 0 -> end
      size_t startRow = 0;
      // We know block size is <= DefaultBatchSize (1000) so it should easily fit into 32bit...
      size_t endRow = block->size();

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

bool AqlItemMatrix::empty() const noexcept { return _blocks.empty(); }

void AqlItemMatrix::clear() {
  _blocks.clear();
  _size = 0;
  _startIndexInFirstBlock = 0;
  _stopIndexInLastBlock = InvalidRowIndex;
}

RegisterId AqlItemMatrix::getNrRegisters() const noexcept { return _nrRegs; }

uint64_t AqlItemMatrix::size() const noexcept { return _size; }

void AqlItemMatrix::addBlock(SharedAqlItemBlockPtr blockPtr) {
  // If we are stopped by shadow row, we first need to solve this blockage
  // by popShadowRow calls. In order to continue.
  // The schadowRow logic is only based on the last node.
  TRI_ASSERT(!stoppedOnShadowRow());

  TRI_ASSERT(blockPtr->getNrRegs() == getNrRegisters());
  // Test if we have more than uint32_t many blocks
  if (ADB_UNLIKELY(_blocks.size() == std::numeric_limits<uint32_t>::max())) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_RESOURCE_LIMIT,
        "Reaching the limit of AqlItems to SORT, please consider using a "
        "limit after sorting.");
  }
  // Test if we have more than uint32_t many rows within a block
  if (ADB_UNLIKELY(blockPtr->size() > std::numeric_limits<uint32_t>::max())) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_RESOURCE_LIMIT,
        "Reaching the limit of AqlItems to SORT, please consider lowering "
        "the batch size.");
  }

  // ShadowRow handling
  if (blockPtr->hasShadowRows()) {
    TRI_ASSERT(!blockPtr->getShadowRowIndexes().empty());
    // Let us stop on the first
    _stopIndexInLastBlock = *blockPtr->getShadowRowIndexes().begin();
    _size += _stopIndexInLastBlock;
  } else {
    _size += blockPtr->size();
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
  auto const& shadowIndexes = blockPtr->getShadowRowIndexes();
  auto next = shadowIndexes.find(_stopIndexInLastBlock);
  _startIndexInFirstBlock = _stopIndexInLastBlock + 1;

  next++;

  if (next != shadowIndexes.end()) {
    _stopIndexInLastBlock = *next;
    TRI_ASSERT(stoppedOnShadowRow());
    // We move always forward
    TRI_ASSERT(_stopIndexInLastBlock >= _startIndexInFirstBlock);
    _size = _stopIndexInLastBlock - _startIndexInFirstBlock;
  } else {
    _stopIndexInLastBlock = InvalidRowIndex;
    TRI_ASSERT(!stoppedOnShadowRow());
    // _stopIndexInLastBlock a 0 based index. size is a counter.
    TRI_ASSERT(blockPtr->size() >= _startIndexInFirstBlock);
    _size = blockPtr->size() - _startIndexInFirstBlock;
  }
  // Remove all but the last block
  _blocks.erase(_blocks.begin(), _blocks.end() - 1);
  TRI_ASSERT(_blocks.size() == 1);
  return shadowRow;
}

ShadowAqlItemRow AqlItemMatrix::peekShadowRow() const {
  TRI_ASSERT(stoppedOnShadowRow());
  return ShadowAqlItemRow{_blocks.back(), _stopIndexInLastBlock};
}

AqlItemMatrix::AqlItemMatrix(RegisterId nrRegs)
    : _size(0), _nrRegs(nrRegs), _stopIndexInLastBlock(InvalidRowIndex) {}

[[nodiscard]] auto AqlItemMatrix::countDataRows() const noexcept -> std::size_t {
  size_t num = 0;
  for (auto const& block : _blocks) {
    // We only have valid blocks
    TRI_ASSERT(block != nullptr);
    num += block->size();
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
  auto const& rows = block->getShadowRowIndexes();
  return std::count_if(rows.begin(), rows.end(),
                       [&](auto r) -> bool { return r >= _stopIndexInLastBlock; });
}

[[nodiscard]] auto AqlItemMatrix::hasMoreAfterShadowRow() const noexcept -> bool {
  if (_blocks.empty()) {
    return false;
  }
  return _stopIndexInLastBlock + 1 < _blocks.back()->size();
}