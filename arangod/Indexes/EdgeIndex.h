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

#ifndef ARANGOD_INDEXES_EDGE_INDEX_H
#define ARANGOD_INDEXES_EDGE_INDEX_H 1

#include "Basics/Common.h"
#include "Basics/AssocMulti.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "VocBase/edge-collection.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"

namespace arangodb {
namespace aql {
class SortCondition;
}

class EdgeIndexIterator final : public IndexIterator {
 public:
  typedef arangodb::basics::AssocMulti<TRI_edge_header_t, TRI_doc_mptr_t,
                                       uint32_t, true> TRI_EdgeIndexHash_t;

  TRI_doc_mptr_t* next() override;

  void reset() override;

  EdgeIndexIterator(arangodb::Transaction* trx,
                    TRI_EdgeIndexHash_t const* index,
                    std::vector<TRI_edge_header_t>& searchValues)
      : _trx(trx),
        _index(index),
        _keys(std::move(searchValues)),
        _position(0),
        _last(nullptr),
        _buffer(nullptr),
        _posInBuffer(0),
        _batchSize(50) {  // This might be adjusted
  }

  ~EdgeIndexIterator() {
    // Free the vector space, not the content
    delete _buffer;
  }

 private:
  arangodb::Transaction* _trx;
  TRI_EdgeIndexHash_t const* _index;
  std::vector<TRI_edge_header_t> _keys;
  size_t _position;
  TRI_doc_mptr_t* _last;
  std::vector<TRI_doc_mptr_t*>* _buffer;
  size_t _posInBuffer;
  size_t _batchSize;
};

class EdgeIndex final : public Index {
 public:
  EdgeIndex() = delete;

  EdgeIndex(TRI_idx_iid_t, struct TRI_document_collection_t*);

  explicit EdgeIndex(VPackSlice const&);

  ~EdgeIndex();

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief typedef for hash tables
  //////////////////////////////////////////////////////////////////////////////

  typedef arangodb::basics::AssocMulti<TRI_edge_header_t, TRI_doc_mptr_t,
                                       uint32_t, true> TRI_EdgeIndexHash_t;

 public:
  IndexType type() const override final {
    return Index::TRI_IDX_TYPE_EDGE_INDEX;
  }

  bool isSorted() const override final { return false; }

  bool hasSelectivityEstimate() const override final { return true; }

  double selectivityEstimate() const override final;

  bool dumpFields() const override final { return true; }

  size_t memory() const override final;

  void toVelocyPack(VPackBuilder&, bool) const override final;

  void toVelocyPackFigures(VPackBuilder&) const override final;

  int insert(arangodb::Transaction*, struct TRI_doc_mptr_t const*,
             bool) override final;

  int remove(arangodb::Transaction*, struct TRI_doc_mptr_t const*,
             bool) override final;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief looks up edges using the index, restarting at the edge pointed at
  /// by next
  //////////////////////////////////////////////////////////////////////////////

  void lookup(arangodb::Transaction*, TRI_edge_index_iterator_t const*,
              std::vector<TRI_doc_mptr_copy_t>&, TRI_doc_mptr_t*&, size_t);

  int batchInsert(arangodb::Transaction*,
                  std::vector<TRI_doc_mptr_t const*> const*,
                  size_t) override final;

  int sizeHint(arangodb::Transaction*, size_t) override final;

  bool hasBatchInsert() const override final { return true; }

  TRI_EdgeIndexHash_t* from() { return _edgesFrom; }

  TRI_EdgeIndexHash_t* to() { return _edgesTo; }

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
  /// @brief the hash table for _from
  //////////////////////////////////////////////////////////////////////////////

  TRI_EdgeIndexHash_t* _edgesFrom;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the hash table for _to
  //////////////////////////////////////////////////////////////////////////////

  TRI_EdgeIndexHash_t* _edgesTo;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief number of buckets effectively used by the index
  //////////////////////////////////////////////////////////////////////////////

  size_t _numBuckets;
};
}

#endif
