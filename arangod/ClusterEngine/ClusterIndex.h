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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_ENGINE_CLUSTER_INDEX_H
#define ARANGOD_CLUSTER_ENGINE_CLUSTER_INDEX_H 1

#include <velocypack/Builder.h>
#include <velocypack/StringRef.h>

#include "Basics/Common.h"
#include "ClusterEngine/ClusterTransactionState.h"
#include "ClusterEngine/Common.h"
#include "Indexes/Index.h"
#include "VocBase/Identifiers/IndexId.h"

namespace arangodb {
class LogicalCollection;

class ClusterIndex : public Index {
 public:
  ClusterIndex(IndexId id, LogicalCollection& collection, ClusterEngineType engineType,
               Index::IndexType type, arangodb::velocypack::Slice const& info);

  ~ClusterIndex();

  void toVelocyPackFigures(velocypack::Builder& builder) const override;

  /// @brief return a VelocyPack representation of the index
  void toVelocyPack(velocypack::Builder& builder,
                    std::underlying_type<Index::Serialize>::type) const override;

  /// @brief if true this index should not be shown externally
  bool isHidden() const override {
    return false;  // do not generally hide indexes
  }

  IndexType type() const override { return _indexType; }

  char const* typeName() const override {
    return Index::oldtypeName(_indexType);
  }

  bool canBeDropped() const override {
    return _indexType != Index::TRI_IDX_TYPE_PRIMARY_INDEX &&
           _indexType != Index::TRI_IDX_TYPE_EDGE_INDEX;
  }

  bool isSorted() const override;

  bool hasSelectivityEstimate() const override;

  double selectivityEstimate(arangodb::velocypack::StringRef const& = arangodb::velocypack::StringRef()) const override;

  /// @brief update the cluster selectivity estimate
  void updateClusterSelectivityEstimate(double estimate) override;

  void load() override {}
  void unload() override {}
  size_t memory() const override { return 0; }
  
  Result drop() override { return Result(TRI_ERROR_NOT_IMPLEMENTED); }

  bool hasCoveringIterator() const override;

  /// @brief Checks if this index is identical to the given definition
  bool matchesDefinition(arangodb::velocypack::Slice const&) const override;

  Index::FilterCosts supportsFilterCondition(std::vector<std::shared_ptr<arangodb::Index>> const& allIndexes,
                                             arangodb::aql::AstNode const* node,
                                             arangodb::aql::Variable const* reference, 
                                             size_t itemsInIndex) const override;

  Index::SortCosts supportsSortCondition(arangodb::aql::SortCondition const* sortCondition,
                                         arangodb::aql::Variable const* reference, 
                                         size_t itemsInIndex) const override;

  /// @brief specializes the condition for use with the index
  arangodb::aql::AstNode* specializeCondition(arangodb::aql::AstNode* node,
                                              arangodb::aql::Variable const* reference) const override;

  void updateProperties(velocypack::Slice const&);

  std::vector<std::vector<arangodb::basics::AttributeName>> const& coveredFields() const override;

 protected:
  ClusterEngineType _engineType;
  Index::IndexType _indexType;
  velocypack::Builder _info;
  bool _estimates;
  double _clusterSelectivity;

  // Only used in RocksDB edge index.
  std::vector<std::vector<arangodb::basics::AttributeName>> _coveredFields;
};
}  // namespace arangodb

#endif
