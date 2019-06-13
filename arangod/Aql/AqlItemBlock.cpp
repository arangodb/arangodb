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
#include "Aql/BlockCollector.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionNode.h"
#include "Aql/SharedAqlItemBlockPtr.h"
#include "Basics/VelocyPackHelper.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

using VelocyPackHelper = arangodb::basics::VelocyPackHelper;

/// @brief create the block
AqlItemBlock::AqlItemBlock(AqlItemBlockManager& manager, size_t nrItems, RegisterId nrRegs)
    : _nrItems(nrItems), _nrRegs(nrRegs), _manager(manager), _refCount(0) {
  TRI_ASSERT(nrItems > 0);  // empty AqlItemBlocks are not allowed!

  if (nrRegs > 0) {
    // check that the nrRegs value is somewhat sensible
    // this compare value is arbitrary, but having so many registers in a single
    // query seems unlikely
    TRI_ASSERT(nrRegs <= ExecutionNode::MaxRegisterId);

    increaseMemoryUsage(sizeof(AqlValue) * nrItems * nrRegs);
    try {
      _data.resize(nrItems * nrRegs);
    } catch (...) {
      decreaseMemoryUsage(sizeof(AqlValue) * nrItems * nrRegs);
      throw;
    }
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
  if (_nrRegs > 0) {
    increaseMemoryUsage(sizeof(AqlValue) * _nrItems * _nrRegs);
    try {
      _data.resize(_nrItems * _nrRegs);
    } catch (...) {
      decreaseMemoryUsage(sizeof(AqlValue) * _nrItems * _nrRegs);
      throw;
    }
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
      setValue(row, column, a);  // if this throws, a is destroyed again
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

    for (RegisterId column = 0; column < _nrRegs; column++) {
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
              setValue(i, column, madeHere[tablePos]);
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

              setValue(i, column, madeHere[tablePos]);
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
          emplaceValue(i, column, low, high);
        } else if (n >= 2) {
          if (static_cast<size_t>(n) >= madeHere.size()) {
            // safeguard against out-of-bounds accesses
            THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                           "found undefined data value");
          }

          setValue(i, column, madeHere[static_cast<size_t>(n)]);
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
      auto &it = _data[i];
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
  
  for (size_t i = _nrItems * _nrRegs; i < _data.size(); ++i) {
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
  TRI_ASSERT(nrRegs <= ExecutionNode::MaxRegisterId);

  size_t const targetSize = nrItems * nrRegs;
  size_t const currentSize = _nrItems * _nrRegs;
  TRI_ASSERT(currentSize == numEntries());

  // TODO Previously, _data.size() was used for the memory usage.
  // _data.capacity() might have been more accurate. Now, _data.size() stays at
  // _data.capacity(), or at least, is never reduced.
  // So I decided for now to report the memory usage based on numEntries() only,
  // to mimic the previous behaviour. I'm not sure whether it should stay this
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
    for(size_t i = currentSize; i < targetSize; i++) {
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
      AqlValue& a(_data[_nrRegs * i + reg]);

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
  TRI_ASSERT(from < to && to <= _nrItems);

  std::unordered_set<AqlValue> cache;
  cache.reserve((to - from) * _nrRegs / 4 + 1);

  SharedAqlItemBlockPtr res{_manager.requestBlock(to - from, _nrRegs)};

  for (size_t row = from; row < to; row++) {
    for (RegisterId col = 0; col < _nrRegs; col++) {
      AqlValue const& a(_data[row * _nrRegs + col]);

      if (!a.isEmpty()) {
        if (a.requiresDestruction()) {
          auto it = cache.find(a);

          if (it == cache.end()) {
            AqlValue b = a.clone();
            try {
              res->setValue(row - from, col, b);
            } catch (...) {
              b.destroy();
              throw;
            }
            cache.emplace(b);
          } else {
            res->setValue(row - from, col, (*it));
          }
        } else {
          // simple copying of values
          res->setValue(row - from, col, a);
        }
      }
    }
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

    AqlValue const& a(_data[row * _nrRegs + col]);

    if (!a.isEmpty()) {
      if (a.requiresDestruction()) {
        auto it = cache.find(a);

        if (it == cache.end()) {
          AqlValue b = a.clone();
          try {
            res->setValue(0, col, b);
          } catch (...) {
            b.destroy();
            throw;
          }
          cache.emplace(b);
        } else {
          res->setValue(0, col, (*it));
        }
      } else {
        res->setValue(0, col, a);
      }
    }
  }

  return res;
}

/// @brief slice/clone chosen rows for a subset, this does a deep copy
/// of all entries
SharedAqlItemBlockPtr AqlItemBlock::slice(std::vector<size_t> const& chosen,
                                          size_t from, size_t to) const {
  TRI_ASSERT(from < to && to <= chosen.size());

  std::unordered_set<AqlValue> cache;
  cache.reserve((to - from) * _nrRegs / 4 + 1);

  SharedAqlItemBlockPtr res{_manager.requestBlock(to - from, _nrRegs)};

  for (size_t row = from; row < to; row++) {
    for (RegisterId col = 0; col < _nrRegs; col++) {
      AqlValue const& a(_data[chosen[row] * _nrRegs + col]);

      if (!a.isEmpty()) {
        if (a.requiresDestruction()) {
          auto it = cache.find(a);

          if (it == cache.end()) {
            AqlValue b = a.clone();
            try {
              res->setValue(row - from, col, b);
            } catch (...) {
              b.destroy();
            }
            cache.emplace(b);
          } else {
            res->setValue(row - from, col, (*it));
          }
        } else {
          res->setValue(row - from, col, a);
        }
      }
    }
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
      AqlValue& a(_data[chosen[row] * _nrRegs + col]);

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
void AqlItemBlock::toVelocyPack(transaction::Methods* trx, VPackBuilder& result) const {
  VPackOptions options(VPackOptions::Defaults);
  options.buildUnindexedArrays = true;
  options.buildUnindexedObjects = true;

  VPackBuilder raw(&options);
  raw.openArray();
  // Two nulls in the beginning such that indices start with 2
  raw.add(VPackValue(VPackValueType::Null));
  raw.add(VPackValue(VPackValueType::Null));

  result.add("nrItems", VPackValue(_nrItems));
  result.add("nrRegs", VPackValue(_nrRegs));
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
  for (RegisterId column = 0; column < _nrRegs; column++) {
    for (size_t i = 0; i < _nrItems; i++) {
      AqlValue const& a(_data[i * _nrRegs + column]);

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

ResourceMonitor& AqlItemBlock::resourceMonitor() noexcept {
  return *_manager.resourceMonitor();
}
