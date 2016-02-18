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

#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionNode.h"

using namespace arangodb::aql;

using Json = arangodb::basics::Json;
using JsonHelper = arangodb::basics::JsonHelper;

////////////////////////////////////////////////////////////////////////////////
/// @brief create the block
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock::AqlItemBlock(size_t nrItems, RegisterId nrRegs)
    : _nrItems(nrItems), _nrRegs(nrRegs) {
  TRI_ASSERT(nrItems > 0);  // no, empty AqlItemBlocks are not allowed!

  if (nrRegs > 0) {
    // check that the nrRegs value is somewhat sensible
    // this compare value is arbitrary, but having so many registers in a single
    // query seems unlikely
    TRI_ASSERT(nrRegs <= ExecutionNode::MaxRegisterId);

    _data.resize(nrItems * nrRegs);

    _docColls.reserve(nrRegs);
    for (size_t i = 0; i < nrRegs; ++i) {
      _docColls.emplace_back(nullptr);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the block from Json, note that this can throw
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock::AqlItemBlock(Json const& json) {
  bool exhausted = JsonHelper::getBooleanValue(json.json(), "exhausted", false);

  if (exhausted) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "exhausted must be false");
  }

  _nrItems = JsonHelper::getNumericValue<size_t>(json.json(), "nrItems", 0);
  if (_nrItems == 0) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "nrItems must be > 0");
  }

  _nrRegs = JsonHelper::getNumericValue<RegisterId>(json.json(), "nrRegs", 0);

  // Initialize the data vector:
  if (_nrRegs > 0) {
    _data.resize(_nrItems * _nrRegs);
    _docColls.reserve(_nrRegs);
    for (size_t i = 0; i < _nrRegs; ++i) {
      _docColls.emplace_back(nullptr);
    }
  }

  // Now put in the data:
  Json data(json.get("data"));
  Json raw(json.get("raw"));

  std::vector<AqlValue> madeHere;
  madeHere.reserve(raw.size());
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
          Json dataEntry(data.at(static_cast<int>(posInData++)));
          if (!dataEntry.isNumber()) {
            THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                           "data must contain only numbers");
          }
          int64_t n = JsonHelper::getNumericValue<int64_t>(dataEntry.json(), 0);
          if (n == 0) {
            // empty, do nothing here
          } else if (n == -1) {
            // empty run:
            Json runLength(data.at(static_cast<int>(posInData++)));
            emptyRun =
                JsonHelper::getNumericValue<int64_t>(runLength.json(), 0);
            TRI_ASSERT(emptyRun > 0);
            emptyRun--;
          } else if (n == -2) {
            // a range
            Json lowBound(data.at(static_cast<int>(posInData++)));
            Json highBound(data.at(static_cast<int>(posInData++)));
            int64_t low =
                JsonHelper::getNumericValue<int64_t>(lowBound.json(), 0);
            int64_t high =
                JsonHelper::getNumericValue<int64_t>(highBound.json(), 0);
            AqlValue a(low, high);
            try {
              setValue(i, column, a);
            } catch (...) {
              a.destroy();
              throw;
            }
          } else if (n == 1) {
            // a JSON value
            Json x(raw.at(static_cast<int>(posInRaw++)));
            AqlValue a(new Json(TRI_UNKNOWN_MEM_ZONE, x.copy().steal()));
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

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the block, used in the destructor and elsewhere
////////////////////////////////////////////////////////////////////////////////

void AqlItemBlock::destroy() {
  if (_valueCount.empty()) {
    return;
  }

  for (auto& it : _data) {
    if (it.requiresDestruction()) {
      try {  // can find() really throw???
        auto it2 = _valueCount.find(it);
        if (it2 !=
            _valueCount.end()) {  // if we know it, we are still responsible
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

////////////////////////////////////////////////////////////////////////////////
/// @brief shrink the block to the specified number of rows
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @brief clears out some columns (registers), this deletes the values if
/// necessary, using the reference count.
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @brief slice/clone, this does a deep copy of all entries
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* AqlItemBlock::slice(size_t from, size_t to) const {
  TRI_ASSERT(from < to && to <= _nrItems);

  std::unordered_map<AqlValue, AqlValue> cache;
  cache.reserve((to - from) * _nrRegs / 4 + 1);

  auto res = std::make_unique<AqlItemBlock>(to - from, _nrRegs);

  for (RegisterId col = 0; col < _nrRegs; col++) {
    res->_docColls[col] = _docColls[col];
  }

  for (size_t row = from; row < to; row++) {
    for (RegisterId col = 0; col < _nrRegs; col++) {
      AqlValue const& a(_data[row * _nrRegs + col]);

      if (!a.isEmpty()) {
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
      }
    }
  }

  return res.release();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief slice/clone, this does a deep copy of all entries
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* AqlItemBlock::slice(
    size_t row, std::unordered_set<RegisterId> const& registers) const {
  std::unordered_map<AqlValue, AqlValue> cache;

  auto res = std::make_unique<AqlItemBlock>(1, _nrRegs);

  for (RegisterId col = 0; col < _nrRegs; col++) {
    if (registers.find(col) == registers.end()) {
      continue;
    }

    res->_docColls[col] = _docColls[col];

    AqlValue const& a(_data[row * _nrRegs + col]);

    if (!a.isEmpty()) {
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
    }
  }

  return res.release();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief slice/clone chosen rows for a subset, this does a deep copy
/// of all entries
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* AqlItemBlock::slice(std::vector<size_t>& chosen, size_t from,
                                  size_t to) const {
  TRI_ASSERT(from < to && to <= chosen.size());

  std::unordered_map<AqlValue, AqlValue> cache;
  cache.reserve((to - from) * _nrRegs / 4 + 1);

  auto res = std::make_unique<AqlItemBlock>(to - from, _nrRegs);

  for (RegisterId col = 0; col < _nrRegs; col++) {
    res->_docColls[col] = _docColls[col];
  }

  for (size_t row = from; row < to; row++) {
    for (RegisterId col = 0; col < _nrRegs; col++) {
      AqlValue const& a(_data[chosen[row] * _nrRegs + col]);

      if (!a.isEmpty()) {
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
      }
    }
  }

  return res.release();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief steal for a subset, this does not copy the entries, rather,
/// it remembers which it has taken. This is stored in the
/// this by removing the value counts in _valueCount.
/// It is highly recommended to delete this object right after this
/// operation, because it is unclear, when the values to which our
/// AqlValues point will vanish! In particular, do not use setValue
/// any more.
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* AqlItemBlock::steal(std::vector<size_t>& chosen, size_t from,
                                  size_t to) {
  TRI_ASSERT(from < to && to <= chosen.size());

  auto res = std::make_unique<AqlItemBlock>(to - from, _nrRegs);

  for (RegisterId col = 0; col < _nrRegs; col++) {
    res->_docColls[col] = _docColls[col];
  }

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

////////////////////////////////////////////////////////////////////////////////
/// @brief concatenate multiple blocks, note that the new block now owns all
/// AqlValue pointers in the old blocks, therefore, the latter are all
/// set to nullptr, just to be sure.
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* AqlItemBlock::concatenate(
    std::vector<AqlItemBlock*> const& blocks) {
  TRI_ASSERT(!blocks.empty());

  auto it = blocks.begin();
  TRI_ASSERT(it != blocks.end());
  size_t totalSize = (*it)->size();
  RegisterId nrRegs = (*it)->getNrRegs();

  while (true) {
    if (++it == blocks.end()) {
      break;
    }
    totalSize += (*it)->size();
    TRI_ASSERT((*it)->getNrRegs() == nrRegs);
  }

  TRI_ASSERT(totalSize > 0);
  TRI_ASSERT(nrRegs > 0);

  auto res = std::make_unique<AqlItemBlock>(totalSize, nrRegs);

  size_t pos = 0;
  for (it = blocks.begin(); it != blocks.end(); ++it) {
    // handle collections
    if (it == blocks.begin()) {
      // copy collection info over
      for (RegisterId col = 0; col < nrRegs; ++col) {
        res->setDocumentCollection(col, (*it)->getDocumentCollection(col));
      }
    } else {
      for (RegisterId col = 0; col < nrRegs; ++col) {
        TRI_ASSERT(res->getDocumentCollection(col) ==
                   (*it)->getDocumentCollection(col));
      }
    }

    TRI_ASSERT((*it) != res.get());
    size_t const n = (*it)->size();
    for (size_t row = 0; row < n; ++row) {
      for (RegisterId col = 0; col < nrRegs; ++col) {
        // copy over value
        if (!(*it)->getValueReference(row, col).isEmpty()) {
          res->setValue(pos + row, col, (*it)->getValue(row, col));
        }
      }
    }
    (*it)->eraseAll();
    pos += (*it)->size();
  }

  return res.release();
}

////////////////////////////////////////////////////////////////////////////////
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
////////////////////////////////////////////////////////////////////////////////

Json AqlItemBlock::toJson(arangodb::AqlTransaction* trx) const {
  Json json(Json::Object, 6);
  json("nrItems", Json(static_cast<double>(_nrItems)))(
      "nrRegs", Json(static_cast<double>(_nrRegs)));
  Json data(Json::Array, _data.size());
  Json raw(Json::Array, _data.size() + 2);
  raw(Json(Json::Null))(
      Json(Json::Null));  // Two nulls in the beginning such that
                          // indices start with 2

  std::unordered_map<AqlValue, size_t> table;  // remember duplicates

  size_t emptyCount = 0;  // here we count runs of empty AqlValues

  auto commitEmpties = [&]() {  // this commits an empty run to the data
    if (emptyCount > 0) {
      if (emptyCount == 1) {
        data(Json(0.0));
      } else {
        data(Json(-1.0))(Json(static_cast<double>(emptyCount)));
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
        if (a._type == AqlValue::RANGE) {
          data(Json(-2.0))(Json(static_cast<double>(a._range->_low)))(
              Json(static_cast<double>(a._range->_high)));
        } else {
          auto it = table.find(a);
          if (it == table.end()) {
            raw(a.toJson(trx, _docColls[column], true));
            data(Json(1.0));
            table.emplace(a, pos++);
          } else {
            data(Json(static_cast<double>(it->second)));
          }
        }
      }
    }
  }
  commitEmpties();
  json("data", data)("raw", raw)("error", Json(false))("exhausted",
                                                       Json(false));
  return json;
}
