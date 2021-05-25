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
#include "IResearch/IResearchCommon.h"

namespace arangodb {
namespace iresearch {

class IResearchInvertedIndex : public Index {
 public:
  struct IResearchInvertedIndexMeta : public IResearchViewMeta {
    size_t _cleanupIntervalStep; // issue cleanup after <count> commits (0 == disable)
    size_t _commitIntervalMsec; // issue commit after <interval> milliseconds (0 == disable)
    size_t _consolidationIntervalMsec; // issue consolidation after <interval> milliseconds (0 == disable)
    ConsolidationPolicy _consolidationPolicy; // the consolidation policy to use
    uint32_t _version; // the version of the iresearch interface e.g. which how data is stored in iresearch (default == latest)
    size_t _writebufferActive; // maximum number of concurrent segments before segment aquisition blocks, e.g. max number of concurrent transacitons) (0 == unlimited)
    size_t _writebufferIdle; // maximum number of segments cached in the pool
    size_t _writebufferSizeMax; // maximum memory byte size per segment before a segment flush is triggered (0 == unlimited)
  };

  IResearchInvertedIndex(IndexId id, LogicalCollection& collection,
                         velocypack::Slice const& info);


  void toVelocyPack(VPackBuilder& builder,
                    std::underlying_type<Index::Serialize>::type flags) const override;


  Index::IndexType type() const override { return  Index::TRI_IDX_TYPE_SEARCH_INDEX; }

  std::unique_ptr<IndexIterator> iteratorForCondition(transaction::Methods* trx,
                                                      aql::AstNode const* node,
                                                      aql::Variable const* reference,
                                                      IndexIteratorOptions const& opts) override;


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
    return false; // FIXME: sometimes we can be sorted
  }

  bool hasSelectivityEstimate() const override {
    return false;
  }

  size_t memory() const override {
    // FIXME return in memory size
    //return stats().indexSize;
    return 0;
  }

  void load() override {}
  void unload() override {}

  Index::SortCosts supportsSortCondition(aql::SortCondition const* sortCondition,
                                         aql::Variable const* reference,
                                         size_t itemsInIndex) const override;

  Index::FilterCosts supportsFilterCondition(std::vector<std::shared_ptr<Index>> const& allIndexes,
                                             aql::AstNode const* node,
                                             aql::Variable const* reference,
                                             size_t itemsInIndex) const override;

  aql::AstNode* specializeCondition(aql::AstNode* node,
                                    aql::Variable const* reference) const override;
};

class IResearchInvertedIndexFactory : public IndexTypeFactory {
 public:
  explicit IResearchInvertedIndexFactory(application_features::ApplicationServer& server);
  virtual ~IResearchInvertedIndexFactory() = default;

  bool equal(velocypack::Slice const& lhs, velocypack::Slice const& rhs,
             std::string const& dbname) const override;

  /// @brief instantiate an Index definition
  std::shared_ptr<Index> instantiate(LogicalCollection& collection,
                                     velocypack::Slice const& definition, IndexId id,
                                     bool isClusterConstructor) const override;

  /// @brief normalize an Index definition prior to instantiation/persistence
  Result normalize( // normalize definition
    velocypack::Builder& normalized, // normalized definition (out-param)
    velocypack::Slice definition, // source definition
    bool isCreation, // definition for index creation
    TRI_vocbase_t const& vocbase // index vocbase
  ) const override;

  bool attributeOrderMatters() const override {
    return false;
  }
};

} // namespace iresearch
} // namespace arangodb
#endif // ARANGOD_IRESEARCH_INVERTED_INDEX
