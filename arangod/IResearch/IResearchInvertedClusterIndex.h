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

namespace arangodb::iresearch {

class IResearchInvertedClusterIndex final : public Index,
                                            public IResearchInvertedIndex {
  Index& index() noexcept final { return *this; }
  Index const& index() const noexcept final { return *this; }

 public:
  IndexType type() const final { return Index::TRI_IDX_TYPE_INVERTED_INDEX; }

  ~IResearchInvertedClusterIndex() final {
    // should be in final dtor, otherwise its vtable already destroyed
    unload();
  }

  void toVelocyPack(
      VPackBuilder& builder,
      std::underlying_type<Index::Serialize>::type flags) const final;

  void toVelocyPackFigures(velocypack::Builder& builder) const final {
    IResearchDataStore::toVelocyPackStats(builder);
  }

  std::string getCollectionName() const;

  Stats stats() const final;

  size_t memory() const final { return stats().indexSize; }

  bool isHidden() const final { return false; }

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

  Result drop() final {
    unload();
    return {};
  }
  void load() final {}
  void unload() final /*noexcept*/ { _asyncSelf->reset(); }

  bool matchesDefinition(velocypack::Slice const& other) const final;

  std::unique_ptr<IndexIterator> iteratorForCondition(
      ResourceMonitor& monitor, transaction::Methods* trx,
      aql::AstNode const* node, aql::Variable const* reference,
      IndexIteratorOptions const& opts, ReadOwnWrites readOwnWrites,
      int mutableConditionIdx) final {
    // FIXME: check - should we ever care?
    TRI_ASSERT(readOwnWrites == ReadOwnWrites::no);
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
        trx, id(), Index::fields(), allIndexes, node, reference, itemsInIndex);
  }

  aql::AstNode* specializeCondition(
      transaction::Methods& trx, aql::AstNode* node,
      aql::Variable const* reference) const final {
    return IResearchInvertedIndex::specializeCondition(trx, node, reference);
  }

  IResearchInvertedClusterIndex(IndexId iid, uint64_t /*objectId*/,
                                LogicalCollection& collection,
                                std::string const& name);

  void initFields() {
    TRI_ASSERT(_fields.empty());
    *const_cast<std::vector<std::vector<basics::AttributeName>>*>(&_fields) =
        IResearchInvertedIndex::fields(meta());
  }
};

}  // namespace arangodb::iresearch
