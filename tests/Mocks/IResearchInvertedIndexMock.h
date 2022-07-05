////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Alexey Bakharew
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "IResearch/IResearchInvertedIndex.h"
#include "IResearch/IResearchInvertedIndexMeta.h"
#include "Indexes/Index.h"
#include "Indexes/SortedIndexAttributeMatcher.h"

namespace arangodb {
namespace iresearch {

class IResearchInvertedIndexMock final : public Index,
                                         public IResearchInvertedIndex {
public:
  IResearchInvertedIndexMock(
      IndexId iid, arangodb::LogicalCollection &collection,
      const std::string &idxName,
      std::vector<std::vector<arangodb::basics::AttributeName>> const
          &attributes,
      bool unique, bool sparse);

  virtual ~IResearchInvertedIndexMock(){};

  IndexType type() const override { return Index::TRI_IDX_TYPE_INVERTED_INDEX; }

  bool needsReversal() const override { return false; }

  void toVelocyPack(
      VPackBuilder &builder,
      std::underlying_type<Index::Serialize>::type flags) const override {
    return Index::toVelocyPack(builder, flags);
  }

  size_t memory() const override {
    // FIXME return in memory size
    return stats().indexSize;
  }

  bool isHidden() const override { return false; }

  char const *typeName() const override { return "inverted"; }

  bool canBeDropped() const override { return false; }

  bool isSorted() const override { return IResearchInvertedIndex::isSorted(); }

  bool hasSelectivityEstimate() const override {
    return IResearchDataStore::hasSelectivityEstimate();
  }

  bool inProgress() const override {
    return IResearchInvertedIndex::inProgress();
  }

  bool covers(arangodb::aql::Projections &projections) const override {
    return IResearchInvertedIndex::covers(projections);
  }

  Result drop() override { return {}; }

  void load() override {}

  void afterTruncate(TRI_voc_tick_t tick, transaction::Methods *trx) override {
    return IResearchDataStore::afterTruncate(tick, trx);
  }

  bool
  matchesDefinition(arangodb::velocypack::Slice const &other) const override {
    return IResearchInvertedIndex::matchesFieldsDefinition(other);
  }

  std::unique_ptr<IndexIterator> iteratorForCondition(
      transaction::Methods *trx, aql::AstNode const *node,
      aql::Variable const *reference, IndexIteratorOptions const &opts,
      ReadOwnWrites readOwnWrites, int mutableConditionIdx) override {
    TRI_ASSERT(readOwnWrites ==
               ReadOwnWrites::no); // FIXME: check - should we ever care?
    return IResearchInvertedIndex::iteratorForCondition(
        &IResearchDataStore::collection(), trx, node, reference, opts,
        mutableConditionIdx);
  }

  Index::SortCosts
  supportsSortCondition(aql::SortCondition const *sortCondition,
                        aql::Variable const *reference,
                        size_t itemsInIndex) const override {
    return IResearchInvertedIndex::supportsSortCondition(
        sortCondition, reference, itemsInIndex);
  }

  Index::FilterCosts
  supportsFilterCondition(std::vector<std::shared_ptr<Index>> const &allIndexes,
                          aql::AstNode const *node,
                          aql::Variable const *reference,
                          size_t itemsInIndex) const override {

    return IResearchInvertedIndex::supportsFilterCondition(
        IResearchDataStore::id(), this->_fields, allIndexes, node, reference,
        itemsInIndex);
  }

  aql::AstNode *
  specializeCondition(aql::AstNode *node,
                      aql::Variable const *reference) const override {
    return IResearchInvertedIndex::specializeCondition(node, reference);
  }

  Result insert(transaction::Methods &trx, LocalDocumentId const &documentId,
                velocypack::Slice const doc) {
    IResearchInvertedIndexMeta meta;
    using InvertedIndexFieldIterator = arangodb::iresearch::FieldIterator<
        arangodb::iresearch::IResearchInvertedIndexMeta,
        arangodb::iresearch::InvertedIndexField>;

    return IResearchDataStore::insert<InvertedIndexFieldIterator,
                                      IResearchInvertedIndexMeta>(
        trx, documentId, doc, meta);
  }

  AnalyzerPool::ptr findAnalyzer(AnalyzerPool const &analyzer) const override {
    return nullptr;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a JSON description of a IResearchLink object
  /// @param withFigures output 'figures' section with e.g. memory size
  ////////////////////////////////////////////////////////////////////////////////

  void toVelocyPackFigures(velocypack::Builder &builder) const override {
    IResearchInvertedIndex::toVelocyPackStats(builder);
  }

  void unload() override { shutdownDataStore(); }

  void invalidateQueryCache(TRI_vocbase_t *vocbase) override {
    return IResearchInvertedIndex::invalidateQueryCache(vocbase);
  }

  irs::comparer const *getComparator() const noexcept override {
    return IResearchInvertedIndex::getComparator();
  }
};

} // namespace iresearch
} // namespace arangodb
