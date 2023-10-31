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

#include "Basics/Common.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "Indexes/IndexFactory.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchDataStore.h"
#include "IResearch/IResearchInvertedIndexMeta.h"

#include "search/boolean_filter.hpp"
#include "search/bitset_doc_iterator.hpp"
#include "search/score.hpp"
#include "search/conjunction.hpp"
#include "search/cost.hpp"
#include "search/proxy_filter.hpp"

namespace arangodb::iresearch {

class IResearchInvertedIndex : public IResearchDataStore {
 public:
  using IResearchDataStore::IResearchDataStore;

  void toVelocyPack(ArangodServer& server, TRI_vocbase_t const* defaultVocbase,
                    velocypack::Builder& builder,
                    bool writeAnalyzerDefinition) const;

  std::string const& getDbName() const noexcept;

  bool isSorted() const { return !_meta._sort.empty(); }

  Result init(VPackSlice definition, bool& pathExists,
              InitCallback const& initCallback = {});

  static std::vector<std::vector<basics::AttributeName>> fields(
      IResearchInvertedIndexMeta const& meta);
  static std::vector<std::vector<basics::AttributeName>> sortedFields(
      IResearchInvertedIndexMeta const& meta);

  bool matchesDefinition(VPackSlice other, TRI_vocbase_t const& vocbase) const;

  AnalyzerPool::ptr findAnalyzer(AnalyzerPool const& analyzer) const override;

  bool covers(aql::Projections& projections) const;

  std::vector<std::vector<basics::AttributeName>> const& coveredFields()
      const noexcept {
    return _coveredFields;
  }

  std::unique_ptr<IndexIterator> iteratorForCondition(
      ResourceMonitor& monitor, LogicalCollection* collection,
      transaction::Methods* trx, aql::AstNode const* node,
      aql::Variable const* reference, IndexIteratorOptions const& opts,
      int mutableConditionIdx);

  Index::SortCosts supportsSortCondition(
      aql::SortCondition const* sortCondition, aql::Variable const* reference,
      size_t itemsInIndex) const;

  Index::FilterCosts supportsFilterCondition(
      transaction::Methods& trx, IndexId id,
      std::vector<std::vector<basics::AttributeName>> const& fields,
      std::vector<std::shared_ptr<Index>> const& allIndexes,
      aql::AstNode const* node, aql::Variable const* reference,
      size_t itemsInIndex) const;

  aql::AstNode* specializeCondition(transaction::Methods& trx,
                                    aql::AstNode* node,
                                    aql::Variable const* reference) const;

  IResearchInvertedIndexMeta const& meta() const noexcept { return _meta; }

 protected:
  void invalidateQueryCache(TRI_vocbase_t* vocbase) override;

  irs::Comparer const* getComparator() const noexcept final {
    return &_comparer;
  }

  std::vector<std::vector<basics::AttributeName>> _coveredFields;

 private:
  IResearchInvertedIndexMeta _meta;
  VPackComparer<IResearchInvertedIndexSort> _comparer;
};

}  // namespace arangodb::iresearch
