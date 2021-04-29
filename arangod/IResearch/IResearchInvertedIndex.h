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

#include "IResearchRocksDBLink.h"
#include "Indexes/IndexIterator.h"

namespace arangodb {
namespace iresearch {

class IResearchInvertedIndex : public IResearchRocksDBLink {
 public:
  IResearchInvertedIndex(IndexId iid, LogicalCollection& collection, uint64_t objectId);
  Result init(velocypack::Slice const& definition,
              InitCallback const& initCallback = {}) override;

  void toVelocyPack(VPackBuilder& builder,
                    std::underlying_type<Index::Serialize>::type flags) const override;

  Result properties(velocypack::Builder& builder, bool forPersistence) const override;
  Index::IndexType type() const override { return  Index::TRI_IDX_TYPE_SEARCH_INDEX; }

  std::unique_ptr<IndexIterator> iteratorForCondition(transaction::Methods* trx,
                                                      aql::AstNode const* node,
                                                      aql::Variable const* reference,
                                                      IndexIteratorOptions const& opts) override;


  bool isHidden() const override {
    return false;
  }

  FilterCosts supportsFilterCondition(std::vector<std::shared_ptr<arangodb::Index>> const& allIndexes,
                                      arangodb::aql::AstNode const* node,
                                      arangodb::aql::Variable const* reference, 
                                      size_t itemsInIndex) const override;

  aql::AstNode* specializeCondition(aql::AstNode* node,
                                    aql::Variable const* reference) const override;

};

//arangodb::Index::TRI_IDX_TYPE_SEARCH_INDEX>
class IResearchInvertedIndexFactory : public IndexTypeFactory {
 public:
  explicit IResearchInvertedIndexFactory(application_features::ApplicationServer& server);
  virtual ~IResearchInvertedIndexFactory() = default;  // define to silence warning

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

  /// @brief the order of attributes matters by default  
  bool attributeOrderMatters() const override {
    return false;
  }
};

} // namespace iresearch
} // namespace arangodb
#endif // ARANGOD_IRESEARCH_INVERTED_INDEX
