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
#include "Indexes/IndexResult.h"
#include "Indexes/SimpleAttributeEqualityMatcher.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBComparator.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBToken.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "RocksDBEngine/RocksDBValue.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

#include "RocksDBEngine/RocksDBPrefixExtractor.h"

#include <rocksdb/iterator.h>
#include <rocksdb/utilities/transaction.h>

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

namespace {
constexpr bool PrimaryIndexFillBlockCache = false;
}

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

  while (limit > 0) {
    // TODO: prevent copying of the value into result, as we don't need it here!
    RocksDBToken token = _index->lookupKey(_trx, StringRef(*_iterator));
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

// ================ PrimaryIndex ================

RocksDBPrimaryIndex::RocksDBPrimaryIndex(
    arangodb::LogicalCollection* collection, VPackSlice const& info)
    : RocksDBIndex(0, collection,
                   std::vector<std::vector<arangodb::basics::AttributeName>>(
                       {{arangodb::basics::AttributeName(
                           StaticStrings::KeyString, false)}}),
                   true, false, RocksDBColumnFamily::primary(),
                   basics::VelocyPackHelper::stringUInt64(info, "objectId"),
                   static_cast<RocksDBCollection*>(collection->getPhysical())->cacheEnabled()) {
  TRI_ASSERT(_cf == RocksDBColumnFamily::primary());
  TRI_ASSERT(_objectId != 0);
}

RocksDBPrimaryIndex::~RocksDBPrimaryIndex() {}

void RocksDBPrimaryIndex::load() {
  RocksDBIndex::load();
  if (useCache()) {
    // FIXME: make the factor configurable
    RocksDBCollection* rdb = static_cast<RocksDBCollection*>(_collection->getPhysical());
    _cache->sizeHint(0.3 * rdb->numberDocuments());
  }
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

RocksDBToken RocksDBPrimaryIndex::lookupKey(transaction::Methods* trx,
                                            arangodb::StringRef keyRef) const {
  RocksDBKeyLeaser key(trx);
  key->constructPrimaryIndexValue(_objectId, keyRef);
  auto value = RocksDBValue::Empty(RocksDBEntryType::PrimaryIndexValue);

  bool lockTimeout = false;
  if (useCache()) {
    TRI_ASSERT(_cache != nullptr);
    // check cache first for fast path
    auto f = _cache->find(key->string().data(),
                          static_cast<uint32_t>(key->string().size()));
    if (f.found()) {
      rocksdb::Slice s(reinterpret_cast<char const*>(f.value()->value()),
                       f.value()->valueSize());
      return RocksDBToken(RocksDBValue::revisionId(s));
    } else if (f.result().errorNumber() == TRI_ERROR_LOCK_TIMEOUT) {
      // assuming someone is currently holding a write lock, which
      // is why we cannot access the TransactionalBucket.
      lockTimeout = true; // we skip the insert in this case
    }
  }

  // acquire rocksdb transaction
  RocksDBMethods* mthds = RocksDBTransactionState::toMethods(trx);
  auto options = mthds->readOptions();  // intentional copy
  options.fill_cache = PrimaryIndexFillBlockCache;
  TRI_ASSERT(options.snapshot != nullptr);

  arangodb::Result r = mthds->Get(_cf, key.ref(), value.buffer());
  if (!r.ok()) {
    return RocksDBToken();
  }

  if (useCache() && !lockTimeout) {
    TRI_ASSERT(_cache != nullptr);

    // write entry back to cache
    auto entry = cache::CachedValue::construct(
        key->string().data(), static_cast<uint32_t>(key->string().size()),
        value.buffer()->data(), static_cast<uint64_t>(value.buffer()->size()));
    if (entry) {
      Result status = _cache->insert(entry);
      if (status.errorNumber() == TRI_ERROR_LOCK_TIMEOUT) {
        //the writeLock uses cpu_relax internally, so we can try yield
        std::this_thread::yield();
        status = _cache->insert(entry);
      }
      if (status.fail()) {
        delete entry;
      }
    }
  }

  return RocksDBToken(RocksDBValue::revisionId(value));
}

Result RocksDBPrimaryIndex::insertInternal(transaction::Methods* trx,
                                           RocksDBMethods* mthd,
                                           TRI_voc_rid_t revisionId,
                                           VPackSlice const& slice) {
  VPackSlice keySlice = transaction::helpers::extractKeyFromDocument(slice);
  RocksDBKeyLeaser key(trx);
  key->constructPrimaryIndexValue(_objectId, StringRef(keySlice));
  auto value = RocksDBValue::PrimaryIndexValue(revisionId);

  // acquire rocksdb transaction
  if (mthd->Exists(_cf, key.ref())) {
    return TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED;
  }

  blackListKey(key->string().data(), static_cast<uint32_t>(key->string().size()));

  Result status = mthd->Put(_cf, key.ref(), value.string(), rocksutils::index);
  return IndexResult(status.errorNumber(), this);
}

Result RocksDBPrimaryIndex::updateInternal(transaction::Methods* trx,
                                           RocksDBMethods* mthd,
                      TRI_voc_rid_t oldRevision,
                      arangodb::velocypack::Slice const& oldDoc,
                      TRI_voc_rid_t newRevision,
                      velocypack::Slice const& newDoc) {
  VPackSlice keySlice = transaction::helpers::extractKeyFromDocument(oldDoc);
  TRI_ASSERT(keySlice == oldDoc.get(StaticStrings::KeyString));
  RocksDBKeyLeaser key(trx);
  key->constructPrimaryIndexValue(_objectId, StringRef(keySlice));
  auto value = RocksDBValue::PrimaryIndexValue(newRevision);

  TRI_ASSERT(mthd->Exists(_cf, key.ref()));
  blackListKey(key->string().data(),
              static_cast<uint32_t>(key->string().size()));
  Result status = mthd->Put(_cf, key.ref(), value.string(), rocksutils::index);

  return IndexResult(status.errorNumber(), this);
}

Result RocksDBPrimaryIndex::removeInternal(transaction::Methods* trx,
                                           RocksDBMethods* mthd,
                                           TRI_voc_rid_t revisionId,
                                           VPackSlice const& slice) {
  // TODO: deal with matching revisions?
  RocksDBKeyLeaser key(trx);
  key->constructPrimaryIndexValue(
      _objectId, StringRef(slice.get(StaticStrings::KeyString)));

  blackListKey(key->string().data(), static_cast<uint32_t>(key->string().size()));

  // acquire rocksdb transaction
  RocksDBMethods* mthds = RocksDBTransactionState::toMethods(trx);
  Result r = mthds->Delete(_cf, key.ref());
  // rocksutils::convertStatus(status, rocksutils::StatusHint::index);
  return IndexResult(r.errorNumber(), this);
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
