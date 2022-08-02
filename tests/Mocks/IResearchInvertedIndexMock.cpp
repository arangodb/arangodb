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

#include "IResearchInvertedIndexMock.h"
#include "IResearch/IResearchDataStore.h"

namespace arangodb {
namespace iresearch {

IResearchInvertedIndexMock::IResearchInvertedIndexMock(
    IndexId iid, arangodb::LogicalCollection& collection,
    const std::string& idxName,
    std::vector<std::vector<arangodb::basics::AttributeName>> const& attributes,
    bool unique, bool sparse)
    : Index(iid, collection, idxName, attributes, unique, sparse),
      IResearchInvertedIndex(iid, collection) {}

void IResearchInvertedIndexMock::toVelocyPack(
    velocypack::Builder& builder,
    std::underlying_type<Index::Serialize>::type flags) const {
  auto const forPersistence =
      Index::hasFlag(flags, Index::Serialize::Internals);
  VPackObjectBuilder objectBuilder(&builder);
  IResearchInvertedIndex::toVelocyPack(
      IResearchDataStore::collection().vocbase().server(),
      &IResearchDataStore::collection().vocbase(), builder, forPersistence);

  builder.add(arangodb::StaticStrings::IndexId,
              arangodb::velocypack::Value(std::to_string(_iid.id())));
  builder.add(arangodb::StaticStrings::IndexType,
              arangodb::velocypack::Value(oldtypeName(type())));
  builder.add(arangodb::StaticStrings::IndexName,
              arangodb::velocypack::Value(name()));
  builder.add(arangodb::StaticStrings::IndexUnique, VPackValue(unique()));
  builder.add(arangodb::StaticStrings::IndexSparse, VPackValue(sparse()));
}

Index::IndexType IResearchInvertedIndexMock::type() const {
  return Index::TRI_IDX_TYPE_INVERTED_INDEX;
}

bool IResearchInvertedIndexMock::needsReversal() const { return true; }

size_t IResearchInvertedIndexMock::memory() const {
  // FIXME return in memory size
  return stats().indexSize;
}

bool IResearchInvertedIndexMock::isHidden() const { return false; }

char const* IResearchInvertedIndexMock::typeName() const { return "inverted"; }

bool IResearchInvertedIndexMock::canBeDropped() const { return true; }

bool IResearchInvertedIndexMock::isSorted() const {
  return IResearchInvertedIndex::isSorted();
}

bool IResearchInvertedIndexMock::hasSelectivityEstimate() const {
  return false;
}

bool IResearchInvertedIndexMock::inProgress() const {
  return IResearchInvertedIndex::inProgress();
}

bool IResearchInvertedIndexMock::covers(
    arangodb::aql::Projections& projections) const {
  return IResearchInvertedIndex::covers(projections);
}

Result IResearchInvertedIndexMock::drop() { return deleteDataStore(); }

void IResearchInvertedIndexMock::load() {}

void IResearchInvertedIndexMock::afterTruncate(TRI_voc_tick_t tick,
                                               transaction::Methods* trx) {
  return IResearchDataStore::afterTruncate(tick, trx);
}

std::unique_ptr<IndexIterator> IResearchInvertedIndexMock::iteratorForCondition(
    transaction::Methods* trx, aql::AstNode const* node,
    aql::Variable const* reference, IndexIteratorOptions const& opts,
    ReadOwnWrites readOwnWrites, int mutableConditionIdx) {
  return IResearchInvertedIndex::iteratorForCondition(
      &IResearchDataStore::collection(), trx, node, reference, opts,
      mutableConditionIdx);
}

Index::SortCosts IResearchInvertedIndexMock::supportsSortCondition(
    aql::SortCondition const* sortCondition, aql::Variable const* reference,
    size_t itemsInIndex) const {
  return IResearchInvertedIndex::supportsSortCondition(sortCondition, reference,
                                                       itemsInIndex);
}

Index::FilterCosts IResearchInvertedIndexMock::supportsFilterCondition(
    std::vector<std::shared_ptr<Index>> const& allIndexes,
    aql::AstNode const* node, aql::Variable const* reference,
    size_t itemsInIndex) const {
  return IResearchInvertedIndex::supportsFilterCondition(
      IResearchDataStore::id(), _fields, allIndexes, node, reference,
      itemsInIndex);
}

aql::AstNode* IResearchInvertedIndexMock::specializeCondition(
    aql::AstNode* node, aql::Variable const* reference) const {
  return IResearchInvertedIndex::specializeCondition(node, reference);
}

Result IResearchInvertedIndexMock::insert(transaction::Methods& trx,
                                          LocalDocumentId documentId,
                                          velocypack::Slice doc) {
  IResearchInvertedIndexMetaIndexingContext ctx(&this->meta());
  return IResearchDataStore::insert<
      FieldIterator<IResearchInvertedIndexMetaIndexingContext>,
      IResearchInvertedIndexMetaIndexingContext>(trx, documentId, doc, ctx);
}

AnalyzerPool::ptr IResearchInvertedIndexMock::findAnalyzer(
    AnalyzerPool const& analyzer) const {
  return IResearchInvertedIndex::findAnalyzer(analyzer);
}

void IResearchInvertedIndexMock::toVelocyPackFigures(
    velocypack::Builder& builder) const {
  IResearchInvertedIndex::toVelocyPackStats(builder);
}

void IResearchInvertedIndexMock::unload() { shutdownDataStore(); }

void IResearchInvertedIndexMock::invalidateQueryCache(TRI_vocbase_t* vocbase) {
  return IResearchInvertedIndex::invalidateQueryCache(vocbase);
}

irs::comparer const* IResearchInvertedIndexMock::getComparator()
    const noexcept {
  return IResearchInvertedIndex::getComparator();
}

std::function<irs::directory_attributes()>
    IResearchInvertedIndexMock::InitCallback;

}  // namespace iresearch
}  // namespace arangodb
