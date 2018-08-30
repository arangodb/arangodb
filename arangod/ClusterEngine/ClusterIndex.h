////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/Common.h"
#include "ClusterEngine/ClusterTransactionState.h"
#include "ClusterEngine/Common.h"
#include "Indexes/Index.h"

namespace arangodb {
class LogicalCollection;

class ClusterIndex : public Index {
 public:
  ClusterIndex(
    TRI_idx_iid_t id,
    LogicalCollection& collection,
    ClusterEngineType engineType,
    Index::IndexType type,
    arangodb::velocypack::Slice const& info
  );

  ~ClusterIndex();

  void toVelocyPackFigures(velocypack::Builder& builder) const override;

  /// @brief return a VelocyPack representation of the index
  void toVelocyPack(velocypack::Builder& builder, unsigned flags) const override;

  IndexType type() const override { return _indexType; }

  char const* typeName() const override {
    return Index::oldtypeName(_indexType);
  }

  bool isPersistent() const override;

  bool canBeDropped() const override {
    return _indexType != Index::TRI_IDX_TYPE_PRIMARY_INDEX &&
           _indexType != Index::TRI_IDX_TYPE_EDGE_INDEX;
  }
  
  bool isSorted() const override;

  bool hasSelectivityEstimate() const override;

  double selectivityEstimate(arangodb::StringRef const* = nullptr) const override;
  
  /// @brief update the cluster selectivity estimate
  void updateClusterSelectivityEstimate(double estimate) override;

  void load() override {}
  void unload() override {}
  size_t memory() const override { return 0; }

  int drop() override { return TRI_ERROR_NOT_IMPLEMENTED; }
  void afterTruncate() override { }

  bool hasCoveringIterator() const override;

  /// @brief Checks if this index is identical to the given definition
  bool matchesDefinition(arangodb::velocypack::Slice const&) const override;

  bool supportsFilterCondition(arangodb::aql::AstNode const*,
                               arangodb::aql::Variable const*, size_t, size_t&,
                               double&) const override;

  bool supportsSortCondition(arangodb::aql::SortCondition const*,
                             arangodb::aql::Variable const*, size_t, double&,
                             size_t&) const override;

  /// @brief specializes the condition for use with the index
  arangodb::aql::AstNode* specializeCondition(
      arangodb::aql::AstNode*, arangodb::aql::Variable const*) const override;

  /// @brief provides a size hint for the index
  int sizeHint(transaction::Methods* /*trx*/, size_t /*size*/) override final {
    // nothing to do here
    return TRI_ERROR_NO_ERROR;
  }

  Result insert(transaction::Methods* trx, LocalDocumentId const& documentId,
                velocypack::Slice const& doc, OperationMode mode) override {
    return TRI_ERROR_NOT_IMPLEMENTED;
  }

  Result remove(transaction::Methods* trx, LocalDocumentId const& documentId,
                arangodb::velocypack::Slice const& doc,
                OperationMode mode) override {
    return TRI_ERROR_NOT_IMPLEMENTED;
  }

  void updateProperties(velocypack::Slice const&);

 protected:
  ClusterEngineType _engineType;
  Index::IndexType _indexType;
  velocypack::Builder _info;
  double _clusterSelectivity;
};
}  // namespace arangodb

#endif
