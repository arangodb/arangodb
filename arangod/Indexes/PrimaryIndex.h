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

namespace arangodb {
namespace aql {
class SortCondition;
}
namespace basics {
struct AttributeName;
}

class Transaction;

class PrimaryIndexIterator final : public IndexIterator {
 public:
  PrimaryIndexIterator(arangodb::Transaction* trx,
                       PrimaryIndex const* index,
                       std::vector<char const*>& keys)
      : _trx(trx), _index(index), _keys(std::move(keys)), _position(0) {}

  ~PrimaryIndexIterator() {}

  TRI_doc_mptr_t* next() override;

  void reset() override;

 private:
  arangodb::Transaction* _trx;
  PrimaryIndex const* _index;
  std::vector<char const*> _keys;
  size_t _position;
};

class PrimaryIndex final : public Index {
  
 public:
  PrimaryIndex() = delete;

  explicit PrimaryIndex(struct TRI_document_collection_t*);

  explicit PrimaryIndex(VPackSlice const&);

  ~PrimaryIndex();

  
 private:
  typedef arangodb::basics::AssocUnique<char const, TRI_doc_mptr_t>
      TRI_PrimaryIndex_t;

  
 public:
  IndexType type() const override final {
    return Index::TRI_IDX_TYPE_PRIMARY_INDEX;
  }

  bool isSorted() const override final { return false; }

  bool hasSelectivityEstimate() const override final { return true; }

  double selectivityEstimate() const override final { return 1.0; }

  bool dumpFields() const override final { return true; }

  size_t size() const;

  size_t memory() const override final;

  void toVelocyPack(VPackBuilder&, bool) const override final;
  void toVelocyPackFigures(VPackBuilder&) const override final;

  int insert(arangodb::Transaction*, TRI_doc_mptr_t const*,
             bool) override final;

  int remove(arangodb::Transaction*, TRI_doc_mptr_t const*,
             bool) override final;

  TRI_doc_mptr_t* lookupKey(arangodb::Transaction*, char const*) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief looks up an element given a key
  /// returns the index position into which a key would belong in the second
  /// parameter. also returns the hash value for the object
  //////////////////////////////////////////////////////////////////////////////

  TRI_doc_mptr_t* lookupKey(arangodb::Transaction*, char const*,
                            arangodb::basics::BucketPosition&, uint64_t&) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief a method to iterate over all elements in the index in
  ///        a random order.
  ///        Returns nullptr if all documents have been returned.
  ///        Convention: step === 0 indicates a new start.
  //////////////////////////////////////////////////////////////////////////////

  TRI_doc_mptr_t* lookupRandom(
      arangodb::Transaction*,
      arangodb::basics::BucketPosition& initialPosition,
      arangodb::basics::BucketPosition& position, uint64_t& step,
      uint64_t& total);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief a method to iterate over all elements in the index in
  ///        a sequential order.
  ///        Returns nullptr if all documents have been returned.
  ///        Convention: position === 0 indicates a new start.
  //////////////////////////////////////////////////////////////////////////////

  TRI_doc_mptr_t* lookupSequential(arangodb::Transaction*,
                                   arangodb::basics::BucketPosition& position,
                                   uint64_t& total);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief a method to iterate over all elements in the index in
  ///        reversed sequential order.
  ///        Returns nullptr if all documents have been returned.
  ///        Convention: position === UINT64_MAX indicates a new start.
  //////////////////////////////////////////////////////////////////////////////

  TRI_doc_mptr_t* lookupSequentialReverse(
      arangodb::Transaction*,
      arangodb::basics::BucketPosition& position);

  int insertKey(arangodb::Transaction*, TRI_doc_mptr_t*, void const**);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief adds a key/element to the index
  /// this is a special, optimized version that receives the target slot index
  /// from a previous lookupKey call
  //////////////////////////////////////////////////////////////////////////////

  int insertKey(arangodb::Transaction*, struct TRI_doc_mptr_t*,
                arangodb::basics::BucketPosition const&);

  TRI_doc_mptr_t* removeKey(arangodb::Transaction*, char const*);

  int resize(arangodb::Transaction*, size_t);

  static uint64_t calculateHash(arangodb::Transaction*, char const*);

  static uint64_t calculateHash(arangodb::Transaction*, char const*,
                                size_t);

  void invokeOnAllElements(std::function<void(TRI_doc_mptr_t*)>);

  bool supportsFilterCondition(arangodb::aql::AstNode const*,
                               arangodb::aql::Variable const*, size_t, size_t&,
                               double&) const override;

  IndexIterator* iteratorForCondition(arangodb::Transaction*,
                                      IndexIteratorContext*,
                                      arangodb::aql::Ast*,
                                      arangodb::aql::AstNode const*,
                                      arangodb::aql::Variable const*,
                                      bool) const override;

  arangodb::aql::AstNode* specializeCondition(
      arangodb::aql::AstNode*, arangodb::aql::Variable const*) const override;

  
 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief create the iterator
  //////////////////////////////////////////////////////////////////////////////

  IndexIterator* createIterator(
      arangodb::Transaction*, IndexIteratorContext*,
      arangodb::aql::AstNode const*,
      std::vector<arangodb::aql::AstNode const*> const&) const;

  
 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief the actual index
  //////////////////////////////////////////////////////////////////////////////

  TRI_PrimaryIndex_t* _primaryIndex;
};
}

#endif


