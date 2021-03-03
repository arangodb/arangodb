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

#include "RocksDBZkdIndex.h"
#include <Aql/Variable.h>
#include "RocksDBColumnFamilyManager.h"

using namespace arangodb;

namespace arangodb {
class RocksDBZkdIndexIterator final : public IndexIterator {
 public:
  RocksDBZkdIndexIterator(LogicalCollection* collection, transaction::Methods* trx)
      : IndexIterator(collection, trx) {}

  char const* typeName() const override { return "rocksdb-zkd-index-iterator"; }

 protected:
  bool nextImpl(const LocalDocumentIdCallback& callback, size_t limit) override {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
};

}  // namespace arangodb

const char* arangodb::RocksDBZkdIndex::typeName() const { return "zkd"; }
arangodb::Index::IndexType arangodb::RocksDBZkdIndex::type() const {
  return TRI_IDX_TYPE_ZKD_INDEX;
}
bool arangodb::RocksDBZkdIndex::canBeDropped() const { return true; }
bool arangodb::RocksDBZkdIndex::isSorted() const { return false; }
bool arangodb::RocksDBZkdIndex::hasSelectivityEstimate() const { return false; }

arangodb::Result arangodb::RocksDBZkdIndex::insert(
    arangodb::transaction::Methods& trx, arangodb::RocksDBMethods* methods,
    const arangodb::LocalDocumentId& documentId,
    arangodb::velocypack::Slice doc, const arangodb::OperationOptions& options) {
  LOG_DEVEL << "RocksDBZkdIndex::insert documentId = " << documentId.id()
            << " doc = " << doc.toJson();

  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

arangodb::Result arangodb::RocksDBZkdIndex::remove(arangodb::transaction::Methods& trx,
                                                   arangodb::RocksDBMethods* methods,
                                                   const arangodb::LocalDocumentId& documentId,
                                                   arangodb::velocypack::Slice doc) {
  LOG_DEVEL << "RocksDBZkdIndex::remove documentId = " << documentId.id()
            << " doc = " << doc.toJson();

  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

arangodb::RocksDBZkdIndex::RocksDBZkdIndex(arangodb::IndexId iid,
                                           arangodb::LogicalCollection& coll,
                                           const arangodb::velocypack::Slice& info)
    : RocksDBIndex(iid, coll, info,
                   /* TODO maybe we want to add a new column family? However, we can not use VPackIndexes because
                    * they use the vpack comparator. For now use GeoIndex because it uses just the 8 byte prefix.
                    */
                   RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::GeoIndex),
                   false) {}

void arangodb::RocksDBZkdIndex::toVelocyPack(
    arangodb::velocypack::Builder& builder,
    std::underlying_type<arangodb::Index::Serialize>::type type) const {
  VPackObjectBuilder ob(&builder);
  RocksDBIndex::toVelocyPack(builder, type);
  builder.add("zkd", VPackValue(true));
}

arangodb::Index::FilterCosts arangodb::RocksDBZkdIndex::supportsFilterCondition(
    const std::vector<std::shared_ptr<arangodb::Index>>& allIndexes,
    const arangodb::aql::AstNode* node,
    const arangodb::aql::Variable* reference, size_t itemsInIndex) const {
  LOG_DEVEL << "RocksDBZkdIndex::supportsFilterCondition node = " << node->toString()
            << " reference = " << reference->name;

  return Index::supportsFilterCondition(allIndexes, node, reference, itemsInIndex);
}
arangodb::aql::AstNode* arangodb::RocksDBZkdIndex::specializeCondition(
    arangodb::aql::AstNode* node, const arangodb::aql::Variable* reference) const {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

std::unique_ptr<IndexIterator> arangodb::RocksDBZkdIndex::iteratorForCondition(
    arangodb::transaction::Methods* trx, const arangodb::aql::AstNode* node,
    const arangodb::aql::Variable* reference, const arangodb::IndexIteratorOptions& opts) {
  return std::make_unique<RocksDBZkdIndexIterator>(&_collection, trx);
}
