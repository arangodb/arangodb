////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
  char const* typeName() const override { return "zkd"; };
  IndexType type() const override { return TRI_IDX_TYPE_ZKD_INDEX; };
  bool canBeDropped() const override { return true; }
  bool isSorted() const override { return false; }
  bool hasSelectivityEstimate() const override { return false; /* TODO */ }

  std::vector<std::vector<basics::AttributeName>> const& coveredFields()
      const override {
    return _coveredFields;
  }
  std::vector<std::vector<basics::AttributeName>> const& prefixFields()
      const noexcept {
    return _prefixFields;
  }

  Result insert(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId documentId, velocypack::Slice doc,
                OperationOptions const& options, bool performChecks) override;
  Result remove(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId documentId, velocypack::Slice doc,
                OperationOptions const& /*options*/) override;

  FilterCosts supportsFilterCondition(
      transaction::Methods& /*trx*/,
      std::vector<std::shared_ptr<Index>> const& allIndexes,
      aql::AstNode const* node, aql::Variable const* reference,
      size_t itemsInIndex) const override;

  aql::AstNode* specializeCondition(
      transaction::Methods& trx, aql::AstNode* condition,
      aql::Variable const* reference) const override;

  std::unique_ptr<IndexIterator> iteratorForCondition(
      ResourceMonitor& monitor, transaction::Methods* trx,
      aql::AstNode const* node, aql::Variable const* reference,
      IndexIteratorOptions const& opts, ReadOwnWrites readOwnWrites,
      int) override;

  std::vector<std::vector<basics::AttributeName>> const _storedValues;
  std::vector<std::vector<basics::AttributeName>> const _prefixFields;
  std::vector<std::vector<basics::AttributeName>> const _coveredFields;
};

class RocksDBZkdIndex final : public RocksDBZkdIndexBase {
  using RocksDBZkdIndexBase::RocksDBZkdIndexBase;
};

class RocksDBUniqueZkdIndex final : public RocksDBZkdIndexBase {
  using RocksDBZkdIndexBase::RocksDBZkdIndexBase;

  Result insert(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId documentId, velocypack::Slice doc,
                OperationOptions const& options, bool performChecks) override;
  Result remove(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId documentId, velocypack::Slice doc,
                OperationOptions const& /*options*/) override;

  std::unique_ptr<IndexIterator> iteratorForCondition(
      ResourceMonitor& monitor, transaction::Methods* trx,
      aql::AstNode const* node, aql::Variable const* reference,
      IndexIteratorOptions const& opts, ReadOwnWrites readOwnWrites,
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
    Index const* index, aql::AstNode const* condition,
    aql::Variable const* reference,
    std::unordered_map<size_t, aql::AstNode const*>& extractedPrefix,
    std::unordered_map<size_t, ExpressionBounds>& extractedBounds,
    std::unordered_set<aql::AstNode const*>& unusedExpressions);

auto supportsFilterCondition(
    Index const* index, std::vector<std::shared_ptr<Index>> const& allIndexes,
    aql::AstNode const* node, aql::Variable const* reference,
    size_t itemsInIndex) -> Index::FilterCosts;

auto specializeCondition(Index const* index, aql::AstNode* condition,
                         aql::Variable const* reference) -> aql::AstNode*;
}  // namespace zkd

}  // namespace arangodb
