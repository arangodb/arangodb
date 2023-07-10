////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "IResearch/IResearchInvertedIndex.h"
#include "RocksDBEngine/RocksDBIndex.h"

namespace arangodb {

struct ResourceMonitor;

namespace iresearch {

class IResearchRocksDBInvertedIndexFactory : public IndexTypeFactory {
 public:
  explicit IResearchRocksDBInvertedIndexFactory(ArangodServer& server);

  bool equal(velocypack::Slice lhs, velocypack::Slice rhs,
             std::string const& dbname) const final;

  /// @brief instantiate an Index definition
  std::shared_ptr<Index> instantiate(LogicalCollection& collection,
                                     velocypack::Slice definition, IndexId id,
                                     bool isClusterConstructor) const final;

  /// @brief normalize an Index definition prior to instantiation/persistence
  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   TRI_vocbase_t const& vocbase) const final;

  bool attributeOrderMatters() const final { return false; }
};

class IResearchRocksDBInvertedIndex final : public RocksDBIndex,
                                            public IResearchInvertedIndex {
  Index& index() noexcept final { return *this; }
  Index const& index() const noexcept final { return *this; }

 public:
  IResearchRocksDBInvertedIndex(IndexId id, LogicalCollection& collection,
                                uint64_t objectId, std::string const& name);

  IndexType type() const final { return Index::TRI_IDX_TYPE_INVERTED_INDEX; }

  std::string getCollectionName() const;
  std::string const& getShardName() const noexcept;

  void insertMetrics() final;
  void removeMetrics() final;

  void toVelocyPackFigures(velocypack::Builder& builder) const final {
    IResearchDataStore::toVelocyPackStats(builder);
  }

  void toVelocyPack(VPackBuilder& builder,
                    std::underlying_type_t<Index::Serialize> flags) const final;

  size_t memory() const final { return stats().indexSize; }

  bool isHidden() const final { return false; }

  bool needsReversal() const final { return true; }

  char const* typeName() const final { return oldtypeName(); }

  bool canBeDropped() const final { return IResearchDataStore::canBeDropped(); }

  bool isSorted() const final { return IResearchInvertedIndex::isSorted(); }

  bool hasSelectivityEstimate() const final {
    return IResearchDataStore::hasSelectivityEstimate();
  }

  bool inProgress() const final { return false; }

  bool covers(aql::Projections& projections) const final {
    return IResearchInvertedIndex::covers(projections);
  }

  std::vector<std::vector<basics::AttributeName>> const& coveredFields()
      const final {
    return IResearchInvertedIndex::coveredFields();
  }

  Result drop() final /*noexcept*/ { return deleteDataStore(); }

  void load() final {}
  void unload() final /*noexcept*/ { shutdownDataStore(); }

  ResultT<TruncateGuard> truncateBegin(rocksdb::WriteBatch& batch) final {
    auto r = RocksDBIndex::truncateBegin(batch);
    if (!r.ok()) {
      return r;
    }
    return IResearchDataStore::truncateBegin();
  }

  void truncateCommit(TruncateGuard&& guard, TRI_voc_tick_t tick,
                      transaction::Methods* trx) final {
    IResearchDataStore::truncateCommit(std::move(guard), tick, trx);
  }

  bool matchesDefinition(velocypack::Slice const& other) const final;

  std::unique_ptr<IndexIterator> iteratorForCondition(
      ResourceMonitor& monitor, transaction::Methods* trx,
      aql::AstNode const* node, aql::Variable const* reference,
      IndexIteratorOptions const& opts, ReadOwnWrites readOwnWrites,
      int mutableConditionIdx) final {
    TRI_ASSERT(readOwnWrites ==
               ReadOwnWrites::no);  // FIXME: check - should we ever care?

    return IResearchInvertedIndex::iteratorForCondition(
        monitor, &collection(), trx, node, reference, opts,
        mutableConditionIdx);
  }

  Index::SortCosts supportsSortCondition(
      aql::SortCondition const* sortCondition, aql::Variable const* reference,
      size_t itemsInIndex) const final {
    return IResearchInvertedIndex::supportsSortCondition(
        sortCondition, reference, itemsInIndex);
  }

  Index::FilterCosts supportsFilterCondition(
      transaction::Methods& trx,
      std::vector<std::shared_ptr<Index>> const& allIndexes,
      aql::AstNode const* node, aql::Variable const* reference,
      size_t itemsInIndex) const final {
    return IResearchInvertedIndex::supportsFilterCondition(
        trx, id(), RocksDBIndex::fields(), allIndexes, node, reference,
        itemsInIndex);
  }

  aql::AstNode* specializeCondition(
      transaction::Methods& trx, aql::AstNode* node,
      aql::Variable const* reference) const final {
    return IResearchInvertedIndex::specializeCondition(trx, node, reference);
  }

  void recoveryInsert(uint64_t tick, LocalDocumentId documentId,
                      VPackSlice doc) {
    IResearchDataStore::recoveryInsert<
        FieldIterator<IResearchInvertedIndexMetaIndexingContext>,
        IResearchInvertedIndexMetaIndexingContext>(tick, documentId, doc,
                                                   *meta()._indexingContext);
  }

  Result insert(transaction::Methods& trx, RocksDBMethods* /*methods*/,
                LocalDocumentId const& documentId, VPackSlice doc,
                OperationOptions const& /*options*/,
                bool /*performChecks*/) final {
    return IResearchDataStore::insert<
        FieldIterator<IResearchInvertedIndexMetaIndexingContext>,
        IResearchInvertedIndexMetaIndexingContext>(trx, documentId, doc,
                                                   *meta()._indexingContext);
  }

  Result remove(transaction::Methods& trx, RocksDBMethods*,
                LocalDocumentId const& documentId, VPackSlice,
                OperationOptions const& /*options*/) final {
    return IResearchDataStore::remove(trx, documentId);
  }

 private:
  // required for calling initFields()
  friend class iresearch::IResearchRocksDBInvertedIndexFactory;

  void initFields() {
    TRI_ASSERT(_fields.empty());
    *const_cast<std::vector<std::vector<basics::AttributeName>>*>(&_fields) =
        IResearchInvertedIndex::fields(meta());
  }
};

}  // namespace iresearch
}  // namespace arangodb
