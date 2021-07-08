////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
#include "IResearchViewMeta.h"
#include "IResearchLinkMeta.h"

namespace arangodb {
namespace iresearch {

class IResearchInvertedIndex  : public IResearchDataStore {
 public:
  explicit IResearchInvertedIndex(IndexId iid, LogicalCollection& collection, InvertedIndexFieldMeta&& meta);

  void toVelocyPack(application_features::ApplicationServer& server,
                    TRI_vocbase_t const* defaultVocbase,
                    velocypack::Builder& builder, bool forPersistence) const;

  bool isSorted() const {
    return !_meta._sort.empty();
  }
  
  Result init(InitCallback const& initCallback = {});

  static std::vector<std::vector<arangodb::basics::AttributeName>> fields(InvertedIndexFieldMeta const& meta);

  bool matchesFieldsDefinition(VPackSlice other) const;

  AnalyzerPool::ptr findAnalyzer(AnalyzerPool const& analyzer) const override;

  bool inProgress() const {
    return false;
  }

  std::unique_ptr<IndexIterator> iteratorForCondition(LogicalCollection* collection,
                                                      transaction::Methods* trx,
                                                      aql::AstNode const* node,
                                                      aql::Variable const* reference,
                                                      IndexIteratorOptions const& opts);

  Index::SortCosts supportsSortCondition(aql::SortCondition const* sortCondition,
                                         aql::Variable const* reference,
                                         size_t itemsInIndex) const;

  Index::FilterCosts supportsFilterCondition(IndexId id,
                                             std::vector<std::vector<arangodb::basics::AttributeName>> const& fields,
                                             std::vector<std::shared_ptr<Index>> const& allIndexes,
                                             aql::AstNode const* node,
                                             aql::Variable const* reference,
                                             size_t itemsInIndex) const;

  aql::AstNode* specializeCondition(aql::AstNode* node,
                                    aql::Variable const* reference) const;

  InvertedIndexFieldMeta _meta;
};
} // namespace iresearch
} // namespace arangodb
