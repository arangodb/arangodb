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

using namespace arangodb;
using namespace arangodb::aql;

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
    for (auto const& block : _blocks) {
      for (uint32_t row = 0; row < block->size(); ++row) {
        result.emplace_back(index, row);
      }
      ++index;
    }
  }
  return result;
}

bool AqlItemMatrix::empty() const noexcept { return _blocks.empty(); }

size_t AqlItemMatrix::getNrRegisters() const noexcept { return _nrRegs; }

uint64_t AqlItemMatrix::size() const noexcept { return _size; }

void AqlItemMatrix::addBlock(SharedAqlItemBlockPtr blockPtr) {
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
  _size += blockPtr->size();
  _blocks.emplace_back(std::move(blockPtr));
}

AqlItemMatrix::AqlItemMatrix(size_t nrRegs) : _size(0), _nrRegs(nrRegs) {}
