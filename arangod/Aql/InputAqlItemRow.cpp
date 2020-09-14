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
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "InputAqlItemRow.h"

#include "Aql/AqlItemBlockManager.h"
#include "Aql/AqlValue.h"
#include "Aql/Range.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>
#include <boost/container/flat_set.hpp>

#include <algorithm>
#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
bool InputAqlItemRow::internalBlockIs(SharedAqlItemBlockPtr const& other, size_t index) const {
  return _block == other && _baseIndex == index;
}
#endif

SharedAqlItemBlockPtr InputAqlItemRow::cloneToBlock(AqlItemBlockManager& manager,
                                                    RegIdFlatSet const& registers,
                                                    size_t newNrRegs) const {
  SharedAqlItemBlockPtr block =
      manager.requestBlock(1, static_cast<RegisterId>(newNrRegs));
  if (isInitialized()) {
    std::unordered_set<AqlValue> cache;
    TRI_ASSERT(getNumRegisters() <= newNrRegs);
    // Should we transform this to output row and reuse copy row?
    for (RegisterId col = 0; col < getNumRegisters(); col++) {
      if (registers.find(col) == registers.end()) {
        continue;
      }

      AqlValue const& a = getValue(col);

      if (!a.isEmpty()) {
        if (a.requiresDestruction()) {
          auto it = cache.find(a);

          if (it == cache.end()) {
            AqlValue b = a.clone();
            try {
              block->setValue(0, col, b);
            } catch (...) {
              b.destroy();
              throw;
            }
            cache.emplace(b);
          } else {
            block->setValue(0, col, (*it));
          }
        } else {
          block->setValue(0, col, a);
        }
      }
    }
  }
  return block;
}

/// @brief toJson, transfer a whole AqlItemBlock to Json, the result can
/// be used to recreate the AqlItemBlock via the Json constructor
/// Here is a description of the data format: The resulting Json has
/// the following attributes:
///  "nrItems": the number of rows of the AqlItemBlock
///  "nrRegs":  the number of registers of the AqlItemBlock
///  "error":   always set to false
///  "data":    this contains the actual data in the form of a list of
///             numbers. The AqlItemBlock is stored columnwise, starting
///             from the first column (top to bottom) and going right.
///             Each entry found is encoded in the following way:
///               0  means a single empty entry
///              -1  followed by a positive integer N (encoded as number)
///                  means a run of that many empty entries. this is a
///                  compression for multiple "0" entries
///              -2  followed by two numbers LOW and HIGH means a range
///                  and LOW and HIGH are the boundaries (inclusive)
///              -3  followed by a positive integer N (encoded as number)
///                  means a run of that many JSON entries which can
///                  be found at the "next" position in "raw". this is
///                  a compression for multiple "1" entries
///              -4  followed by a positive integer N (encoded as number)
///                  and followed by a positive integer P (encoded as number)
///                  means a run of that many JSON entries which can
///                  be found in the "raw" list at the position P
///               1  means a JSON entry at the "next" position in "raw"
///                  the "next" position starts with 2 and is increased
///                  by one for every 1 found in data
///             integer values >= 2 mean a JSON entry, in this
///                  case the "raw" list contains an entry in the
///                  corresponding position
///  "raw":     List of actual values, positions 0 and 1 are always null
///                  such that actual indices start at 2
void InputAqlItemRow::toVelocyPack(velocypack::Options const* const trxOptions,
                                   VPackBuilder& result) const {
  TRI_ASSERT(isInitialized());
  TRI_ASSERT(result.isOpenObject());
  _block->toVelocyPack(_baseIndex, _baseIndex + 1, trxOptions, result);
}

void InputAqlItemRow::toSimpleVelocyPack(velocypack::Options const* trxOpts,
                                         arangodb::velocypack::Builder& result) const {
  TRI_ASSERT(_block != nullptr);
  _block->rowToSimpleVPack(_baseIndex, trxOpts, result);
}

InputAqlItemRow::InputAqlItemRow(SharedAqlItemBlockPtr const& block, size_t baseIndex) noexcept
    : _block(block), _baseIndex(baseIndex) {
  TRI_ASSERT(_block != nullptr);
  TRI_ASSERT(_baseIndex < _block->numRows());
  TRI_ASSERT(!_block->isShadowRow(baseIndex));
}

InputAqlItemRow::InputAqlItemRow(SharedAqlItemBlockPtr&& block, size_t baseIndex) noexcept
    : _block(std::move(block)), _baseIndex(baseIndex) {
  TRI_ASSERT(_block != nullptr);
  TRI_ASSERT(_baseIndex < _block->numRows());
  TRI_ASSERT(!_block->isShadowRow(baseIndex));
}

AqlValue const& InputAqlItemRow::getValue(RegisterId registerId) const {
  TRI_ASSERT(isInitialized());
  TRI_ASSERT(registerId < getNumRegisters());
  return block().getValueReference(_baseIndex, registerId);
}

AqlValue InputAqlItemRow::stealValue(RegisterId registerId) {
  TRI_ASSERT(isInitialized());
  TRI_ASSERT(registerId < getNumRegisters());
  AqlValue const& a = block().getValueReference(_baseIndex, registerId);
  if (!a.isEmpty() && a.requiresDestruction()) {
    // Now no one is responsible for AqlValue a
    block().steal(a);
  }
  // This cannot fail, caller needs to take immediate ownership.
  return a;
}

RegisterCount InputAqlItemRow::getNumRegisters() const noexcept {
  return block().numRegisters();
}

bool InputAqlItemRow::isSameBlockAndIndex(InputAqlItemRow const& other) const noexcept {
  return this->_block == other._block && this->_baseIndex == other._baseIndex;
}

#ifdef ARANGODB_USE_GOOGLE_TESTS
bool InputAqlItemRow::equates(InputAqlItemRow const& other,
                              velocypack::Options const* const options) const noexcept {
  if (!isInitialized() || !other.isInitialized()) {
    return isInitialized() == other.isInitialized();
  }
  TRI_ASSERT(getNumRegisters() == other.getNumRegisters());
  if (getNumRegisters() != other.getNumRegisters()) {
    return false;
  }
  auto const eq = [options](auto left, auto right) {
    return 0 == AqlValue::Compare(options, left, right, false);
  };
  for (RegisterId i = 0; i < getNumRegisters(); ++i) {
    if (!eq(getValue(i), other.getValue(i))) {
      return false;
    }
  }

  return true;
}
#endif

bool InputAqlItemRow::isInitialized() const noexcept {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  if (_block != nullptr) {
    TRI_ASSERT(!block().isShadowRow(_baseIndex));
  }
#endif
  return _block != nullptr;
}

InputAqlItemRow::operator bool() const noexcept { return isInitialized(); }

bool InputAqlItemRow::isFirstDataRowInBlock() const noexcept {
  TRI_ASSERT(isInitialized());
  TRI_ASSERT(_baseIndex < block().numRows());

  auto [shadowRowsBegin, shadowRowsEnd] = block().getShadowRowIndexesFrom(0);

  // Count the number of shadow rows before this row.
  size_t const numShadowRowsBeforeCurrentRow = [this, shadowRowsBegin = shadowRowsBegin]() {
    // this is the last shadow row after _baseIndex, i.e.
    // nextShadowRowIt := min { it \in shadowRowIndexes | _baseIndex <= it }
    auto [offsetBegin, offsetEnd] = block().getShadowRowIndexesFrom(_baseIndex);
    // But, as _baseIndex must not be a shadow row, it's actually
    // nextShadowRowIt = min { it \in shadowRowIndexes | _baseIndex < it }
    // so the same as shadowRowIndexes.upper_bound(_baseIndex)
    TRI_ASSERT(offsetBegin == offsetEnd || _baseIndex < *offsetBegin);

    return std::distance(shadowRowsBegin, offsetBegin);
  }();
  TRI_ASSERT(numShadowRowsBeforeCurrentRow <= static_cast<size_t>(std::distance(shadowRowsBegin, shadowRowsEnd)));

  return numShadowRowsBeforeCurrentRow == _baseIndex;
}

bool InputAqlItemRow::blockHasMoreDataRowsAfterThis() const noexcept {
  TRI_ASSERT(isInitialized());
  TRI_ASSERT(_baseIndex < block().numRows());

  // Count the number of shadow rows after this row.
  size_t const numShadowRowsAfterCurrentRow = [this]() {
    // this is the last shadow row after _baseIndex, i.e.
    // shadowRowsBegin := min { it \in shadowRowIndexes | _baseIndex <= it }
    auto [shadowRowsBegin, shadowRowsEnd] = block().getShadowRowIndexesFrom(_baseIndex);
    // But, as _baseIndex must not be a shadow row, it's actually
    // shadowRowsBegin = min { it \in shadowRowIndexes | _baseIndex < it }
    // so the same as shadowRowIndexes.upper_bound(_baseIndex)
    TRI_ASSERT(shadowRowsBegin == shadowRowsEnd || _baseIndex < *shadowRowsBegin);

    return std::distance(shadowRowsBegin, shadowRowsEnd);
  }();

  // block().size() is strictly greater than baseIndex
  size_t const totalRowsAfterCurrentRow = block().numRows() - _baseIndex - 1;

  return totalRowsAfterCurrentRow > numShadowRowsAfterCurrentRow;
}

AqlItemBlock& InputAqlItemRow::block() noexcept {
  TRI_ASSERT(_block != nullptr);
  return *_block;
}

AqlItemBlock const& InputAqlItemRow::block() const noexcept {
  TRI_ASSERT(_block != nullptr);
  return *_block;
}
