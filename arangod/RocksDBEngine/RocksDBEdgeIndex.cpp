////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBEdgeIndex.h"
#include "Aql/AstNode.h"
#include "Aql/SortCondition.h"
#include "Basics/Exceptions.h"
#include "Basics/LocalTaskQueue.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringRef.h"
#include "Indexes/SimpleAttributeEqualityMatcher.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "RocksDBEngine/RocksDBToken.h"

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/slice.h>
#include <rocksdb/utilities/transaction_db.h>

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;

/// @brief hard-coded vector of the index attributes
/// note that the attribute names must be hard-coded here to avoid an init-order
/// fiasco with StaticStrings::FromString etc.
/*static std::vector<std::vector<arangodb::basics::AttributeName>> const
    IndexAttributes{{arangodb::basics::AttributeName("_from", false)},
                    {arangodb::basics::AttributeName("_to", false)}};*/

RocksDBEdgeIndexIterator::RocksDBEdgeIndexIterator(
    LogicalCollection* collection, transaction::Methods* trx,
    ManagedDocumentResult* mmdr, arangodb::RocksDBEdgeIndex const* index,
    std::unique_ptr<VPackBuilder>& keys)
    : IndexIterator(collection, trx, mmdr, index),
      _keys(keys.get()),
      _iterator(_keys->slice()),
      _index(index) {
  keys.release();  // now we have ownership for _keys
}

RocksDBEdgeIndexIterator::~RocksDBEdgeIndexIterator() {
  if (_keys != nullptr) {
    // return the VPackBuilder to the transaction context
    _trx->transactionContextPtr()->returnBuilder(_keys.release());
  }
}

bool RocksDBEdgeIndexIterator::next(TokenCallback const& cb, size_t limit) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  
  if (limit == 0 || !_iterator.valid()) {
    // No limit no data, or we are actually done. The last call should have returned false
    TRI_ASSERT(limit > 0); // Someone called with limit == 0. Api broken
    return false;
  }
  RocksDBCollection *rocksColl = RocksDBCollection::toRocksDBCollection(_collection);

  while (limit > 0) {
    
    VPackSlice fromTo = _iterator.value();
    TRI_ASSERT(fromTo.isString());
    //if (tmp.isObject()) {
    //  tmp = tmp.get(StaticStrings::IndexEq);
    //}
    
    size_t prefixSize;
    std::unique_ptr<char> str = _index->buildRangePrefix(fromTo, prefixSize);
    
    rocksdb::ReadOptions readOptions;
    std::unique_ptr<rocksdb::Iterator> iter(_index->_db->NewIterator(readOptions));
    
    rocksdb::Slice rSlice(str.get(), prefixSize);
    iter->Seek(rSlice);
    while (iter->Valid() && iter->key().starts_with(rSlice)) {
      TRI_ASSERT(iter->key().size() > prefixSize);
      VPackSlice edgeKey = VPackSlice(iter->key().data() + prefixSize);
      TRI_ASSERT(edgeKey.isString());

      // TODO do we need to handle failed lookups here?
      RocksDBToken token;
      Result res = rocksColl->lookupDocumentToken(_trx, edgeKey, token);
      if (res.ok()) {
        cb(token);
        if (--limit == 0) {
          break;
        }
      }
      
      iter->Next();
    }
    if (limit > 0) {
      _iterator.next();
      if (!_iterator.valid()) {
        return false;
      }
    }
  }
  return true;
}

void RocksDBEdgeIndexIterator::reset() {
  _iterator.reset();
}

// ============================= Index ====================================

RocksDBEdgeIndex::RocksDBEdgeIndex(rocksdb::TransactionDB* db,
                                   TRI_idx_iid_t iid,
                                   arangodb::LogicalCollection* collection,
                                   std::string const& attr)
    : Index(iid, collection, std::vector<std::vector<AttributeName>>(
                                 {{AttributeName(attr, false)}}),
            false, false),
      _db(db),
      _directionAttr(attr) {
  /*std::vector<std::vector<arangodb::basics::AttributeName>>(
                  {{arangodb::basics::AttributeName(StaticStrings::FromString,
                                                    false)},
                   {arangodb::basics::AttributeName(StaticStrings::ToString,
                                                    false)}})*/
  TRI_ASSERT(iid != 0);
#warning fix object ID
  TRI_voc_tick_t databaseId = collection->vocbase()->id();
  RocksDBKey entry = RocksDBKey::Index(databaseId, collection->cid(), iid);
  _objectId = 3413415 + iid;//entry.key;
}

RocksDBEdgeIndex::~RocksDBEdgeIndex() {}

/// @brief return a selectivity estimate for the index
double RocksDBEdgeIndex::selectivityEstimate(
    arangodb::StringRef const* attribute) const {
  if (ServerState::instance()->isCoordinator()) {
    // use hard-coded selectivity estimate in case of cluster coordinator
    return 0.1;
  }

  if (attribute != nullptr) {
    // the index attribute is given here
    // now check if we can restrict the selectivity estimation to the correct
    // part of the index
    if (attribute->compare(_directionAttr) == 0) {
      // _from
      return 0.2;  //_edgesFrom->selectivity();
    } else {
      return 0;
    }
    // other attribute. now return the average selectivity
  }

  // return average selectivity of the two index parts
  // double estimate = (_edgesFrom->selectivity() + _edgesTo->selectivity()) *
  // 0.5;
  // TRI_ASSERT(estimate >= 0.0 &&
  //           estimate <= 1.00001);  // floating-point tolerance
  return 0.1;
}

/// @brief return the memory usage for the index
size_t RocksDBEdgeIndex::memory() const {
  // TODO
  return 0;
}

/// @brief return a VelocyPack representation of the index
void RocksDBEdgeIndex::toVelocyPack(VPackBuilder& builder,
                                    bool withFigures) const {
  Index::toVelocyPack(builder, withFigures);

  // hard-coded
  builder.add("unique", VPackValue(false));
  builder.add("sparse", VPackValue(false));
}

/// @brief return a VelocyPack representation of the index figures
void RocksDBEdgeIndex::toVelocyPackFigures(VPackBuilder& builder) const {
  Index::toVelocyPackFigures(builder);
  // TODO
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
}

int RocksDBEdgeIndex::insert(transaction::Methods* trx,
                             TRI_voc_rid_t revisionId, VPackSlice const& doc,
                             bool isRollback) {
  // uint64_t collId = this->_collection->cid();
  // RocksDBEntry entry = RocksDBEntry::IndexValue(_objectId, revisionId, doc);
  /*VPackSlice key;
  if (_directionAttr == StaticStrings::FromString) {
    key = doc.get(StaticStrings::ToString);
  } else {
    key = doc.get(StaticStrings::FromString);
  }*/

  size_t keySize;
  std::unique_ptr<char> key = buildIndexValue(doc, keySize);
  if (key) {
    rocksdb::WriteOptions writeOptions;
    rocksdb::Status status = _db->Put(
        writeOptions, rocksdb::Slice(key.get(), keySize), rocksdb::Slice());
    if (status.ok()) {
      return TRI_ERROR_NO_ERROR;
    } else {
      Result res = rocksutils::convertStatus(status);
      return res.errorNumber();
    }
  } else {
    return TRI_ERROR_INTERNAL;
  }
}

int RocksDBEdgeIndex::remove(transaction::Methods* trx,
                             TRI_voc_rid_t revisionId, VPackSlice const& doc,
                             bool isRollback) {
  size_t keySize;
  std::unique_ptr<char> key = buildIndexValue(doc, keySize);
  if (key) {
    rocksdb::WriteOptions writeOptions;
    rocksdb::Status status =
        _db->Delete(writeOptions, rocksdb::Slice(key.get(), keySize));
    if (status.ok()) {
      return TRI_ERROR_NO_ERROR;
    } else {
      Result res = rocksutils::convertStatus(status, rocksutils::StatusHint::index);
      return res.errorNumber();
    }
  } else {
    return TRI_ERROR_INTERNAL;
  }
}
/*
struct RDBEdgeInsertTask : public LocalTask {
  RocksDBEdgeIndex* _index;
  std::shared_ptr<rocksdb::Transaction> _rtrx;
  VPackSlice _doc;

  RDBEdgeInsertTask(arangodb::basics::LocalTaskQueue* queue,
                    RocksDBEdgeIndex* index,
                    std::shared_ptr<rocksdb::Transaction> rtrx, VPackSlice doc)
      : LocalTask(queue), _index(index), _rtrx(rtrx), _doc(doc) {}

  void run() override {
    size_t keySize;
    std::unique_ptr<char> key = buildIndexValue(
        _index->_objectId, _index->_directionAttr, _doc, keySize);

    rocksdb::Status status =
        _rtrx->Put(rocksdb::Slice(key.get(), keySize), rocksdb::Slice());
    if (!status.ok()) {
      Result res = convertRocksDBStatus(status, StatusHint::index);
      _queue->setStatus(res.errorNumber());
    }
  }
};*/

void RocksDBEdgeIndex::batchInsert(
    transaction::Methods* trx,
    std::vector<std::pair<TRI_voc_rid_t, VPackSlice>> const& documents,
    arangodb::basics::LocalTaskQueue* queue) {
  // setup rocksdb transaction
  rocksdb::WriteOptions writeOptions;
  rocksdb::TransactionOptions transactionOptions;
  std::unique_ptr<rocksdb::Transaction> rtxr(
      _db->BeginTransaction(writeOptions, transactionOptions));

  for (std::pair<TRI_voc_rid_t, VPackSlice> doc : documents) {
    size_t keySize;
    std::unique_ptr<char> key = buildIndexValue(doc.second, keySize);

    rocksdb::Status status =
        rtxr->Put(rocksdb::Slice(key.get(), keySize), rocksdb::Slice());
    if (!status.ok()) {
      Result res = rocksutils::convertStatus(status, rocksutils::StatusHint::index);
      queue->setStatus(res.errorNumber());
      rtxr->Rollback();
      break;
    }

    // auto task =
    // std::make_shared<RDBEdgeInsertTask>(queue, this, rtxr, doc.second);
    // queue->enqueue(task);
  }
  rocksdb::Status status = rtxr->Commit();
  if (!status.ok()) {
    Result res = rocksutils::convertStatus(status, rocksutils::StatusHint::index);
    queue->setStatus(res.errorNumber());
  }

  // commit in callback called after all tasks finish
  /*std::shared_ptr<LocalCallbackTask> callback(
      new LocalCallbackTask(queue, [rtxr, queue] {
        rocksdb::Status status = rtxr->Commit();
        if (!status.ok()) {
          Result res = convertRocksDBStatus(status);
          queue->setStatus(res.errorNumber());
        }
      }));
  try {
    for (std::pair<TRI_voc_rid_t, VPackSlice> doc : documents) {
      auto task =
          std::make_shared<RDBEdgeInsertTask>(queue, this, rtxr, doc.second);
      queue->enqueue(task);
    }
  } catch (...) {
    queue->setStatus(TRI_ERROR_INTERNAL);
  }
  queue->enqueueCallback(callback);*/
}

/// @brief unload the index data from memory
int RocksDBEdgeIndex::unload() {
  // nothing to do here
  return TRI_ERROR_NO_ERROR;
}

/// @brief provides a size hint for the edge index
int RocksDBEdgeIndex::sizeHint(transaction::Methods* trx, size_t size) {
  // nothing to do here
  return TRI_ERROR_NO_ERROR;
}

/// @brief checks whether the index supports the condition
bool RocksDBEdgeIndex::supportsFilterCondition(
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    size_t& estimatedItems, double& estimatedCost) const {
  SimpleAttributeEqualityMatcher matcher(this->_fields);
  return matcher.matchOne(this, node, reference, itemsInIndex, estimatedItems,
                          estimatedCost);
}

/// @brief creates an IndexIterator for the given Condition
IndexIterator* RocksDBEdgeIndex::iteratorForCondition(
    transaction::Methods* trx, ManagedDocumentResult* mmdr,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, bool reverse) const {
  TRI_ASSERT(node->type == aql::NODE_TYPE_OPERATOR_NARY_AND);

  TRI_ASSERT(node->numMembers() == 1);

  auto comp = node->getMember(0);

  // assume a.b == value
  auto attrNode = comp->getMember(0);
  auto valNode = comp->getMember(1);

  if (attrNode->type != aql::NODE_TYPE_ATTRIBUTE_ACCESS) {
    // got value == a.b  -> flip sides
    attrNode = comp->getMember(1);
    valNode = comp->getMember(0);
  }
  TRI_ASSERT(attrNode->type == aql::NODE_TYPE_ATTRIBUTE_ACCESS);
  TRI_ASSERT(attrNode->stringEquals(_directionAttr));

  if (comp->type == aql::NODE_TYPE_OPERATOR_BINARY_EQ) {
    // a.b == value
    return createEqIterator(trx, mmdr, attrNode, valNode);
  }

  if (comp->type == aql::NODE_TYPE_OPERATOR_BINARY_IN) {
    // a.b IN values
    if (!valNode->isArray()) {
      return nullptr;
    }

    return createInIterator(trx, mmdr, attrNode, valNode);
  }

  // operator type unsupported
  return nullptr;
}

/// @brief specializes the condition for use with the index
arangodb::aql::AstNode* RocksDBEdgeIndex::specializeCondition(
    arangodb::aql::AstNode* node,
    arangodb::aql::Variable const* reference) const {
  // SimpleAttributeEqualityMatcher matcher(IndexAttributes);
  SimpleAttributeEqualityMatcher matcher(this->_fields);
  return matcher.specializeOne(this, node, reference);
}

/// @brief Transform the list of search slices to search values.
///        This will multiply all IN entries and simply return all other
///        entries.
void RocksDBEdgeIndex::expandInSearchValues(VPackSlice const slice,
                                            VPackBuilder& builder) const {
  TRI_ASSERT(slice.isArray());
  builder.openArray();
  for (auto const& side : VPackArrayIterator(slice)) {
    if (side.isNull()) {
      builder.add(side);
    } else {
      TRI_ASSERT(side.isArray());
      builder.openArray();
      for (auto const& item : VPackArrayIterator(side)) {
        TRI_ASSERT(item.isObject());
        if (item.hasKey(StaticStrings::IndexEq)) {
          TRI_ASSERT(!item.hasKey(StaticStrings::IndexIn));
          builder.add(item);
        } else {
          TRI_ASSERT(item.hasKey(StaticStrings::IndexIn));
          VPackSlice list = item.get(StaticStrings::IndexIn);
          TRI_ASSERT(list.isArray());
          for (auto const& it : VPackArrayIterator(list)) {
            builder.openObject();
            builder.add(StaticStrings::IndexEq, it);
            builder.close();
          }
        }
      }
      builder.close();
    }
  }
  builder.close();
}

// ===================== Helpers ==================

/// @brief create the iterator
IndexIterator* RocksDBEdgeIndex::createEqIterator(
    transaction::Methods* trx, ManagedDocumentResult* mmdr,
    arangodb::aql::AstNode const* attrNode,
    arangodb::aql::AstNode const* valNode) const {
  // lease builder, but immediately pass it to the unique_ptr so we don't leak
  transaction::BuilderLeaser builder(trx);
  std::unique_ptr<VPackBuilder> keys(builder.steal());
  keys->openArray();

  handleValNode(keys.get(), valNode);
  TRI_IF_FAILURE("EdgeIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  keys->close();

  return new RocksDBEdgeIndexIterator(_collection, trx, mmdr, this, keys);
}

/// @brief create the iterator
IndexIterator* RocksDBEdgeIndex::createInIterator(
    transaction::Methods* trx, ManagedDocumentResult* mmdr,
    arangodb::aql::AstNode const* attrNode,
    arangodb::aql::AstNode const* valNode) const {
  // lease builder, but immediately pass it to the unique_ptr so we don't leak
  transaction::BuilderLeaser builder(trx);
  std::unique_ptr<VPackBuilder> keys(builder.steal());
  keys->openArray();

  size_t const n = valNode->numMembers();
  for (size_t i = 0; i < n; ++i) {
    handleValNode(keys.get(), valNode->getMemberUnchecked(i));
    TRI_IF_FAILURE("EdgeIndex::iteratorValNodes") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
  }

  TRI_IF_FAILURE("EdgeIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  keys->close();

  return new RocksDBEdgeIndexIterator(_collection, trx, mmdr, this, keys);
}

/// @brief add a single value node to the iterator's keys
void RocksDBEdgeIndex::handleValNode(
    VPackBuilder* keys, arangodb::aql::AstNode const* valNode) const {
  if (!valNode->isStringValue() || valNode->getStringLength() == 0) {
    return;
  }

  keys->openObject();
  keys->add(StaticStrings::IndexEq,
            VPackValuePair(valNode->getStringValue(),
                           valNode->getStringLength(), VPackValueType::String));
  keys->close();

  TRI_IF_FAILURE("EdgeIndex::collectKeys") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
}

// ================================= TODO use RocksDBKey ===========================

std::unique_ptr<char> RocksDBEdgeIndex::buildIndexValue(VPackSlice const& doc,
                                                        size_t& outSize) const {
  VPackSlice key = doc.get(StaticStrings::KeyString);
  VPackSlice fromTo = doc.get(_directionAttr);
  TRI_ASSERT(key.isString() && fromTo.isString());
  uint64_t keySize, fromToSize;
  const char* keyPtr = key.getString(keySize);
  const char* fromToPtr = fromTo.getString(fromToSize);
  TRI_ASSERT(keySize > 0 && fromToSize > 0);

  size_t bufSize = 2 * sizeof(char) + sizeof(uint64_t) + fromToSize + keySize;
  std::unique_ptr<char> buffer(new char[bufSize]);

  // TODO maybe use StringBuffer
  char* ptr = buffer.get();
  ptr[0] = (char)RocksDBEntryType::UniqueIndexValue;
  ptr += sizeof(char);
  rocksutils::uint64ToPersistent(ptr, _objectId);
  ptr += sizeof(uint64_t);
  memcpy(ptr, fromToPtr, fromToSize);
  ptr += fromToSize;
  *(++ptr) = '\0';
  memcpy(ptr, keyPtr, keySize);
  TRI_ASSERT(ptr + keySize == buffer.get() + bufSize);

  outSize = bufSize;
  return buffer;
}

std::unique_ptr<char> RocksDBEdgeIndex::buildRangePrefix(VPackSlice const& fromTo,
                                                         size_t& outSize) const {
  uint64_t fromToSize;
  const char* fromToPtr = fromTo.getString(fromToSize);
  TRI_ASSERT(fromToSize > 0);

  size_t bufSize = 2 * sizeof(char) + sizeof(uint64_t) + fromToSize;
  std::unique_ptr<char> buffer(new char[bufSize]);

  // TODO maybe use StringBuffer
  char* ptr = buffer.get();
  ptr[0] = (char)RocksDBEntryType::UniqueIndexValue;
  ptr += sizeof(char);
  rocksutils::uint64ToPersistent(ptr, _objectId);
  ptr += sizeof(uint64_t);
  memcpy(ptr, fromToPtr, fromToSize);
  ptr += fromToSize;
  *(++ptr) = '\0';

  outSize = bufSize;
  return buffer;
}
