////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include "RocksDBEngine/RocksDBIndex.h"

namespace arangodb {

class RocksDBZkdIndexBase : public RocksDBIndex {
 public:
  RocksDBZkdIndexBase(IndexId iid, LogicalCollection& coll,
                      velocypack::Slice info);
  void toVelocyPack(
      velocypack::Builder& builder,
      std::underlying_type<Index::Serialize>::type type) const override;
  const char* typeName() const override { return "zkd"; };
  IndexType type() const override { return TRI_IDX_TYPE_ZKD_INDEX; };
  bool canBeDropped() const override { return true; }
  bool isSorted() const override { return false; }
  bool hasSelectivityEstimate() const override { return false; /* TODO */ }

  std::vector<std::vector<basics::AttributeName>> const& coveredFields()
      const override {
    // index does not cover the index attributes!
    return Index::emptyCoveredFields;
  }

  Result insert(transaction::Methods& trx, RocksDBMethods* methods,
                const LocalDocumentId& documentId, velocypack::Slice doc,
                const OperationOptions& options, bool performChecks) override;
  Result remove(transaction::Methods& trx, RocksDBMethods* methods,
                const LocalDocumentId& documentId,
                velocypack::Slice doc) override;

  FilterCosts supportsFilterCondition(
      transaction::Methods& /*trx*/,
      const std::vector<std::shared_ptr<Index>>& allIndexes,
      const aql::AstNode* node, const aql::Variable* reference,
      size_t itemsInIndex) const override;

  aql::AstNode* specializeCondition(
      transaction::Methods& trx, aql::AstNode* condition,
      const aql::Variable* reference) const override;

  std::unique_ptr<IndexIterator> iteratorForCondition(
      ResourceMonitor& monitor, transaction::Methods* trx,
      const aql::AstNode* node, const aql::Variable* reference,
      const IndexIteratorOptions& opts, ReadOwnWrites readOwnWrites,
      int) override;
};

class RocksDBZkdIndex final : public RocksDBZkdIndexBase {
  using RocksDBZkdIndexBase::RocksDBZkdIndexBase;
};

class RocksDBUniqueZkdIndex final : public RocksDBZkdIndexBase {
  using RocksDBZkdIndexBase::RocksDBZkdIndexBase;

  Result insert(transaction::Methods& trx, RocksDBMethods* methods,
                const LocalDocumentId& documentId, velocypack::Slice doc,
                const OperationOptions& options, bool performChecks) override;
  Result remove(transaction::Methods& trx, RocksDBMethods* methods,
                const LocalDocumentId& documentId,
                velocypack::Slice doc) override;

  std::unique_ptr<IndexIterator> iteratorForCondition(
      ResourceMonitor& monitor, transaction::Methods* trx,
      const aql::AstNode* node, const aql::Variable* reference,
      const IndexIteratorOptions& opts, ReadOwnWrites readOwnWrites,
      int) override;
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
    Index const* index, const aql::AstNode* condition,
    const aql::Variable* reference,
    std::unordered_map<size_t, ExpressionBounds>& extractedBounds,
    std::unordered_set<aql::AstNode const*>& unusedExpressions);

auto supportsFilterCondition(
    Index const* index, const std::vector<std::shared_ptr<Index>>& allIndexes,
    const aql::AstNode* node, const aql::Variable* reference,
    size_t itemsInIndex) -> Index::FilterCosts;

auto specializeCondition(Index const* index, aql::AstNode* condition,
                         const aql::Variable* reference) -> aql::AstNode*;
}  // namespace zkd

}  // namespace arangodb
