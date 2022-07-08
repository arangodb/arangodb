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

#include "Basics/Common.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "Indexes/IndexFactory.h"
#include "IResearchCommon.h"
#include "IResearchDataStore.h"
#include "IResearchInvertedIndexMeta.h"
#include "search/boolean_filter.hpp"
#include "search/bitset_doc_iterator.hpp"
#include "search/score.hpp"
#include "search/conjunction.hpp"
#include "search/cost.hpp"
#include <search/proxy_filter.hpp>

namespace arangodb {
namespace iresearch {
class IResearchInvertedIndex : public IResearchDataStore {
 public:
  explicit IResearchInvertedIndex(IndexId iid, LogicalCollection& collection);

  virtual ~IResearchInvertedIndex() = default;

  void toVelocyPack(ArangodServer& server, TRI_vocbase_t const* defaultVocbase,
                    velocypack::Builder& builder, bool forPersistence) const;

  bool isSorted() const { return !_meta._sort.empty(); }

  Result init(VPackSlice definition, bool& pathExists,
              InitCallback const& initCallback = {});

  static std::vector<std::vector<arangodb::basics::AttributeName>> fields(
      IResearchInvertedIndexMeta const& meta);
  static std::vector<std::vector<arangodb::basics::AttributeName>> sortedFields(
      IResearchInvertedIndexMeta const& meta);

  bool matchesFieldsDefinition(VPackSlice other,
                               LogicalCollection const& collection) const;

  AnalyzerPool::ptr findAnalyzer(AnalyzerPool const& analyzer) const override;

  bool inProgress() const { return false; }

  bool covers(arangodb::aql::Projections& projections) const;

  std::unique_ptr<IndexIterator> iteratorForCondition(
      LogicalCollection* collection, transaction::Methods* trx,
      aql::AstNode const* node, aql::Variable const* reference,
      IndexIteratorOptions const& opts, int mutableConditionIdx);

  Index::SortCosts supportsSortCondition(
      aql::SortCondition const* sortCondition, aql::Variable const* reference,
      size_t itemsInIndex) const;

  Index::FilterCosts supportsFilterCondition(
      IndexId id,
      std::vector<std::vector<arangodb::basics::AttributeName>> const& fields,
      std::vector<std::shared_ptr<Index>> const& allIndexes,
      aql::AstNode const* node, aql::Variable const* reference,
      size_t itemsInIndex) const;

  aql::AstNode* specializeCondition(aql::AstNode* node,
                                    aql::Variable const* reference) const;

  IResearchInvertedIndexMeta const& meta() const noexcept { return _meta; }

 protected:
  void invalidateQueryCache(TRI_vocbase_t* vocbase) override;

  irs::comparer const* getComparator() const noexcept override {
    return &_comparer;
  }

 private:
  IResearchInvertedIndexMeta _meta;
  VPackComparer<IResearchInvertedIndexSort> _comparer;
};

class IResearchInvertedClusterIndex : public IResearchInvertedIndex,
                                      public Index {
 public:
  Index::IndexType type() const override {
    return Index::TRI_IDX_TYPE_INVERTED_INDEX;
  }

  void toVelocyPack(
      VPackBuilder& builder,
      std::underlying_type<Index::Serialize>::type flags) const override;

  size_t memory() const override {
    // FIXME return in memory size
    // return stats().indexSize;
    return 0;
  }

  bool isHidden() const override { return false; }

  char const* typeName() const override { return oldtypeName(); }

  bool canBeDropped() const override { return true; }

  bool isSorted() const override { return IResearchInvertedIndex::isSorted(); }

  bool hasSelectivityEstimate() const override { return false; }

  bool inProgress() const override {
    return IResearchInvertedIndex::inProgress();
  }

  bool covers(arangodb::aql::Projections& projections) const override {
    return IResearchInvertedIndex::covers(projections);
  }

  Result drop() override { return {}; }
  void load() override {}
  void unload() override {}

  bool matchesDefinition(
      arangodb::velocypack::Slice const& other) const override;

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
      std::vector<std::shared_ptr<Index>> const& allIndexes,
      aql::AstNode const* node, aql::Variable const* reference,
      size_t itemsInIndex) const override {
    return IResearchInvertedIndex::supportsFilterCondition(
        IResearchDataStore::id(), Index::fields(), allIndexes, node, reference,
        itemsInIndex);
  }

  aql::AstNode* specializeCondition(
      aql::AstNode* node, aql::Variable const* reference) const override {
    return IResearchInvertedIndex::specializeCondition(node, reference);
  }

  IResearchInvertedClusterIndex(IndexId iid, uint64_t objectId,
                                LogicalCollection& collection,
                                std::string const& name)
      : IResearchInvertedIndex(iid, collection),
        Index(iid, collection, name, {}, false, true) {}

  void initFields() {
    TRI_ASSERT(_fields.empty());
    *const_cast<std::vector<std::vector<arangodb::basics::AttributeName>>*>(
        &_fields) = IResearchInvertedIndex::fields(meta());
  }
};
}  // namespace iresearch
}  // namespace arangodb
