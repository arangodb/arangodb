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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_INDEXES_PRIMARY_INDEX_H
#define ARANGOD_INDEXES_PRIMARY_INDEX_H 1

#include "Basics/Common.h"
#include "Basics/AssocUnique.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {

class PrimaryIndex;
class Transaction;

class PrimaryIndexIterator final : public IndexIterator {
 public:
  PrimaryIndexIterator(arangodb::Transaction* trx, PrimaryIndex const* index,
                       std::unique_ptr<VPackBuilder>& keys)
      : _trx(trx), 
        _index(index), 
        _keys(keys.get()), 
        _iterator(_keys->slice()) {

        keys.release(); // now we have ownership for _keys
        TRI_ASSERT(_keys->slice().isArray());
      }

  ~PrimaryIndexIterator();

  TRI_doc_mptr_t* next() override;

  void reset() override;

 private:
  arangodb::Transaction* _trx;
  PrimaryIndex const* _index;
  std::unique_ptr<VPackBuilder> _keys;
  arangodb::velocypack::ArrayIterator _iterator;
};

class AllIndexIterator final : public IndexIterator {
 private:
  typedef arangodb::basics::AssocUnique<uint8_t, TRI_doc_mptr_t>
      TRI_PrimaryIndex_t;

 public:
  AllIndexIterator(arangodb::Transaction* trx, TRI_PrimaryIndex_t const* index,
                   bool reverse)
      : _trx(trx), _index(index), _reverse(reverse), _total(0) {}

  ~AllIndexIterator() {}

  TRI_doc_mptr_t* next() override;
  
  void nextBabies(std::vector<TRI_doc_mptr_t*>&, size_t) override;

  void reset() override;

 private:
  arangodb::Transaction* _trx;
  TRI_PrimaryIndex_t const* _index;
  arangodb::basics::BucketPosition _position;
  bool const _reverse;
  uint64_t _total;
};

class AnyIndexIterator final : public IndexIterator {
 private:
  typedef arangodb::basics::AssocUnique<uint8_t, TRI_doc_mptr_t>
      TRI_PrimaryIndex_t;

 public:
  AnyIndexIterator(arangodb::Transaction* trx, TRI_PrimaryIndex_t const* index)
      : _trx(trx), _index(index), _step(0), _total(0) {}

  ~AnyIndexIterator() {}

  TRI_doc_mptr_t* next() override;

  void reset() override;

 private:
  arangodb::Transaction* _trx;
  TRI_PrimaryIndex_t const* _index;
  arangodb::basics::BucketPosition _initial;
  arangodb::basics::BucketPosition _position;
  uint64_t _step;
  uint64_t _total;
};

class PrimaryIndex final : public Index {
  friend class PrimaryIndexIterator;

 public:
  PrimaryIndex() = delete;

  explicit PrimaryIndex(arangodb::LogicalCollection*);

  explicit PrimaryIndex(VPackSlice const&);

  ~PrimaryIndex();

 private:
  typedef arangodb::basics::AssocUnique<uint8_t, TRI_doc_mptr_t>
      TRI_PrimaryIndex_t;

 public:
  IndexType type() const override final {
    return Index::TRI_IDX_TYPE_PRIMARY_INDEX;
  }
  
  bool allowExpansion() const override final { return false; }

  bool canBeDropped() const override final { return false; }

  bool isSorted() const override final { return false; }

  bool hasSelectivityEstimate() const override final { return true; }

  double selectivityEstimate() const override final { return 1.0; }

  size_t size() const;

  size_t memory() const override final;

  void toVelocyPack(VPackBuilder&, bool) const override final;
  void toVelocyPackFigures(VPackBuilder&) const override final;

  int insert(arangodb::Transaction*, TRI_doc_mptr_t const*,
             bool) override final;

  int remove(arangodb::Transaction*, TRI_doc_mptr_t const*,
             bool) override final;

  int unload() override final;

 public:
  TRI_doc_mptr_t* lookupKey(arangodb::Transaction*, VPackSlice const&) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief a method to iterate over all elements in the index in
  ///        a sequential order.
  ///        Returns nullptr if all documents have been returned.
  ///        Convention: position === 0 indicates a new start.
  ///        DEPRECATED
  //////////////////////////////////////////////////////////////////////////////

  TRI_doc_mptr_t* lookupSequential(arangodb::Transaction*,
                                   arangodb::basics::BucketPosition& position,
                                   uint64_t& total);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief request an iterator over all elements in the index in
  ///        a sequential order.
  //////////////////////////////////////////////////////////////////////////////

  IndexIterator* allIterator(arangodb::Transaction*, bool) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief request an iterator over all elements in the index in
  ///        a random order. It is guaranteed that each element is found
  ///        exactly once unless the collection is modified.
  //////////////////////////////////////////////////////////////////////////////

  IndexIterator* anyIterator(arangodb::Transaction*) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief a method to iterate over all elements in the index in
  ///        reversed sequential order.
  ///        Returns nullptr if all documents have been returned.
  ///        Convention: position === UINT64_MAX indicates a new start.
  ///        DEPRECATED
  //////////////////////////////////////////////////////////////////////////////

  TRI_doc_mptr_t* lookupSequentialReverse(
      arangodb::Transaction*, arangodb::basics::BucketPosition& position);

  int insertKey(arangodb::Transaction*, TRI_doc_mptr_t*, void const**);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief adds a key/element to the index
  /// this is a special, optimized version that receives the target slot index
  /// from a previous lookupKey call
  //////////////////////////////////////////////////////////////////////////////

  int insertKey(arangodb::Transaction*, struct TRI_doc_mptr_t*,
                arangodb::basics::BucketPosition const&);

  TRI_doc_mptr_t* removeKey(arangodb::Transaction* trx,
                            VPackSlice const&);

  int resize(arangodb::Transaction*, size_t);

  static uint64_t calculateHash(arangodb::Transaction*, VPackSlice const&);
  static uint64_t calculateHash(arangodb::Transaction*, uint8_t const*);

  void invokeOnAllElements(std::function<bool(TRI_doc_mptr_t*)>);
  void invokeOnAllElementsForRemoval(std::function<bool(TRI_doc_mptr_t*)>);

  bool supportsFilterCondition(arangodb::aql::AstNode const*,
                               arangodb::aql::Variable const*, size_t, size_t&,
                               double&) const override;

  IndexIterator* iteratorForCondition(arangodb::Transaction*,
                                      IndexIteratorContext*,
                                      arangodb::aql::AstNode const*,
                                      arangodb::aql::Variable const*,
                                      bool) const override;

  IndexIterator* iteratorForSlice(arangodb::Transaction*, IndexIteratorContext*,
                                  arangodb::velocypack::Slice const,
                                  bool) const override;

  arangodb::aql::AstNode* specializeCondition(
      arangodb::aql::AstNode*, arangodb::aql::Variable const*) const override;

 private:

  //////////////////////////////////////////////////////////////////////////////
  /// @brief looks up an element given a request Slice. The slice has to be
  ///        of type string.
  //////////////////////////////////////////////////////////////////////////////

  TRI_doc_mptr_t* lookup(arangodb::Transaction*, VPackSlice const&) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief create the iterator, for a single attribute, IN operator
  //////////////////////////////////////////////////////////////////////////////

  IndexIterator* createInIterator(
      arangodb::Transaction*, IndexIteratorContext*,
      arangodb::aql::AstNode const*,
      arangodb::aql::AstNode const*) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief create the iterator, for a single attribute, EQ operator
  //////////////////////////////////////////////////////////////////////////////
  
  IndexIterator* createEqIterator(
      arangodb::Transaction*, IndexIteratorContext*,
      arangodb::aql::AstNode const*,
      arangodb::aql::AstNode const*) const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief add a single value node to the iterator's keys
  ////////////////////////////////////////////////////////////////////////////////
   
  void handleValNode(IndexIteratorContext* context,
                     VPackBuilder* keys,
                     arangodb::aql::AstNode const* valNode,
                     bool isId) const; 

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief the actual index
  //////////////////////////////////////////////////////////////////////////////

  TRI_PrimaryIndex_t* _primaryIndex;
};
}

#endif
