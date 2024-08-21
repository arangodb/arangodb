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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <faiss/invlists/InvertedLists.h>
#include <faiss/IndexFlat.h>
#include <faiss/IndexIVFFlat.h>
#include <cstdint>

#include "RocksDBIndex.h"
#include "Indexes/VectorIndexDefinition.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "Transaction/Methods.h"
#include "VocBase/Identifiers/IndexId.h"
#include "VocBase/Identifiers/LocalDocumentId.h"
#include "faiss/MetricType.h"

namespace arangodb {
class LogicalCollection;
class RocksDBMethods;

namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

class RocksDBVectorIndex;

struct RocksDBInvertedListsIterator : faiss::InvertedListsIterator {
  RocksDBInvertedListsIterator(RocksDBVectorIndex* index,
                               std::size_t listNumber, std::size_t codeSize);
  virtual bool is_available() const override;
  virtual void next() override;
  virtual std::pair<std::int64_t, const uint8_t*> get_id_and_codes() override;

 private:
  RocksDBKey _rocksdbKey;
  arangodb::RocksDBVectorIndex* _index = nullptr;

  std::unique_ptr<rocksdb::Iterator> _it;
  std::size_t _listNumber;
  std::size_t _codeSize;
  std::vector<uint8_t> _codes;  // buffer for returning codes in next()
};

struct RocksDBInvertedLists : faiss::InvertedLists {
  RocksDBInvertedLists(RocksDBVectorIndex* index,
                       RocksDBMethods* rocksDBMethods,
                       rocksdb::ColumnFamilyHandle* cf, std::size_t nlist,
                       size_t codeSize);

  std::size_t list_size(std::size_t listNumber) const override;

  const std::uint8_t* get_codes(std::size_t listNumber) const override;

  const faiss::idx_t* get_ids(std::size_t listNumber) const override;

  size_t add_entries(std::size_t listNumber, std::size_t n_entry,
                     const faiss::idx_t* ids,
                     const std::uint8_t* code) override;

  void update_entries(std::size_t listNumber, std::size_t offset,
                      std::size_t n_entry, const faiss::idx_t* ids,
                      const std::uint8_t* code) override;

  void resize(std::size_t listNumber, std::size_t new_size) override;

  faiss::InvertedListsIterator* get_iterator(
      std::size_t listNumber, void* inverted_list_context) const override;

 private:
  RocksDBVectorIndex* _index;
  RocksDBMethods* _rocksDBMethods;
  rocksdb::ColumnFamilyHandle* _cf;
};

class RocksDBVectorIndex final : public RocksDBIndex {
 public:
  RocksDBVectorIndex(IndexId iid, LogicalCollection& coll,
                     arangodb::velocypack::Slice info);

  IndexType type() const override { return Index::TRI_IDX_TYPE_VECTOR_INDEX; }

  bool isSorted() const override { return false; }

  bool canBeDropped() const override { return true; }

  // TODO
  bool hasSelectivityEstimate() const override { return false; }

  char const* typeName() const override { return "rocksdb-vector"; }

  bool matchesDefinition(VPackSlice const&) const override;

  void prepareIndex(std::unique_ptr<rocksdb::Iterator> it, rocksdb::Slice upper,
                    RocksDBMethods* methods) override;

  void toVelocyPack(
      arangodb::velocypack::Builder& builder,
      std::underlying_type<Index::Serialize>::type flags) const override;

  std::vector<std::vector<arangodb::basics::AttributeName>> const&
  coveredFields() const override {
    // index does not cover the vector index attribute!
    return Index::emptyCoveredFields;
  }

  FullVectorIndexDefinition const& getDefinition() const noexcept {
    return _definition;
  }

  FilterCosts supportsFilterCondition(
      transaction::Methods& /*trx*/,
      std::vector<std::shared_ptr<Index>> const& allIndexes,
      aql::AstNode const* node, aql::Variable const* reference,
      size_t itemsInIndex) const override;

  aql::AstNode* specializeCondition(
      transaction::Methods& trx, aql::AstNode* condition,
      aql::Variable const* reference) const override;

 protected:
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

 private:
  void finishTraining();

  FullVectorIndexDefinition _definition;
  faiss::IndexFlatL2 _quantizer;
  int count{0};
};

}  // namespace arangodb
