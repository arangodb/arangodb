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
#include "Basics/VelocyPackHelper.h"
#include "Cache/CachedValue.h"
#include "Cache/TransactionalCache.h"
#include "Indexes/SimpleAttributeEqualityMatcher.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBToken.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "RocksDBEngine/RocksDBTypes.h"

#include <rocksdb/db.h>
#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/utilities/write_batch_with_index.h>

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;

RocksDBEdgeIndexIterator::RocksDBEdgeIndexIterator(
    LogicalCollection* collection, transaction::Methods* trx,
    ManagedDocumentResult* mmdr, arangodb::RocksDBEdgeIndex const* index,
    std::unique_ptr<VPackBuilder>& keys,
    bool useCache, cache::Cache* cache)
    : IndexIterator(collection, trx, mmdr, index),
      _keys(keys.get()),
      _keysIterator(_keys->slice()),
      _index(index),
      _arrayIterator(VPackSlice::emptyArraySlice()),
      _bounds(RocksDBKeyBounds::EdgeIndex(0)),
      _doUpdateBounds(true),
      _useCache(useCache),
      _cache(cache),
      _cacheValueSize(0) {
  keys.release();  // now we have ownership for _keys
  TRI_ASSERT(_keys->slice().isArray());
  RocksDBTransactionState* state = rocksutils::toRocksTransactionState(_trx);
  TRI_ASSERT(state != nullptr);
  rocksdb::Transaction* rtrx = state->rocksTransaction();
  _iterator.reset(rtrx->GetIterator(state->readOptions()));
}

void RocksDBEdgeIndexIterator::updateBounds(StringRef fromTo) {
    _bounds = RocksDBKeyBounds::EdgeIndexVertex(_index->_objectId, fromTo);
    _iterator->Seek(_bounds.start());
}

RocksDBEdgeIndexIterator::~RocksDBEdgeIndexIterator() {
  if (_keys != nullptr) {
    // return the VPackBuilder to the transaction context
    _trx->transactionContextPtr()->returnBuilder(_keys.release());
  }
}

StringRef getFromToFromIterator(arangodb::velocypack::ArrayIterator const& it){
    VPackSlice fromTo = it.value();
    if (fromTo.isObject()) {
      fromTo = fromTo.get(StaticStrings::IndexEq);
    }
    TRI_ASSERT(fromTo.isString());
    return StringRef(fromTo);
}

bool RocksDBEdgeIndexIterator::next(TokenCallback const& cb, size_t limit) {
  //LOG_TOPIC(ERR, Logger::FIXME) << "rockdb edge index next";
  std::size_t cacheValueSizeLimit = limit;
  TRI_ASSERT(_trx->state()->isRunning());

  if (limit == 0 || !_keysIterator.valid()) {
    // No limit no data, or we are actually done. The last call should have
    // returned false
    TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
    return false;
  }

  // acquire RocksDB collection
  RocksDBToken token;

  while (_keysIterator.valid()) {
    TRI_ASSERT(limit > 0);
    StringRef fromTo = getFromToFromIterator(_keysIterator);
    bool foundInCache = false;

    if (_useCache){
      //LOG_TOPIC(ERR, Logger::FIXME) << "using cache";
      // find in cache
      foundInCache = false;
      // handle resume of next() in cached case
      if(!_arrayBuffer.empty()){
        // resume iteration after batch size limit was hit
        // do not modify buffer or iterator
        foundInCache = true;
      } else {
        // try to find cached value
        auto f = _cache->find(fromTo.data(), (uint32_t)fromTo.size());
        foundInCache = f.found();
        if (foundInCache) {
          VPackSlice cachedPrimaryKeys(f.value()->value());
          TRI_ASSERT(cachedPrimaryKeys.isArray());

          if(cachedPrimaryKeys.length() <= limit){
            // do not copy
            _arraySlice = cachedPrimaryKeys; 
          } else {
            // copy data if there are more documents than the batch size limit allows
            _arrayBuffer.append(cachedPrimaryKeys.start(),cachedPrimaryKeys.byteSize());
            _arraySlice = VPackSlice(_arrayBuffer.data());
          }
          // update iterator
          _arrayIterator = VPackArrayIterator(_arraySlice);
        } else {
          //LOG_TOPIC(ERR, Logger::FIXME) << "not found in cache " << fromTo;
        }
      }

      if (foundInCache) {
        //iterate over cached primary keys
        for(VPackSlice const& slice: _arrayIterator){
          StringRef edgeKey(slice);
          //LOG_TOPIC(ERR, Logger::FIXME) << "using value form cache " << slice.toJson();
          if(lookupDocumentAndUseCb(edgeKey, cb, limit, token)){
            return true; // more documents - function will be re-entered
          } else {
            //iterate over keys
          }
        }
        _arrayIterator.reset();
        _arrayBuffer.clear();
        _keysIterator.next(); // handle next key
        continue; // do not use the code below that does a lookup in RocksDB
      }
    } else {
      //LOG_TOPIC(ERR, Logger::FIXME) << "not using cache";
    }

    if(!foundInCache) {
      // cache lookup failed for key value we need to look up
      // primary keys in RocksDB

      // if there are more documents in the iterator then
      // we are not allowed to reset the index.
      // if _doUpdateBound is set to false we resume
      // returning from an valid iterator after hitting 
      // the batch size limit
      if(_doUpdateBounds){
        updateBounds(fromTo);
        if(_useCache){
          _cacheValueBuilder.openArray();
          _cacheValueSize = cacheValueSizeLimit;
        }
      } else {
        _doUpdateBounds = true;
      }

      while (_iterator->Valid() && (_index->_cmp->Compare(_iterator->key(), _bounds.end()) < 0)) {
        StringRef edgeKey = RocksDBKey::primaryKey(_iterator->key());
        if(_useCache){
          if (_cacheValueSize < cacheValueSizeLimit){
            _cacheValueBuilder.add(VPackValuePair(edgeKey.data(),edgeKey.size()));
            ++_cacheValueSize;
          } else {
            _cacheValueBuilder.clear();
            _cacheValueBuilder.openArray();
          }
        }

        if(lookupDocumentAndUseCb(edgeKey, cb, limit, token)){
          return true; // more documents - function will be re-entered
        } else {
          // batch size limit not reached continue loop
        }
      }

      //only add entry if cache is available and entry did not hit size limit
      if (_useCache) {
        _cacheValueBuilder.close();
        if(!_cacheValueBuilder.isEmpty() && _cacheValueBuilder.slice().length() > 0) {
          auto entry = cache::CachedValue::construct(
              fromTo.data(), static_cast<uint32_t>(fromTo.size()),
              _cacheValueBuilder.slice().start(), static_cast<uint64_t>(_cacheValueBuilder.slice().byteSize()));
          bool cached = _cache->insert(entry);
          if (!cached) {
            delete entry;
          }
        } // builder not empty
      } // use cache

    } // not found in cache

    _keysIterator.next(); // handle next key
  }
  return false; // no more documents in this iterator
}

// acquire the document token through the primary index
bool RocksDBEdgeIndexIterator::lookupDocumentAndUseCb(
    StringRef primaryKey, TokenCallback const& cb,
    size_t& limit, RocksDBToken& token){
  //we pass the token in as ref to avoid allocations
  auto rocksColl = toRocksDBCollection(_collection);
  Result res = rocksColl->lookupDocumentToken(_trx, primaryKey, token);
  _iterator->Next();
  if (res.ok()) {
    cb(token);
    --limit;
    if (limit == 0) {
      _doUpdateBounds=false; //limit hit continue with next batch
      return true;
    }
  }  // TODO do we need to handle failed lookups here?
  return false; // limit not hit continue in while loop
}

void RocksDBEdgeIndexIterator::reset() {
  //rest offsets into iterators
  _doUpdateBounds = true;
  _cacheValueBuilder.clear();
  _arrayBuffer.clear();
  _arraySlice = VPackSlice::emptyArraySlice();
  _arrayIterator = VPackArrayIterator(_arraySlice);
  _keysIterator.reset();
}

// ============================= Index ====================================

RocksDBEdgeIndex::RocksDBEdgeIndex(TRI_idx_iid_t iid,
                                   arangodb::LogicalCollection* collection,
                                   VPackSlice const& info,
                                   std::string const& attr)
    : RocksDBIndex(iid
                  ,collection
                  ,std::vector<std::vector<AttributeName>>({{AttributeName(attr, false)}})
                  ,false
                  ,false
                  ,basics::VelocyPackHelper::stringUInt64(info, "objectId")
                  ,false //!ServerState::instance()->isCoordinator() /*useCache*/
                  )
    , _directionAttr(attr)
{
  TRI_ASSERT(iid != 0);
  TRI_ASSERT(_objectId != 0);
  if (_objectId == 0) {
    //disable cache?
    _useCache = false;
  }
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
  rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
  RocksDBKeyBounds bounds = RocksDBKeyBounds::EdgeIndex(_objectId);
  rocksdb::Range r(bounds.start(), bounds.end());
  uint64_t out;
  db->GetApproximateSizes(&r, 1, &out, true);
  return (size_t)out;
}

/// @brief return a VelocyPack representation of the index
void RocksDBEdgeIndex::toVelocyPack(VPackBuilder& builder, bool withFigures,
                                    bool forPersistence) const {
  builder.openObject();
  RocksDBIndex::toVelocyPack(builder, withFigures, forPersistence);
  // add selectivity estimate hard-coded
  builder.add("unique", VPackValue(false));
  builder.add("sparse", VPackValue(false));
  builder.close();
}

int RocksDBEdgeIndex::insert(transaction::Methods* trx,
                             TRI_voc_rid_t revisionId, VPackSlice const& doc,
                             bool isRollback) {
  VPackSlice primaryKey = doc.get(StaticStrings::KeyString);
  VPackSlice fromTo = doc.get(_directionAttr);
  TRI_ASSERT(primaryKey.isString() && fromTo.isString());
  auto fromToRef=StringRef(fromTo);
  RocksDBKey key = RocksDBKey::EdgeIndexValue(_objectId, fromToRef,
                                              StringRef(primaryKey));
  //blacklist key in cache
  blackListKey(fromToRef);

  // acquire rocksdb transaction
  RocksDBTransactionState* state = rocksutils::toRocksTransactionState(trx);
  rocksdb::Transaction* rtrx = state->rocksTransaction();

  rocksdb::Status status =
      rtrx->Put(rocksdb::Slice(key.string()), rocksdb::Slice());
  if (status.ok()) {
    return TRI_ERROR_NO_ERROR;
  } else {
    return rocksutils::convertStatus(status).errorNumber();
  }
}

int RocksDBEdgeIndex::insertRaw(rocksdb::WriteBatchWithIndex*, TRI_voc_rid_t,
                                VPackSlice const&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

int RocksDBEdgeIndex::remove(transaction::Methods* trx,
                             TRI_voc_rid_t revisionId, VPackSlice const& doc,
                             bool isRollback) {
  VPackSlice primaryKey = doc.get(StaticStrings::KeyString);
  VPackSlice fromTo = doc.get(_directionAttr);
  auto fromToRef=StringRef(fromTo);
  TRI_ASSERT(primaryKey.isString() && fromTo.isString());
  RocksDBKey key = RocksDBKey::EdgeIndexValue(_objectId, fromToRef,
                                              StringRef(primaryKey));

  //blacklist key in cache
  blackListKey(fromToRef);

  // acquire rocksdb transaction
  RocksDBTransactionState* state = rocksutils::toRocksTransactionState(trx);
  rocksdb::Transaction* rtrx = state->rocksTransaction();
  rocksdb::Status status = rtrx->Delete(rocksdb::Slice(key.string()));
  if (status.ok()) {
    return TRI_ERROR_NO_ERROR;
  } else {
    return rocksutils::convertStatus(status).errorNumber();
  }
}

/// optimization for truncateNoTrx, never called in fillIndex
int RocksDBEdgeIndex::removeRaw(rocksdb::WriteBatchWithIndex*, TRI_voc_rid_t,
                                VPackSlice const&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

void RocksDBEdgeIndex::batchInsert(
    transaction::Methods* trx,
    std::vector<std::pair<TRI_voc_rid_t, VPackSlice>> const& documents,
    std::shared_ptr<arangodb::basics::LocalTaskQueue> queue) {
  // acquire rocksdb transaction
  RocksDBTransactionState* state = rocksutils::toRocksTransactionState(trx);
  rocksdb::Transaction* rtrx = state->rocksTransaction();

  for (std::pair<TRI_voc_rid_t, VPackSlice> const& doc : documents) {
    VPackSlice primaryKey = doc.second.get(StaticStrings::KeyString);
    VPackSlice fromTo = doc.second.get(_directionAttr);
    TRI_ASSERT(primaryKey.isString() && fromTo.isString());
    auto fromToRef=StringRef(fromTo);
    RocksDBKey key = RocksDBKey::EdgeIndexValue(_objectId, fromToRef,
                                                StringRef(primaryKey));

    blackListKey(fromToRef);
    rocksdb::Status status =
        rtrx->Put(rocksdb::Slice(key.string()), rocksdb::Slice());
    if (!status.ok()) {
      Result res =
          rocksutils::convertStatus(status, rocksutils::StatusHint::index);
      queue->setStatus(res.errorNumber());
      break;
    }
  }
}

/// @brief called when the index is dropped
int RocksDBEdgeIndex::drop() {
  // First drop the cache all indexes can work without it.
  RocksDBIndex::drop();
  return rocksutils::removeLargeRange(rocksutils::globalRocksDB(),
                                      RocksDBKeyBounds::EdgeIndex(_objectId))
      .errorNumber();
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

    arangodb::aql::Variable const* reference, bool reverse) {

  //get computation node
  TRI_ASSERT(node->type == aql::NODE_TYPE_OPERATOR_NARY_AND);
  TRI_ASSERT(node->numMembers() == 1);
  auto comp = node->getMember(0);

  // assume a.b == value
  auto attrNode = comp->getMember(0);
  auto valNode = comp->getMember(1);

  // got value == a.b  -> flip sides
  if (attrNode->type != aql::NODE_TYPE_ATTRIBUTE_ACCESS) {
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
      // a.b IN non-array
      return new EmptyIndexIterator(_collection, trx, mmdr, this);
    }
    return createInIterator(trx, mmdr, attrNode, valNode);
  }

  // operator type unsupported
  return new EmptyIndexIterator(_collection, trx, mmdr, this);
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

  //LOG_TOPIC(ERR, Logger::FIXME) << "_useCache: " << _useCache
  //                              << " _cachePresent: " << _cachePresent
  //                              << " useCache():" << useCache();
  return new RocksDBEdgeIndexIterator(
      _collection, trx, mmdr, this, keys, useCache(), _cache.get());
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

  //LOG_TOPIC(ERR, Logger::FIXME) << "useCache: " << _useCache << useCache();
  return new RocksDBEdgeIndexIterator(
      _collection, trx, mmdr, this, keys, useCache(), _cache.get());
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

int RocksDBEdgeIndex::cleanup() {
  rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
  rocksdb::CompactRangeOptions opts;
  RocksDBKeyBounds bounds = RocksDBKeyBounds::EdgeIndex(_objectId);
  rocksdb::Slice b = bounds.start(), e = bounds.end();
  db->CompactRange(opts, &b, &e);
  return TRI_ERROR_NO_ERROR;
}

Result RocksDBEdgeIndex::postprocessRemove(transaction::Methods* trx,
                                              rocksdb::Slice const& key,
                                              rocksdb::Slice const& value) {
  //blacklist keys during truncate
  blackListKey(key.data(), key.size());
  return {TRI_ERROR_NO_ERROR};
}
