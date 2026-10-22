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

#include "Indexes/Index.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "VocBase/Identifiers/IndexId.h"

#include <string>
#include <vector>

namespace arangodb {

/// Inverted index over the elements of an array attribute. The last field is
/// the array attribute; all preceding fields are scalar equality prefixes
/// (e.g. `_from` for traversals). Per (prefix values, element) it stores
/// Roaring bitmaps of LocalDocumentIds, sharded by the high id bits, plus a
/// document count. Writes are blind RocksDB merge operands.
class RocksDBBitmapIndex final : public RocksDBIndex {
 public:
  RocksDBBitmapIndex(IndexId iid, LogicalCollection& collection,
                     velocypack::Slice info);

  IndexType type() const override { return IndexType::Bitmap; }

  char const* typeName() const override { return "bitmap"; }

  bool canBeDropped() const override { return true; }

  bool isSorted() const override { return false; }

  bool hasSelectivityEstimate() const override { return false; }

  void toVelocyPack(
      velocypack::Builder& builder,
      std::underlying_type<Index::Serialize>::type flags) const override;

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

  /// encoded prefix values of a document; false if the document has a
  /// non-scalar prefix value and is therefore not indexed
  bool encodePrefix(velocypack::Slice doc, std::string& out) const;

  /// distinct encoded elements of the document's array attribute
  std::vector<std::string> encodeTerms(velocypack::Slice doc) const;

 protected:
  Result insert(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId documentId, velocypack::Slice doc,
                OperationOptions const& options, bool performChecks) override;

  Result remove(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId documentId, velocypack::Slice doc,
                OperationOptions const& options) override;

 private:
  Result write(RocksDBMethods* methods, LocalDocumentId documentId,
               velocypack::Slice doc, bool isInsert);
};

}  // namespace arangodb
