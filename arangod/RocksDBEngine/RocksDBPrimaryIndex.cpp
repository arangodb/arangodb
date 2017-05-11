////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBPrimaryIndex.h"
#include "Aql/AstNode.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Cache/CachedValue.h"
#include "Cache/TransactionalCache.h"
#include "Cluster/ServerState.h"
#include "Indexes/SimpleAttributeEqualityMatcher.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBComparator.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBToken.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "RocksDBEngine/RocksDBValue.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

#include <rocksdb/iterator.h>
#include <rocksdb/utilities/transaction.h>

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

// ================ Primary Index Iterator ================

/// @brief hard-coded vector of the index attributes
/// note that the attribute names must be hard-coded here to avoid an init-order
/// fiasco with StaticStrings::FromString etc.
static std::vector<std::vector<arangodb::basics::AttributeName>> const
    IndexAttributes{{arangodb::basics::AttributeName("_id", false)},
                    {arangodb::basics::AttributeName("_key", false)}};

RocksDBPrimaryIndexIterator::RocksDBPrimaryIndexIterator(
    LogicalCollection* collection, transaction::Methods* trx,
    ManagedDocumentResult* mmdr, RocksDBPrimaryIndex* index,
    std::unique_ptr<VPackBuilder>& keys)
    : IndexIterator(collection, trx, mmdr, index),
      _index(index),
      _keys(keys.get()),
      _iterator(_keys->slice()) {
  keys.release();  // now we have ownership for _keys
  TRI_ASSERT(_keys->slice().isArray());
}

RocksDBPrimaryIndexIterator::~RocksDBPrimaryIndexIterator() {
  if (_keys != nullptr) {
    // return the VPackBuilder to the transaction context
    _trx->transactionContextPtr()->returnBuilder(_keys.release());
  }
}

bool RocksDBPrimaryIndexIterator::next(TokenCallback const& cb, size_t limit) {
  if (limit == 0 || !_iterator.valid()) {
    // No limit no data, or we are actually done. The last call should have
    // returned false
    TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
    return false;
  }

  ManagedDocumentResult result;
  while (limit > 0) {
    // TODO: prevent copying of the value into result, as we don't need it here!
    RocksDBToken token = _index->lookupKey(_trx, *_iterator, result);
    cb(token);

    --limit;
    _iterator.next();
    if (!_iterator.valid()) {
      return false;
    }
  }
  return true;
}

void RocksDBPrimaryIndexIterator::reset() { _iterator.reset(); }

// ================ All Iterator ==================

RocksDBAllIndexIterator::RocksDBAllIndexIterator(
    LogicalCollection* collection, transaction::Methods* trx,
    ManagedDocumentResult* mmdr, RocksDBPrimaryIndex const* index, bool reverse)
    : IndexIterator(collection, trx, mmdr, index),
      _reverse(reverse),
      _bounds(RocksDBKeyBounds::PrimaryIndex(index->objectId())) {
  // acquire rocksdb transaction
  RocksDBTransactionState* state = rocksutils::toRocksTransactionState(trx);
  TRI_ASSERT(state != nullptr);

  rocksdb::Transaction* rtrx = state->rocksTransaction();
  auto const& options = state->readOptions();
  TRI_ASSERT(options.snapshot != nullptr);
  TRI_ASSERT(options.prefix_same_as_start);

  _iterator.reset(rtrx->GetIterator(options));
  if (reverse) {
    _iterator->SeekForPrev(_bounds.end());
  } else {
    _iterator->Seek(_bounds.start());
  }
}

bool RocksDBAllIndexIterator::next(TokenCallback const& cb, size_t limit) {
  TRI_ASSERT(_trx->state()->isRunning());

  if (limit == 0 || !_iterator->Valid()) {
    // No limit no data, or we are actually done. The last call should have
    // returned false
    TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
    return false;
  }

  while (limit > 0) {
    RocksDBToken token(RocksDBValue::revisionId(_iterator->value()));
    cb(token);

    --limit;

    if (_reverse) {
      _iterator->Prev();
    } else {
      _iterator->Next();
    }

    if (!_iterator->Valid()) {
      return false;
    }
  }

  return true;
}

/// special method to expose the document key for incremental replication
bool RocksDBAllIndexIterator::nextWithKey(TokenKeyCallback const& cb,
                                          size_t limit) {
  TRI_ASSERT(_trx->state()->isRunning());

  if (limit == 0 || !_iterator->Valid()) {
    // No limit no data, or we are actually done. The last call should have
    // returned false
    TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
    return false;
  }

  while (limit > 0) {
    RocksDBToken token(RocksDBValue::revisionId(_iterator->value()));
    StringRef key = RocksDBKey::primaryKey(_iterator->key());
    cb(token, key);
    --limit;

    if (_reverse) {
      _iterator->Prev();
    } else {
      _iterator->Next();
    }
    if (!_iterator->Valid()) {
      return false;
    }
  }
  return true;
}

void RocksDBAllIndexIterator::seek(StringRef const& key) {
  TRI_ASSERT(_trx->state()->isRunning());
  // don't want to get the index pointer just for this
  uint64_t objectId = _bounds.objectId();
  RocksDBKey val = RocksDBKey::PrimaryIndexValue(objectId, key);
  if (_reverse) {
    _iterator->SeekForPrev(val.string());
  } else {
    _iterator->Seek(val.string());
  }
}

void RocksDBAllIndexIterator::reset() {
  TRI_ASSERT(_trx->state()->isRunning());

  if (_reverse) {
    _iterator->SeekForPrev(_bounds.end());
  } else {
    _iterator->Seek(_bounds.start());
  }
}

// ================ Any Iterator ================

RocksDBAnyIndexIterator::RocksDBAnyIndexIterator(
    LogicalCollection* collection, transaction::Methods* trx,
    ManagedDocumentResult* mmdr, RocksDBPrimaryIndex const* index)
    : IndexIterator(collection, trx, mmdr, index),
      _cmp(index->_cmp),
      _bounds(RocksDBKeyBounds::PrimaryIndex(index->objectId())),
      _total(0),
      _returned(0) {
  // acquire rocksdb transaction
  RocksDBTransactionState* state = rocksutils::toRocksTransactionState(trx);
  TRI_ASSERT(state != nullptr);

  rocksdb::Transaction* rtrx = state->rocksTransaction();
  auto const& options = state->readOptions();
  TRI_ASSERT(options.snapshot != nullptr);

  _iterator.reset(rtrx->GetIterator(options));
  _total = collection->numberDocuments(trx);
  uint64_t off = RandomGenerator::interval(_total - 1);
  // uint64_t goal = off;
  if (_total > 0) {
    if (off <= _total / 2) {
      _iterator->Seek(_bounds.start());
      while (_iterator->Valid() && off-- > 0) {
        _iterator->Next();
      }
    } else {
      off = _total - (off + 1);
      _iterator->SeekForPrev(_bounds.end());
      while (_iterator->Valid() && off-- > 0) {
        _iterator->Prev();
      }
    }
    if (!_iterator->Valid()) {
      _iterator->Seek(_bounds.start());
    }
  }
}

bool RocksDBAnyIndexIterator::next(TokenCallback const& cb, size_t limit) {
  TRI_ASSERT(_trx->state()->isRunning());

  if (limit == 0 || !_iterator->Valid()) {
    // No limit no data, or we are actually done. The last call should have
    // returned false
    TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
    return false;
  }

  while (limit > 0) {
    RocksDBToken token(RocksDBValue::revisionId(_iterator->value()));
    cb(token);

    --limit;
    _returned++;
    _iterator->Next();
    if (!_iterator->Valid()) {
      if (_returned < _total) {
        _iterator->Seek(_bounds.start());
        continue;
      }
      return false;
    }
  }
  return true;
}

void RocksDBAnyIndexIterator::reset() { _iterator->Seek(_bounds.start()); }

// ================ PrimaryIndex ================

RocksDBPrimaryIndex::RocksDBPrimaryIndex(
    arangodb::LogicalCollection* collection, VPackSlice const& info)
    : RocksDBIndex(0, collection,
                   std::vector<std::vector<arangodb::basics::AttributeName>>(
                       {{arangodb::basics::AttributeName(
                           StaticStrings::KeyString, false)}}),
                   true, false,
                   basics::VelocyPackHelper::stringUInt64(info, "objectId"),
                   !ServerState::instance()->isCoordinator() /*useCache*/) {
  TRI_ASSERT(_objectId != 0);
}

RocksDBPrimaryIndex::~RocksDBPrimaryIndex() {}

/// @brief return the number of documents from the index
size_t RocksDBPrimaryIndex::size() const {
  // TODO
  return 0;
}

/// @brief return the memory usage of the index
size_t RocksDBPrimaryIndex::memory() const {
  rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
  RocksDBKeyBounds bounds = RocksDBKeyBounds::PrimaryIndex(_objectId);
  rocksdb::Range r(bounds.start(), bounds.end());
  uint64_t out;
  db->GetApproximateSizes(&r, 1, &out, true);
  return (size_t)out;
}

int RocksDBPrimaryIndex::cleanup() {
  rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
  rocksdb::CompactRangeOptions opts;
  RocksDBKeyBounds bounds = RocksDBKeyBounds::PrimaryIndex(_objectId);
  rocksdb::Slice b = bounds.start(), e = bounds.end();
  db->CompactRange(opts, &b, &e);
  return TRI_ERROR_NO_ERROR;
}

/// @brief return a VelocyPack representation of the index
void RocksDBPrimaryIndex::toVelocyPack(VPackBuilder& builder, bool withFigures,
                                       bool forPersistence) const {
  builder.openObject();
  RocksDBIndex::toVelocyPack(builder, withFigures, forPersistence);
  // hard-coded
  builder.add("unique", VPackValue(true));
  builder.add("sparse", VPackValue(false));
  builder.close();
}

/// @brief return a VelocyPack representation of the index figures
void RocksDBPrimaryIndex::toVelocyPackFigures(VPackBuilder& builder) const {
  Index::toVelocyPackFigures(builder);
  // TODO: implement
}

RocksDBToken RocksDBPrimaryIndex::lookupKey(transaction::Methods* trx,
                                            arangodb::StringRef keyRef) const {
  auto key = RocksDBKey::PrimaryIndexValue(_objectId, keyRef);
  auto value = RocksDBValue::Empty(RocksDBEntryType::PrimaryIndexValue);

  if (useCache()) {
    TRI_ASSERT(_cache != nullptr);
    // check cache first for fast path
    auto f = _cache->find(key.string().data(),
                          static_cast<uint32_t>(key.string().size()));
    if (f.found()) {
      value.buffer()->append(reinterpret_cast<char const*>(f.value()->value()),
                             static_cast<size_t>(f.value()->valueSize));
      return RocksDBToken(RocksDBValue::revisionId(value));
    }
  }

  // acquire rocksdb transaction
  RocksDBTransactionState* state = rocksutils::toRocksTransactionState(trx);
  rocksdb::Transaction* rtrx = state->rocksTransaction();
  auto& options = state->readOptions();
  TRI_ASSERT(options.snapshot != nullptr);

  auto status = rtrx->Get(options, key.string(), value.buffer());
  if (!status.ok()) {
    return RocksDBToken();
  }

  if (useCache()) {
    TRI_ASSERT(_cache != nullptr);
    // write entry back to cache
    auto entry = cache::CachedValue::construct(
        key.string().data(), static_cast<uint32_t>(key.string().size()),
        value.buffer()->data(), static_cast<uint64_t>(value.buffer()->size()));
    bool cached = _cache->insert(entry);
    if (!cached) {
      delete entry;
    }
  }

  return RocksDBToken(RocksDBValue::revisionId(value));
}

// TODO: remove this method?
RocksDBToken RocksDBPrimaryIndex::lookupKey(
    transaction::Methods* trx, VPackSlice slice,
    ManagedDocumentResult& result) const {
  return lookupKey(trx, StringRef(slice));
}

int RocksDBPrimaryIndex::insert(transaction::Methods* trx,
                                TRI_voc_rid_t revisionId,
                                VPackSlice const& slice, bool) {
  auto key = RocksDBKey::PrimaryIndexValue(
      _objectId, StringRef(slice.get(StaticStrings::KeyString)));
  auto value = RocksDBValue::PrimaryIndexValue(revisionId);

  // acquire rocksdb transaction
  RocksDBTransactionState* state = rocksutils::toRocksTransactionState(trx);
  rocksdb::Transaction* rtrx = state->rocksTransaction();
  auto& options = state->readOptions();
  TRI_ASSERT(options.snapshot != nullptr);

  std::string empty;
  auto existing = rtrx->Get(options, key.string(), &empty);
  if (!existing.IsNotFound()) {
    return TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED;
  }

  blackListKey(key.string().data(), static_cast<uint32_t>(key.string().size()));

  auto status = rtrx->Put(key.string(), value.string());
  if (!status.ok()) {
    auto converted =
        rocksutils::convertStatus(status, rocksutils::StatusHint::index);
    return converted.errorNumber();
  }

  return TRI_ERROR_NO_ERROR;
}

int RocksDBPrimaryIndex::insertRaw(rocksdb::WriteBatchWithIndex*, TRI_voc_rid_t,
                                   VPackSlice const&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

int RocksDBPrimaryIndex::remove(transaction::Methods* trx,
                                TRI_voc_rid_t revisionId,
                                VPackSlice const& slice, bool) {
  // TODO: deal with matching revisions?
  auto key = RocksDBKey::PrimaryIndexValue(
      _objectId, StringRef(slice.get(StaticStrings::KeyString)));

  blackListKey(key.string().data(), static_cast<uint32_t>(key.string().size()));

  // acquire rocksdb transaction
  RocksDBTransactionState* state = rocksutils::toRocksTransactionState(trx);
  rocksdb::Transaction* rtrx = state->rocksTransaction();

  auto status = rtrx->Delete(key.string());
  auto converted =
      rocksutils::convertStatus(status, rocksutils::StatusHint::index);

  return converted.errorNumber();
}

/// optimization for truncateNoTrx, never called in fillIndex
int RocksDBPrimaryIndex::removeRaw(rocksdb::WriteBatchWithIndex*, TRI_voc_rid_t,
                                   VPackSlice const&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

/// @brief called when the index is dropped
int RocksDBPrimaryIndex::drop() {
  // First drop the cache all indexes can work without it.
  RocksDBIndex::drop();
  return rocksutils::removeLargeRange(rocksutils::globalRocksDB(),
                                      RocksDBKeyBounds::PrimaryIndex(_objectId))
      .errorNumber();
}

/// @brief checks whether the index supports the condition
bool RocksDBPrimaryIndex::supportsFilterCondition(
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    size_t& estimatedItems, double& estimatedCost) const {
  SimpleAttributeEqualityMatcher matcher(IndexAttributes);
  return matcher.matchOne(this, node, reference, itemsInIndex, estimatedItems,
                          estimatedCost);
}

/// @brief creates an IndexIterator for the given Condition
IndexIterator* RocksDBPrimaryIndex::iteratorForCondition(
    transaction::Methods* trx, ManagedDocumentResult* mmdr,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, bool reverse) {
  TRI_ASSERT(node->type == aql::NODE_TYPE_OPERATOR_NARY_AND);

  TRI_ASSERT(node->numMembers() == 1);

  auto comp = node->getMember(0);

  // assume a.b == value
  auto attrNode = comp->getMember(0);
  auto valNode = comp->getMember(1);

  if (attrNode->type != aql::NODE_TYPE_ATTRIBUTE_ACCESS) {
    // value == a.b  ->  flip the two sides
    attrNode = comp->getMember(1);
    valNode = comp->getMember(0);
  }
  TRI_ASSERT(attrNode->type == aql::NODE_TYPE_ATTRIBUTE_ACCESS);

  if (comp->type == aql::NODE_TYPE_OPERATOR_BINARY_EQ) {
    // a.b == value
    return createEqIterator(trx, mmdr, attrNode, valNode);
  } else if (comp->type == aql::NODE_TYPE_OPERATOR_BINARY_IN) {
    // a.b IN values
    if (!valNode->isArray()) {
      // a.b IN non-array
      return new EmptyIndexIterator(_collection, trx, mmdr, this);
    }

    return createInIterator(trx, mmdr, attrNode, valNode);
  }

  // operator type unsupported
  return new EmptyIndexIterator(_collection, trx, mmdr, this);
}

/// @brief specializes the condition for use with the index
arangodb::aql::AstNode* RocksDBPrimaryIndex::specializeCondition(
    arangodb::aql::AstNode* node,
    arangodb::aql::Variable const* reference) const {
  SimpleAttributeEqualityMatcher matcher(IndexAttributes);
  return matcher.specializeOne(this, node, reference);
}

/// @brief request an iterator over all elements in the index in
///        a sequential order.
IndexIterator* RocksDBPrimaryIndex::allIterator(transaction::Methods* trx,
                                                ManagedDocumentResult* mmdr,
                                                bool reverse) const {
  return new RocksDBAllIndexIterator(_collection, trx, mmdr, this, reverse);
}

/// @brief request an iterator over all elements in the index in
///        a sequential order.
IndexIterator* RocksDBPrimaryIndex::anyIterator(
    transaction::Methods* trx, ManagedDocumentResult* mmdr) const {
  return new RocksDBAnyIndexIterator(_collection, trx, mmdr, this);
}

void RocksDBPrimaryIndex::invokeOnAllElements(
    transaction::Methods* trx,
    std::function<bool(DocumentIdentifierToken const&)> callback) const {
  ManagedDocumentResult mmdr;
  std::unique_ptr<IndexIterator> cursor(allIterator(trx, &mmdr, false));
  bool cnt = true;
  auto cb = [&](DocumentIdentifierToken token) {
    if (cnt) {
      cnt = callback(token);
    }
  };
  while (cursor->next(cb, 1000) && cnt) {
  }
}

Result RocksDBPrimaryIndex::postprocessRemove(transaction::Methods* trx,
                                              rocksdb::Slice const& key,
                                              rocksdb::Slice const& value) {
  blackListKey(key.data(), key.size());
  return {TRI_ERROR_NO_ERROR};
}

/// @brief create the iterator, for a single attribute, IN operator
IndexIterator* RocksDBPrimaryIndex::createInIterator(
    transaction::Methods* trx, ManagedDocumentResult* mmdr,
    arangodb::aql::AstNode const* attrNode,
    arangodb::aql::AstNode const* valNode) {
  // _key or _id?
  bool const isId = (attrNode->stringEquals(StaticStrings::IdString));

  TRI_ASSERT(valNode->isArray());

  // lease builder, but immediately pass it to the unique_ptr so we don't leak
  transaction::BuilderLeaser builder(trx);
  std::unique_ptr<VPackBuilder> keys(builder.steal());
  keys->openArray();

  size_t const n = valNode->numMembers();

  // only leave the valid elements
  for (size_t i = 0; i < n; ++i) {
    handleValNode(trx, keys.get(), valNode->getMemberUnchecked(i), isId);
    TRI_IF_FAILURE("PrimaryIndex::iteratorValNodes") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
  }

  TRI_IF_FAILURE("PrimaryIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  keys->close();
  return new RocksDBPrimaryIndexIterator(_collection, trx, mmdr, this, keys);
}

/// @brief create the iterator, for a single attribute, EQ operator
IndexIterator* RocksDBPrimaryIndex::createEqIterator(
    transaction::Methods* trx, ManagedDocumentResult* mmdr,
    arangodb::aql::AstNode const* attrNode,
    arangodb::aql::AstNode const* valNode) {
  // _key or _id?
  bool const isId = (attrNode->stringEquals(StaticStrings::IdString));

  // lease builder, but immediately pass it to the unique_ptr so we don't leak
  transaction::BuilderLeaser builder(trx);
  std::unique_ptr<VPackBuilder> keys(builder.steal());
  keys->openArray();

  // handle the sole element
  handleValNode(trx, keys.get(), valNode, isId);

  TRI_IF_FAILURE("PrimaryIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  keys->close();
  return new RocksDBPrimaryIndexIterator(_collection, trx, mmdr, this, keys);
}

/// @brief add a single value node to the iterator's keys
void RocksDBPrimaryIndex::handleValNode(transaction::Methods* trx,
                                        VPackBuilder* keys,
                                        arangodb::aql::AstNode const* valNode,
                                        bool isId) const {
  if (!valNode->isStringValue() || valNode->getStringLength() == 0) {
    return;
  }

  if (isId) {
    // lookup by _id. now validate if the lookup is performed for the
    // correct collection (i.e. _collection)
    TRI_voc_cid_t cid;
    char const* key;
    size_t outLength;
    Result res =
        trx->resolveId(valNode->getStringValue(),

                       valNode->getStringLength(), cid, key, outLength);

    if (!res.ok()) {
      return;
    }

    TRI_ASSERT(cid != 0);
    TRI_ASSERT(key != nullptr);

    if (!trx->state()->isRunningInCluster() && cid != _collection->cid()) {
      // only continue lookup if the id value is syntactically correct and
      // refers to "our" collection, using local collection id
      return;
    }

    if (trx->state()->isRunningInCluster() && cid != _collection->planId()) {
      // only continue lookup if the id value is syntactically correct and
      // refers to "our" collection, using cluster collection id
      return;
    }

    // use _key value from _id
    keys->add(VPackValuePair(key, outLength, VPackValueType::String));
  } else {
    keys->add(VPackValuePair(valNode->getStringValue(),
                             valNode->getStringLength(),
                             VPackValueType::String));
  }
}
