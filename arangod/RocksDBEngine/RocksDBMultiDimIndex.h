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
/// @author Tobias Gödderz
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RocksDBEngine/RocksDBIndex.h"

namespace arangodb {

class RocksDBMdiIndexBase : public RocksDBIndex {
 public:
  RocksDBMdiIndexBase(IndexId iid, LogicalCollection& coll,
                      velocypack::Slice info);
  void toVelocyPack(
      velocypack::Builder& builder,
      std::underlying_type<Index::Serialize>::type type) const override;
  char const* typeName() const override;
  IndexType type() const override;
  bool canBeDropped() const override { return true; }
  bool isSorted() const override { return false; }

  bool isPrefixed() const noexcept { return !_prefixFields.empty(); }

  bool matchesDefinition(VPackSlice const& info) const override;

  std::vector<std::vector<basics::AttributeName>> const& coveredFields()
      const override {
    return _coveredFields;
  }
  std::vector<std::vector<basics::AttributeName>> const& prefixFields()
      const noexcept {
    return _prefixFields;
  }

  FilterCosts supportsFilterCondition(
      transaction::Methods& /*trx*/,
      std::vector<std::shared_ptr<Index>> const& allIndexes,
      aql::AstNode const* node, aql::Variable const* reference,
      size_t itemsInIndex) const override;

  aql::AstNode* specializeCondition(
      transaction::Methods& trx, aql::AstNode* condition,
      aql::Variable const* reference) const override;

  std::vector<std::vector<basics::AttributeName>> const _storedValues;
  std::vector<std::vector<basics::AttributeName>> const _prefixFields;
  std::vector<std::vector<basics::AttributeName>> const _coveredFields;

  IndexType const _type;
};

class RocksDBMdiIndex final : public RocksDBMdiIndexBase {
 public:
  RocksDBMdiIndex(IndexId iid, LogicalCollection& coll, velocypack::Slice info);
  bool hasSelectivityEstimate() const override;

  double selectivityEstimate(std::string_view) const override;

  RocksDBCuckooIndexEstimatorType* estimator() override;
  void setEstimator(std::unique_ptr<RocksDBCuckooIndexEstimatorType>) override;
  void recalculateEstimates() override;

  Result insert(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId documentId, velocypack::Slice doc,
                OperationOptions const& options, bool performChecks) override;
  Result remove(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId documentId, velocypack::Slice doc,
                OperationOptions const& /*options*/) override;

  void truncateCommit(TruncateGuard&& guard, TRI_voc_tick_t tick,
                      transaction::Methods* trx) override;

  Result drop() override;

  std::unique_ptr<IndexIterator> iteratorForCondition(
      ResourceMonitor& monitor, transaction::Methods* trx,
      aql::AstNode const* node, aql::Variable const* reference,
      IndexIteratorOptions const& opts, ReadOwnWrites readOwnWrites,
      int) override;

 private:
  bool _estimates;
  std::unique_ptr<RocksDBCuckooIndexEstimatorType> _estimator;
};

class RocksDBUniqueMdiIndex final : public RocksDBMdiIndexBase {
  using RocksDBMdiIndexBase::RocksDBMdiIndexBase;

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

  bool hasSelectivityEstimate() const override { return true; }

  double selectivityEstimate(std::string_view) const override { return 1; }
};

namespace mdi {

struct ExpressionBounds {
  std::optional<zkd::byte_string> lower;
  std::optional<zkd::byte_string> upper;
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
}  // namespace mdi

}  // namespace arangodb
