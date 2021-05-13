#include "IResearchInvertedIndex.h"
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


namespace arangodb {
namespace iresearch {

arangodb::iresearch::IResearchInvertedIndex::IResearchInvertedIndex(
    IndexId id, LogicalCollection& collection, arangodb::velocypack::Slice const& info)
  : Index(id, collection, info) {
}

void arangodb::iresearch::IResearchInvertedIndex::toVelocyPack(
    VPackBuilder& builder, std::underlying_type<Index::Serialize>::type flags) const {
}

std::unique_ptr<IndexIterator> arangodb::iresearch::IResearchInvertedIndex::iteratorForCondition(
    transaction::Methods* trx, aql::AstNode const* node, aql::Variable const* reference,
    IndexIteratorOptions const& opts) {
  return std::unique_ptr<IndexIterator>();
}

Index::SortCosts IResearchInvertedIndex::supportsSortCondition(
    arangodb::aql::SortCondition const* sortCondition, arangodb::aql::Variable const* reference,
    size_t itemsInIndex) const {
  return SortCosts();
}

Index::FilterCosts IResearchInvertedIndex::supportsFilterCondition(
    std::vector<std::shared_ptr<arangodb::Index>> const& allIndexes,
    arangodb::aql::AstNode const* node, arangodb::aql::Variable const* reference,
    size_t itemsInIndex) const {
  return FilterCosts();
}

aql::AstNode* IResearchInvertedIndex::specializeCondition(aql::AstNode* node,
                                                          aql::Variable const* reference) const {
  return nullptr;
}

} // namespace iresearch
} // namespace arangodb
