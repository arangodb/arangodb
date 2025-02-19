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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "VocBase/voc-types.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

namespace arangodb {
class RocksDBKeyLeaser;

namespace transaction {
class Methods;
}

class RocksDBPrimaryIndex final : public RocksDBIndex {
  friend class RocksDBPrimaryIndexEqIterator;
  friend class RocksDBPrimaryIndexInIterator;
  template<bool reverse, bool mustCheckBounds>
  friend class RocksDBPrimaryIndexRangeIterator;

 public:
  RocksDBPrimaryIndex(LogicalCollection& collection, velocypack::Slice info);

  ~RocksDBPrimaryIndex();

  IndexType type() const override { return Index::TRI_IDX_TYPE_PRIMARY_INDEX; }

  char const* typeName() const override { return "primary"; }

  bool canBeDropped() const override { return false; }

  bool isSorted() const override { return true; }

  std::vector<std::vector<basics::AttributeName>> const& coveredFields()
      const override;

  bool hasSelectivityEstimate() const override { return true; }

  double selectivityEstimate(
      std::string_view = std::string_view()) const override {
    return 1.0;
  }

  void load() override;

  void toVelocyPack(VPackBuilder&, std::underlying_type<Index::Serialize>::type)
      const override;

  LocalDocumentId lookupKey(transaction::Methods* trx, std::string_view key,
                            ReadOwnWrites readOwnWrites,
                            bool& foundInCache) const;

  /// @brief reads a revision id from the primary index
  /// the revision id will only be non-zero if the primary index
  /// value contains the document's revision id. note that this is not
  /// the case for older collections
  /// in this case the caller must fetch the revision id from the actual
  /// document
  Result lookupRevision(transaction::Methods* trx, std::string_view key,
                        LocalDocumentId& id, RevisionId& revisionId,
                        ReadOwnWrites, bool lockForUpdate = false) const;

  Index::FilterCosts supportsFilterCondition(
      transaction::Methods& trx,
      std::vector<std::shared_ptr<Index>> const& allIndexes,
      aql::AstNode const* node, aql::Variable const* reference,
      size_t itemsInIndex) const override;

  Index::SortCosts supportsSortCondition(aql::SortCondition const* node,
                                         aql::Variable const* reference,
                                         size_t itemsInIndex) const override;

  std::unique_ptr<IndexIterator> iteratorForCondition(
      ResourceMonitor& monitor, transaction::Methods* trx,
      aql::AstNode const* node, aql::Variable const* reference,
      IndexIteratorOptions const& opts, ReadOwnWrites readOwnWrites,
      int) override;

  aql::AstNode* specializeCondition(
      transaction::Methods& trx, aql::AstNode* node,
      aql::Variable const* reference) const override;

  StreamSupportResult supportsStreamInterface(
      IndexStreamOptions const&) const noexcept override;

  static StreamSupportResult checkSupportsStreamInterface(
      std::vector<std::vector<basics::AttributeName>> const& coveredFields,
      IndexStreamOptions const&) noexcept;

  virtual std::unique_ptr<AqlIndexStreamIterator> streamForCondition(
      transaction::Methods* trx, IndexStreamOptions const&) override;

  /// @brief returns whether the document can be inserted into the primary index
  /// (or if there will be a conflict)
  Result checkInsert(transaction::Methods& trx, RocksDBMethods* methods,
                     LocalDocumentId documentId, velocypack::Slice doc,
                     OperationOptions const& options) override;

  Result checkReplace(transaction::Methods& trx, RocksDBMethods* methods,
                      LocalDocumentId documentId, velocypack::Slice doc,
                      OperationOptions const& options) override;

  /// insert index elements into the specified write batch.
  Result insert(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId documentId, velocypack::Slice doc,
                OperationOptions const& options, bool performChecks) override;

  /// remove index elements and put it in the specified write batch.
  Result remove(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId documentId, velocypack::Slice doc,
                OperationOptions const& /*options*/) override;

  Result update(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId oldDocumentId, velocypack::Slice oldDoc,
                LocalDocumentId newDocumentId, velocypack::Slice newDoc,
                OperationOptions const& /*options*/,
                bool /*performChecks*/) override;

 private:
  /// @brief create the iterator, for a single attribute, IN operator
  std::unique_ptr<IndexIterator> createInIterator(ResourceMonitor& monitor,
                                                  transaction::Methods*,
                                                  aql::AstNode const*,
                                                  aql::AstNode const*,
                                                  bool ascending);

  /// @brief create the iterator, for a single attribute, EQ operator
  std::unique_ptr<IndexIterator> createEqIterator(ResourceMonitor& monitor,
                                                  transaction::Methods*,
                                                  aql::AstNode const*,
                                                  aql::AstNode const*,
                                                  ReadOwnWrites readOwnWrites);

  /// @brief populate the keys builder with the keys from the array, in either
  /// forward or backward order
  void fillInLookupValues(transaction::Methods*, velocypack::Builder& keys,
                          aql::AstNode const* values, bool ascending,
                          bool isId) const;

  /// @brief add a single value node to the iterator's keys
  void handleValNode(transaction::Methods* trx, VPackBuilder& keys,
                     aql::AstNode const* valNode, bool isId) const;

 private:
  std::vector<std::vector<basics::AttributeName>> const _coveredFields;

  // maximum size of values to be stored in the in-memory cache
  size_t const _maxCacheValueSize;
};
}  // namespace arangodb
