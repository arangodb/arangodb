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

#include "Logger/LogMacros.h"
using namespace arangodb;
using namespace arangodb::aql;

static constexpr size_t InvalidRowIndex = std::numeric_limits<size_t>::max();

namespace {
static size_t FirstReleveantDataRowInBlock(SharedAqlItemBlockPtr const& block) {
  auto const& shadowIndexes = block->getShadowRowIndexes();
  TRI_ASSERT(!shadowIndexes.empty());
  return *shadowIndexes.rbegin() + 1;
}
}  // namespace

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
    uint32_t index = 0;
    if (_blocks.size() == 1) {
      // Special case, we only have a single block
      auto const& block = _blocks.front();
      // Default case, 0 -> end
      size_t startRow = 0;
      // We know block size is <= DefaultBatchSize (1000) so it should easily fit into 32bit...
      size_t endRow = block->size();

      if (block->hasShadowRows()) {
        // We have one (or more) shadowRow(s) with this block.
        // We need to adjast start / end row to the slice of ShadowRows we are working on.
        if (stoppedOnShadowRow()) {
          // We need to stop on _lastShadowRow;
          endRow = _lastShadowRow;

          auto const& shadowIndexes = block->getShadowRowIndexes();
          TRI_ASSERT(!shadowIndexes.empty());
          // we need to start at the ShadowRow before _lastShadowRow
          auto before = shadowIndexes.find(_lastShadowRow);
          if (before != shadowIndexes.begin()) {
            before--;
            // Pick the shadowRow before the lastShadowRow
            // And start from the line AFTER.
            // NOTE: This could already be the next shadowRow. in this case we return an empty list
            startRow = *before + 1;
          }
        } else {
          // We need to start after the last shadowRow
          // NOTE: this might be AFTER the block, but this will be sorted out by the loop later.
          startRow = ::FirstReleveantDataRowInBlock(block);
        }
      }
      for (; startRow < endRow; ++startRow) {
        TRI_ASSERT(!block->isShadowRow(startRow));
        result.emplace_back(index, static_cast<uint32_t>(startRow));
      }
    } else {
      for (auto const& block : _blocks) {
        // Default case, 0 -> end
        size_t startRow = 0;
        // We know block size is <= DefaultBatchSize (1000) so it should easily fit into 32bit...
        size_t endRow = block->size();

        if (block == _blocks.front() && block->hasShadowRows()) {
          // The first block was sliced by a ShadowRow, we need to pick everything after the last:
          startRow = ::FirstReleveantDataRowInBlock(block);
        } else if (block == _blocks.back() && block->hasShadowRows()) {
          // The last Block is sliced by a shadowRow. We can only use data up to this shadow row
          endRow = _lastShadowRow;
        } else {
          // Intermediate blocks cannot have shadow rows.
          // Go from 0 -> end
          TRI_ASSERT(!block->hasShadowRows());
        }
        for (; startRow < endRow; ++startRow) {
          TRI_ASSERT(!block->isShadowRow(startRow));
          result.emplace_back(index, static_cast<uint32_t>(startRow));
        }
        ++index;
      }
    }
  }
  return result;
}

bool AqlItemMatrix::empty() const noexcept { return _blocks.empty(); }

RegisterId AqlItemMatrix::getNrRegisters() const noexcept { return _nrRegs; }

uint64_t AqlItemMatrix::size() const noexcept { return _size; }

size_t AqlItemMatrix::memoryUsageForRowIndexes() const noexcept {
  return size() * sizeof(RowIndex);
}

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
    _lastShadowRow = *blockPtr->getShadowRowIndexes().begin();
    _size += _lastShadowRow;
  } else {
    _size += blockPtr->size();
  }

  // Move block into _blocks
  _blocks.emplace_back(std::move(blockPtr));
}

bool AqlItemMatrix::stoppedOnShadowRow() const noexcept {
  return _lastShadowRow != InvalidRowIndex;
}

ShadowAqlItemRow AqlItemMatrix::popShadowRow() {
  TRI_ASSERT(stoppedOnShadowRow());
  auto const& blockPtr = _blocks.back();
  // We need to return this shadow row
  ShadowAqlItemRow shadowRow{_blocks.back(), _lastShadowRow};

  // We need to move forward the next shadow row.
  auto const& shadowIndexes = blockPtr->getShadowRowIndexes();
  auto next = shadowIndexes.find(_lastShadowRow);
  auto lastSize = _lastShadowRow;

  next++;

  if (next != shadowIndexes.end()) {
    _lastShadowRow = *next;
    TRI_ASSERT(stoppedOnShadowRow());
    // We move always forward
    TRI_ASSERT(_lastShadowRow > lastSize);
    _size = _lastShadowRow - lastSize - 1;
  } else {
    _lastShadowRow = InvalidRowIndex;
    TRI_ASSERT(!stoppedOnShadowRow());
    // lastSize a 0 based index. size is a counter.
    TRI_ASSERT(blockPtr->size() > lastSize);
    _size = blockPtr->size() - lastSize - 1;
  }
  // Remove all but the last
  _blocks.erase(_blocks.begin(), _blocks.end() - 1);
  TRI_ASSERT(_blocks.size() == 1);
  return shadowRow;
}

ShadowAqlItemRow AqlItemMatrix::peekShadowRow() const {
  TRI_ASSERT(stoppedOnShadowRow());
  return ShadowAqlItemRow{_blocks.back(), _lastShadowRow};
}

AqlItemMatrix::AqlItemMatrix(RegisterId nrRegs)
    : _size(0), _nrRegs(nrRegs), _lastShadowRow(InvalidRowIndex) {}
