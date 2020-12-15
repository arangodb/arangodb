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
#include "Basics/ResourceUsage.h"
#include "Containers/SmallVector.h"

#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace arangodb {
namespace aql {
class AqlItemBlockManager;
class BlockCollector;
class SharedAqlItemBlockPtr;
enum class SerializationFormat;

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
  // needed for testing only
  friend class BlockCollector;
  friend class SharedAqlItemBlockPtr;
  
 public:
  AqlItemBlock() = delete;
  AqlItemBlock(AqlItemBlock const&) = delete;
  AqlItemBlock& operator=(AqlItemBlock const&) = delete;

  /// @brief create the block
  AqlItemBlock(AqlItemBlockManager&, size_t nrItems, RegisterCount nrRegs);

  void initFromSlice(arangodb::velocypack::Slice);

  SerializationFormat getFormatType() const;

  /// @brief auxiliary struct to track how often the same AqlValue is
  /// present in an AqlItemBlock, and how much dynamic memory on instance
  /// of the item uses
  struct ValueInfo {
    ValueInfo() noexcept 
        : refCount(0), memoryUsage(0) {}
    ValueInfo(ValueInfo&& other) noexcept = default;
    ValueInfo& operator=(ValueInfo&& other) noexcept = default;

    /// @brief set the memory usage for the item, expects refCount to be 
    /// exactly 1
    void setMemoryUsage(size_t value) noexcept {
      TRI_ASSERT(value > 0);
      TRI_ASSERT(refCount == 1);
      TRI_ASSERT(memoryUsage == 0);
      // In theory, there can be items consuming more than 4 GB here.
      // This case will be very rare and likely will cause a lot of other 
      // issues upfront. If we get such huge value, we count it as using
      // 4 GB of memory and ignore the rest.
      if (ADB_UNLIKELY(value > std::numeric_limits<uint32_t>::max())) {
        memoryUsage = std::numeric_limits<uint32_t>::max();
      } else {
        memoryUsage = static_cast<uint32_t>(value);
      }
    }

    /// @brief how many occurrences of the item are in use in this
    /// AqlItemBlock? it is expected to be 1 or more. the caller that sets
    /// the ref count to 0 is expected to remove the item from the
    /// _valueCount map completely
    uint32_t refCount;
    uint32_t memoryUsage;
  };
  
  using ShadowRowIterator = std::vector<uint32_t>::const_iterator;

 protected:
  /// @brief destroy the block
  /// Should only ever be deleted by AqlItemManager::returnBlock, so the
  /// destructor is protected.
  ~AqlItemBlock();

 public:
  /// @brief getValue, get the value of a register
  AqlValue getValue(size_t index, RegisterId varNr) const;

  /// @brief getValue, get the value of a register by reference
  AqlValue const& getValueReference(size_t index, RegisterId varNr) const;

  /// @brief setValue, set the current value of a register
  void setValue(size_t index, RegisterId varNr, AqlValue const& value);

  /// @brief emplaceValue, set the current value of a register, constructing
  /// it in place
  template <typename... Args>
  // std::enable_if_t<!(std::is_same<AqlValue,std::decay_t<Args>>::value || ...), void>
  void emplaceValue(size_t index, RegisterId varNr, Args&&... args) {
    auto address = getAddress(index, varNr);
    AqlValue* p = &_data[address];
    TRI_ASSERT(p->isEmpty());
    // construct the AqlValue in place
    AqlValue* value;
    try {
      value = new (p) AqlValue(std::forward<Args>(args)...);
    } catch (...) {
      // clean up the cell
      _data[address].erase();
      throw;
    }

    // Now update the reference count, if this fails, we'll roll it back
    if (value->requiresDestruction()) {
      try {
        // note: this may create a new entry in _valueCount, which is fine
        auto& valueInfo = _valueCount[value->data()];
        if (++valueInfo.refCount == 1) {
          size_t memoryUsage = value->memoryUsage();
          increaseMemoryUsage(memoryUsage);
          valueInfo.setMemoryUsage(memoryUsage);
        }
      } catch (...) {
        // invoke dtor
        value->~AqlValue();
        _data[address].destroy();
        throw;
      }
    }

    _maxModifiedRowIndex = std::max<size_t>(_maxModifiedRowIndex, index + 1);
  }

  /// @brief eraseValue, erase the current value of a register and freeing it
  /// if this was the last reference to the value
  /// use with caution only in special situations when it can be ensured that
  /// no one else will be pointing to the same value
  void destroyValue(size_t index, RegisterId varNr);

  /// @brief eraseValue, erase the current value of a register not freeing it
  /// this is used if the value is stolen and later released from elsewhere
  void eraseValue(size_t index, RegisterId varNr);

  /// @brief eraseValue, erase the current value of all values, not freeing
  /// them. this is used if the value is stolen and later released from
  /// elsewhere
  void eraseAll();

  void referenceValuesFromRow(size_t currentRow, RegIdFlatSet const& regs, size_t fromRow);

  /// @brief steal, steal an AqlValue from an AqlItemBlock, it will never free
  /// the same value again. Note that once you do this for a single AqlValue
  /// you should delete the AqlItemBlock soon, because the stolen AqlValues
  /// might be deleted at any time!
  void steal(AqlValue const& value);

  AqlValue stealAndEraseValue(size_t index, RegisterId varNr);

  /// @brief getter for _nrRegs
  RegisterCount getNrRegs() const noexcept;

  /// @brief getter for _nrItems
  size_t size() const noexcept;

  /// @brief get the relevant consumable range of the block
  std::tuple<size_t, size_t> getRelevantRange() const;

  /// @brief Number of entries in the matrix. If this changes, the memory usage
  /// must be / in- or decreased appropriately as well.
  /// All entries _data[i] for numEntries() <= i < _data.size() always have to
  /// be erased, i.e. empty / none!
  size_t numEntries() const;

  /// @brief number of modified entries
  size_t maxModifiedEntries() const;

  size_t capacity() const noexcept;

  /// @brief shrink the block to the specified number of rows
  /// the superfluous rows are cleaned
  void shrink(size_t nrItems);

  /// @brief rescales the block to the specified dimensions
  /// note that the block should be empty before rescaling to prevent
  /// losses of still managed AqlValues
  void rescale(size_t nrItems, RegisterCount nrRegs);

  /// @brief clears out some columns (registers), this deletes the values if
  /// necessary, using the reference count.
  void clearRegisters(RegIdFlatSet const& toClear);

  /// @brief clone all data rows, but move all shadow rows
  SharedAqlItemBlockPtr cloneDataAndMoveShadow();

  /// @brief slice/clone, this does a deep copy of all entries
  SharedAqlItemBlockPtr slice(size_t from, size_t to) const;

  /**
   * @brief Slice multiple ranges out of this AqlItemBlock.
   *        This does a deep copy of all entries
   *
   * @param ranges list of ranges from(included) -> to(excluded)
   *        Every range needs to be valid from[i] < to[i]
   *        And every range needs to be within the block to[i] <= size()
   *        The list is required to be ordered to[i] <= from[i+1]
   *
   * @return SharedAqlItemBlockPtr A block where all the slices are contained in the order of the list
   */
  auto slice(arangodb::containers::SmallVector<std::pair<size_t, size_t>> const& ranges) const -> SharedAqlItemBlockPtr;

  /// @brief create an AqlItemBlock with a single row, with copies of the
  /// specified registers from the current block
  SharedAqlItemBlockPtr slice(size_t row, std::unordered_set<RegisterId> const& registers,
                              RegisterCount newNrRegs) const;

  /// @brief slice/clone chosen rows for a subset, this does a deep copy
  /// of all entries
  SharedAqlItemBlockPtr slice(std::vector<size_t> const& chosen, size_t from, size_t to) const;

  /// @brief steal for a subset, this does not copy the entries, rather,
  /// it remembers which it has taken. This is stored in the
  /// this AqlItemBlock. It is highly recommended to delete it right
  /// after this operation, because it is unclear, when the values
  /// to which our AqlValues point will vanish.
  SharedAqlItemBlockPtr steal(std::vector<size_t> const& chosen, size_t from, size_t to);

  /// @brief toJson, transfer all rows of this AqlItemBlock to Json, the result
  /// can be used to recreate the AqlItemBlock via the Json constructor
  void toVelocyPack(velocypack::Options const*, arangodb::velocypack::Builder&) const;

  /// @brief toJson, transfer a slice of this AqlItemBlock to Json, the result can
  /// be used to recreate the AqlItemBlock via the Json constructor
  /// The slice will be starting at line `from` (including) and end at line `to` (excluding).
  /// Only calls with 0 <= from < to <= this.size() are allowed.
  /// If you want to transfer the full block, use from == 0, to == this.size()
  void toVelocyPack(size_t from, size_t to, velocypack::Options const*,
                    arangodb::velocypack::Builder&) const;

  /// @brief Creates a human-readable velocypack of the block. Adds an object
  /// `{nrItems, nrRegs, matrix}` to the builder.
  ///
  // `matrix` is an array of rows (of length nrItems). Each entry is an array
  // (of length nrRegs+1 (sic)). The first entry contains the shadow row depth,
  // or `null` for data rows. The entries with indexes 1..nrRegs contain the
  // registers 0..nrRegs-1, respectively.
  void toSimpleVPack(velocypack::Options const*, arangodb::velocypack::Builder&) const;

  void rowToSimpleVPack(size_t row, velocypack::Options const*,
                        velocypack::Builder& builder) const;

  /// @brief test if the given row is a shadow row and conveys subquery
  /// information only. It should not be handed to any non-subquery executor.
  bool isShadowRow(size_t row) const;

  /// @brief get the ShadowRowDepth 
  /// Does only work if this row is a shadow row
  /// Asserts on Maintainer, returns 0 on production
  size_t getShadowRowDepth(size_t row) const;

  /// @brief Transform the given row into a ShadowRow.
  void makeShadowRow(size_t row, size_t depth);

  /// @brief Transform the given row into a DataRow.
  void makeDataRow(size_t row);
  
  /// @brief Return the indexes of ShadowRows within this block, starting at lower.
  std::pair<ShadowRowIterator, ShadowRowIterator> getShadowRowIndexesFrom(size_t lower) const noexcept;

  /// @brief Quick test if we have any ShadowRows within this block;
  bool hasShadowRows() const noexcept;

  /// @brief return the number of ShadowRows
  size_t numShadowRows() const noexcept;

  /// @brief Moves all values *from* source *to* this block.
  /// Returns the row index of the last written row plus one (may equal size()).
  /// Expects size() - targetRow >= source->size(); and, of course, an equal
  /// number of registers.
  /// The source block will be cleared after this.
  size_t moveOtherBlockHere(size_t targetRow, AqlItemBlock& source);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
// MaintainerMode method to validate if ShadowRows organization are consistent.
// e.g. If a block always follows this pattern:
// ((Data* Shadow(0))* Shadow(1))* ...
  void validateShadowRowConsistency() const;
#endif

 protected:
  AqlItemBlockManager& aqlItemBlockManager() noexcept;
  size_t getRefCount() const noexcept;
  void incrRefCount() const noexcept;
  size_t decrRefCount() const noexcept;

 private:
  void destroy() noexcept;

  arangodb::ResourceMonitor& resourceMonitor() noexcept;

  void increaseMemoryUsage(size_t value);

  void decreaseMemoryUsage(size_t value) noexcept;

  void copySubqueryDepthFromOtherBlock(size_t targetRow, AqlItemBlock const& source,
                                       size_t sourceRow, bool forceShadowRow);

  /// @brief get the computed address within the data vector
  size_t getAddress(size_t index, RegisterId varNr) const noexcept;

  void copySubqueryDepth(size_t currentRow, size_t fromRow);

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
  /// note: only AqlValues that point to dynamically allocated memory
  /// should be added to this map. Other types (VPACK_INLINE) are not supported.
  std::unordered_map<void const*, ValueInfo> _valueCount;

  /// @brief _nrItems, number of rows
  size_t _nrItems = 0;

  /// @brief _nrRegs, number of columns
  RegisterCount _nrRegs = 0;
  
  /// @brief highest index of modified row
  size_t _maxModifiedRowIndex = 0;

  /// @brief manager for this item block
  AqlItemBlockManager& _manager;

  /// @brief number of SharedAqlItemBlockPtr instances. shall be returned to
  /// the _manager when it reaches 0.
  mutable size_t _refCount = 0;

  /// @brief current row index we want to read from. This will be increased
  /// after getRelevantRange function will be called, which will return a tuple
  /// of the old _rowIndex and the newly calculated _rowIndex - 1
  size_t _rowIndex;
  
  /// @brief a helper class that manages the storage of shadow rows
  /// in an AqlItemBlock
  class ShadowRows {
   public:
    /// @brief create a shadow row manager for at most nrItems rows
    explicit ShadowRows(size_t nrItems);

    /// @brief whether or not there are any shadow rows
    bool empty() const noexcept;

    /// @brief return the number of shadow rows
    size_t size() const noexcept;
    
    /// @brief whether or not the row is a shadow row
    bool is(size_t row) const noexcept;
    
    /// @brief get the shadow row depth for row
    size_t getDepth(size_t row) const noexcept;

    /// @brief clear all shadow rows
    void clear() noexcept;
    
    /// @brief resize the container to at most nrItems items
    void resize(size_t nrItems);
    
    /// @brief make a shadow row
    void make(size_t row, size_t depth);
    
    /// @brief make a data row
    void clear(size_t row);
    
    /// @brief clear all shadow rows from row to the end
    void clearFrom(size_t row);
    
    /// @brief return the indexes of ShadowRows, starting at lower
    std::pair<ShadowRowIterator, ShadowRowIterator> getIndexesFrom(size_t lower) const noexcept;

   private:
    /// @brief A list of indexes with all ShadowRows within
    /// this ItemBlock. Used to easier split data based on them.
    std::vector<uint32_t> _indexes;

    /// @brief all shadow row depths. a value of 0 means "no shadow row",
    /// values of 1 and higher indicate the actual shadow row depths plus one.
    /// the vector is lazily allocated and only populated when needed
    std::vector<uint16_t> _depths;

    /// @brief maximum size of _depths
    size_t _nrItems;
  };

  ShadowRows _shadowRows;
};

}  // namespace aql
}  // namespace arangodb

#endif
