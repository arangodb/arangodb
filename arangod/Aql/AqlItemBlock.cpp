////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionNode.h"
#include "Aql/Range.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SharedAqlItemBlockPtr.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Containers/HashSet.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <algorithm>
#include <limits>

using namespace arangodb;
using namespace arangodb::aql;

using VelocyPackHelper = arangodb::basics::VelocyPackHelper;

namespace {
inline void copyValueOver(arangodb::containers::HashSet<void*>& cache, 
                          AqlValue const& a,
                          size_t rowNumber, 
                          RegisterId::value_t col, 
                          SharedAqlItemBlockPtr& res) {
  if (a.requiresDestruction()) {
    auto it = cache.find(a.data());

    if (it == cache.end()) {
      AqlValue b = a.clone();
      try {
        res->setValue(rowNumber, col, b);
      } catch (...) {
        b.destroy();
        throw;
      }
      cache.emplace(b.data());
    } else {
      res->setValue(rowNumber, col, AqlValue(a, (*it)));
    }
  } else {
    res->setValue(rowNumber, col, a);
  }
}
}  // namespace

/// @brief create the block
AqlItemBlock::AqlItemBlock(AqlItemBlockManager& manager, size_t numRows, RegisterCount numRegisters)
    : _numRows(numRows),
      _numRegisters(numRegisters), 
      _maxModifiedRowIndex(0),
      _manager(manager), 
      _refCount(0), 
      _rowIndex(0), 
      _shadowRows(numRows) {
  TRI_ASSERT(numRows > 0);  // empty AqlItemBlocks are not allowed!
  // check that the _numRegisters value is somewhat sensible
  // this compare value is arbitrary, but having so many registers in a single
  // query seems unlikely
  TRI_ASSERT(_numRegisters <= RegisterId::maxRegisterId);
  increaseMemoryUsage(sizeof(AqlValue) * numEntries());
  try {
    _data.resize(numEntries());
  } catch (...) {
    decreaseMemoryUsage(sizeof(AqlValue) * numEntries());
    throw;
  }
}

/// @brief init the block from VelocyPack, note that this can throw
void AqlItemBlock::initFromSlice(VPackSlice const slice) {
  int64_t numRows = VelocyPackHelper::getNumericValue<int64_t>(slice, "nrItems", 0);
  if (numRows <= 0) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "nrItems must be > 0");
  }
  
  TRI_ASSERT(_shadowRows.empty());
  TRI_ASSERT(_maxModifiedRowIndex == 0);

  // will be increased by later setValue() calls
  _maxModifiedRowIndex = 0;

  rescale(static_cast<size_t>(numRows), VelocyPackHelper::getNumericValue<RegisterCount>(slice, "nrRegs", 0));

  // Now put in the data:
  VPackSlice data = slice.get("data");
  VPackSlice raw = slice.get("raw");

  VPackArrayIterator rawIterator(raw);

  std::vector<AqlValue> madeHere;
  madeHere.reserve(2 + static_cast<size_t>(rawIterator.size()));
  madeHere.emplace_back();  // an empty AqlValue
  madeHere.emplace_back();  // another empty AqlValue, indices start w. 2

  VPackArrayIterator dataIterator(data);

  auto storeSingleValue = [this](size_t row, RegisterId::value_t column, VPackArrayIterator& it,
                                 std::vector<AqlValue>& madeHere) {
    AqlValue a(it.value());
    it.next();
    try {
      if (column == 0) {
        if (!a.isEmpty()) {
          makeShadowRow(row, static_cast<size_t>(a.toInt64()));
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
    RegisterId::value_t startColumn = 0;
    if (getFormatType() == SerializationFormat::CLASSIC) {
      startColumn = 1;
    }
 
    // note that the end column is always _numRegisters + 1 here, as the first "real"
    // input column is 1. column 0 contains shadow row information in case the
    // serialization format is not CLASSIC.
    for (RegisterId::value_t column = startColumn; column < _numRegisters + 1; column++) {
      for (size_t i = 0; i < _numRows; i++) {
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
                  makeShadowRow(i, static_cast<size_t>(madeHere[tablePos].toInt64()));
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
                  makeShadowRow(i, static_cast<size_t>(madeHere[tablePos].toInt64()));
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
            if (!madeHere[static_cast<size_t>(n)].isEmpty()) {
              makeShadowRow(i, static_cast<size_t>(madeHere[static_cast<size_t>(n)].toInt64()));
            }
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
    } else {
      size_t totalUsed = 0;
      size_t const n = maxModifiedEntries();
      for (size_t i = 0; i < n; i++) {
        auto& it = _data[i];
        if (it.requiresDestruction()) {
          auto it2 = _valueCount.find(it.data());
          if (it2 != _valueCount.end()) {  // if we know it, we are still responsible
            auto& valueInfo = (*it2).second;
            TRI_ASSERT(valueInfo.refCount > 0);

            if (--valueInfo.refCount == 0) {
              totalUsed += valueInfo.memoryUsage;
              it.destroy(); 
              // destroy() calls erase, so no need to call erase() again later
              continue;
            }
          }
        }
        // Note that if we do not know it the thing it has been stolen from us!
        it.erase();
      }
      _valueCount.clear();
      decreaseMemoryUsage(totalUsed);
    }
  } catch (...) {
  }

  TRI_ASSERT(_valueCount.empty());

  rescale(0, 0);
  TRI_ASSERT(numEntries() == 0);
  TRI_ASSERT(maxModifiedRowIndex() == 0);
  TRI_ASSERT(_shadowRows.empty());
}

/// @brief shrink the block to the specified number of rows
/// the superfluous rows are cleaned
void AqlItemBlock::shrink(size_t numRows) {
  TRI_ASSERT(numRows > 0);

  if (numRows == _numRows) {
    // nothing to do
    return;
  }

  if (ADB_UNLIKELY(numRows > _numRows)) {
    // cannot use shrink() to increase the size of the block
    std::string errorMessage("cannot use shrink() to increase block");
    errorMessage.append(". numRows: ");
    errorMessage.append(std::to_string(numRows));
    errorMessage.append(". _numRows: ");
    errorMessage.append(std::to_string(_numRows));
    errorMessage.append(". _numRegisters: ");
    errorMessage.append(std::to_string(_numRegisters));
    errorMessage.append(". _maxModifiedRowIndex: ");
    errorMessage.append(std::to_string(_maxModifiedRowIndex));
    errorMessage.append(". _rowIndex: ");
    errorMessage.append(std::to_string(_rowIndex));
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, errorMessage);
  }

  decreaseMemoryUsage(sizeof(AqlValue) * (_numRows - numRows) * _numRegisters);

  // adjust the size of the block
  _numRows = numRows;

  // remove the shadow row indices pointing to now invalid rows.
  _shadowRows.resize(numRows);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto const [shadowRowsBegin, shadowRowsEnd] = _shadowRows.getIndexesFrom(_numRows);
  TRI_ASSERT(shadowRowsBegin == shadowRowsEnd);
#endif

  size_t totalUsed = 0;
  size_t const n = maxModifiedEntries();
  for (size_t i = numEntries(); i < n; ++i) {
    AqlValue& a = _data[i];
    if (a.requiresDestruction()) {
      auto it = _valueCount.find(a.data());

      if (it != _valueCount.end()) {
        auto& valueInfo = (*it).second;
        TRI_ASSERT(valueInfo.refCount > 0);

        if (--valueInfo.refCount == 0) {
          totalUsed += valueInfo.memoryUsage;
          // destroy calls erase() for AqlValues with dynamic memory,
          // no need for an extra a.erase() here
          a.destroy();
          _valueCount.erase(it);
          continue;  
        }
      }
    }
    a.erase();
  }
          
  _maxModifiedRowIndex = std::min<size_t>(_maxModifiedRowIndex, _numRows);
  TRI_ASSERT(_maxModifiedRowIndex <= _numRows);
  decreaseMemoryUsage(totalUsed);
}

void AqlItemBlock::rescale(size_t numRows, RegisterCount numRegisters) {
  _shadowRows.clear();
  _shadowRows.resize(numRows);
  TRI_ASSERT(_valueCount.empty());
  TRI_ASSERT(numRegisters <= RegisterId::maxRegisterId);

  size_t const targetSize = numRows * numRegisters;
  size_t const currentSize = numEntries();

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

  _numRows = numRows;
  _numRegisters = numRegisters;
  _maxModifiedRowIndex = std::min<size_t>(_maxModifiedRowIndex, _numRows);
  TRI_ASSERT(_maxModifiedRowIndex <= _numRows);
}

/// @brief clears out some columns (registers), this deletes the values if
/// necessary, using the reference count.
void AqlItemBlock::clearRegisters(RegIdFlatSet const& toClear) {
  bool const checkShadowRows = hasShadowRows();

  size_t totalUsed = 0;
  for (size_t i = 0; i < _maxModifiedRowIndex; i++) {
    if (checkShadowRows && isShadowRow(i)) {
      // Do not clear shadow rows:
      // 1) our toClear set is only valid for data rows
      // 2) there will never be anything to clear for shadow rows
      continue;
    }
    for (auto const& reg : toClear) {
      AqlValue& a(_data[getAddress(i, reg.value())]);

      if (a.requiresDestruction()) {
        auto it = _valueCount.find(a.data());

        if (it != _valueCount.end()) {
          auto& valueInfo = (*it).second;
          TRI_ASSERT(valueInfo.refCount > 0);

          if (--valueInfo.refCount == 0) {
            totalUsed += valueInfo.memoryUsage;
            // destroy calls erase() for AqlValues with dynamic memory,
            // no need for an extra a.erase() here
            a.destroy();
            _valueCount.erase(it);
            continue;  
          }
        }
      }
      a.erase();
    }
  }

  decreaseMemoryUsage(totalUsed);
}

SharedAqlItemBlockPtr AqlItemBlock::cloneDataAndMoveShadow() {
  auto const numRows = _numRows;
  auto const numRegs = _numRegisters;
  
  SharedAqlItemBlockPtr res{aqlItemBlockManager().requestBlock(numRows, numRegs)};

  // these structs are used to pass in type information into the following lambda
  struct WithoutShadowRows {};
  struct WithShadowRows {};
  
  auto const numModifiedRows = _maxModifiedRowIndex;

  auto copyRows = [&](arangodb::containers::HashSet<void*>& cache, auto type) {
    constexpr bool checkShadowRows = std::is_same<decltype(type), WithShadowRows>::value;
    cache.reserve(_valueCount.size());

    for (size_t row = 0; row < numModifiedRows; row++) {
      if (checkShadowRows && isShadowRow(row)) {
        // this is a shadow row. needs special handling
        for (RegisterId::value_t col = 0; col < numRegs; col++) {
          AqlValue a = stealAndEraseValue(row, col);
          if (a.requiresDestruction()) {
            AqlValueGuard guard{a, true};
            auto [it, inserted] = cache.emplace(a.data());
            res->setValue(row, col, AqlValue(a, (*it)));
            if (inserted) {
              // otherwise, destroy this; we used a cached value.
              guard.steal();
            }
          } else {
            res->setValue(row, col, a);
          }
        }
 
        // make it a shadow row
        res->copySubqueryDepthFromOtherBlock(row, *this, row, true);
      } else {
        // this is a normal row. 
        for (RegisterId::value_t col = 0; col < numRegs; col++) {
          ::copyValueOver(cache, _data[getAddress(row, col)], row, col, res);
        }
      }
    }
  };

  arangodb::containers::HashSet<void*> cache;

  if (hasShadowRows()) {
    // optimized version for when no shadow rows exist
    copyRows(cache, WithShadowRows{});
  } else {
    // at least one shadow row exists. this is the slow path
    copyRows(cache, WithoutShadowRows{});
  }
  TRI_ASSERT(res->numRows() == numRows);

  return res;
}

/// @brief slice/clone, this does a deep copy of all entries
SharedAqlItemBlockPtr AqlItemBlock::slice(size_t from, size_t to) const {
  TRI_ASSERT(from < to);
  TRI_ASSERT(to <= _numRows);

  arangodb::containers::SmallVector<std::pair<size_t, size_t>>::allocator_type::arena_type arena;
  arangodb::containers::SmallVector<std::pair<size_t, size_t>> ranges{arena};
  ranges.emplace_back(from, to);
  return slice(ranges);
}

/**
 * @brief Slice multiple ranges out of this AqlItemBlock.
 *        This does a deep copy of all entries
 *
 * @param ranges list of ranges from(included) -> to(excluded)
 *        Every range needs to be valid from[i] < to[i]
 *        And every range needs to be within the block to[i] <= numRows()
 *        The list is required to be ordered to[i] <= from[i+1]
 *
 * @return SharedAqlItemBlockPtr A block where all the slices are contained in the order of the list
 */
auto AqlItemBlock::slice(arangodb::containers::SmallVector<std::pair<size_t, size_t>> const& ranges) const
    -> SharedAqlItemBlockPtr {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // Analyze correctness of ranges
  TRI_ASSERT(!ranges.empty());
  for (size_t i = 0; i < ranges.size(); ++i) {
    auto const& [from, to] = ranges[i];
    // Range is valid
    TRI_ASSERT(from < to);
    TRI_ASSERT(to <= _numRows);
    if (i > 0) {
      // List is ordered
      TRI_ASSERT(ranges[i - 1].second <= from);
    }
  }
#endif
  size_t numRows = 0;
  for (auto const& [from, to] : ranges) {
    numRows += to - from;
  }

  arangodb::containers::HashSet<void*> cache;
  cache.reserve(numRows * _numRegisters / 4 + 1);

  SharedAqlItemBlockPtr res{_manager.requestBlock(numRows, _numRegisters)};
  size_t targetRow = 0;
  for (auto const& [from, to] : ranges) {
    size_t const modifiedTo = std::min<size_t>(to, _maxModifiedRowIndex);

    for (size_t row = from; row < modifiedTo; row++, targetRow++) {
      for (RegisterId::value_t col = 0; col < _numRegisters; col++) {
        ::copyValueOver(cache, _data[getAddress(row, col)], targetRow, col, res);
      }
      res->copySubqueryDepthFromOtherBlock(targetRow, *this, row, false);
    }
  }

  TRI_ASSERT(res->numRows() == numRows);

  return res;
}

/// @brief slice/clone, this does a deep copy of all entries
SharedAqlItemBlockPtr AqlItemBlock::slice(size_t row,
                                          std::unordered_set<RegisterId> const& registers,
                                          RegisterCount newNumRegisters) const {
  TRI_ASSERT(_numRegisters <= newNumRegisters);

  arangodb::containers::HashSet<void*> cache;
  SharedAqlItemBlockPtr res{_manager.requestBlock(1, newNumRegisters)};
  for (auto const& col : registers) {
    TRI_ASSERT(col.isRegularRegister() && col < _numRegisters);
    ::copyValueOver(cache, _data[getAddress(row, col.value())], 0, col.value(), res);
  }
  res->copySubqueryDepthFromOtherBlock(0, *this, row, false);

  return res;
}

/// @brief slice/clone chosen rows for a subset, this does a deep copy
/// of all entries
SharedAqlItemBlockPtr AqlItemBlock::slice(std::vector<size_t> const& chosen,
                                          size_t from, size_t to) const {
  TRI_ASSERT(from < to && to <= chosen.size());

  arangodb::containers::HashSet<void*> cache;
  cache.reserve((to - from) * _numRegisters / 4 + 1);

  SharedAqlItemBlockPtr res{_manager.requestBlock(to - from, _numRegisters)};

  size_t const modifiedTo = std::min<size_t>(to, _maxModifiedRowIndex);

  size_t resultRow = 0;
  for (size_t chosenIdx = from; chosenIdx < modifiedTo; ++chosenIdx, ++resultRow) {
    size_t const row = chosen[chosenIdx];

    for (RegisterId::value_t col = 0; col < _numRegisters; col++) {
      ::copyValueOver(cache, _data[getAddress(row, col)], resultRow, col, res);
    }
    res->copySubqueryDepthFromOtherBlock(resultRow, *this, row, false);
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

  SharedAqlItemBlockPtr res{_manager.requestBlock(to - from, _numRegisters)};
  
  to = std::min<size_t>(to, _maxModifiedRowIndex);

  for (size_t row = from; row < to; row++) {
    for (RegisterId::value_t col = 0; col < _numRegisters; col++) {
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
  return toVelocyPack(0, numRows(), trxOptions, result);
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
  TRI_ASSERT(to <= _numRows);

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
  result.add("nrRegs", VPackValue(_numRegisters));
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

  // hack hack hack: we use a fake register to serialize shadow rows
  RegisterId::value_t const subqueryDepthColumn =
      std::numeric_limits<RegisterId::value_t>::max();
  RegisterId::value_t startColumn = subqueryDepthColumn;
  if (getFormatType() == SerializationFormat::CLASSIC) {
    // skip shadow rows
    startColumn = 0;
  }

  // we start the serializaton at the fake register, which may then overflow to 0...
  for (RegisterId::value_t column = startColumn;
       column < _numRegisters || column == subqueryDepthColumn; column++) {
    AqlValue a;

    for (size_t i = from; i < to; i++) {
      if (column == subqueryDepthColumn) {
        // serialize shadow row depth
        if (_shadowRows.is(i)) {
          // shadow row
          a = AqlValue(AqlValueHintUInt(_shadowRows.getDepth(i)));
        } else {
          // no shadow row
          a = AqlValue(AqlValueHintNone());
        }
      } else {
        // regular value 
        TRI_ASSERT(column < _numRegisters);
        a = getValueReference(i, column);
      }

      // determine current state
      if (a.isEmpty()) {
        currentState = Empty;
      } else if (a.isRange()) {
        currentState = Range;
      } else {
        auto it = table.find(a);

        if (it == table.end()) {
          currentState = Next;
          a.toVelocyPack(trxOptions, raw, /*resolveExternals*/false,
                         /*allowUnindexed*/true);
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
  {
    VPackArrayBuilder rowBuilder{&builder};

    if (isShadowRow(row)) {
      builder.add(VPackValue(getShadowRowDepth(row)));
    } else {
      builder.add(VPackSlice::nullSlice());
    }
    auto const n = numRegisters();
    for (RegisterId::value_t reg = 0; reg < n; ++reg) {
      AqlValue const& ref = getValueReference(row, reg);
      if (ref.isEmpty()) {
        builder.add(VPackSlice::noneSlice());
      } else {
        ref.toVelocyPack(options, builder, /*resolveExternals*/false,
                         /*allowUnindexed*/true);
      }
    }
  }
}

void AqlItemBlock::toSimpleVPack(velocypack::Options const* options,
                                 arangodb::velocypack::Builder& builder) const {
  VPackObjectBuilder block{&builder};
  block->add("nrItems", VPackValue(numRows()));
  block->add("nrRegs", VPackValue(numRegisters()));
  block->add(VPackValue("matrix"));
  {
    size_t const n = numRows();
    VPackArrayBuilder matrixBuilder{&builder};
    for (size_t row = 0; row < n; ++row) {
      rowToSimpleVPack(row, options, builder);
    }
  }
}

arangodb::ResourceMonitor& AqlItemBlock::resourceMonitor() noexcept {
  return _manager.resourceMonitor();
}

void AqlItemBlock::copySubqueryDepthFromOtherBlock(size_t targetRow,
                                                   AqlItemBlock const& source,
                                                   size_t sourceRow,
                                                   bool forceShadowRow) {
  TRI_ASSERT(!forceShadowRow || source.isShadowRow(sourceRow));

  if (forceShadowRow || source.isShadowRow(sourceRow)) {
    makeShadowRow(targetRow, source.getShadowRowDepth(sourceRow));
  }
}

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
// MaintainerMode method to validate if ShadowRows organization are consistent.
// e.g. If a block always follows this pattern:
// ((Data* Shadow(0))* Shadow(1))* ...
void AqlItemBlock::validateShadowRowConsistency() const {
  auto [it, shadowRowsEnd] = getShadowRowIndexesFrom(0);
  while (it != shadowRowsEnd) {
    auto r = (*it);
    TRI_ASSERT(r < numRows());
    // The block can start with any ShadowRow
    // of any depth, we do not know the history.
    if (r > 0) {
      size_t depth = getShadowRowDepth(r);
      // depth 0 is always allowed, we can start a new subquery with
      // no data.
      if (depth > 0) {
        // Row before needs to be a ShadowRow
        TRI_ASSERT(isShadowRow(r - 1));

        size_t compDepth = static_cast<uint64_t>(getShadowRowDepth(r - 1));
        // The row before can only be one less then d (result of subquery)
        // Or greater or equal to d. (The subquery either has no result or is skipped)
        TRI_ASSERT(depth - 1 <= compDepth);
      }
    }
    ++it;
  }
}
#endif


AqlItemBlock::~AqlItemBlock() {
  TRI_ASSERT(_refCount == 0);
  destroy();
  decreaseMemoryUsage(sizeof(AqlValue) * _numRows * _numRegisters);
}

void AqlItemBlock::increaseMemoryUsage(size_t value) {
  resourceMonitor().increaseMemoryUsage(value);
}

void AqlItemBlock::decreaseMemoryUsage(size_t value) noexcept {
  resourceMonitor().decreaseMemoryUsage(value);
}

AqlValue AqlItemBlock::getValue(size_t index, RegisterId varNr) const {
  if (varNr.isConstRegister()) {
    TRI_ASSERT(_manager.getConstValueBlock() != nullptr);
    return _manager.getConstValueBlock()->getValue(0, varNr.value());
  }
  return getValue(index, varNr.value());
}

AqlValue AqlItemBlock::getValue(size_t index, RegisterId::value_t column) const {
  return getValueReference(index, column);
}

AqlValue const& AqlItemBlock::getValueReference(size_t index, RegisterId varNr) const {
  if (varNr.isConstRegister()) {
    TRI_ASSERT(_manager.getConstValueBlock() != nullptr);
    return _manager.getConstValueBlock()->getValueReference(0, varNr.value());
  }
  return getValueReference(index, varNr.value());
}

AqlValue const& AqlItemBlock::getValueReference(size_t index, RegisterId::value_t column) const {
  return _data[getAddress(index, column)];
}

void AqlItemBlock::setValue(size_t index, RegisterId varNr, AqlValue const& value) {
  TRI_ASSERT(varNr.isRegularRegister());
  setValue(index, varNr.value(), value);
}

void AqlItemBlock::setValue(size_t index, RegisterId::value_t column, AqlValue const& value) {
  TRI_ASSERT(_data[getAddress(index, column)].isEmpty());

  // First update the reference count, if this fails, the value is empty
  if (value.requiresDestruction()) {
    // note: this may create a new entry in _valueCount, which is fine
    auto& valueInfo = _valueCount[value.data()];
    if (++valueInfo.refCount == 1) {
      // we just inserted the item
      size_t memoryUsage = value.memoryUsage();
      increaseMemoryUsage(memoryUsage);
      valueInfo.setMemoryUsage(memoryUsage);
    }
  }

  _data[getAddress(index, column)] = value;
  _maxModifiedRowIndex = std::max<size_t>(_maxModifiedRowIndex, index + 1);
}

void AqlItemBlock::destroyValue(size_t index, RegisterId varNr) {
  TRI_ASSERT(varNr.isRegularRegister());
  destroyValue(index, varNr.value());
}

void AqlItemBlock::destroyValue(size_t index, RegisterId::value_t column) {
  auto& element = _data[getAddress(index, column)];

  if (element.requiresDestruction()) {
    auto it = _valueCount.find(element.data());

    if (it != _valueCount.end()) {
      auto& valueInfo = (*it).second;
      if (--valueInfo.refCount == 0) {
        decreaseMemoryUsage(valueInfo.memoryUsage);
        _valueCount.erase(it);
        element.destroy();
        // no need for an extra element.erase() in this case, as
        // destroy() calls erase() for AqlValues with dynamic memory
        return;  
      }
    }
  }

  element.erase();
}

void AqlItemBlock::eraseValue(size_t index, RegisterId varNr) {
  TRI_ASSERT(varNr.isRegularRegister());
  auto& element = _data[getAddress(index, varNr.value())];

  if (element.requiresDestruction()) {
    auto it = _valueCount.find(element.data());

    if (it != _valueCount.end()) {
      auto& valueInfo = (*it).second;
      if (--valueInfo.refCount == 0) {
        decreaseMemoryUsage(valueInfo.memoryUsage);
        _valueCount.erase(it);
      }
    }
  }

  element.erase();
}

void AqlItemBlock::eraseAll() {
  size_t const n = maxModifiedEntries();
  for (size_t i = 0; i < n; i++) {
    auto& it = _data[i];
    it.erase();
  }

  size_t totalUsed = 0;
  for (auto const& it : _valueCount) {
    if (ADB_LIKELY(it.second.refCount > 0)) {
      totalUsed += it.second.memoryUsage;
    }
  }
  decreaseMemoryUsage(totalUsed);
  _valueCount.clear();
  _shadowRows.clear();
}

void AqlItemBlock::referenceValuesFromRow(size_t currentRow,
                                          RegIdFlatSet const& regs, size_t fromRow) {
  TRI_ASSERT(currentRow != fromRow);

  for (auto const& reg : regs) {
    TRI_ASSERT(reg < numRegisters());
    if (getValueReference(currentRow, reg).isEmpty()) {
      // First update the reference count, if this fails, the value is empty
      AqlValue const& a = getValueReference(fromRow, reg);
      if (a.requiresDestruction()) {
        TRI_ASSERT(_valueCount.find(a.data()) != _valueCount.end());
        ++_valueCount[a.data()].refCount;
      }
      _data[getAddress(currentRow, reg.value())] = a;
      _maxModifiedRowIndex = std::max<size_t>(_maxModifiedRowIndex, currentRow + 1);
    }
  }
  // Copy over subqueryDepth
  copySubqueryDepth(currentRow, fromRow);
}

void AqlItemBlock::steal(AqlValue const& value) {
  if (value.requiresDestruction()) {
    auto it = _valueCount.find(value.data());
    if (it != _valueCount.end()) {
      decreaseMemoryUsage((*it).second.memoryUsage);
      _valueCount.erase(it);
    }
  }
}

AqlValue AqlItemBlock::stealAndEraseValue(size_t index, RegisterId varNr) {
  TRI_ASSERT(varNr.isRegularRegister());
  auto& element = _data[getAddress(index, varNr.value())];

  auto value = element;

  steal(element);

  element.erase();

  return value;
}

RegisterCount AqlItemBlock::numRegisters() const noexcept { return _numRegisters; }

size_t AqlItemBlock::numRows() const noexcept { return _numRows; }
size_t AqlItemBlock::maxModifiedRowIndex() const noexcept { return _maxModifiedRowIndex; }

std::tuple<size_t, size_t> AqlItemBlock::getRelevantRange() const {
  // NOTE:
  // Right now we can only support a range of datarows, that ends
  // In a range of ShadowRows.
  // After a shadow row, we do NOT know how to continue with
  // The next Executor.
  // So we can hardcode to return 0 -> firstShadowRow || endOfBlock
  if (hasShadowRows()) {
    TRI_ASSERT(!_shadowRows.empty());
    auto [shadowRowsBegin, shadowRowsEnd] = getShadowRowIndexesFrom(0);
    return {0, *shadowRowsBegin};
  }
  return {0, numRows()};
}

size_t AqlItemBlock::numEntries() const noexcept { return _numRegisters * _numRows; }

size_t AqlItemBlock::maxModifiedEntries() const noexcept { return _numRegisters * _maxModifiedRowIndex; }

size_t AqlItemBlock::capacity() const noexcept { return _data.capacity(); }

bool AqlItemBlock::isShadowRow(size_t row) const {
  return _shadowRows.is(row);
}

size_t AqlItemBlock::getShadowRowDepth(size_t row) const {
  TRI_ASSERT(isShadowRow(row));
  TRI_ASSERT(hasShadowRows());
  return _shadowRows.getDepth(row);
}

void AqlItemBlock::makeShadowRow(size_t row, size_t depth) {
  _maxModifiedRowIndex = std::max<size_t>(_maxModifiedRowIndex, row + 1);
  _shadowRows.make(row, depth);
  TRI_ASSERT(isShadowRow(row));
}

void AqlItemBlock::makeDataRow(size_t row) {
  TRI_ASSERT(isShadowRow(row));
  _shadowRows.clear(row);
  TRI_ASSERT(!isShadowRow(row));
}

/// @brief Return the indexes of ShadowRows within this block
std::pair<AqlItemBlock::ShadowRowIterator, AqlItemBlock::ShadowRowIterator> AqlItemBlock::getShadowRowIndexesFrom(size_t lower) const noexcept {
  return _shadowRows.getIndexesFrom(lower);
}

/// @brief Quick test if we have any ShadowRows within this block
bool AqlItemBlock::hasShadowRows() const noexcept {
  return !_shadowRows.empty();
}

/// @brief Returns the number of ShadowRows
size_t AqlItemBlock::numShadowRows() const noexcept {
  return _shadowRows.size();
}

AqlItemBlockManager& AqlItemBlock::aqlItemBlockManager() noexcept {
  return _manager;
}

size_t AqlItemBlock::getRefCount() const noexcept { return _refCount; }

void AqlItemBlock::incrRefCount() const noexcept { ++_refCount; }

size_t AqlItemBlock::decrRefCount() const noexcept {
  TRI_ASSERT(_refCount > 0);
  return --_refCount;
}

size_t AqlItemBlock::getAddress(size_t index, RegisterId::value_t reg) const noexcept {
  TRI_ASSERT(index < _numRows);
  TRI_ASSERT(reg < _numRegisters);
  return index * _numRegisters + reg;
}

void AqlItemBlock::copySubqueryDepth(size_t currentRow, size_t fromRow) {
  if (_shadowRows.is(fromRow) && !_shadowRows.is(currentRow)) {
    _shadowRows.make(currentRow, _shadowRows.getDepth(fromRow));
  }
}

size_t AqlItemBlock::moveOtherBlockHere(size_t const targetRow, AqlItemBlock& source) {
  TRI_ASSERT(targetRow + source.numRows() <= this->numRows());
  TRI_ASSERT(numRegisters() == source.numRegisters());
  auto const numRows = source.numRows();
  auto const nrRegs = numRegisters();
  
  auto const modifiedRows = source._maxModifiedRowIndex;

  size_t thisRow = targetRow;
  for (size_t sourceRow = 0; sourceRow < modifiedRows; ++sourceRow, ++thisRow) {
    for (RegisterId::value_t col = 0; col < nrRegs; ++col) {
      // copy over value
      AqlValue const& a = source.getValueReference(sourceRow, col);
      if (!a.isEmpty()) {
        setValue(thisRow, col, a);
      }
    }
    copySubqueryDepthFromOtherBlock(thisRow, source, sourceRow, false);
    _maxModifiedRowIndex = std::max<size_t>(_maxModifiedRowIndex, thisRow + 1);
  }
  source.eraseAll();

  TRI_ASSERT(thisRow == targetRow + modifiedRows);

  return targetRow + numRows;
}

AqlItemBlock::ShadowRows::ShadowRows(size_t numRows)
    : _numRows(numRows) {}

bool AqlItemBlock::ShadowRows::empty() const noexcept {
  return _indexes.empty();
}

size_t AqlItemBlock::ShadowRows::size() const noexcept {
  return _indexes.size();
}

bool AqlItemBlock::ShadowRows::is(size_t row) const noexcept {
  return _depths.size() > row && _depths[row] > 0;
}

size_t AqlItemBlock::ShadowRows::getDepth(size_t row) const noexcept {
  TRI_ASSERT(_depths.size() > row);
  if (ADB_UNLIKELY(_depths.size() <= row)) {
    // should not happen
    return 0;
  }
  decltype(_depths)::value_type depth = _depths[row];
  TRI_ASSERT(depth > 0);
  return static_cast<size_t>(depth - 1);
}

/// @brief Return the indexes of shadowRows within this block.
std::pair<AqlItemBlock::ShadowRowIterator, AqlItemBlock::ShadowRowIterator> AqlItemBlock::ShadowRows::getIndexesFrom(size_t lower) const noexcept {
  if (lower == 0 || _indexes.empty()) {
    return {_indexes.begin(), _indexes.end()};
  }
  return {std::lower_bound(_indexes.begin(), _indexes.end(), lower), _indexes.end()};
}

void AqlItemBlock::ShadowRows::clear() noexcept {
  _indexes.clear();
  _depths.clear();
}

void AqlItemBlock::ShadowRows::resize(size_t numRows) {
  _numRows = numRows;
  clearFrom(_numRows);
  if (_depths.size() > _numRows) {
    _depths.resize(_numRows);
  }
}

void AqlItemBlock::ShadowRows::make(size_t row, size_t depth) {
  TRI_ASSERT(row < _numRows);
  if (ADB_UNLIKELY(depth >= std::numeric_limits<decltype(_depths)::value_type>::max() - 1U)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "invalid subquery depth");
  }
  if (ADB_UNLIKELY(row > std::numeric_limits<decltype(_indexes)::value_type>::max())) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "invalid subquery row");
  }

  if (_depths.size() <= row) {
    // allocate enough space for all rows at once. the goal here
    // is to minmize the number of allocations.
    _depths.resize(_numRows);
  }
  TRI_ASSERT(row < _depths.size());
  _depths[row] = static_cast<decltype(_depths)::value_type>(depth + 1); 

  // shadow row indexes _must_ be sequentially ordered!
  TRI_ASSERT(_indexes.empty() || _indexes.back() <= row);
  if (_indexes.empty()) {
    // allocate some space for a few elements
    _indexes.reserve(std::min<size_t>(_numRows, 16));
  } else if (_indexes.back() == row) {
    // don't insert the same value twice...
    return;
  }
  _indexes.push_back(static_cast<decltype(_indexes)::value_type>(row));
}

void AqlItemBlock::ShadowRows::clear(size_t row) {
  if (_depths.size() > row) {
    _depths[row] = 0;
  }

  if (!_indexes.empty()) {
    TRI_ASSERT(_indexes.back() == row);
    _indexes.pop_back();
  }
}
  
void AqlItemBlock::ShadowRows::clearFrom(size_t lower) {
  if (_depths.size() > lower) {
    std::fill(_depths.begin() + lower, _depths.end(), 0);
  }
  _indexes.erase(std::lower_bound(_indexes.begin(), _indexes.end(), lower), _indexes.end());
}
