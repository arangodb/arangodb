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

#include "Basics/Common.h"
#include "Basics/JsonHelper.h"
#include "Aql/AqlValue.h"
#include "Aql/Range.h"
#include "Aql/types.h"

struct TRI_document_collection_t;

namespace arangodb {
namespace aql {

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

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief create the block
  //////////////////////////////////////////////////////////////////////////////

  AqlItemBlock(size_t nrItems, RegisterId nrRegs);

  AqlItemBlock(arangodb::basics::Json const& json);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief destroy the block
  //////////////////////////////////////////////////////////////////////////////

  ~AqlItemBlock() { destroy(); }

 private:
  void destroy();

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief getValue, get the value of a register
  //////////////////////////////////////////////////////////////////////////////

  AqlValue getValue(size_t index, RegisterId varNr) const {
    TRI_ASSERT(_data.capacity() > index * _nrRegs + varNr);
    return _data[index * _nrRegs + varNr];
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief getValue, get the value of a register by reference
  //////////////////////////////////////////////////////////////////////////////

  AqlValue const& getValueReference(size_t index, RegisterId varNr) const {
    TRI_ASSERT(_data.capacity() > index * _nrRegs + varNr);
    return _data[index * _nrRegs + varNr];
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief setValue, set the current value of a register
  //////////////////////////////////////////////////////////////////////////////

  void setValue(size_t index, RegisterId varNr, AqlValue const& value) {
    TRI_ASSERT(_data.capacity() > index * _nrRegs + varNr);
    TRI_ASSERT(_data[index * _nrRegs + varNr].isEmpty());

    // First update the reference count, if this fails, the value is empty
    if (value.requiresDestruction()) {
      auto it = _valueCount.find(value);

      if (it == _valueCount.end()) {
        TRI_IF_FAILURE("AqlItemBlock::setValue") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        _valueCount.emplace(value, 1);
      } else {
        TRI_ASSERT(it->second > 0);
        ++(it->second);
      }
    }

    _data[index * _nrRegs + varNr] = value;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief fill a slot in the item block with a SHAPED AqlValue
  /// this is a specialization without reference counting
  //////////////////////////////////////////////////////////////////////////////

  void setShaped(size_t index, RegisterId varNr,
                 TRI_df_marker_t const* marker) {
    TRI_ASSERT(_data.capacity() > index * _nrRegs + varNr);
    TRI_ASSERT(!_data[index * _nrRegs + varNr].requiresDestruction());

    auto& v = _data[index * _nrRegs + varNr];
    v._marker = marker;
    v._type = AqlValue::SHAPED;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief eraseValue, erase the current value of a register and freeing it
  /// if this was the last reference to the value
  /// use with caution only in special situations when it can be ensured that
  /// no one else will be pointing to the same value
  //////////////////////////////////////////////////////////////////////////////

  void destroyValue(size_t index, RegisterId varNr) {
    size_t const pos = index * _nrRegs + varNr;
    auto& element = _data[pos];

    if (element.requiresDestruction()) {
      auto it = _valueCount.find(element);

      if (it != _valueCount.end()) {
        if (--(it->second) == 0) {
          try {
            _valueCount.erase(it);
            element.destroy();
            return;  // no need for an extra element.erase() in this case
          } catch (...) {
          }
        }
      }
    }

    element.erase();
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief eraseValue, erase the current value of a register not freeing it
  /// this is used if the value is stolen and later released from elsewhere
  //////////////////////////////////////////////////////////////////////////////

  void eraseValue(size_t index, RegisterId varNr) {
    size_t const pos = index * _nrRegs + varNr;
    auto& element = _data[pos];

    if (element.requiresDestruction()) {
      auto it = _valueCount.find(element);

      if (it != _valueCount.end()) {
        if (--(it->second) == 0) {
          try {
            _valueCount.erase(it);
          } catch (...) {
          }
        }
      }
    }

    element.erase();
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief eraseValue, erase the current value of a register not freeing it
  /// this is used if the value is stolen and later released from elsewhere
  //////////////////////////////////////////////////////////////////////////////

  void eraseAll() {
    for (auto& it : _data) {
      if (!it.isEmpty()) {
        it.erase();
      }
    }

    _valueCount.clear();
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief valueCount
  /// this is used if the value is stolen and later released from elsewhere
  //////////////////////////////////////////////////////////////////////////////

  uint32_t valueCount(AqlValue const& v) const {
    auto it = _valueCount.find(v);

    if (it == _valueCount.end()) {
      return 0;
    }
    return it->second;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief steal, steal an AqlValue from an AqlItemBlock, it will never free
  /// the same value again. Note that once you do this for a single AqlValue
  /// you should delete the AqlItemBlock soon, because the stolen AqlValues
  /// might be deleted at any time!
  //////////////////////////////////////////////////////////////////////////////

  void steal(AqlValue const& v) {
    if (v.requiresDestruction()) {
      auto it = _valueCount.find(v);

      if (it != _valueCount.end()) {
        _valueCount.erase(it);
      }
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief getDocumentCollection
  //////////////////////////////////////////////////////////////////////////////

  inline TRI_document_collection_t const* getDocumentCollection(
      RegisterId varNr) const {
    return _docColls[varNr];
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief setDocumentCollection, set the current value of a variable or
  /// attribute
  //////////////////////////////////////////////////////////////////////////////

  inline void setDocumentCollection(RegisterId varNr,
                                    TRI_document_collection_t const* docColl) {
    _docColls[varNr] = docColl;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief getter for _nrRegs
  //////////////////////////////////////////////////////////////////////////////

  inline RegisterId getNrRegs() const { return _nrRegs; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief getter for _nrItems
  //////////////////////////////////////////////////////////////////////////////

  inline size_t size() const { return _nrItems; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief getter for _docColls
  //////////////////////////////////////////////////////////////////////////////

  std::vector<TRI_document_collection_t const*>& getDocumentCollections() {
    return _docColls;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief shrink the block to the specified number of rows
  //////////////////////////////////////////////////////////////////////////////

  void shrink(size_t nrItems);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief clears out some columns (registers), this deletes the values if
  /// necessary, using the reference count.
  //////////////////////////////////////////////////////////////////////////////

  void clearRegisters(std::unordered_set<RegisterId> const& toClear);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief slice/clone, this does a deep copy of all entries
  //////////////////////////////////////////////////////////////////////////////

  AqlItemBlock* slice(size_t from, size_t to) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief create an AqlItemBlock with a single row, with copies of the
  /// specified registers from the current block
  //////////////////////////////////////////////////////////////////////////////

  AqlItemBlock* slice(size_t row, std::unordered_set<RegisterId> const&) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief slice/clone chosen rows for a subset, this does a deep copy
  /// of all entries
  //////////////////////////////////////////////////////////////////////////////

  AqlItemBlock* slice(std::vector<size_t>& chosen, size_t from,
                      size_t to) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief steal for a subset, this does not copy the entries, rather,
  /// it remembers which it has taken. This is stored in the
  /// this AqlItemBlock. It is highly recommended to delete it right
  /// after this operation, because it is unclear, when the values
  /// to which our AqlValues point will vanish.
  //////////////////////////////////////////////////////////////////////////////

  AqlItemBlock* steal(std::vector<size_t>& chosen, size_t from, size_t to);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief concatenate multiple blocks, note that the new block now owns all
  /// AqlValue pointers in the old blocks, therefore, the latter are all
  /// set to nullptr, just to be sure.
  //////////////////////////////////////////////////////////////////////////////

  static AqlItemBlock* concatenate(std::vector<AqlItemBlock*> const& blocks);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief toJson, transfer a whole AqlItemBlock to Json, the result can
  /// be used to recreate the AqlItemBlock via the Json constructor
  //////////////////////////////////////////////////////////////////////////////

  arangodb::basics::Json toJson(arangodb::AqlTransaction* trx) const;

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief _data, the actual data as a single vector of dimensions _nrItems
  /// times _nrRegs
  //////////////////////////////////////////////////////////////////////////////

  std::vector<AqlValue> _data;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief _valueCount, since we have to allow for identical AqlValues
  /// in an AqlItemBlock, this map keeps track over which AqlValues we
  /// have in this AqlItemBlock and how often.
  /// setValue above puts values in the map and increases the count if they
  /// are already there, eraseValue decreases the count. One can ask the
  /// count with valueCount.
  //////////////////////////////////////////////////////////////////////////////

  std::unordered_map<AqlValue, uint32_t> _valueCount;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief _docColls, for every column a possible collection, which contains
  /// all AqlValues of type SHAPED in this column.
  //////////////////////////////////////////////////////////////////////////////

  std::vector<TRI_document_collection_t const*> _docColls;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief _nrItems, number of rows
  //////////////////////////////////////////////////////////////////////////////

  size_t _nrItems;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief _nrRegs, number of rows
  //////////////////////////////////////////////////////////////////////////////

  RegisterId _nrRegs;
};

}  // namespace arangodb::aql
}  // namespace arangodb

#endif
