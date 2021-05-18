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

namespace arangodb {
namespace iresearch {

class IResearchInvertedIndex : public Index {
 public:
  IResearchInvertedIndex(IndexId id, LogicalCollection& collection,
                         arangodb::velocypack::Slice const& info);


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

  Index::SortCosts supportsSortCondition(arangodb::aql::SortCondition const* sortCondition,
                                          arangodb::aql::Variable const* reference,
                                          size_t itemsInIndex) const override;

  Index::FilterCosts supportsFilterCondition(std::vector<std::shared_ptr<arangodb::Index>> const& allIndexes,
                                      arangodb::aql::AstNode const* node,
                                      arangodb::aql::Variable const* reference,
                                      size_t itemsInIndex) const override;

  aql::AstNode* specializeCondition(aql::AstNode* node,
                                    aql::Variable const* reference) const override;
};

} // namespace iresearch
} // namespace arangodb
#endif // ARANGOD_IRESEARCH_INVERTED_INDEX
