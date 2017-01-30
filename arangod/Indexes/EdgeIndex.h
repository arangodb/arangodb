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

#include "Basics/AssocMulti.h"
#include "Basics/Common.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

namespace arangodb {
namespace basics {
class LocalTaskQueue;
}

class EdgeIndex;

typedef arangodb::basics::AssocMulti<arangodb::velocypack::Slice,
                                     SimpleIndexElement, uint32_t, false>
    TRI_EdgeIndexHash_t;

class EdgeIndexIterator final : public IndexIterator {
 public:
  EdgeIndexIterator(LogicalCollection* collection, arangodb::Transaction* trx,
                    ManagedDocumentResult* mmdr,
                    arangodb::EdgeIndex const* index,
                    TRI_EdgeIndexHash_t const* indexImpl,
                    std::unique_ptr<VPackBuilder>& keys);

  ~EdgeIndexIterator();

  char const* typeName() const override { return "edge-index-iterator"; }

  IndexLookupResult next() override;

  void nextBabies(std::vector<IndexLookupResult>&, size_t) override;

  void reset() override;

 private:
  TRI_EdgeIndexHash_t const* _index;
  std::unique_ptr<arangodb::velocypack::Builder> _keys;
  arangodb::velocypack::ArrayIterator _iterator;
  std::vector<SimpleIndexElement> _buffer;
  size_t _posInBuffer;
  size_t _batchSize;
  SimpleIndexElement _lastElement;
};

class AnyDirectionEdgeIndexIterator final : public IndexIterator {
 public:
  AnyDirectionEdgeIndexIterator(LogicalCollection* collection,
                                arangodb::Transaction* trx,
                                ManagedDocumentResult* mmdr,
                                arangodb::EdgeIndex const* index,
                                EdgeIndexIterator* outboundIterator,
                                EdgeIndexIterator* inboundIterator);

  ~AnyDirectionEdgeIndexIterator() {
    delete _outbound;
    delete _inbound;
  }

  char const* typeName() const override { return "any-edge-index-iterator"; }

  IndexLookupResult next() override;

  void nextBabies(std::vector<IndexLookupResult>&, size_t) override;

  void reset() override;

 private:
  EdgeIndexIterator* _outbound;
  EdgeIndexIterator* _inbound;
  std::unordered_set<TRI_voc_rid_t> _seen;
  bool _useInbound;
};

class EdgeIndex final : public Index {
 public:
  EdgeIndex() = delete;

  EdgeIndex(TRI_idx_iid_t, arangodb::LogicalCollection*);

  ~EdgeIndex();

  static void buildSearchValue(TRI_edge_direction_e, std::string const&,
                               arangodb::velocypack::Builder&);

  static void buildSearchValue(TRI_edge_direction_e,
                               arangodb::velocypack::Slice const&,
                               arangodb::velocypack::Builder&);

  static void buildSearchValueFromArray(TRI_edge_direction_e,
                                        arangodb::velocypack::Slice const,
                                        arangodb::velocypack::Builder&);

 public:
  /// @brief typedef for hash tables
 public:
  IndexType type() const override { return Index::TRI_IDX_TYPE_EDGE_INDEX; }

  bool allowExpansion() const override { return false; }

  bool canBeDropped() const override { return false; }

  bool isSorted() const override { return false; }

  bool hasSelectivityEstimate() const override { return true; }

  double selectivityEstimate(
      arangodb::StringRef const* = nullptr) const override;

  size_t memory() const override;

  void toVelocyPack(VPackBuilder&, bool) const override;

  void toVelocyPackFigures(VPackBuilder&) const override;

  int insert(arangodb::Transaction*, TRI_voc_rid_t,
             arangodb::velocypack::Slice const&, bool isRollback) override;

  int remove(arangodb::Transaction*, TRI_voc_rid_t,
             arangodb::velocypack::Slice const&, bool isRollback) override;

  void batchInsert(arangodb::Transaction*,
                   std::vector<std::pair<TRI_voc_rid_t, VPackSlice>> const&,
                   arangodb::basics::LocalTaskQueue*) override;

  int unload() override;

  int sizeHint(arangodb::Transaction*, size_t) override;

  bool hasBatchInsert() const override { return true; }

  TRI_EdgeIndexHash_t* from() { return _edgesFrom; }

  TRI_EdgeIndexHash_t* to() { return _edgesTo; }

  bool supportsFilterCondition(arangodb::aql::AstNode const*,
                               arangodb::aql::Variable const*, size_t, size_t&,
                               double&) const override;

  IndexIterator* iteratorForCondition(arangodb::Transaction*,
                                      ManagedDocumentResult*,
                                      arangodb::aql::AstNode const*,
                                      arangodb::aql::Variable const*,
                                      bool) const override;

  arangodb::aql::AstNode* specializeCondition(
      arangodb::aql::AstNode*, arangodb::aql::Variable const*) const override;

  /// @brief Transform the list of search slices to search values.
  ///        This will multiply all IN entries and simply return all other
  ///        entries.
  void expandInSearchValues(arangodb::velocypack::Slice const,
                            arangodb::velocypack::Builder&) const override;

  /// @brief creates an IndexIterator for the given VelocyPackSlices.
  ///        The searchValue is a an Array with exactly two Entries, one of them
  ///        has to be NONE.
  ///        If the first is set it means we are searching for _from (OUTBOUND),
  ///        if the second is set we are searching for _to (INBOUND).
  ///        The slice that is set has to be list of keys to search for.
  ///        Each key needs to have the following formats:
  ///
  ///        1) {"eq": <compareValue>} // The value in index is exactly this
  ///
  ///        Reverse is not supported, hence ignored
  ///        NOTE: The iterator is only valid as long as the slice points to
  ///        a valid memory region.
  IndexIterator* iteratorForSlice(arangodb::Transaction*,
                                  ManagedDocumentResult*,
                                  arangodb::velocypack::Slice const,
                                  bool) const override;

 private:
  /// @brief create the iterator
  IndexIterator* createEqIterator(arangodb::Transaction*,
                                  ManagedDocumentResult*,
                                  arangodb::aql::AstNode const*,
                                  arangodb::aql::AstNode const*) const;

  IndexIterator* createInIterator(arangodb::Transaction*,
                                  ManagedDocumentResult*,
                                  arangodb::aql::AstNode const*,
                                  arangodb::aql::AstNode const*) const;

  /// @brief add a single value node to the iterator's keys
  void handleValNode(VPackBuilder* keys,
                     arangodb::aql::AstNode const* valNode) const;

  SimpleIndexElement buildFromElement(
      TRI_voc_rid_t, arangodb::velocypack::Slice const& doc) const;
  SimpleIndexElement buildToElement(
      TRI_voc_rid_t, arangodb::velocypack::Slice const& doc) const;

 private:
  /// @brief the hash table for _from
  TRI_EdgeIndexHash_t* _edgesFrom;

  /// @brief the hash table for _to
  TRI_EdgeIndexHash_t* _edgesTo;

  /// @brief number of buckets effectively used by the index
  size_t _numBuckets;
};
}

#endif
