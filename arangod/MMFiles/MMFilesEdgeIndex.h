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

#ifndef ARANGOD_MMFILES_MMFILES_EDGE_INDEX_H
#define ARANGOD_MMFILES_MMFILES_EDGE_INDEX_H 1

#include "Basics/AssocMulti.h"
#include "Basics/Common.h"
#include "Basics/fasthash.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "Indexes/IndexLookupContext.h"
#include "MMFiles/MMFilesIndex.h"
#include "MMFiles/MMFilesIndexElement.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

namespace arangodb {
namespace basics {
class LocalTaskQueue;
}

class ManagedDocumentResult;
class MMFilesEdgeIndex;

struct MMFilesEdgeIndexHelper {
  /// @brief hashes an edge key
  static inline uint64_t HashKey(void*, VPackSlice const* key) {
    TRI_ASSERT(key != nullptr);
    // we can get away with the fast hash function here, as edge
    // index values are restricted to strings
    return MMFilesSimpleIndexElement::hash(*key);
  }

  /// @brief hashes an edge
  static inline uint64_t HashElement(void*, MMFilesSimpleIndexElement const& element,
                                     bool byKey) {
    if (byKey) {
      return element.hash();
    }

    uint64_t documentId = element.localDocumentIdValue();
    return fasthash64_uint64(documentId, 0x56781234);
  }

  /// @brief checks if key and element match
  inline bool IsEqualKeyElement(void* userData, VPackSlice const* left,
                                MMFilesSimpleIndexElement const& right) const {
    TRI_ASSERT(left != nullptr);
    IndexLookupContext* context = static_cast<IndexLookupContext*>(userData);
    TRI_ASSERT(context != nullptr);

    try {
      VPackSlice tmp = right.slice(context);
      TRI_ASSERT(tmp.isString());
      return left->equals(tmp);
    } catch (...) {
      return false;
    }
  }

  /// @brief checks for elements are equal
  inline bool IsEqualElementElement(void*, MMFilesSimpleIndexElement const& left,
                                    MMFilesSimpleIndexElement const& right) const {
    return left.localDocumentId() == right.localDocumentId();
  }

  /// @brief checks for elements are equal
  inline bool IsEqualElementElementByKey(void* userData,
                                         MMFilesSimpleIndexElement const& left,
                                         MMFilesSimpleIndexElement const& right) const {
    IndexLookupContext* context = static_cast<IndexLookupContext*>(userData);
    try {
      VPackSlice lSlice = left.slice(context);
      VPackSlice rSlice = right.slice(context);

      TRI_ASSERT(lSlice.isString());
      TRI_ASSERT(rSlice.isString());

      return lSlice.equals(rSlice);
    } catch (...) {
      return false;
    }
  }
};

typedef arangodb::basics::AssocMulti<arangodb::velocypack::Slice,
                                     MMFilesSimpleIndexElement, uint32_t, false, MMFilesEdgeIndexHelper>
    TRI_MMFilesEdgeIndexHash_t;

class MMFilesEdgeIndexIterator final : public IndexIterator {
 public:
  MMFilesEdgeIndexIterator(LogicalCollection* collection,
                           transaction::Methods* trx,
                           ManagedDocumentResult* mmdr,
                           arangodb::MMFilesEdgeIndex const* index,
                           TRI_MMFilesEdgeIndexHash_t const* indexImpl,
                           std::unique_ptr<VPackBuilder> keys);

  ~MMFilesEdgeIndexIterator();

  char const* typeName() const override { return "edge-index-iterator"; }

  bool next(LocalDocumentIdCallback const& cb, size_t limit) override;

  void reset() override;

 private:
  TRI_MMFilesEdgeIndexHash_t const* _index;
  ManagedDocumentResult* _mmdr;
  IndexLookupContext _context;
  std::unique_ptr<arangodb::velocypack::Builder> _keys;
  arangodb::velocypack::ArrayIterator _iterator;
  std::vector<MMFilesSimpleIndexElement> _buffer;
  size_t _posInBuffer;
  size_t _batchSize;
  MMFilesSimpleIndexElement _lastElement;
};

class MMFilesEdgeIndex final : public MMFilesIndex {
 public:
  MMFilesEdgeIndex() = delete;

  MMFilesEdgeIndex(TRI_idx_iid_t, arangodb::LogicalCollection*);

 public:
  IndexType type() const override { return Index::TRI_IDX_TYPE_EDGE_INDEX; }

  char const* typeName() const override { return "edge"; }

  bool allowExpansion() const override { return false; }

  bool canBeDropped() const override { return false; }

  bool isSorted() const override { return false; }

  bool hasSelectivityEstimate() const override { return true; }

  double selectivityEstimateLocal(
      arangodb::StringRef const* = nullptr) const override;

  size_t memory() const override;

  void toVelocyPack(VPackBuilder&, bool, bool) const override;

  void toVelocyPackFigures(VPackBuilder&) const override;

  Result insert(transaction::Methods*, LocalDocumentId const& documentId,
             arangodb::velocypack::Slice const&, OperationMode mode) override;

  Result remove(transaction::Methods*, LocalDocumentId const& documentId,
             arangodb::velocypack::Slice const&, OperationMode mode) override;

  void batchInsert(transaction::Methods*,
                   std::vector<std::pair<LocalDocumentId, VPackSlice>> const&,
                   std::shared_ptr<arangodb::basics::LocalTaskQueue>) override;

  void load() override {}
  void unload() override;

  int sizeHint(transaction::Methods*, size_t) override;

  bool hasBatchInsert() const override { return true; }

  TRI_MMFilesEdgeIndexHash_t* from() { return _edgesFrom.get(); }

  TRI_MMFilesEdgeIndexHash_t* to() { return _edgesTo.get(); }

  bool supportsFilterCondition(arangodb::aql::AstNode const*,
                               arangodb::aql::Variable const*, size_t, size_t&,
                               double&) const override;

  IndexIterator* iteratorForCondition(transaction::Methods*,
                                      ManagedDocumentResult*,
                                      arangodb::aql::AstNode const*,
                                      arangodb::aql::Variable const*,
                                      bool) override;

  arangodb::aql::AstNode* specializeCondition(
      arangodb::aql::AstNode*, arangodb::aql::Variable const*) const override;

  /// @brief Transform the list of search slices to search values.
  ///        This will multiply all IN entries and simply return all other
  ///        entries.
  void expandInSearchValues(arangodb::velocypack::Slice const,
                            arangodb::velocypack::Builder&) const override;

 private:
  /// @brief create the iterator
  IndexIterator* createEqIterator(transaction::Methods*, ManagedDocumentResult*,
                                  arangodb::aql::AstNode const*,
                                  arangodb::aql::AstNode const*) const;

  IndexIterator* createInIterator(transaction::Methods*, ManagedDocumentResult*,
                                  arangodb::aql::AstNode const*,
                                  arangodb::aql::AstNode const*) const;

  /// @brief add a single value node to the iterator's keys
  void handleValNode(VPackBuilder* keys,
                     arangodb::aql::AstNode const* valNode) const;

  MMFilesSimpleIndexElement buildFromElement(
      LocalDocumentId const& documentId, arangodb::velocypack::Slice const& doc) const;
  MMFilesSimpleIndexElement buildToElement(
      LocalDocumentId const& documentId, arangodb::velocypack::Slice const& doc) const;

 private:
  /// @brief the hash table for _from
  std::unique_ptr<TRI_MMFilesEdgeIndexHash_t> _edgesFrom;

  /// @brief the hash table for _to
  std::unique_ptr<TRI_MMFilesEdgeIndexHash_t> _edgesTo;
};
}

#endif
