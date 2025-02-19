////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
class LogicalCollection;
}
namespace arangodb::iresearch {

class IResearchInvertedIndexMock final : public Index,
                                         public IResearchInvertedIndex {
  Index& index() noexcept final { return *this; }
  Index const& index() const noexcept final { return *this; }

 public:
  IResearchInvertedIndexMock(
      IndexId iid, arangodb::LogicalCollection& collection,
      const std::string& idxName,
      std::vector<std::vector<arangodb::basics::AttributeName>> const&
          attributes,
      bool unique, bool sparse);

  ~IResearchInvertedIndexMock() final { unload(); }

  [[nodiscard]] static auto setCallbackForScope(
      std::function<irs::directory_attributes()> callback) {
    InitCallback = callback;
    return irs::Finally{[]() noexcept { InitCallback = nullptr; }};
  }

  void toVelocyPack(
      VPackBuilder& builder,
      std::underlying_type<Index::Serialize>::type flags) const final;

  void toVelocyPackFigures(velocypack::Builder& builder) const final {
    IResearchDataStore::toVelocyPackStats(builder);
  }

  IndexType type() const final;

  // CHECK IT
  bool needsReversal() const final;

  size_t memory() const final;

  bool isHidden() const final;

  char const* typeName() const final;

  bool canBeDropped() const final;

  bool isSorted() const final;

  bool hasSelectivityEstimate() const final;

  bool inProgress() const final;

  bool covers(arangodb::aql::Projections& projections) const final;

  Result drop() final;

  void load() final;

  std::unique_ptr<IndexIterator> iteratorForCondition(
      ResourceMonitor& monitor, transaction::Methods* trx,
      aql::AstNode const* node, aql::Variable const* reference,
      IndexIteratorOptions const& opts, ReadOwnWrites readOwnWrites,
      int mutableConditionIdx) final;

  Index::SortCosts supportsSortCondition(
      aql::SortCondition const* sortCondition, aql::Variable const* reference,
      size_t itemsInIndex) const final;

  Index::FilterCosts supportsFilterCondition(
      transaction::Methods& trx,
      std::vector<std::shared_ptr<Index>> const& allIndexes,
      aql::AstNode const* node, aql::Variable const* reference,
      size_t itemsInIndex) const final;

  aql::AstNode* specializeCondition(transaction::Methods& trx,
                                    aql::AstNode* node,
                                    aql::Variable const* reference) const final;

  Result insert(transaction::Methods& trx, LocalDocumentId documentId,
                velocypack::Slice doc);

  AnalyzerPool::ptr findAnalyzer(AnalyzerPool const& analyzer) const final;

  void unload() final;

  void invalidateQueryCache(TRI_vocbase_t* vocbase) final;

  static std::function<irs::directory_attributes()> InitCallback;
};

}  // namespace arangodb::iresearch
