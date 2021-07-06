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
#ifndef ARANGOD_IRESEARCH_INVERTED_INDEX
#define ARANGOD_IRESEARCH_INVERTED_INDEX 1

#include "Basics/Common.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "Indexes/IndexFactory.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "IResearchCommon.h"
#include "IResearchDataStore.h"
#include "IResearchViewMeta.h"
#include "IResearchLinkMeta.h"

namespace arangodb {
namespace iresearch {

class IResearchInvertedIndex  : public IResearchDataStore {
 public:
  explicit IResearchInvertedIndex(IndexId iid, LogicalCollection& collection, IResearchLinkMeta&& meta);

  void toVelocyPack(application_features::ApplicationServer& server,
                    TRI_vocbase_t const* defaultVocbase,
                    velocypack::Builder& builder, bool forPersistence) const;

  bool isSorted() const {
    return !_meta._sort.empty();
  }
  
  Result init(InitCallback const& initCallback = {});

  static std::vector<std::vector<arangodb::basics::AttributeName>> fields(IResearchLinkMeta const& meta);

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
};

class IResearchRocksDBInvertedIndex : public IResearchInvertedIndex, public RocksDBIndex {
 public:
  IResearchRocksDBInvertedIndex(IndexId id, LogicalCollection& collection, uint64_t objectId,
                                std::string const& name, IResearchLinkMeta&& meta);

  Index::IndexType type() const override { return  Index::TRI_IDX_TYPE_INVERTED_INDEX; }

  void toVelocyPack(VPackBuilder& builder,
                    std::underlying_type<Index::Serialize>::type flags) const override;

  size_t memory() const override {
    // FIXME return in memory size
    //return stats().indexSize;
    return 0;
  }

  bool isHidden() const override {
    return false;
  }

  char const* typeName() const override {
    return oldtypeName();
  }

  bool canBeDropped() const override {
    return true;
  }

  bool isSorted() const override {
    return  IResearchInvertedIndex::isSorted();
  }

  bool hasSelectivityEstimate() const override {
    return false;
  }

  bool inProgress() const override {
    return IResearchInvertedIndex::inProgress();
  }

  void load() override {}
  void unload() override {}

  bool matchesDefinition(arangodb::velocypack::Slice const& other) const override;

  std::unique_ptr<IndexIterator> iteratorForCondition(transaction::Methods* trx,
                                                    aql::AstNode const* node,
                                                    aql::Variable const* reference,
                                                    IndexIteratorOptions const& opts) override {
    return IResearchInvertedIndex::iteratorForCondition(&IResearchDataStore::collection(), trx, node, reference, opts);
  }

  Index::SortCosts supportsSortCondition(aql::SortCondition const* sortCondition,
                                         aql::Variable const* reference,
                                         size_t itemsInIndex) const override {
     return IResearchInvertedIndex::supportsSortCondition(sortCondition, reference, itemsInIndex);
  }

  Index::FilterCosts supportsFilterCondition(std::vector<std::shared_ptr<Index>> const& allIndexes,
                                             aql::AstNode const* node,
                                             aql::Variable const* reference,
                                             size_t itemsInIndex) const override {
     return IResearchInvertedIndex::supportsFilterCondition(IResearchDataStore::id(), RocksDBIndex::fields(), allIndexes,
                                                            node, reference, itemsInIndex);
  }

  aql::AstNode* specializeCondition(aql::AstNode* node,
                                    aql::Variable const* reference) const override {
    return IResearchInvertedIndex::specializeCondition(node, reference);
  }

  Result insert(transaction::Methods& trx,
                RocksDBMethods* /*methods*/,
                LocalDocumentId const& documentId,
                VPackSlice doc,
                OperationOptions const& /*options*/,
                bool /*performChecks*/) override {
    return IResearchDataStore::insert(trx, documentId, doc);
  }

  Result remove(transaction::Methods& trx,
                RocksDBMethods*,
                LocalDocumentId const& documentId,
                VPackSlice doc) override {
    return IResearchDataStore::remove(trx, documentId, doc);
  }
};

class IResearchRocksDBInvertedIndexFactory : public IndexTypeFactory {
 public:
  explicit IResearchRocksDBInvertedIndexFactory(application_features::ApplicationServer& server);
  virtual ~IResearchRocksDBInvertedIndexFactory() = default;

  bool equal(velocypack::Slice const& lhs, velocypack::Slice const& rhs,
             std::string const& dbname) const override;

  /// @brief instantiate an Index definition
  std::shared_ptr<Index> instantiate(LogicalCollection& collection,
                                     velocypack::Slice const& definition, IndexId id,
                                     bool isClusterConstructor) const override;

  /// @brief normalize an Index definition prior to instantiation/persistence
  Result normalize(
    velocypack::Builder& normalized,
    velocypack::Slice definition,
    bool isCreation,
    TRI_vocbase_t const& vocbase) const override;

  bool attributeOrderMatters() const override {
    return false;
  }
};

} // namespace iresearch
} // namespace arangodb
#endif // ARANGOD_IRESEARCH_INVERTED_INDEX
