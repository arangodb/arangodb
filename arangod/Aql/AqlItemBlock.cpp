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
#include "Aql/ExecutionNode.h"
#include "Basics/VelocyPackHelper.h"

using namespace arangodb::aql;

using VelocyPackHelper = arangodb::basics::VelocyPackHelper;

/// @brief create the block
AqlItemBlock::AqlItemBlock(size_t nrItems, RegisterId nrRegs)
    : _nrItems(nrItems), _nrRegs(nrRegs) {
  TRI_ASSERT(nrItems > 0);  // empty AqlItemBlocks are not allowed!

  if (nrRegs > 0) {
    // check that the nrRegs value is somewhat sensible
    // this compare value is arbitrary, but having so many registers in a single
    // query seems unlikely
    TRI_ASSERT(nrRegs <= ExecutionNode::MaxRegisterId);

    _data.resize(nrItems * nrRegs);
  }
}

/// @brief create the block from VelocyPack, note that this can throw
AqlItemBlock::AqlItemBlock(VPackSlice const slice) {
  bool exhausted = VelocyPackHelper::getBooleanValue(slice, "exhausted", false);

  if (exhausted) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "exhausted must be false");
  }

  _nrItems = VelocyPackHelper::getNumericValue<size_t>(slice, "nrItems", 0);
  if (_nrItems == 0) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "nrItems must be > 0");
  }

  _nrRegs = VelocyPackHelper::getNumericValue<RegisterId>(slice, "nrRegs", 0);

  // Initialize the data vector:
  if (_nrRegs > 0) {
    _data.resize(_nrItems * _nrRegs);
  }

  // Now put in the data:
  VPackSlice data = slice.get("data");
  VPackSlice raw = slice.get("raw");

  std::vector<AqlValue> madeHere;
  madeHere.reserve(static_cast<size_t>(raw.length()));
  madeHere.emplace_back();  // an empty AqlValue
  madeHere.emplace_back();  // another empty AqlValue, indices start w. 2

  try {
    size_t posInRaw = 2;
    size_t posInData = 0;
    int64_t emptyRun = 0;

    for (RegisterId column = 0; column < _nrRegs; column++) {
      for (size_t i = 0; i < _nrItems; i++) {
        if (emptyRun > 0) {
          emptyRun--;
        } else {
          VPackSlice dataEntry = data.at(posInData++);
          if (!dataEntry.isNumber()) {
            THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                           "data must contain only numbers");
          }
          int64_t n = dataEntry.getNumericValue<int64_t>();
          if (n == 0) {
            // empty, do nothing here
          } else if (n == -1) {
            // empty run:
            VPackSlice runLength = data.at(posInData++);
            TRI_ASSERT(runLength.isNumber());
            emptyRun = runLength.getNumericValue<int64_t>();
            emptyRun--;
          } else if (n == -2) {
            // a range
            VPackSlice lowBound = data.at(posInData++);
            VPackSlice highBound = data.at(posInData++);
            
            int64_t low =
                VelocyPackHelper::getNumericValue<int64_t>(lowBound, 0);
            int64_t high =
                VelocyPackHelper::getNumericValue<int64_t>(highBound, 0);
            AqlValue a(low, high);
            try {
              setValue(i, column, a);
            } catch (...) {
              a.destroy();
              throw;
            }
          } else if (n == 1) {
            // a VelocyPack value
            AqlValue a(raw.at(posInRaw++));
            try {
              setValue(i, column, a);  // if this throws, a is destroyed again
            } catch (...) {
              a.destroy();
              throw;
            }
            madeHere.emplace_back(a);
          } else if (n >= 2) {
            setValue(i, column, madeHere[static_cast<size_t>(n)]);
            // If this throws, all is OK, because it was already put into
            // the block elsewhere.
          } else {
            THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                           "found undefined data value");
          }
        }
      }
    }
  } catch (...) {
    destroy();
  }
}

/// @brief destroy the block, used in the destructor and elsewhere
void AqlItemBlock::destroy() {
  if (_valueCount.empty()) {
    return;
  }

  for (auto& it : _data) {
    if (it.requiresDestruction()) {
      try {  // can find() really throw???
        auto it2 = _valueCount.find(it);
        if (it2 != _valueCount.end()) {  // if we know it, we are still responsible
          TRI_ASSERT(it2->second > 0);

          if (--(it2->second) == 0) {
            it.destroy();
            try {
              _valueCount.erase(it2);
            } catch (...) {
            }
          }
        }
      } catch (...) {
      }
      // Note that if we do not know it the thing it has been stolen from us!
    } else {
      it.erase();
    }
  }

  _valueCount.clear();
}

/// @brief shrink the block to the specified number of rows
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

  // erase all stored values in the region that we freed
  for (size_t i = nrItems; i < _nrItems; ++i) {
    for (RegisterId j = 0; j < _nrRegs; ++j) {
      AqlValue& a(_data[_nrRegs * i + j]);

      if (a.requiresDestruction()) {
        auto it = _valueCount.find(a);

        if (it != _valueCount.end()) {
          TRI_ASSERT(it->second > 0);

          if (--it->second == 0) {
            a.destroy();
            try {
              _valueCount.erase(it);
            } catch (...) {
            }
          }
        }
      }
      a.erase();
    }
  }

  // adjust the size of the block
  _nrItems = nrItems;
}

/// @brief clears out some columns (registers), this deletes the values if
/// necessary, using the reference count.
void AqlItemBlock::clearRegisters(
    std::unordered_set<RegisterId> const& toClear) {
  for (auto const& reg : toClear) {
    for (size_t i = 0; i < _nrItems; i++) {
      AqlValue& a(_data[_nrRegs * i + reg]);

      if (a.requiresDestruction()) {
        auto it = _valueCount.find(a);

        if (it != _valueCount.end()) {
          TRI_ASSERT(it->second > 0);

          if (--it->second == 0) {
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
AqlItemBlock* AqlItemBlock::slice(size_t from, size_t to) const {
  TRI_ASSERT(from < to && to <= _nrItems);

  std::unordered_map<AqlValue, AqlValue> cache;
  cache.reserve((to - from) * _nrRegs / 4 + 1);

  auto res = std::make_unique<AqlItemBlock>(to - from, _nrRegs);

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
            cache.emplace(a, b);
          } else {
            res->setValue(row - from, col, it->second);
          }
        } else {
          // simple copying of values
          res->setValue(row - from, col, a);
        } 
      }
    }
  }

  return res.release();
}

/// @brief slice/clone, this does a deep copy of all entries
AqlItemBlock* AqlItemBlock::slice(
    size_t row, std::unordered_set<RegisterId> const& registers) const {
  std::unordered_map<AqlValue, AqlValue> cache;

  auto res = std::make_unique<AqlItemBlock>(1, _nrRegs);

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
          cache.emplace(a, b);
        } else {
          res->setValue(0, col, it->second);
        }
      } else {
        res->setValue(0, col, a);
      }
    }
  }

  return res.release();
}

/// @brief slice/clone chosen rows for a subset, this does a deep copy
/// of all entries
AqlItemBlock* AqlItemBlock::slice(std::vector<size_t> const& chosen, size_t from,
                                  size_t to) const {
  TRI_ASSERT(from < to && to <= chosen.size());

  std::unordered_map<AqlValue, AqlValue> cache;
  cache.reserve((to - from) * _nrRegs / 4 + 1);

  auto res = std::make_unique<AqlItemBlock>(to - from, _nrRegs);

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
            cache.emplace(a, b);
          } else {
            res->setValue(row - from, col, it->second);
          }
        } else {
          res->setValue(row - from, col, a);
        }
      }
    }
  }

  return res.release();
}

/// @brief steal for a subset, this does not copy the entries, rather,
/// it remembers which it has taken. This is stored in the
/// this by removing the value counts in _valueCount.
/// It is highly recommended to delete this object right after this
/// operation, because it is unclear, when the values to which our
/// AqlValues point will vanish! In particular, do not use setValue
/// any more.
AqlItemBlock* AqlItemBlock::steal(std::vector<size_t> const& chosen, size_t from,
                                  size_t to) {
  TRI_ASSERT(from < to && to <= chosen.size());

  auto res = std::make_unique<AqlItemBlock>(to - from, _nrRegs);

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

  return res.release();
}

/// @brief concatenate multiple blocks, note that the new block now owns all
/// AqlValue pointers in the old blocks, therefore, the latter are all
/// set to nullptr, just to be sure.
AqlItemBlock* AqlItemBlock::concatenate(
    std::vector<AqlItemBlock*> const& blocks) {
  TRI_ASSERT(!blocks.empty());
  
  size_t totalSize = 0;
  RegisterId nrRegs = 0;
  for (auto& it : blocks) {
    totalSize += it->size();
    if (nrRegs == 0) {
      nrRegs = it->getNrRegs();
    } else {
      TRI_ASSERT(it->getNrRegs() == nrRegs);
    }
  }

  TRI_ASSERT(totalSize > 0);
  TRI_ASSERT(nrRegs > 0);

  auto res = std::make_unique<AqlItemBlock>(totalSize, nrRegs);

  size_t pos = 0;
  for (auto& it : blocks) {
    TRI_ASSERT(it != res.get());
    size_t const n = it->size();
    for (size_t row = 0; row < n; ++row) {
      for (RegisterId col = 0; col < nrRegs; ++col) {
        // copy over value
        AqlValue const& a = it->getValueReference(row, col);
        if (!a.isEmpty()) {
          res->setValue(pos + row, col, a);
        }
      }
    }
    it->eraseAll();
    pos += it->size();
  }

  return res.release();
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
///               0.0  means a single empty entry
///               -1.0 followed by a positive integer N (encoded as number)
///                      means a run of that many empty entries
///               -2.0 followed by two numbers LOW and HIGH means a range
///                      and LOW and HIGH are the boundaries (inclusive)
///               1.0 means a JSON entry at the "next" position in "raw"
///                      the "next" position starts with 2 and is increased
///                      by one for every 1.0 found in data
///               integer values >= 2.0 mean a JSON entry, in this
///                      case the "raw" list contains an entry in the
///                      corresponding position
///  "raw":     List of actual values, positions 0 and 1 are always null
///                      such that actual indices start at 2
void AqlItemBlock::toVelocyPack(arangodb::Transaction* trx,
                                VPackBuilder& result) const {
  VPackBuilder data;
  data.openArray();

  VPackBuilder raw;
  raw.openArray();
  // Two nulls in the beginning such that indices start with 2
  raw.add(VPackValue(VPackValueType::Null));
  raw.add(VPackValue(VPackValueType::Null));

  std::unordered_map<AqlValue, size_t> table;  // remember duplicates

  size_t emptyCount = 0;  // here we count runs of empty AqlValues

  auto commitEmpties = [&]() {  // this commits an empty run to the data
    if (emptyCount > 0) {
      if (emptyCount == 1) {
        data.add(VPackValue(0));
      } else {
        data.add(VPackValue(-1));
        data.add(VPackValue(emptyCount));
      }
      emptyCount = 0;
    }
  };

  size_t pos = 2;  // write position in raw
  for (RegisterId column = 0; column < _nrRegs; column++) {
    for (size_t i = 0; i < _nrItems; i++) {
      AqlValue const& a(_data[i * _nrRegs + column]);
      if (a.isEmpty()) {
        emptyCount++;
      } else {
        commitEmpties();
        if (a.isRange()) {
          data.add(VPackValue(-2));
          data.add(VPackValue(a.range()->_low));
          data.add(VPackValue(a.range()->_high));
        } else {
          auto it = table.find(a);

          if (it == table.end()) {
            a.toVelocyPack(trx, raw, false);
            data.add(VPackValue(1));
            table.emplace(a, pos++);
          } else {
            data.add(VPackValue(it->second));
          }
        }
      }
    }
  }
  commitEmpties();

  raw.close();
  data.close();

  result.add("nrItems", VPackValue(_nrItems));
  result.add("nrRegs", VPackValue(_nrRegs));
  result.add("data", data.slice());
  result.add("raw", raw.slice());
  result.add("error", VPackValue(false));
  result.add("exhausted", VPackValue(false));
}
