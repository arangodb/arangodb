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

#include "Basics/AttributeNameParser.h"
#include "Basics/Common.h"
#include "Indexes/Index.h"
#include "ClusterEngine/ClusterTransactionState.h"

namespace arangodb {
class LogicalCollection;

class ClusterIndex : public Index {

 public:
  /*ClusterIndex(TRI_idx_iid_t, LogicalCollection*,
               Index::IndexType type,
               std::vector<std::vector<arangodb::basics::AttributeName>> const&
                   attributes, bool unique, bool sparse);*/

  ClusterIndex(TRI_idx_iid_t, LogicalCollection*,
               Index::IndexType type,
               arangodb::velocypack::Slice const&);

 public:
  
  ~ClusterIndex();
  
  void toVelocyPackFigures(VPackBuilder& builder) const override;

  /// @brief return a VelocyPack representation of the index
  void toVelocyPack(velocypack::Builder& builder, bool withFigures,
                    bool forPersistence) const override;
  
  IndexType type() const override { return _type; }
  
  char const* typeName() const override { return Index::oldtypeName(_type); }
  
  bool allowExpansion() const override { return false; }
  
  bool isPersistent() const override final { return true; }
  
  bool canBeDropped() const override { return _type != Index::TRI_IDX_TYPE_PRIMARY_INDEX &&
                                              _type != Index::TRI_IDX_TYPE_EDGE_INDEX; }
  
  bool hasCoveringIterator() const override { return true; }
  
  bool isSorted() const override { return false; }
  
  bool hasSelectivityEstimate() const override { return true; }
  
  double selectivityEstimateLocal(arangodb::StringRef const* = nullptr) const override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
    return 0;
  }

  int drop() override {
    return TRI_ERROR_NOT_IMPLEMENTED;
  }
  int afterTruncate() override {
    return TRI_ERROR_NOT_IMPLEMENTED;
  }

  void load() override {}
  void unload() override {}

  size_t memory() const override;


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
  Index::IndexType _type;
  velocypack::Builder _indexDefinition;
};
}  // namespace arangodb

#endif
