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

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include "Basics/Common.h"
#include "Basics/fasthash.h"
#include "Containers/AssocMulti.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "MMFiles/MMFilesIndex.h"
#include "MMFiles/MMFilesIndexElement.h"
#include "MMFiles/MMFilesIndexLookupContext.h"
#include "VocBase/Identifiers/IndexId.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

namespace arangodb {
namespace basics {
class LocalTaskQueue;
}

class MMFilesEdgeIndexIterator;

struct MMFilesEdgeIndexHelper {
  /// @brief hashes an edge key
  static inline uint64_t HashKey(VPackSlice const* key) {
    TRI_ASSERT(key != nullptr);
    // we can get away with the fast hash function here, as edge
    // index values are restricted to strings
    return MMFilesSimpleIndexElement::hash(*key);
  }

  /// @brief hashes an edge
  static inline uint64_t HashElement(MMFilesSimpleIndexElement const& element, bool byKey) {
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
    MMFilesIndexLookupContext* context = static_cast<MMFilesIndexLookupContext*>(userData);
    TRI_ASSERT(context != nullptr);

    try {
      VPackSlice tmp = right.slice(context);
      TRI_ASSERT(tmp.isString());
      return left->binaryEquals(tmp);
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
    MMFilesIndexLookupContext* context = static_cast<MMFilesIndexLookupContext*>(userData);
    try {
      VPackSlice lSlice = left.slice(context);
      VPackSlice rSlice = right.slice(context);

      TRI_ASSERT(lSlice.isString());
      TRI_ASSERT(rSlice.isString());

      return lSlice.binaryEquals(rSlice);
    } catch (...) {
      return false;
    }
  }
};

class MMFilesEdgeIndex final : public MMFilesIndex {
 public:
  using ImplType =
      ::arangodb::containers::AssocMulti<arangodb::velocypack::Slice, MMFilesSimpleIndexElement,
                                         uint32_t, false, MMFilesEdgeIndexHelper>;

  MMFilesEdgeIndex() = delete;

  MMFilesEdgeIndex(IndexId iid, arangodb::LogicalCollection& collection);

 public:
  IndexType type() const override { return Index::TRI_IDX_TYPE_EDGE_INDEX; }

  char const* typeName() const override { return "edge"; }

  bool canBeDropped() const override { return false; }

  bool isSorted() const override { return false; }

  bool hasSelectivityEstimate() const override { return true; }

  double selectivityEstimate(arangodb::velocypack::StringRef const& = arangodb::velocypack::StringRef()) const override;

  size_t memory() const override;

  void toVelocyPack(VPackBuilder&, std::underlying_type<Index::Serialize>::type) const override;

  void toVelocyPackFigures(VPackBuilder&) const override;

  void batchInsert(transaction::Methods& trx,
                   std::vector<std::pair<LocalDocumentId, velocypack::Slice>> const& docs,
                   std::shared_ptr<arangodb::basics::LocalTaskQueue> queue) override;

  Result insert(transaction::Methods& trx, LocalDocumentId const& documentId,
                velocypack::Slice const& doc, Index::OperationMode mode) override;

  Result remove(transaction::Methods& trx, LocalDocumentId const& documentId,
                velocypack::Slice const& doc, Index::OperationMode mode) override;

  void load() override {}
  void unload() override;

  Result sizeHint(transaction::Methods& trx, size_t size) override;

  MMFilesEdgeIndex::ImplType* from() const { return _edgesFrom.get(); }

  MMFilesEdgeIndex::ImplType* to() const { return _edgesTo.get(); }

  Index::FilterCosts supportsFilterCondition(std::vector<std::shared_ptr<arangodb::Index>> const& allIndexes,
                                             arangodb::aql::AstNode const* node,
                                             arangodb::aql::Variable const* reference, 
                                             size_t itemsInIndex) const override;

  std::unique_ptr<IndexIterator> iteratorForCondition(transaction::Methods* trx, 
                                                      arangodb::aql::AstNode const* node,
                                                      arangodb::aql::Variable const* reference,
                                                      IndexIteratorOptions const& opts) override;

  arangodb::aql::AstNode* specializeCondition(arangodb::aql::AstNode* node,
                                              arangodb::aql::Variable const* reference) const override;

 private:
  /// @brief create the iterator
  std::unique_ptr<IndexIterator> createEqIterator(transaction::Methods*, 
                                                  arangodb::aql::AstNode const*,
                                                  arangodb::aql::AstNode const*) const;

  std::unique_ptr<IndexIterator> createInIterator(transaction::Methods*, 
                                                  arangodb::aql::AstNode const*,
                                                  arangodb::aql::AstNode const*) const;

  /// @brief add a single value node to the iterator's keys
  void handleValNode(VPackBuilder* keys, arangodb::aql::AstNode const* valNode) const;

  MMFilesSimpleIndexElement buildFromElement(LocalDocumentId const& documentId,
                                             arangodb::velocypack::Slice const& doc) const;
  MMFilesSimpleIndexElement buildToElement(LocalDocumentId const& documentId,
                                           arangodb::velocypack::Slice const& doc) const;

 private:
  /// @brief the hash table for _from
  std::unique_ptr<MMFilesEdgeIndex::ImplType> _edgesFrom;

  /// @brief the hash table for _to
  std::unique_ptr<MMFilesEdgeIndex::ImplType> _edgesTo;
};

class MMFilesEdgeIndexIterator final : public IndexIterator {
 public:
  MMFilesEdgeIndexIterator(LogicalCollection* collection, transaction::Methods* trx,
                           arangodb::MMFilesEdgeIndex const* index,
                           MMFilesEdgeIndex::ImplType const* indexImpl,
                           std::unique_ptr<VPackBuilder> keys);

  ~MMFilesEdgeIndexIterator();

  char const* typeName() const override { return "edge-index-iterator"; }

  bool next(LocalDocumentIdCallback const& cb, size_t limit) override;
  bool nextDocument(DocumentCallback const& cb, size_t limit) override;

  void reset() override;

 private:
  MMFilesEdgeIndex::ImplType const* _index;
  MMFilesIndexLookupContext _context;
  std::unique_ptr<arangodb::velocypack::Builder> _keys;
  arangodb::velocypack::ArrayIterator _iterator;
  std::vector<MMFilesSimpleIndexElement> _buffer;
  size_t _posInBuffer;
  size_t _batchSize;
  MMFilesSimpleIndexElement _lastElement;
  std::vector<std::pair<LocalDocumentId, uint8_t const*>> _documentIds;
};

}  // namespace arangodb

#endif
