////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#pragma once

#include <velocypack/Builder.h>

#include "ClusterEngine/ClusterTransactionState.h"
#include "ClusterEngine/Common.h"
#include "Indexes/Index.h"
#include "VocBase/Identifiers/IndexId.h"

#include <atomic>

namespace arangodb {
class LogicalCollection;

class ClusterIndex : public Index {
 public:
  ClusterIndex(IndexId id, LogicalCollection& collection,
               ClusterEngineType engineType, Index::IndexType type,
               velocypack::Slice info);

  ClusterIndex(ClusterIndex const&) = delete;
  ClusterIndex& operator=(ClusterIndex const&) = delete;

  ~ClusterIndex();

  void toVelocyPackFigures(velocypack::Builder& builder) const override;

  /// @brief return a VelocyPack representation of the index
  void toVelocyPack(
      velocypack::Builder& builder,
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

  double selectivityEstimate(
      std::string_view = std::string_view()) const override;

  /// @brief update the cluster selectivity estimate
  void updateClusterSelectivityEstimate(double estimate) override;

  void load() override {}
  void unload() override {}
  size_t memory() const override { return 0; }

  Result drop() override { return Result(TRI_ERROR_NOT_IMPLEMENTED); }

  /// @brief Checks if this index is identical to the given definition
  bool matchesDefinition(velocypack::Slice const&) const override;

  Index::FilterCosts supportsFilterCondition(
      transaction::Methods& trx,
      std::vector<std::shared_ptr<Index>> const& allIndexes,
      aql::AstNode const* node, aql::Variable const* reference,
      size_t itemsInIndex) const override;

  Index::SortCosts supportsSortCondition(
      aql::SortCondition const* sortCondition, aql::Variable const* reference,
      size_t itemsInIndex) const override;

  /// @brief specializes the condition for use with the index
  aql::AstNode* specializeCondition(
      transaction::Methods& trx, aql::AstNode* node,
      aql::Variable const* reference) const override;

  void updateProperties(velocypack::Slice slice);

  std::vector<std::vector<basics::AttributeName>> const& coveredFields()
      const override;

  bool supportsStreamInterface(
      IndexStreamOptions const&) const noexcept override;

  std::vector<std::vector<basics::AttributeName>> const& prefixFields()
      const noexcept {
    return _prefixFields;
  }

 protected:
  ClusterEngineType _engineType;
  Index::IndexType _indexType;
  velocypack::Builder _info;
  bool _estimates;
  std::atomic<double> _clusterSelectivity;

  // Only used in RocksDB edge index.
  std::vector<std::vector<basics::AttributeName>> _coveredFields;
  // Only used in TRI_IDX_TYPE_MDI_PREFIXED_INDEX
  std::vector<std::vector<basics::AttributeName>> _prefixFields;
};
}  // namespace arangodb
