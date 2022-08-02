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
#include "VocBase/LogicalCollection.h"

namespace arangodb {
namespace iresearch {

class IResearchInvertedIndexMock final : public Index,
                                         public IResearchInvertedIndex {
 public:
  IResearchInvertedIndexMock(
      IndexId iid, arangodb::LogicalCollection& collection,
      const std::string& idxName,
      std::vector<std::vector<arangodb::basics::AttributeName>> const&
          attributes,
      bool unique, bool sparse);

  virtual ~IResearchInvertedIndexMock(){};

  [[nodiscard]] static auto setCallbakForScope(
      std::function<irs::directory_attributes()> callback) {
    InitCallback = callback;
    return irs::make_finally([]() noexcept { InitCallback = nullptr; });
  }

  void toVelocyPack(
      VPackBuilder& builder,
      std::underlying_type<Index::Serialize>::type flags) const override;

  IndexType type() const override;

  // CHECK IT
  bool needsReversal() const override;

  size_t memory() const override;

  bool isHidden() const override;

  char const* typeName() const override;

  bool canBeDropped() const override;

  bool isSorted() const override;

  bool hasSelectivityEstimate() const override;

  bool inProgress() const override;

  bool covers(arangodb::aql::Projections& projections) const override;

  Result drop() override;

  void load() override;

  void afterTruncate(TRI_voc_tick_t tick, transaction::Methods* trx) override;

  std::unique_ptr<IndexIterator> iteratorForCondition(
      transaction::Methods* trx, aql::AstNode const* node,
      aql::Variable const* reference, IndexIteratorOptions const& opts,
      ReadOwnWrites readOwnWrites, int mutableConditionIdx) override;

  Index::SortCosts supportsSortCondition(
      aql::SortCondition const* sortCondition, aql::Variable const* reference,
      size_t itemsInIndex) const override;

  Index::FilterCosts supportsFilterCondition(
      std::vector<std::shared_ptr<Index>> const& allIndexes,
      aql::AstNode const* node, aql::Variable const* reference,
      size_t itemsInIndex) const override;

  aql::AstNode* specializeCondition(
      aql::AstNode* node, aql::Variable const* reference) const override;

  Result insert(transaction::Methods& trx, LocalDocumentId documentId,
                velocypack::Slice doc);

  AnalyzerPool::ptr findAnalyzer(AnalyzerPool const& analyzer) const override;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a JSON description of a IResearchLink object
  /// @param withFigures output 'figures' section with e.g. memory size
  ////////////////////////////////////////////////////////////////////////////////

  void toVelocyPackFigures(velocypack::Builder& builder) const override;

  void unload() override;

  void invalidateQueryCache(TRI_vocbase_t* vocbase) override;

  irs::comparer const* getComparator() const noexcept override;

  static std::function<irs::directory_attributes()> InitCallback;
};

}  // namespace iresearch
}  // namespace arangodb
