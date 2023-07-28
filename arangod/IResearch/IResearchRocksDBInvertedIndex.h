////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include "IResearchInvertedIndex.h"
#include "RocksDBEngine/RocksDBIndex.h"

namespace arangodb {
namespace iresearch {

class IResearchRocksDBInvertedIndexFactory : public IndexTypeFactory {
 public:
  explicit IResearchRocksDBInvertedIndexFactory(ArangodServer& server);

  bool equal(velocypack::Slice lhs, velocypack::Slice rhs,
             std::string const& dbname) const override;

  /// @brief instantiate an Index definition
  std::shared_ptr<Index> instantiate(LogicalCollection& collection,
                                     velocypack::Slice definition, IndexId id,
                                     bool isClusterConstructor) const override;

  /// @brief normalize an Index definition prior to instantiation/persistence
  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   TRI_vocbase_t const& vocbase) const override;

  bool attributeOrderMatters() const override { return false; }
};

class IResearchRocksDBInvertedIndex final : public IResearchInvertedIndex,
                                            public RocksDBIndex {
 public:
  IResearchRocksDBInvertedIndex(IndexId id, LogicalCollection& collection,
                                uint64_t objectId, std::string const& name);

  Index::IndexType type() const override {
    return Index::TRI_IDX_TYPE_INVERTED_INDEX;
  }

  std::string getCollectionName() const;
  std::string const& getShardName() const noexcept;

  void insertMetrics() final;
  void removeMetrics() final;

  void toVelocyPackFigures(velocypack::Builder& builder) const final {
    IResearchDataStore::toVelocyPackStats(builder);
  }

  void toVelocyPack(
      VPackBuilder& builder,
      std::underlying_type<Index::Serialize>::type flags) const override;

  size_t memory() const override { return stats().indexSize; }

  bool isHidden() const override { return false; }

  bool needsReversal() const override { return true; }

  char const* typeName() const override { return oldtypeName(); }

  bool canBeDropped() const override { return true; }

  bool isSorted() const override { return IResearchInvertedIndex::isSorted(); }

  bool hasSelectivityEstimate() const override { return false; }

  bool inProgress() const override {
    return IResearchInvertedIndex::inProgress();
  }

  bool covers(aql::Projections& projections) const override {
    return IResearchInvertedIndex::covers(projections);
  }

  Result drop() override;

  void load() override {}
  void unload() override;

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
    guard = {};
  }

  bool matchesDefinition(velocypack::Slice const& other) const override;

  std::unique_ptr<IndexIterator> iteratorForCondition(
      transaction::Methods* trx, aql::AstNode const* node,
      aql::Variable const* reference, IndexIteratorOptions const& opts,
      ReadOwnWrites readOwnWrites, int mutableConditionIdx) override {
    TRI_ASSERT(readOwnWrites ==
               ReadOwnWrites::no);  // FIXME: check - should we ever care?
    return IResearchInvertedIndex::iteratorForCondition(
        &IResearchDataStore::collection(), trx, node, reference, opts,
        mutableConditionIdx);
  }

  Index::SortCosts supportsSortCondition(
      aql::SortCondition const* sortCondition, aql::Variable const* reference,
      size_t itemsInIndex) const override {
    return IResearchInvertedIndex::supportsSortCondition(
        sortCondition, reference, itemsInIndex);
  }

  Index::FilterCosts supportsFilterCondition(
      transaction::Methods& trx,
      std::vector<std::shared_ptr<Index>> const& allIndexes,
      aql::AstNode const* node, aql::Variable const* reference,
      size_t itemsInIndex) const override {
    return IResearchInvertedIndex::supportsFilterCondition(
        trx, IResearchDataStore::id(), RocksDBIndex::fields(), allIndexes, node,
        reference, itemsInIndex);
  }

  aql::AstNode* specializeCondition(
      transaction::Methods& trx, aql::AstNode* node,
      aql::Variable const* reference) const override {
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
                bool /*performChecks*/) override {
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
