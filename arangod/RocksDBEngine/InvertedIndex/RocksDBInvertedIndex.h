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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/ResultT.h"
#include "RocksDBEngine/InvertedIndex/Format.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "VocBase/Identifiers/IndexId.h"

#include <velocypack/Slice.h>

#include <string>
#include <vector>

namespace arangodb {
namespace inverted_index {

/// Distinct stored forms of the elements of the array at `field`, sorted,
/// see normalizedTerm. Empty if the attribute is missing or not an array.
/// Fails for an element without a stored form.
ResultT<std::vector<std::string>> indexedTerms(
    velocypack::Slice doc, std::vector<basics::AttributeName> const& field);

}  // namespace inverted_index

/// Inverted index over the scalar elements of one array attribute. Per
/// element it stores a document count and Roaring bitmaps of
/// LocalDocumentIds, sharded by the high id bits. Writes are blind RocksDB
/// merge operands.
class RocksDBInvertedIndex final : public RocksDBIndex {
 public:
  RocksDBInvertedIndex(IndexId iid, LogicalCollection& collection,
                       velocypack::Slice info);

  IndexType type() const override { return IndexType::RocksDBInverted; }

  char const* typeName() const override { return "rocksdb-inverted"; }

  bool canBeDropped() const override { return true; }

  bool isSorted() const override { return false; }

  bool hasSelectivityEstimate() const override { return false; }

  void toVelocyPack(
      velocypack::Builder& builder,
      std::underlying_type<Index::Serialize>::type flags) const override;

  inverted_index::IdLayout idLayout() const noexcept { return _layout; }

  std::vector<std::vector<basics::AttributeName>> const& coveredFields()
      const override {
    return Index::emptyCoveredFields;
  }

  FilterCosts supportsFilterCondition(
      transaction::Methods& trx,
      std::vector<std::shared_ptr<Index>> const& allIndexes,
      aql::AstNode const* node, aql::Variable const* reference,
      size_t itemsInIndex) const override;

  aql::AstNode* specializeCondition(
      transaction::Methods& trx, aql::AstNode* node,
      aql::Variable const* reference) const override;

  std::unique_ptr<IndexIterator> iteratorForCondition(
      ResourceMonitor& monitor, transaction::Methods* trx,
      aql::AstNode const* node, aql::Variable const* reference,
      IndexIteratorOptions const& opts, ReadOwnWrites readOwnWrites,
      int mutableConditionIdx) override;

 protected:
  Result insert(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId documentId, velocypack::Slice doc,
                OperationOptions const& options, bool performChecks) override;

  Result remove(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId documentId, velocypack::Slice doc,
                OperationOptions const& options) override;

 private:
  Result write(RocksDBMethods& methods, LocalDocumentId documentId,
               velocypack::Slice doc, bool isInsert);

  inverted_index::IdLayout _layout;
};

}  // namespace arangodb
