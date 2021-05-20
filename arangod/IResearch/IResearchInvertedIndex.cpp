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

#include "IResearch/IResearchInvertedIndex.h"
#include "IResearch/AqlHelper.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFilterFactory.h"


namespace {
using namespace arangodb;

struct CheckFieldsAccess {

  CheckFieldsAccess(std::vector<std::vector<arangodb::basics::AttributeName>> const& f) : _fields(f) {}

  bool attributeAccess(aql::AstNode const&) const { 
    return true;
  }

  bool indexAccess(aql::AstNode const&) const {
    return true;
  }

  bool expansion(aql::AstNode const&) const {
    return false;  // do not support [*]
  }

  std::vector<std::vector<arangodb::basics::AttributeName>> const& _fields;
};
}

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
  TRI_ASSERT(node);
  TRI_ASSERT(reference);
  auto filterCosts = FilterCosts::defaultCosts(itemsInIndex);

  // non-deterministic condition will mean full-scan. So we should
  // not use index here.
  // FIXME: maybe in future we will be able to optimize just deterministic part?
  if (!node->isDeterministic()) {
    LOG_TOPIC("750e6", TRACE, iresearch::TOPIC)
             << "Found non-deterministic condition. Skipping index " << id().id();
    return  filterCosts;
  }

  // check that only covered attributes are referenced
  if (!visit<true>(*node, CheckFieldsAccess(fields()))) {
    LOG_TOPIC("d2beb", TRACE, iresearch::TOPIC)
             << "Found unknown attribute access. Skipping index " << id().id();
    return  filterCosts;
  }


  QueryContext const queryCtx = {nullptr, nullptr, nullptr, // We don`t want byExpression filters
                                 nullptr, nullptr, reference};
  auto rv = FilterFactory::filter(nullptr, queryCtx, *node, false);
  LOG_TOPIC_IF("ee0f7", TRACE, iresearch::TOPIC, rv.fail())
             << "Failed to build filter with error'" << rv.errorMessage() <<"' Skipping index " << id().id();

  if (rv.ok()) {
    filterCosts.supportsCondition = true;
    filterCosts.coveredAttributes = 0; // FIXME: we may use stored values!
  } 
  return filterCosts;
}

aql::AstNode* IResearchInvertedIndex::specializeCondition(aql::AstNode* node,
                                                          aql::Variable const* reference) const {
  return nullptr;
}

} // namespace iresearch
} // namespace arangodb
