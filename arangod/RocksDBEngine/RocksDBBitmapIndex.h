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
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "VocBase/Identifiers/IndexId.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include <string>
#include <string_view>
#include <vector>

namespace arangodb {
namespace bitmap_index {

/// Stores every value as it is. Keys are compared like velocypack index keys,
/// so values AQL considers equal share a key.
struct AnyValues {
  static constexpr std::string_view kName = "any";
  static constexpr auto kFamily =
      RocksDBColumnFamilyManager::Family::BitmapIndex;
  static constexpr auto kEntryType = RocksDBEntryType::BitmapIndexValue;

  /// append the stored form of `value`; false if it cannot be stored
  static bool add(velocypack::Builder& out, velocypack::Slice value) {
    out.add(value);
    return true;
  }
};

/// Stores scalars only, normalized so that values AQL considers equal have
/// equal bytes (numbers as doubles) and keys can be compared bytewise.
/// Documents with other values are rejected.
struct ScalarValues {
  static constexpr std::string_view kName = "scalar";
  static constexpr auto kFamily =
      RocksDBColumnFamilyManager::Family::BitmapIndexScalar;
  static constexpr auto kEntryType = RocksDBEntryType::BitmapIndexScalarValue;

  static bool add(velocypack::Builder& out, velocypack::Slice value);
};

}  // namespace bitmap_index

/// Inverted index over the elements of an array attribute. The last field is
/// the array attribute; all preceding fields are equality prefixes (e.g.
/// `_from` for traversals). Per (prefix values, element) it stores Roaring
/// bitmaps of LocalDocumentIds, sharded by the high id bits, plus a document
/// count. Writes are blind RocksDB merge operands. How values are stored is
/// chosen per index with the `values` attribute, see bitmap_index::AnyValues
/// and bitmap_index::ScalarValues.
class RocksDBBitmapIndex : public RocksDBIndex {
 public:
  static constexpr std::string_view kValuesAttribute = "values";

  static std::shared_ptr<RocksDBBitmapIndex> create(IndexId iid,
                                                    LogicalCollection& collection,
                                                    velocypack::Slice info);

  /// bounds of a bitmap index given its definition, e.g. to drop it
  static RocksDBKeyBounds boundsFor(velocypack::Slice definition,
                                    std::uint64_t objectId);

  IndexType type() const override { return IndexType::Bitmap; }

  char const* typeName() const override { return "bitmap"; }

  bool canBeDropped() const override { return true; }

  bool isSorted() const override { return false; }

  bool hasSelectivityEstimate() const override { return false; }

  void toVelocyPack(
      velocypack::Builder& builder,
      std::underlying_type<Index::Serialize>::type flags) const override;

  /// on edge collections, an index with fields [_from|_to, attr] covers
  /// [_from, _to] (resp. [_to, _from]): the other vertex of each matching edge
  /// is taken from the edge index, so traversals need not read edges
  std::vector<std::vector<basics::AttributeName>> const& coveredFields()
      const override {
    return _coveredFields;
  }

  /// covered fields of a bitmap index with the given fields
  static std::vector<std::vector<basics::AttributeName>> coveredFieldsFor(
      LogicalCollection const& collection,
      std::vector<std::vector<basics::AttributeName>> const& fields);

  bool matchesDefinition(velocypack::Slice const& info) const override;

  FilterCosts supportsFilterCondition(
      transaction::Methods& trx,
      std::vector<std::shared_ptr<Index>> const& allIndexes,
      aql::AstNode const* node, aql::Variable const* reference,
      size_t itemsInIndex) const override;

  aql::AstNode* specializeCondition(
      transaction::Methods& trx, aql::AstNode* node,
      aql::Variable const* reference) const override;

  /// value of the i-th prefix field of a document; null if missing
  velocypack::Slice prefixValue(velocypack::Slice doc, std::size_t i) const;

  /// distinct elements of the document's array attribute, compared like AQL
  /// does; none if the attribute is missing or not an array
  std::vector<velocypack::Slice> distinctElements(velocypack::Slice doc) const;

 protected:
  RocksDBBitmapIndex(IndexId iid, LogicalCollection& collection,
                     velocypack::Slice info, rocksdb::ColumnFamilyHandle* cf);

  virtual std::string_view valuesName() const = 0;

 private:
  std::vector<std::vector<basics::AttributeName>> const _coveredFields;
};

template<typename Values>
class RocksDBBitmapIndexImpl final : public RocksDBBitmapIndex {
 public:
  RocksDBBitmapIndexImpl(IndexId iid, LogicalCollection& collection,
                         velocypack::Slice info);

  using RocksDBIndex::getBounds;
  RocksDBKeyBounds getBounds(std::uint64_t objectId) const override {
    return RocksDBKeyBounds::BitmapIndex(objectId, Values::kEntryType);
  }

  std::unique_ptr<IndexIterator> iteratorForCondition(
      ResourceMonitor& monitor, transaction::Methods* trx,
      aql::AstNode const* node, aql::Variable const* reference,
      IndexIteratorOptions const& opts, ReadOwnWrites readOwnWrites,
      int mutableConditionIdx) override;

 protected:
  std::string_view valuesName() const override { return Values::kName; }

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

extern template class RocksDBBitmapIndexImpl<bitmap_index::AnyValues>;
extern template class RocksDBBitmapIndexImpl<bitmap_index::ScalarValues>;

}  // namespace arangodb
