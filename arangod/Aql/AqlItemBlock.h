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

#ifndef ARANGOD_AQL_AQL_ITEM_BLOCK_H
#define ARANGOD_AQL_AQL_ITEM_BLOCK_H 1

#include "Aql/AqlValue.h"
#include "Aql/Range.h"
#include "Aql/ResourceUsage.h"
#include "Aql/types.h"
#include "Basics/Common.h"

#include <utility>

namespace arangodb {
namespace aql {
class AqlItemBlockManager;
class BlockCollector;
class SharedAqlItemBlockPtr;

// an <AqlItemBlock> is a <nrItems>x<nrRegs> vector of <AqlValue>s (not
// pointers). The size of an <AqlItemBlock> is the number of items.
// Entries in a given column (i.e. all the values of a given register
// for all items in the block) have the same type and belong to the
// same collection (if they are of type SHAPED). The document collection
// for a particular column is accessed via <getDocumentCollection>,
// and the entire array of document collections is accessed by
// <getDocumentCollections>. There is no access to an entire item, only
// access to particular registers of an item (via getValue).
//
// An AqlItemBlock is responsible to explicitly destroy all the
// <AqlValue>s it contains at destruction time. It is however allowed
// that multiple of the <AqlValue>s in it are pointing to identical
// structures, and thus must be destroyed only once for all identical
// copies. Furthermore, when parts of an AqlItemBlock are handed on
// to another AqlItemBlock, then the <AqlValue>s inside must be copied
// (deep copy) to make the blocks independent.

class AqlItemBlock {
  friend class AqlItemBlockManager;
  friend class BlockCollector;
  friend class SharedAqlItemBlockPtr;

 public:
  AqlItemBlock() = delete;
  AqlItemBlock(AqlItemBlock const&) = delete;
  AqlItemBlock& operator=(AqlItemBlock const&) = delete;

  /// @brief create the block
  AqlItemBlock(AqlItemBlockManager&, size_t nrItems, RegisterId nrRegs);

  void initFromSlice(arangodb::velocypack::Slice);

 protected:
  /// @brief destroy the block
  /// Should only ever be deleted by AqlItemManager::returnBlock, so the
  /// destructor is protected.
  ~AqlItemBlock() {
    TRI_ASSERT(_refCount == 0);
    destroy();
    decreaseMemoryUsage(sizeof(AqlValue) * _nrItems * _nrRegs);
  }

 private:
  void destroy() noexcept;

  ResourceMonitor& resourceMonitor() noexcept;

  inline void increaseMemoryUsage(size_t value) {
    resourceMonitor().increaseMemoryUsage(value);
  }

  inline void decreaseMemoryUsage(size_t value) noexcept {
    resourceMonitor().decreaseMemoryUsage(value);
  }

 public:
  /// @brief getValue, get the value of a register
  inline AqlValue getValue(size_t index, RegisterId varNr) const {
    TRI_ASSERT(index < _nrItems);
    TRI_ASSERT(varNr < _nrRegs);
    return _data[index * _nrRegs + varNr];
  }

  /// @brief getValue, get the value of a register by reference
  inline AqlValue const& getValueReference(size_t index, RegisterId varNr) const {
    TRI_ASSERT(index < _nrItems);
    TRI_ASSERT(varNr < _nrRegs);
    return _data[index * _nrRegs + varNr];
  }

  /// @brief setValue, set the current value of a register
  inline void setValue(size_t index, RegisterId varNr, AqlValue const& value) {
    TRI_ASSERT(index < _nrItems);
    TRI_ASSERT(varNr < _nrRegs);
    TRI_ASSERT(_data[index * _nrRegs + varNr].isEmpty());

    // First update the reference count, if this fails, the value is empty
    if (value.requiresDestruction()) {
      if (++_valueCount[value] == 1) {
        size_t mem = value.memoryUsage();
        increaseMemoryUsage(mem);
      }
    }

    _data[index * _nrRegs + varNr] = value;
  }

  /// @brief emplaceValue, set the current value of a register, constructing
  /// it in place
  template <typename... Args>
  //std::enable_if_t<!(std::is_same<AqlValue,std::decay_t<Args>>::value || ...), void>
  void
  emplaceValue(size_t index, RegisterId varNr, Args&&... args) {
    TRI_ASSERT(index < _nrItems);
    TRI_ASSERT(varNr < _nrRegs);

    AqlValue* p = &_data[index * _nrRegs + varNr];
    TRI_ASSERT(p->isEmpty());
    // construct the AqlValue in place
    AqlValue* value;
    try {
      value = new (p) AqlValue(std::forward<Args>(args)...);
    } catch (...) {
      // clean up the cell
      _data[index * _nrRegs + varNr].erase();
      throw;
    }

    try {
      // Now update the reference count, if this fails, we'll roll it back
      if (value->requiresDestruction()) {
        if (++_valueCount[*value] == 1) {
          increaseMemoryUsage(value->memoryUsage());
        }
      }
    } catch (...) {
      // invoke dtor
      value->~AqlValue();
      // TODO - instead of disabling it completly we could you use
      // a constexpr if() with c++17
      _data[index * _nrRegs + varNr].destroy();
      throw;
    }
  }

  /// @brief eraseValue, erase the current value of a register and freeing it
  /// if this was the last reference to the value
  /// use with caution only in special situations when it can be ensured that
  /// no one else will be pointing to the same value
  void destroyValue(size_t index, RegisterId varNr) {
    auto& element = _data[index * _nrRegs + varNr];

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

  /// @brief eraseValue, erase the current value of a register not freeing it
  /// this is used if the value is stolen and later released from elsewhere
  void eraseValue(size_t index, RegisterId varNr) {
    auto& element = _data[index * _nrRegs + varNr];

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

  /// @brief eraseValue, erase the current value of all values, not freeing
  /// them. this is used if the value is stolen and later released from
  /// elsewhere
  void eraseAll() {
    for (size_t i = 0; i < numEntries(); i++) {
      auto &it = _data[i];
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

  void copyValuesFromFirstRow(size_t currentRow, RegisterId curRegs) {
    TRI_ASSERT(currentRow > 0);

    if (curRegs == 0) {
      // nothing to do
      return;
    }
    TRI_ASSERT(currentRow < _nrItems);
    TRI_ASSERT(curRegs <= _nrRegs);

    copyValuesFromRow(currentRow, curRegs, 0);
  }

  void copyValuesFromRow(size_t currentRow, RegisterId curRegs, size_t fromRow) {
    TRI_ASSERT(currentRow != fromRow);

    for (RegisterId i = 0; i < curRegs; i++) {
      if (_data[currentRow * _nrRegs + i].isEmpty()) {
        // First update the reference count, if this fails, the value is empty
        if (_data[fromRow * _nrRegs + i].requiresDestruction()) {
          ++_valueCount[_data[fromRow * _nrRegs + i]];
        }
        TRI_ASSERT(_data[currentRow * _nrRegs + i].isEmpty());
        _data[currentRow * _nrRegs + i] = _data[fromRow * _nrRegs + i];
      }
    }
  }

  void copyValuesFromRow(size_t currentRow,
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
        _data[currentRow * _nrRegs + reg] = getValueReference(fromRow, reg);
      }
    }
  }

  /// @brief valueCount
  /// this is used if the value is stolen and later released from elsewhere
  uint32_t valueCount(AqlValue const& v) const {
    auto it = _valueCount.find(v);

    if (it == _valueCount.end()) {
      return 0;
    }
    return it->second;
  }

  /// @brief steal, steal an AqlValue from an AqlItemBlock, it will never free
  /// the same value again. Note that once you do this for a single AqlValue
  /// you should delete the AqlItemBlock soon, because the stolen AqlValues
  /// might be deleted at any time!
  void steal(AqlValue const& value) {
    if (value.requiresDestruction()) {
      if (_valueCount.erase(value)) {
        decreaseMemoryUsage(value.memoryUsage());
      }
    }
  }

  /// @brief getter for _nrRegs
  inline RegisterId getNrRegs() const noexcept { return _nrRegs; }

  /// @brief getter for _nrItems
  inline size_t size() const noexcept { return _nrItems; }


  /// @brief Number of entries in the matrix. If this changes, the memory usage
  /// must be / in- or decreased appropriately as well.
  /// All entries _data[i] for numEntries() <= i < _data.size() always have to
  /// be erased, i.e. empty / none!
  inline size_t numEntries() const {
    return _nrRegs * _nrItems;
  }

  inline size_t capacity() const noexcept { return _data.capacity(); }

  /// @brief shrink the block to the specified number of rows
  /// the superfluous rows are cleaned
  void shrink(size_t nrItems);

  /// @brief rescales the block to the specified dimensions
  /// note that the block should be empty before rescaling to prevent
  /// losses of still managed AqlValues
  void rescale(size_t nrItems, RegisterId nrRegs);

  /// @brief clears out some columns (registers), this deletes the values if
  /// necessary, using the reference count.
  void clearRegisters(std::unordered_set<RegisterId> const& toClear);

  /// @brief slice/clone, this does a deep copy of all entries
  SharedAqlItemBlockPtr slice(size_t from, size_t to) const;

  /// @brief create an AqlItemBlock with a single row, with copies of the
  /// specified registers from the current block
  SharedAqlItemBlockPtr slice(size_t row, std::unordered_set<RegisterId> const& registers,
                              uint64_t newNrRegs) const;

  /// @brief slice/clone chosen rows for a subset, this does a deep copy
  /// of all entries
  SharedAqlItemBlockPtr slice(std::vector<size_t> const& chosen, size_t from, size_t to) const;

  /// @brief steal for a subset, this does not copy the entries, rather,
  /// it remembers which it has taken. This is stored in the
  /// this AqlItemBlock. It is highly recommended to delete it right
  /// after this operation, because it is unclear, when the values
  /// to which our AqlValues point will vanish.
  SharedAqlItemBlockPtr steal(std::vector<size_t> const& chosen, size_t from, size_t to);

  /// @brief toJson, transfer a whole AqlItemBlock to Json, the result can
  /// be used to recreate the AqlItemBlock via the Json constructor
  void toVelocyPack(transaction::Methods* trx, arangodb::velocypack::Builder&) const;

 protected:
  AqlItemBlockManager& aqlItemBlockManager() noexcept { return _manager; }
  size_t getRefCount() const noexcept { return _refCount; }
  void incrRefCount() const noexcept { ++_refCount; }
  void decrRefCount() const noexcept {
    TRI_ASSERT(_refCount > 0);
    --_refCount;
  }

 private:
  /// @brief _data, the actual data as a single vector of dimensions _nrItems
  /// times _nrRegs
  std::vector<AqlValue> _data;

  /// @brief _valueCount, since we have to allow for identical AqlValues
  /// in an AqlItemBlock, this map keeps track over which AqlValues we
  /// have in this AqlItemBlock and how often.
  /// setValue above puts values in the map and increases the count if they
  /// are already there, eraseValue decreases the count. One can ask the
  /// count with valueCount.
  std::unordered_map<AqlValue, uint32_t> _valueCount;

  /// @brief _nrItems, number of rows
  size_t _nrItems = 0;

  /// @brief _nrRegs, number of columns
  RegisterId _nrRegs = 0;

  /// @brief manager for this item block
  AqlItemBlockManager& _manager;

  /// @brief number of SharedAqlItemBlockPtr instances. shall be returned to
  /// the _manager when it reaches 0.
  mutable size_t _refCount = 0;
};

}  // namespace aql
}  // namespace arangodb

#endif
