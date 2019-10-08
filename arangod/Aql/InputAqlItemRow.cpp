////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018-2018 ArangoDB GmbH, Cologne, Germany
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
#include "Aql/AqlItemBlockSerializationFormat.h"
#include "Aql/AqlValue.h"
#include "Aql/Range.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
bool InputAqlItemRow::internalBlockIs(SharedAqlItemBlockPtr const& other) const {
  return _block == other;
}
#endif

SharedAqlItemBlockPtr InputAqlItemRow::cloneToBlock(AqlItemBlockManager& manager,
                                                    std::unordered_set<RegisterId> const& registers,
                                                    size_t newNrRegs) const {
  SharedAqlItemBlockPtr block =
      manager.requestBlock(1, static_cast<RegisterId>(newNrRegs));
  if (isInitialized()) {
    std::unordered_set<AqlValue> cache;
    TRI_ASSERT(getNrRegisters() <= newNrRegs);
    // Should we transform this to output row and reuse copy row?
    for (RegisterId col = 0; col < getNrRegisters(); col++) {
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
void InputAqlItemRow::toVelocyPack(transaction::Methods* trx, VPackBuilder& result) const {
  TRI_ASSERT(isInitialized());
  TRI_ASSERT(result.isOpenObject());
  VPackOptions options(VPackOptions::Defaults);
  options.buildUnindexedArrays = true;
  options.buildUnindexedObjects = true;

  VPackBuilder raw(&options);
  raw.openArray();
  // Two nulls in the beginning such that indices start with 2
  raw.add(VPackValue(VPackValueType::Null));
  raw.add(VPackValue(VPackValueType::Null));

  result.add("nrItems", VPackValue(1));
  result.add("nrRegs", VPackValue(getNrRegisters()));
  result.add("error", VPackValue(false));
  // Backwards compatbility 3.3
  result.add("exhausted", VPackValue(false));

  enum State {
    Empty,       // saw an empty value
    Range,       // saw a range value
    Next,        // saw a previously unknown value
    Positional,  // saw a value previously encountered
  };

  std::unordered_map<AqlValue, size_t> table;  // remember duplicates
  size_t lastTablePos = 0;
  State lastState = Positional;

  State currentState = Positional;
  size_t runLength = 0;
  size_t tablePos = 0;

  result.add("data", VPackValue(VPackValueType::Array));

  // write out data buffered for repeated "empty" or "next" values
  auto writeBuffered = [](State lastState, size_t lastTablePos,
                          VPackBuilder& result, size_t runLength) {
    if (lastState == Range) {
      return;
    }

    if (lastState == Positional) {
      if (lastTablePos >= 2) {
        if (runLength == 1) {
          result.add(VPackValue(lastTablePos));
        } else {
          result.add(VPackValue(-4));
          result.add(VPackValue(runLength));
          result.add(VPackValue(lastTablePos));
        }
      }
    } else {
      TRI_ASSERT(lastState == Empty || lastState == Next);
      if (runLength == 1) {
        // saw exactly one value
        result.add(VPackValue(lastState == Empty ? 0 : 1));
      } else {
        // saw multiple values
        result.add(VPackValue(lastState == Empty ? -1 : -3));
        result.add(VPackValue(runLength));
      }
    }
  };

  size_t pos = 2;  // write position in raw
  // We use column == 0 to simulate a shadowRow
  // this is only relevant if all participants can use shadow rows
  RegisterId startRegister = 0;
  if (block().getFormatType() == SerializationFormat::CLASSIC) {
    // Skip over the shadowRows
    startRegister = 1;
  }
  for (RegisterId column = startRegister; column < getNrRegisters() + 1; column++) {
    AqlValue const& a = column == 0 ? AqlValue{} : getValue(column - 1);
    // determine current state
    if (a.isEmpty()) {
      currentState = Empty;
    } else if (a.isRange()) {
      currentState = Range;
    } else {
      auto it = table.find(a);

      if (it == table.end()) {
        currentState = Next;
        a.toVelocyPack(trx, raw, false);
        table.emplace(a, pos++);
      } else {
        currentState = Positional;
        tablePos = it->second;
        TRI_ASSERT(tablePos >= 2);
        if (lastState != Positional) {
          lastTablePos = tablePos;
        }
      }
    }

    // handle state change
    if (currentState != lastState || (currentState == Positional && tablePos != lastTablePos)) {
      // write out remaining buffered data in case of a state change
      writeBuffered(lastState, lastTablePos, result, runLength);

      lastTablePos = 0;
      lastState = currentState;
      runLength = 0;
    }

    switch (currentState) {
      case Empty:
      case Next:
      case Positional:
        ++runLength;
        lastTablePos = tablePos;
        break;

      case Range:
        result.add(VPackValue(-2));
        result.add(VPackValue(a.range()->_low));
        result.add(VPackValue(a.range()->_high));
        break;
    }
  }

  // write out any remaining buffered data
  writeBuffered(lastState, lastTablePos, result, runLength);

  result.close();  // closes "data"

  raw.close();
  result.add("raw", raw.slice());
}

InputAqlItemRow::InputAqlItemRow(SharedAqlItemBlockPtr const& block, size_t baseIndex)
    : _block(block), _baseIndex(baseIndex) {
  TRI_ASSERT(_block != nullptr);
}

InputAqlItemRow::InputAqlItemRow(SharedAqlItemBlockPtr&& block, size_t baseIndex) noexcept
    : _block(std::move(block)), _baseIndex(baseIndex) {
  TRI_ASSERT(_block != nullptr);
  TRI_ASSERT(_baseIndex < _block->size());
  TRI_ASSERT(!_block->isShadowRow(baseIndex));
}

AqlValue const& InputAqlItemRow::getValue(RegisterId registerId) const {
  TRI_ASSERT(isInitialized());
  TRI_ASSERT(registerId < getNrRegisters());
  return block().getValueReference(_baseIndex, registerId);
}

AqlValue InputAqlItemRow::stealValue(RegisterId registerId) {
  TRI_ASSERT(isInitialized());
  TRI_ASSERT(registerId < getNrRegisters());
  AqlValue const& a = block().getValueReference(_baseIndex, registerId);
  if (!a.isEmpty() && a.requiresDestruction()) {
    // Now no one is responsible for AqlValue a
    block().steal(a);
  }
  // This cannot fail, caller needs to take immediate ownership.
  return a;
}

RegisterCount InputAqlItemRow::getNrRegisters() const noexcept { return block().getNrRegs(); }

bool InputAqlItemRow::operator==(InputAqlItemRow const& other) const noexcept {
  return this->_block == other._block && this->_baseIndex == other._baseIndex;
}

bool InputAqlItemRow::operator!=(InputAqlItemRow const& other) const noexcept {
  return !(*this == other);
}

bool InputAqlItemRow::equates(InputAqlItemRow const& other) const noexcept {
  if (!isInitialized() || !other.isInitialized()) {
    return isInitialized() == other.isInitialized();
  }
  TRI_ASSERT(getNrRegisters() == other.getNrRegisters());
  if (getNrRegisters() != other.getNrRegisters()) {
    return false;
  }
  // NOLINTNEXTLINE(modernize-use-transparent-functors)
  auto const eq = std::equal_to<AqlValue>{};
  for (RegisterId i = 0; i < getNrRegisters(); ++i) {
    if (!eq(getValue(i), other.getValue(i))) {
      return false;
    }
  }

  return true;
}

bool InputAqlItemRow::isInitialized() const noexcept {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  if (_block != nullptr) {
    TRI_ASSERT(!block().isShadowRow(_baseIndex));
  }
#endif
  return _block != nullptr;
}

InputAqlItemRow::operator bool() const noexcept { return isInitialized(); }

bool InputAqlItemRow::isFirstRowInBlock() const noexcept {
  TRI_ASSERT(isInitialized());
  TRI_ASSERT(_baseIndex < block().size());
  return _baseIndex == 0;
}

bool InputAqlItemRow::isLastRowInBlock() const noexcept {
  TRI_ASSERT(isInitialized());
  TRI_ASSERT(_baseIndex < block().size());
  return _baseIndex + 1 == block().size();
}

bool InputAqlItemRow::blockHasMoreRows() const noexcept { return !isLastRowInBlock(); }

AqlItemBlock& InputAqlItemRow::block() noexcept {
  TRI_ASSERT(_block != nullptr);
  return *_block;
}

AqlItemBlock const& InputAqlItemRow::block() const noexcept {
  TRI_ASSERT(_block != nullptr);
  return *_block;
}
