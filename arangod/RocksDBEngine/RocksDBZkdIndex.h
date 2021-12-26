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
/// @author Tobias Gödderz
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_ROCKSDB_ZKD_INDEX_H
#define ARANGOD_ROCKSDB_ZKD_INDEX_H

#include "RocksDBEngine/RocksDBIndex.h"

namespace arangodb {

class RocksDBZkdIndexBase : public RocksDBIndex {
 public:
  RocksDBZkdIndexBase(IndexId iid, LogicalCollection& coll,
                      arangodb::velocypack::Slice const& info);
  void toVelocyPack(
      velocypack::Builder& builder,
      std::underlying_type<Index::Serialize>::type type) const override;
  const char* typeName() const override { return "zkd"; };
  IndexType type() const override { return TRI_IDX_TYPE_ZKD_INDEX; };
  bool canBeDropped() const override { return true; }
  bool isSorted() const override { return false; }
  bool hasSelectivityEstimate() const override { return false; /* TODO */ }
  Result insert(transaction::Methods& trx, RocksDBMethods* methods,
                const LocalDocumentId& documentId,
                arangodb::velocypack::Slice doc,
                const OperationOptions& options, bool performChecks) override;
  Result remove(transaction::Methods& trx, RocksDBMethods* methods,
                const LocalDocumentId& documentId,
                arangodb::velocypack::Slice doc) override;

  FilterCosts supportsFilterCondition(
      const std::vector<std::shared_ptr<arangodb::Index>>& allIndexes,
      const arangodb::aql::AstNode* node,
      const arangodb::aql::Variable* reference,
      size_t itemsInIndex) const override;

  aql::AstNode* specializeCondition(
      arangodb::aql::AstNode* condition,
      const arangodb::aql::Variable* reference) const override;

  std::unique_ptr<IndexIterator> iteratorForCondition(
      transaction::Methods* trx, const aql::AstNode* node,
      const aql::Variable* reference, const IndexIteratorOptions& opts,
      ReadOwnWrites readOwnWrites) override;
};

class RocksDBZkdIndex final : public RocksDBZkdIndexBase {
  using RocksDBZkdIndexBase::RocksDBZkdIndexBase;
};

class RocksDBUniqueZkdIndex final : public RocksDBZkdIndexBase {
  using RocksDBZkdIndexBase::RocksDBZkdIndexBase;

  Result insert(transaction::Methods& trx, RocksDBMethods* methods,
                const LocalDocumentId& documentId,
                arangodb::velocypack::Slice doc,
                const OperationOptions& options, bool performChecks) override;
  Result remove(transaction::Methods& trx, RocksDBMethods* methods,
                const LocalDocumentId& documentId,
                arangodb::velocypack::Slice doc) override;

  std::unique_ptr<IndexIterator> iteratorForCondition(
      transaction::Methods* trx, const aql::AstNode* node,
      const aql::Variable* reference, const IndexIteratorOptions& opts,
      ReadOwnWrites readOwnWrites) override;
};

namespace zkd {

struct ExpressionBounds {
  struct Bound {
    aql::AstNode const* op_node = nullptr;
    aql::AstNode const* bounded_expr = nullptr;
    aql::AstNode const* bound_value = nullptr;
    bool isStrict = false;
  };

  Bound lower;
  Bound upper;
};

void extractBoundsFromCondition(
    arangodb::Index const* index, const arangodb::aql::AstNode* condition,
    const arangodb::aql::Variable* reference,
    std::unordered_map<size_t, ExpressionBounds>& extractedBounds,
    std::unordered_set<aql::AstNode const*>& unusedExpressions);

auto supportsFilterCondition(
    arangodb::Index const* index,
    const std::vector<std::shared_ptr<arangodb::Index>>& allIndexes,
    const arangodb::aql::AstNode* node,
    const arangodb::aql::Variable* reference, size_t itemsInIndex)
    -> Index::FilterCosts;

auto specializeCondition(arangodb::Index const* index,
                         arangodb::aql::AstNode* condition,
                         const arangodb::aql::Variable* reference)
    -> aql::AstNode*;
}  // namespace zkd

}  // namespace arangodb

#endif  // ARANGOD_ROCKSDB_ZKD_INDEX_H
