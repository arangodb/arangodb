////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "AqlItemBlock.h"

#include "Aql/AqlItemBlockManager.h"
#include "Aql/AqlItemBlockSerializationFormat.h"
#include "Aql/BlockCollector.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionNode.h"
#include "Aql/Range.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SharedAqlItemBlockPtr.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

using VelocyPackHelper = arangodb::basics::VelocyPackHelper;

namespace {
inline void CopyValueOver(std::unordered_set<AqlValue>& cache, AqlValue const& a,
                          size_t rowNumber, RegisterId col, SharedAqlItemBlockPtr& res) {
  if (!a.isEmpty()) {
    if (a.requiresDestruction()) {
      auto it = cache.find(a);

      if (it == cache.end()) {
        AqlValue b = a.clone();
        try {
          res->setValue(rowNumber, col, b);
        } catch (...) {
          b.destroy();
          throw;
        }
        cache.emplace(b);
      } else {
        res->setValue(rowNumber, col, (*it));
      }
    } else {
      res->setValue(rowNumber, col, a);
    }
  }
}
}  // namespace

/// @brief create the block
AqlItemBlock::AqlItemBlock(AqlItemBlockManager& manager, size_t nrItems, RegisterId nrRegs)
    : _nrItems(nrItems), _nrRegs(nrRegs), _manager(manager), _refCount(0), _rowIndex(0) {
  TRI_ASSERT(nrItems > 0);  // empty AqlItemBlocks are not allowed!
  // check that the nrRegs value is somewhat sensible
  // this compare value is arbitrary, but having so many registers in a single
  // query seems unlikely
  TRI_ASSERT(nrRegs <= RegisterPlan::MaxRegisterId);
  increaseMemoryUsage(sizeof(AqlValue) * nrItems * internalNrRegs());
  try {
    _data.resize(nrItems * internalNrRegs());
  } catch (...) {
    decreaseMemoryUsage(sizeof(AqlValue) * nrItems * internalNrRegs());
    throw;
  }
}

/// @brief init the block from VelocyPack, note that this can throw
void AqlItemBlock::initFromSlice(VPackSlice const slice) {
  int64_t nrItems = VelocyPackHelper::getNumericValue<int64_t>(slice, "nrItems", 0);
  if (nrItems <= 0) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "nrItems must be > 0");
  }
  _nrItems = static_cast<size_t>(nrItems);

  _nrRegs = VelocyPackHelper::getNumericValue<RegisterId>(slice, "nrRegs", 0);

  // Initialize the data vector:
  increaseMemoryUsage(sizeof(AqlValue) * _nrItems * internalNrRegs());
  try {
    _data.resize(_nrItems * internalNrRegs());
  } catch (...) {
    decreaseMemoryUsage(sizeof(AqlValue) * _nrItems * internalNrRegs());
    throw;
  }

  // Now put in the data:
  VPackSlice data = slice.get("data");
  VPackSlice raw = slice.get("raw");

  std::vector<AqlValue> madeHere;
  madeHere.reserve(static_cast<size_t>(raw.length()));
  madeHere.emplace_back();  // an empty AqlValue
  madeHere.emplace_back();  // another empty AqlValue, indices start w. 2

  VPackArrayIterator dataIterator(data);
  VPackArrayIterator rawIterator(raw);

  auto storeSingleValue = [this](size_t row, RegisterId column, VPackArrayIterator& it,
                                 std::vector<AqlValue>& madeHere) {
    AqlValue a(it.value());
    it.next();
    try {
      if (column == 0) {
        if (!a.isEmpty()) {
          setShadowRowDepth(row, a);
        }
      } else {
        setValue(row, column - 1, a);  // if this throws, a is destroyed again
      }
    } catch (...) {
      a.destroy();
      throw;
    }

    madeHere.emplace_back(a);
  };

  enum RunType { NoRun = 0, EmptyRun, NextRun, PositionalRun };

  int64_t runLength = 0;
  RunType runType = NoRun;

  try {
    size_t tablePos = 0;
    // skip the first two records
    rawIterator.next();
    rawIterator.next();
    RegisterId startColumn = 0;
    if (getFormatType() == SerializationFormat::CLASSIC) {
      startColumn = 1;
    }

    for (RegisterId column = startColumn; column < internalNrRegs(); column++) {
      for (size_t i = 0; i < _nrItems; i++) {
        if (runLength > 0) {
          switch (runType) {
            case EmptyRun:
              // nothing to do
              break;

            case NextRun:
              storeSingleValue(i, column, rawIterator, madeHere);
              break;

            case PositionalRun:
              TRI_ASSERT(tablePos < madeHere.size());
              if (column == 0) {
                if (!madeHere[tablePos].isEmpty()) {
                  setShadowRowDepth(i, madeHere[tablePos]);
                }
              } else {
                setValue(i, column - 1, madeHere[tablePos]);
              }
              break;

            case NoRun: {
              TRI_ASSERT(false);
            }
          }

          --runLength;
          if (runLength == 0) {
            runType = NoRun;
            tablePos = 0;
          }

          continue;
        }

        VPackSlice dataEntry = dataIterator.value();
        dataIterator.next();
        if (!dataEntry.isNumber()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                         "data must contain only numbers");
        }

        int64_t n = dataEntry.getNumericValue<int64_t>();
        if (n == 0) {
          // empty, do nothing here
        } else if (n == 1) {
          // a VelocyPack value
          storeSingleValue(i, column, rawIterator, madeHere);
        } else if (n == -1 || n == -3 || n == -4) {
          // -1: empty run, -3: run of "next" values, -4: run of positional
          // values
          VPackSlice v = dataIterator.value();
          dataIterator.next();
          TRI_ASSERT(v.isNumber());
          runLength = v.getNumericValue<int64_t>();
          runLength--;
          switch (n) {
            case -1:
              runType = EmptyRun;
              break;

            case -3:
              runType = NextRun;
              storeSingleValue(i, column, rawIterator, madeHere);
              break;

            case -4: {
              runType = PositionalRun;
              VPackSlice v = dataIterator.value();
              dataIterator.next();
              TRI_ASSERT(v.isNumber());
              tablePos = v.getNumericValue<size_t>();
              if (tablePos >= madeHere.size()) {
                // safeguard against out-of-bounds accesses
                THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                               "found undefined data value");
              }
              if (column == 0) {
                if (!madeHere[tablePos].isEmpty()) {
                  setShadowRowDepth(i, madeHere[tablePos]);
                }
              } else {
                setValue(i, column - 1, madeHere[tablePos]);
              }
            }
          }
        } else if (n == -2) {
          // a range
          VPackSlice lowBound = dataIterator.value();
          dataIterator.next();
          VPackSlice highBound = dataIterator.value();
          dataIterator.next();

          int64_t low = VelocyPackHelper::getNumericValue<int64_t>(lowBound, 0);
          int64_t high = VelocyPackHelper::getNumericValue<int64_t>(highBound, 0);
          TRI_ASSERT(column != 0);
          emplaceValue(i, column - 1, low, high);
        } else if (n >= 2) {
          if (static_cast<size_t>(n) >= madeHere.size()) {
            // safeguard against out-of-bounds accesses
            THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                           "found undefined data value");
          }
          if (column == 0) {
            setShadowRowDepth(i, madeHere[static_cast<size_t>(n)]);
          } else {
            setValue(i, column - 1, madeHere[static_cast<size_t>(n)]);
          }

          // If this throws, all is OK, because it was already put into
          // the block elsewhere.
        } else {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                         "found invalid data encoding value");
        }
      }
    }
  } catch (...) {
    destroy();
    throw;
  }

  TRI_ASSERT(runLength == 0);
  TRI_ASSERT(runType == NoRun);
}

SerializationFormat AqlItemBlock::getFormatType() const {
  return _manager.getFormatType();
}

/// @brief destroy the block, used in the destructor and elsewhere
void AqlItemBlock::destroy() noexcept {
  // none of the functions used here will throw in reality, but
  // technically all the unordered_map functions are not noexcept for
  // arbitrary types. so we put a global try...catch here to be on
  // the safe side
  try {
    if (_valueCount.empty()) {
      eraseAll();
      rescale(0, 0);
      TRI_ASSERT(numEntries() == 0);
      return;
    }

    for (size_t i = 0; i < numEntries(); i++) {
      auto& it = _data[i];
      if (it.requiresDestruction()) {
        auto it2 = _valueCount.find(it);
        if (it2 != _valueCount.end()) {  // if we know it, we are still responsible
          TRI_ASSERT((*it2).second > 0);

          if (--((*it2).second) == 0) {
            decreaseMemoryUsage(it.memoryUsage());
            it.destroy();
            _valueCount.erase(it2);
          }
        }
      }
      // Note that if we do not know it the thing it has been stolen from us!
      it.erase();
    }
    _valueCount.clear();

    rescale(0, 0);
  } catch (...) {
    TRI_ASSERT(false);
  }

  TRI_ASSERT(numEntries() == 0);
}

/// @brief shrink the block to the specified number of rows
/// the superfluous rows are cleaned
void AqlItemBlock::shrink(size_t nrItems) {
  TRI_ASSERT(nrItems > 0);

  if (nrItems == _nrItems) {
    // nothing to do
    return;
  }

  if (nrItems > _nrItems) {
    // cannot use shrink() to increase the size of the block
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "cannot use shrink() to increase block");
  }

  decreaseMemoryUsage(sizeof(AqlValue) * (_nrItems - nrItems) * _nrRegs);

  for (size_t i = numEntries(); i < _data.size(); ++i) {
    AqlValue& a = _data[i];
    if (a.requiresDestruction()) {
      auto it = _valueCount.find(a);

      if (it != _valueCount.end()) {
        TRI_ASSERT((*it).second > 0);

        if (--((*it).second) == 0) {
          decreaseMemoryUsage(a.memoryUsage());
          a.destroy();
          try {
            _valueCount.erase(it);
            continue;  // no need for an extra a.erase() here
          } catch (...) {
          }
        }
      }
    }
    a.erase();
  }

  // adjust the size of the block
  _nrItems = nrItems;
}

void AqlItemBlock::rescale(size_t nrItems, RegisterId nrRegs) {
  TRI_ASSERT(_valueCount.empty());
  TRI_ASSERT(nrRegs <= RegisterPlan::MaxRegisterId);

  size_t const targetSize = nrItems * (nrRegs + 1);
  size_t const currentSize = _nrItems * internalNrRegs();
  TRI_ASSERT(currentSize == numEntries());

  // TODO Previously, _data.size() was used for the memory usage.
  // _data.capacity() might have been more accurate. Now, _data.size() stays at
  // _data.capacity(), or at least, is never reduced.
  // So I decided for now to report the memory usage based on numEntries() only,
  // to mimic the previous behavior. I'm not sure whether it should stay this
  // way; because currently, we are tracking the memory we need, instead of the
  // memory we have.
  if (targetSize > _data.size()) {
    _data.resize(targetSize);
  }

  TRI_ASSERT(targetSize <= _data.size());

  if (targetSize > numEntries()) {
    increaseMemoryUsage(sizeof(AqlValue) * (targetSize - currentSize));

    // Values will not be re-initialized, but are expected to be that way.
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    for (size_t i = currentSize; i < targetSize; i++) {
      TRI_ASSERT(_data[i].isEmpty());
    }
#endif
  } else if (targetSize < numEntries()) {
    decreaseMemoryUsage(sizeof(AqlValue) * (currentSize - targetSize));
  }

  _nrItems = nrItems;
  _nrRegs = nrRegs;
}

/// @brief clears out some columns (registers), this deletes the values if
/// necessary, using the reference count.
void AqlItemBlock::clearRegisters(std::unordered_set<RegisterId> const& toClear) {
  for (size_t i = 0; i < _nrItems; i++) {
    for (auto const& reg : toClear) {
      AqlValue& a(_data[getAddress(i, reg)]);

      if (a.requiresDestruction()) {
        auto it = _valueCount.find(a);

        if (it != _valueCount.end()) {
          TRI_ASSERT((*it).second > 0);

          if (--((*it).second) == 0) {
            decreaseMemoryUsage(a.memoryUsage());
            a.destroy();
            try {
              _valueCount.erase(it);
              continue;  // no need for an extra a.erase() here
            } catch (...) {
            }
          }
        }
      }
      a.erase();
    }
  }
}

/// @brief slice/clone, this does a deep copy of all entries
SharedAqlItemBlockPtr AqlItemBlock::slice(size_t from, size_t to) const {
  TRI_ASSERT(from < to);
  TRI_ASSERT(to <= _nrItems);

  std::unordered_set<AqlValue> cache;
  cache.reserve((to - from) * _nrRegs / 4 + 1);

  SharedAqlItemBlockPtr res{_manager.requestBlock(to - from, _nrRegs)};

  for (size_t row = from; row < to; row++) {
    // Note this loop is special, it will also Copy over the SubqueryDepth data in reg 0
    for (RegisterId col = 0; col < _nrRegs; col++) {
      AqlValue const& a(_data[getAddress(row, col)]);
      ::CopyValueOver(cache, a, row - from, col, res);
    }
    res->copySubQueryDepthFromOtherBlock(row - from, *this, row);
  }

  return res;
}

/// @brief slice/clone, this does a deep copy of all entries
SharedAqlItemBlockPtr AqlItemBlock::slice(size_t row,
                                          std::unordered_set<RegisterId> const& registers,
                                          RegisterCount newNrRegs) const {
  TRI_ASSERT(_nrRegs <= newNrRegs);

  std::unordered_set<AqlValue> cache;

  SharedAqlItemBlockPtr res{_manager.requestBlock(1, newNrRegs)};
  for (RegisterId col = 0; col < _nrRegs; col++) {
    if (registers.find(col) == registers.end()) {
      continue;
    }

    AqlValue const& a(_data[getAddress(row, col)]);
    ::CopyValueOver(cache, a, 0, col, res);
  }
  res->copySubQueryDepthFromOtherBlock(0, *this, row);

  return res;
}

/// @brief slice/clone chosen rows for a subset, this does a deep copy
/// of all entries
SharedAqlItemBlockPtr AqlItemBlock::slice(std::vector<size_t> const& chosen,
                                          size_t from, size_t to) const {
  TRI_ASSERT(from < to && to <= chosen.size());

  std::unordered_set<AqlValue> cache;
  cache.reserve((to - from) * internalNrRegs() / 4 + 1);

  SharedAqlItemBlockPtr res{_manager.requestBlock(to - from, _nrRegs)};

  size_t resultRowIdx = 0;
  for (size_t chosenIdx = from; chosenIdx < to; ++chosenIdx, ++resultRowIdx) {
    size_t const rowIdx = chosen[chosenIdx];
    for (RegisterId col = 0; col < _nrRegs; col++) {
      AqlValue const& a = _data[getAddress(rowIdx, col)];
      ::CopyValueOver(cache, a, resultRowIdx, col, res);
    }
    res->copySubQueryDepthFromOtherBlock(resultRowIdx, *this, rowIdx);
  }

  return res;
}

/// @brief steal for a subset, this does not copy the entries, rather,
/// it remembers which it has taken. This is stored in the
/// this by removing the value counts in _valueCount.
/// It is highly recommended to delete this object right after this
/// operation, because it is unclear, when the values to which our
/// AqlValues point will vanish! In particular, do not use setValue
/// any more.
SharedAqlItemBlockPtr AqlItemBlock::steal(std::vector<size_t> const& chosen,
                                          size_t from, size_t to) {
  TRI_ASSERT(from < to && to <= chosen.size());

  SharedAqlItemBlockPtr res{_manager.requestBlock(to - from, _nrRegs)};

  for (size_t row = from; row < to; row++) {
    for (RegisterId col = 0; col < _nrRegs; col++) {
      AqlValue& a(_data[getAddress(chosen[row], col)]);

      if (!a.isEmpty()) {
        steal(a);
        try {
          res->setValue(row - from, col, a);
        } catch (...) {
          a.destroy();
        }
        eraseValue(chosen[row], col);
      }
    }
  }

  return res;
}

/// @brief toJson, transfer all rows of this AqlItemBlock to Json, the result
/// can be used to recreate the AqlItemBlock via the Json constructor
void AqlItemBlock::toVelocyPack(velocypack::Options const* const trxOptions,
                                VPackBuilder& result) const {
  return toVelocyPack(0, size(), trxOptions, result);
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
void AqlItemBlock::toVelocyPack(size_t from, size_t to,
                                velocypack::Options const* const trxOptions,
                                VPackBuilder& result) const {
  // Can only have positive slice size
  TRI_ASSERT(from < to);
  // We cannot slice over the upper bound.
  // The lower bound (0) is protected by unsigned number type
  TRI_ASSERT(to <= _nrItems);

  TRI_ASSERT(result.isOpenObject());
  VPackOptions options(VPackOptions::Defaults);
  options.buildUnindexedArrays = true;
  options.buildUnindexedObjects = true;

  VPackBuilder raw(&options);
  raw.openArray();
  // Two nulls in the beginning such that indices start with 2
  raw.add(VPackValue(VPackValueType::Null));
  raw.add(VPackValue(VPackValueType::Null));

  result.add("nrItems", VPackValue(to - from));
  result.add("nrRegs", VPackValue(_nrRegs));
  result.add(StaticStrings::Error, VPackValue(false));

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

  RegisterId startRegister = 0;
  if (getFormatType() == SerializationFormat::CLASSIC) {
    // Skip over the shadowRows
    startRegister = 1;
  }
  for (RegisterId column = startRegister; column < internalNrRegs(); column++) {
    for (size_t i = from; i < to; i++) {
      AqlValue const& a(_data[i * internalNrRegs() + column]);

      // determine current state
      if (a.isEmpty()) {
        currentState = Empty;
      } else if (a.isRange()) {
        currentState = Range;
      } else {
        auto it = table.find(a);

        if (it == table.end()) {
          currentState = Next;
          a.toVelocyPack(trxOptions, raw, false);
          table.try_emplace(a, pos++);
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
      if (currentState != lastState ||
          (currentState == Positional && tablePos != lastTablePos)) {
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
  }

  // write out any remaining buffered data
  writeBuffered(lastState, lastTablePos, result, runLength);

  result.close();  // closes "data"

  raw.close();
  result.add("raw", raw.slice());
}

void AqlItemBlock::rowToSimpleVPack(size_t const row, velocypack::Options const* options,
                                    arangodb::velocypack::Builder& builder) const {
  VPackArrayBuilder rowBuilder{&builder};

  if (isShadowRow(row)) {
    getShadowRowDepth(row).toVelocyPack(options, *rowBuilder, false);
  } else {
    AqlValue{AqlValueHintNull{}}.toVelocyPack(options, *rowBuilder, false);
  }
  for (RegisterId reg = 0; reg < getNrRegs(); ++reg) {
    getValueReference(row, reg).toVelocyPack(options, *rowBuilder, false);
  }
}

void AqlItemBlock::toSimpleVPack(velocypack::Options const* options,
                                 arangodb::velocypack::Builder& builder) const {
  VPackObjectBuilder block{&builder};
  block->add("nrItems", VPackValue(size()));
  block->add("nrRegs", VPackValue(getNrRegs()));
  block->add(VPackValue("matrix"));
  {
    VPackArrayBuilder matrixBuilder{block.builder};
    for (size_t row = 0; row < size(); ++row) {
      rowToSimpleVPack(row, options, *matrixBuilder.builder);
    }
  }
}

ResourceMonitor& AqlItemBlock::resourceMonitor() noexcept {
  return *_manager.resourceMonitor();
}

void AqlItemBlock::copySubQueryDepthFromOtherBlock(size_t const targetRow,
                                                   AqlItemBlock const& source,
                                                   size_t const sourceRow) {
  if (source.isShadowRow(sourceRow)) {
    AqlValue const& d = source.getShadowRowDepth(sourceRow);
    // Value set, copy it over
    TRI_ASSERT(!d.requiresDestruction());
    setShadowRowDepth(targetRow, d);
  }
}

AqlItemBlock::~AqlItemBlock() {
  TRI_ASSERT(_refCount == 0);
  destroy();
  decreaseMemoryUsage(sizeof(AqlValue) * _nrItems * internalNrRegs());
}

void AqlItemBlock::increaseMemoryUsage(size_t value) {
  resourceMonitor().increaseMemoryUsage(value);
}

void AqlItemBlock::decreaseMemoryUsage(size_t value) noexcept {
  resourceMonitor().decreaseMemoryUsage(value);
}

AqlValue AqlItemBlock::getValue(size_t index, RegisterId varNr) const {
  return _data[getAddress(index, varNr)];
}

AqlValue const& AqlItemBlock::getValueReference(size_t index, RegisterId varNr) const {
  return _data[getAddress(index, varNr)];
}

void AqlItemBlock::setValue(size_t index, RegisterId varNr, AqlValue const& value) {
  TRI_ASSERT(_data[getAddress(index, varNr)].isEmpty());

  // First update the reference count, if this fails, the value is empty
  if (value.requiresDestruction()) {
    if (++_valueCount[value] == 1) {
      size_t mem = value.memoryUsage();
      increaseMemoryUsage(mem);
    }
  }

  _data[getAddress(index, varNr)] = value;
}

void AqlItemBlock::destroyValue(size_t index, RegisterId varNr) {
  auto& element = _data[getAddress(index, varNr)];

  if (element.requiresDestruction()) {
    auto it = _valueCount.find(element);

    if (it != _valueCount.end()) {
      if (--(it->second) == 0) {
        decreaseMemoryUsage(element.memoryUsage());
        _valueCount.erase(it);
        element.destroy();
        return;  // no need for an extra element.erase() in this case
      }
    }
  }

  element.erase();
}

void AqlItemBlock::eraseValue(size_t index, RegisterId varNr) {
  auto& element = _data[getAddress(index, varNr)];

  if (element.requiresDestruction()) {
    auto it = _valueCount.find(element);

    if (it != _valueCount.end()) {
      if (--(it->second) == 0) {
        decreaseMemoryUsage(element.memoryUsage());
        try {
          _valueCount.erase(it);
        } catch (...) {
        }
      }
    }
  }

  element.erase();
}

void AqlItemBlock::eraseAll() {
  for (size_t i = 0; i < numEntries(); i++) {
    auto& it = _data[i];
    if (!it.isEmpty()) {
      it.erase();
    }
  }

  for (auto const& it : _valueCount) {
    if (it.second > 0) {
      decreaseMemoryUsage(it.first.memoryUsage());
    }
  }
  _valueCount.clear();
}

void AqlItemBlock::copyValuesFromRow(size_t currentRow, RegisterId curRegs, size_t fromRow) {
  TRI_ASSERT(currentRow != fromRow);

  for (RegisterId i = 0; i < curRegs; i++) {
    auto currentAddress = getAddress(currentRow, i);
    auto fromAddress = getAddress(fromRow, i);
    if (_data[currentAddress].isEmpty()) {
      // First update the reference count, if this fails, the value is empty
      if (_data[fromAddress].requiresDestruction()) {
        ++_valueCount[_data[fromAddress]];
      }
      TRI_ASSERT(_data[currentAddress].isEmpty());
      _data[currentAddress] = _data[fromAddress];
    }
  }
  // Copy over subqueryDepth
  copySubqueryDepth(currentRow, fromRow);
}

void AqlItemBlock::copyValuesFromRow(size_t currentRow,
                                     std::unordered_set<RegisterId> const& regs,
                                     size_t fromRow) {
  TRI_ASSERT(currentRow != fromRow);

  for (auto const reg : regs) {
    TRI_ASSERT(reg < getNrRegs());
    if (getValueReference(currentRow, reg).isEmpty()) {
      // First update the reference count, if this fails, the value is empty
      if (getValueReference(fromRow, reg).requiresDestruction()) {
        ++_valueCount[getValueReference(fromRow, reg)];
      }
      _data[getAddress(currentRow, reg)] = getValueReference(fromRow, reg);
    }
  }
  // Copy over subqueryDepth
  copySubqueryDepth(currentRow, fromRow);
}

void AqlItemBlock::steal(AqlValue const& value) {
  if (value.requiresDestruction()) {
    if (_valueCount.erase(value)) {
      decreaseMemoryUsage(value.memoryUsage());
    }
  }
}

RegisterId AqlItemBlock::getNrRegs() const noexcept { return _nrRegs; }

size_t AqlItemBlock::size() const noexcept { return _nrItems; }

std::tuple<size_t, size_t> AqlItemBlock::getRelevantRange() {
  // NOTE:
  // Right now we can only support a range of datarows, that ends
  // In a range of ShadowRows.
  // After a shadow row, we do NOT know how to continue with
  // The next Executor.
  // So we can hardcode to return 0 -> firstShadowRow || endOfBlock
  if (hasShadowRows()) {
    auto const& shadows = getShadowRowIndexes();
    TRI_ASSERT(!shadows.empty());
    return {0, *shadows.begin()};
  }
  return {0, size()};
}

size_t AqlItemBlock::numEntries() const { return internalNrRegs() * _nrItems; }

size_t AqlItemBlock::capacity() const noexcept { return _data.capacity(); }

bool AqlItemBlock::isShadowRow(size_t row) const {
  /// This value is only filled for shadowRows.
  /// And it is guaranteed to be only filled by numbers this way.
  return _data[getSubqueryDepthAddress(row)].isNumber();
}

AqlValue const& AqlItemBlock::getShadowRowDepth(size_t row) const {
  TRI_ASSERT(isShadowRow(row));
  TRI_ASSERT(hasShadowRows());
  return _data[getSubqueryDepthAddress(row)];
}

void AqlItemBlock::setShadowRowDepth(size_t row, AqlValue const& other) {
  TRI_ASSERT(other.isNumber());
  _data[getSubqueryDepthAddress(row)] = other;
  TRI_ASSERT(isShadowRow(row));
  // Might be shadowRow before, but we do not care, set is unique
  _shadowRowIndexes.emplace(row);
}

void AqlItemBlock::makeShadowRow(size_t row) {
  TRI_ASSERT(!isShadowRow(row));
  _data[getSubqueryDepthAddress(row)] = AqlValue{VPackSlice::zeroSlice()};
  TRI_ASSERT(isShadowRow(row));
  _shadowRowIndexes.emplace(row);
}

void AqlItemBlock::makeDataRow(size_t row) {
  TRI_ASSERT(isShadowRow(row));
  _data[getSubqueryDepthAddress(row)] = AqlValue{VPackSlice::noneSlice()};
  TRI_ASSERT(!isShadowRow(row));
  _shadowRowIndexes.erase(row);
}

/// @brief Return the indexes of shadowRows within this block.
std::set<size_t> const& AqlItemBlock::getShadowRowIndexes() const noexcept {
  return _shadowRowIndexes;
}

/// @brief Quick test if we have any ShadowRows within this block;
bool AqlItemBlock::hasShadowRows() const noexcept {
  return !_shadowRowIndexes.empty();
}

AqlItemBlockManager& AqlItemBlock::aqlItemBlockManager() noexcept {
  return _manager;
}

size_t AqlItemBlock::getRefCount() const noexcept { return _refCount; }

void AqlItemBlock::incrRefCount() const noexcept { ++_refCount; }

void AqlItemBlock::decrRefCount() const noexcept {
  TRI_ASSERT(_refCount > 0);
  --_refCount;
}

RegisterCount AqlItemBlock::internalNrRegs() const noexcept {
  return _nrRegs + 1;
}
size_t AqlItemBlock::getAddress(size_t index, RegisterId varNr) const noexcept {
  TRI_ASSERT(index < _nrItems);
  TRI_ASSERT(varNr < _nrRegs);
  return index * internalNrRegs() + varNr + 1;
}

size_t AqlItemBlock::getSubqueryDepthAddress(size_t index) const noexcept {
  TRI_ASSERT(index < _nrItems);
  return index * internalNrRegs();
}

void AqlItemBlock::copySubqueryDepth(size_t currentRow, size_t fromRow) {
  auto currentAddress = getSubqueryDepthAddress(currentRow);
  auto fromAddress = getSubqueryDepthAddress(fromRow);
  if (!_data[fromAddress].isEmpty() && _data[currentAddress].isEmpty()) {
    _data[currentAddress] = _data[fromAddress];
  }
}

size_t AqlItemBlock::moveOtherBlockHere(size_t const targetRow, AqlItemBlock& source) {
  TRI_ASSERT(targetRow + source.size() <= this->size());
  TRI_ASSERT(getNrRegs() == source.getNrRegs());
  auto const n = source.size();
  auto const nrRegs = getNrRegs();

  size_t thisRow = targetRow;
  for (size_t sourceRow = 0; sourceRow < n; ++sourceRow, ++thisRow) {
    for (RegisterId col = 0; col < nrRegs; ++col) {
      // copy over value
      AqlValue const& a = source.getValueReference(sourceRow, col);
      if (!a.isEmpty()) {
        setValue(thisRow, col, a);
      }
    }
    copySubQueryDepthFromOtherBlock(thisRow, source, sourceRow);
  }
  source.eraseAll();

  TRI_ASSERT(thisRow == targetRow + n);

  return targetRow + n;
}
