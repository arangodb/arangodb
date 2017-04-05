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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBCollection.h"
#include "Aql/PlanCache.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterMethods.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBComparator.h"
#include "RocksDBEngine/RocksDBCounterManager.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBPrimaryIndex.h"
#include "RocksDBEngine/RocksDBToken.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "RocksDBEngine/RocksDBValue.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Helpers.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/Events.h"
#include "Utils/OperationOptions.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"

#include <rocksdb/db.h>
#include <rocksdb/utilities/transaction.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::rocksutils;

namespace {

static std::string const Empty;

static inline rocksdb::Transaction* rocksTransaction(
    arangodb::transaction::Methods* trx) {
  return static_cast<RocksDBTransactionState*>(trx->state())
      ->rocksTransaction();
}
}

RocksDBCollection::RocksDBCollection(LogicalCollection* collection,
                                     VPackSlice const& info)
    : PhysicalCollection(collection, info),
      _objectId(basics::VelocyPackHelper::stringUInt64(info, "objectId")),
      _numberDocuments(0) {
  TRI_ASSERT(_objectId != 0);
}

RocksDBCollection::RocksDBCollection(LogicalCollection* collection,
                                     PhysicalCollection* physical)
    : PhysicalCollection(collection, VPackSlice::emptyObjectSlice()),
      _objectId(static_cast<RocksDBCollection*>(physical)->_objectId),
      _numberDocuments(0) {
  TRI_ASSERT(_objectId != 0);
}

RocksDBCollection::~RocksDBCollection() {}

std::string const& RocksDBCollection::path() const {
  return Empty;  // we do not have any path
}

void RocksDBCollection::setPath(std::string const&) {
  // we do not have any path
}

arangodb::Result RocksDBCollection::updateProperties(VPackSlice const& slice,
                                                     bool doSync) {
  // nothing to do
  return arangodb::Result{};
}

arangodb::Result RocksDBCollection::persistProperties() {
  Result res;
  try {
    VPackBuilder infoBuilder =
        _logicalCollection->toVelocyPackIgnore({"path", "statusString"}, true);

    RocksDBKey key(RocksDBKey::Collection(_logicalCollection->vocbase()->id(),
                                          _logicalCollection->cid()));
    RocksDBValue value(RocksDBValue::Document(infoBuilder.slice()));
    res = globalRocksDBPut(key.string(), value.string());

  } catch (arangodb::basics::Exception const& ex) {
    res.reset(ex.code());
  } catch (...) {
    res.reset(TRI_ERROR_INTERNAL);
  }

  if (res.fail()) {
    // TODO: what to do here
    LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
        << "could not save collection change marker in log: "
        << res.errorMessage();
  }
  return res;
}

PhysicalCollection* RocksDBCollection::clone(LogicalCollection* logical,
                                             PhysicalCollection* physical) {
  return new RocksDBCollection(logical, physical);
}

TRI_voc_rid_t RocksDBCollection::revision() const {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return 0;
}

void RocksDBCollection::getPropertiesVPack(velocypack::Builder& result) const {
  TRI_ASSERT(result.isOpenObject());
  result.add("objectId", VPackValue(std::to_string(_objectId)));
  // result.add("isVolatile", VPackValue(false)); // TODO: include?
  result.add(VPackValue("keyOptions"));
  if (_keyGenerator != nullptr) {
    result.openObject();
    _keyGenerator->toVelocyPack(result);
    result.close();
  } else {
    result.openArray();
    result.close();
  }

  TRI_ASSERT(result.isOpenObject());
}

void RocksDBCollection::getPropertiesVPackCoordinator(
    velocypack::Builder& result) const {
  getPropertiesVPack(result);
}

/// @brief closes an open collection
int RocksDBCollection::close() {
  // nothing to do
  return TRI_ERROR_NO_ERROR;
}

uint64_t RocksDBCollection::numberDocuments() const { return _numberDocuments; }

uint64_t RocksDBCollection::numberDocuments(transaction::Methods* trx) const {
  RocksDBTransactionState* state =
      static_cast<RocksDBTransactionState*>(trx->state());

  auto trxCollection = state->findCollection(_logicalCollection->cid());
  TRI_ASSERT(trxCollection != nullptr);

  return _numberDocuments + state->numInserts() - state->numRemoves();
}

/// @brief report extra memory used by indexes etc.
size_t RocksDBCollection::memory() const { return 0; }

void RocksDBCollection::open(bool ignoreErrors) {
  // set the initial number of documents
  // rocksdb::ReadOptions readOptions;
  // rocksdb::TransactionDB* db =
  // static_cast<RocksDBEngine*>(EngineSelectorFeature::ENGINE)->db();
  RocksDBEngine* engine =
      static_cast<RocksDBEngine*>(EngineSelectorFeature::ENGINE);
  _numberDocuments = engine->counterManager()->loadCounter(this->objectId());
  //_numberDocuments = countKeyRange(db, readOptions,
  // RocksDBKeyBounds::CollectionDocuments(_objectId));
}

/// @brief iterate all markers of a collection on load
int RocksDBCollection::iterateMarkersOnLoad(
    arangodb::transaction::Methods* trx) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return 0;
}

bool RocksDBCollection::isFullyCollected() const {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return false;
}

void RocksDBCollection::prepareIndexes(
    arangodb::velocypack::Slice indexesSlice) {
  createInitialIndexes();
  if (indexesSlice.isArray()) {
    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    IndexFactory const* idxFactory = engine->indexFactory();
    TRI_ASSERT(idxFactory != nullptr);
    for (auto const& v : VPackArrayIterator(indexesSlice)) {
      if (arangodb::basics::VelocyPackHelper::getBooleanValue(v, "error",
                                                              false)) {
        // We have an error here.
        // Do not add index.
        // TODO Handle Properly
        continue;
      }

      auto idx =
          idxFactory->prepareIndexFromSlice(v, false, _logicalCollection, true);

      if (idx->type() == Index::TRI_IDX_TYPE_PRIMARY_INDEX ||
          idx->type() == Index::TRI_IDX_TYPE_EDGE_INDEX) {
        continue;
      }

      if (ServerState::instance()->isRunningInCluster()) {
        addIndexCoordinator(idx);
      } else {
        addIndex(idx);
      }
    }
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  if (_indexes[0]->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX ||
      (_logicalCollection->type() == TRI_COL_TYPE_EDGE &&
       (_indexes[1]->type() != Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX ||
        _indexes[2]->type() != Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX))) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "got invalid indexes for collection '" << _logicalCollection->name()
        << "'";
    for (auto const& it : _indexes) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "- " << it.get();
    }
  }
#endif
}

/// @brief Find index by definition
std::shared_ptr<Index> RocksDBCollection::lookupIndex(
    velocypack::Slice const& info) const {
  TRI_ASSERT(info.isObject());

  // extract type
  VPackSlice value = info.get("type");

  if (!value.isString()) {
    // Compatibility with old v8-vocindex.
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  std::string tmp = value.copyString();
  arangodb::Index::IndexType const type = arangodb::Index::type(tmp.c_str());

  for (auto const& idx : _indexes) {
    if (idx->type() == type) {
      // Only check relevant indices
      if (idx->matchesDefinition(info)) {
        // We found an index for this definition.
        return idx;
      }
    }
  }
  return nullptr;
}

std::shared_ptr<Index> RocksDBCollection::createIndex(
    transaction::Methods* trx, arangodb::velocypack::Slice const& info,
    bool& created) {
  // TODO Get LOCK for the vocbase
  auto idx = lookupIndex(info);
  if (idx != nullptr) {
    created = false;
    // We already have this index.
    return idx;
  }

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  IndexFactory const* idxFactory = engine->indexFactory();
  TRI_ASSERT(idxFactory != nullptr);

  // We are sure that we do not have an index of this type.
  // We also hold the lock.
  // Create it

  idx =
      idxFactory->prepareIndexFromSlice(info, true, _logicalCollection, false);
  TRI_ASSERT(idx != nullptr);
  if (ServerState::instance()->isCoordinator()) {
    // In the coordinator case we do not fill the index
    // We only inform the others.
    addIndexCoordinator(idx);
    created = true;
    return idx;
  }

  int res = saveIndex(trx, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  arangodb::aql::PlanCache::instance()->invalidate(
      _logicalCollection->vocbase());
  // Until here no harm is done if sth fails. The shared ptr will clean up. if
  // left before

  addIndex(idx);
  {
    bool const doSync =
        application_features::ApplicationServer::getFeature<DatabaseFeature>(
            "Database")
            ->forceSyncProperties();
    VPackBuilder builder =
        _logicalCollection->toVelocyPackIgnore({"path", "statusString"}, true);
    _logicalCollection->updateProperties(builder.slice(), doSync);
  }
  created = true;
  return idx;
}

/// @brief Restores an index from VelocyPack.
int RocksDBCollection::restoreIndex(transaction::Methods*,
                                    velocypack::Slice const&,
                                    std::shared_ptr<Index>&) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return 0;
}

/// @brief Drop an index with the given iid.
bool RocksDBCollection::dropIndex(TRI_idx_iid_t iid) {
  if (iid == 0) {
    // invalid index id or primary index
    return true;
  }

  size_t i = 0;
  // TODO: need to protect _indexes with an RW-lock!!
  for (auto index : getIndexes()) {
    RocksDBIndex* cindex = static_cast<RocksDBIndex*>(index.get());

    if (iid == cindex->id()) {
      int rv = cindex->drop();

      if (rv == TRI_ERROR_NO_ERROR) {
        _indexes.erase(_indexes.begin() + i);
        events::DropIndex("", std::to_string(iid), TRI_ERROR_NO_ERROR);
        return true;
      }

      break;
    }
    ++i;
  }

  // We tried to remove an index that does not exist
  events::DropIndex("", std::to_string(iid), TRI_ERROR_ARANGO_INDEX_NOT_FOUND);
  return false;
}

std::unique_ptr<IndexIterator> RocksDBCollection::getAllIterator(
    transaction::Methods* trx, ManagedDocumentResult* mdr, bool reverse) {
  return std::unique_ptr<IndexIterator>(
      primaryIndex()->allIterator(trx, mdr, reverse));
}

std::unique_ptr<IndexIterator> RocksDBCollection::getAnyIterator(
    transaction::Methods* trx, ManagedDocumentResult* mdr) {
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_NOT_IMPLEMENTED,
      "this engine does not provide an any iterator");
}

void RocksDBCollection::invokeOnAllElements(
    std::function<bool(DocumentIdentifierToken const&)> callback) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
}

////////////////////////////////////
// -- SECTION DML Operations --
///////////////////////////////////

void RocksDBCollection::truncate(transaction::Methods* trx,
                                 OperationOptions& options) {
  rocksdb::Comparator const* cmp = globalRocksEngine()->cmp();

  RocksDBTransactionState* state = rocksutils::toRocksTransactionState(trx);
  rocksdb::Transaction* rtrx = state->rocksTransaction();
  RocksDBKeyBounds bounds =
      RocksDBKeyBounds::CollectionDocuments(this->objectId());

  std::unique_ptr<rocksdb::Iterator> iter(
      rtrx->GetIterator(state->readOptions()));
  iter->Seek(bounds.start());

  TRI_voc_cid_t cid = _logicalCollection->cid();

  while (iter->Valid() && cmp->Compare(iter->key(), bounds.end()) < 0) {
    rocksdb::Status s = rtrx->Delete(iter->key());
    if (!s.ok()) {
      trx->abort();
      break;
    }
    // TODO
    state->addOperation(cid, TRI_VOC_DOCUMENT_OPERATION_REMOVE, 0);
    iter->Next();
  }

  // TODO maybe we could also reuse Index::drop, if we ensure the
  // implementations
  // don't do anything beyond deleting their contents
  for (std::shared_ptr<Index> const& index : _indexes) {
    RocksDBIndex* rindex = static_cast<RocksDBIndex*>(index.get());
    switch (rindex->type()) {
      case RocksDBIndex::TRI_IDX_TYPE_PRIMARY_INDEX:
        bounds = RocksDBKeyBounds::PrimaryIndex(rindex->objectId());
        break;
      case RocksDBIndex::TRI_IDX_TYPE_EDGE_INDEX:
        bounds = RocksDBKeyBounds::EdgeIndex(rindex->objectId());
        break;

      case RocksDBIndex::TRI_IDX_TYPE_HASH_INDEX:
      case RocksDBIndex::TRI_IDX_TYPE_SKIPLIST_INDEX:
      case RocksDBIndex::TRI_IDX_TYPE_PERSISTENT_INDEX:
        if (rindex->unique()) {
          bounds = RocksDBKeyBounds::UniqueIndex(rindex->objectId());
        } else {
          bounds = RocksDBKeyBounds::IndexEntries(rindex->objectId());
        }
        break;
      // TODO add options for geoindex, fulltext etc

      default:
        THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
    }

    iter->Seek(bounds.start());
    while (iter->Valid() && -1 == cmp->Compare(iter->key(), bounds.end())) {
      rocksdb::Status s = rtrx->Delete(iter->key());
      if (!s.ok()) {
        trx->abort();
        break;
      }
      iter->Next();
    }
  }
}

int RocksDBCollection::read(transaction::Methods* trx,
                            arangodb::velocypack::Slice const key,
                            ManagedDocumentResult& result, bool) {
  TRI_ASSERT(key.isString());
  RocksDBToken token = primaryIndex()->lookupKey(trx, StringRef(key));
  /*
    LOG_TOPIC(ERR, Logger::FIXME)
        << "READ IN COLLECTION '" << _logicalCollection->name()
        << "', KEY: " << key.copyString()
        << ", FOUND REVISION ID: " << token.revisionId();
  */
  if (token.revisionId()) {
    if (readDocument(trx, token, result)) {
      // found
      return TRI_ERROR_NO_ERROR;
    }
  }
  // not found
  return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
}

bool RocksDBCollection::readDocument(transaction::Methods* trx,
                                     DocumentIdentifierToken const& token,
                                     ManagedDocumentResult& result) {
  // TODO: why do we have read(), readDocument() and lookupKey()?
  auto tkn = static_cast<RocksDBToken const*>(&token);
  TRI_voc_rid_t revisionId = tkn->revisionId();
  lookupRevisionVPack(revisionId, trx, result);
  return !result.empty();
}

bool RocksDBCollection::readDocumentConditional(
    transaction::Methods* trx, DocumentIdentifierToken const& token,
    TRI_voc_tick_t maxTick, ManagedDocumentResult& result) {
  // should not be called for RocksDB engine. TODO: move this out of general
  // API!
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return false;
}

int RocksDBCollection::insert(arangodb::transaction::Methods* trx,
                              arangodb::velocypack::Slice const slice,
                              arangodb::ManagedDocumentResult& mdr,
                              OperationOptions& options,
                              TRI_voc_tick_t& resultMarkerTick, bool /*lock*/) {
  // store the tick that was used for writing the document
  // note that we don't need it for this engine
  resultMarkerTick = 0;

  VPackSlice fromSlice;
  VPackSlice toSlice;

  bool const isEdgeCollection =
      (_logicalCollection->type() == TRI_COL_TYPE_EDGE);

  if (isEdgeCollection) {
    // _from:
    fromSlice = slice.get(StaticStrings::FromString);
    if (!fromSlice.isString()) {
      return TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE;
    }
    VPackValueLength len;
    char const* docId = fromSlice.getString(len);
    size_t split;
    if (!TRI_ValidateDocumentIdKeyGenerator(docId, static_cast<size_t>(len),
                                            &split)) {
      return TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE;
    }
    // _to:
    toSlice = slice.get(StaticStrings::ToString);
    if (!toSlice.isString()) {
      return TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE;
    }
    docId = toSlice.getString(len);
    if (!TRI_ValidateDocumentIdKeyGenerator(docId, static_cast<size_t>(len),
                                            &split)) {
      return TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE;
    }
  }

  transaction::BuilderLeaser builder(trx);
  int res = newObjectForInsert(trx, slice, fromSlice, toSlice, isEdgeCollection,
                               *builder.get(), options.isRestore);
  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
  VPackSlice newSlice = builder->slice();

  TRI_voc_rid_t revisionId =
      transaction::helpers::extractRevFromDocument(newSlice);

  RocksDBSavePoint guard(rocksTransaction(trx));

  res = insertDocument(trx, revisionId, newSlice, options.waitForSync);

  if (res == TRI_ERROR_NO_ERROR) {
    Result result = lookupRevisionVPack(revisionId, trx, mdr);

    if (!result.ok()) {
      return result.errorNumber();
    }

    static_cast<RocksDBTransactionState*>(trx->state())
        ->addOperation(_logicalCollection->cid(),
                       TRI_VOC_DOCUMENT_OPERATION_INSERT, newSlice.byteSize());
    guard.commit();
  }

  return res;
}

int RocksDBCollection::update(arangodb::transaction::Methods* trx,
                              arangodb::velocypack::Slice const newSlice,
                              arangodb::ManagedDocumentResult& mdr,
                              OperationOptions& options,
                              TRI_voc_tick_t& resultMarkerTick, bool /*lock*/,
                              TRI_voc_rid_t& prevRev,
                              ManagedDocumentResult& previous,
                              TRI_voc_rid_t const& revisionId,
                              arangodb::velocypack::Slice const key) {
  resultMarkerTick = 0;

  bool const isEdgeCollection =
      (_logicalCollection->type() == TRI_COL_TYPE_EDGE);
  int res = lookupDocument(trx, key, previous);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  TRI_ASSERT(!previous.empty());

  VPackSlice oldDoc(previous.vpack());
  TRI_voc_rid_t oldRevisionId =
      transaction::helpers::extractRevFromDocument(oldDoc);
  prevRev = oldRevisionId;

  // Check old revision:
  if (!options.ignoreRevs) {
    TRI_voc_rid_t expectedRev = 0;
    if (newSlice.isObject()) {
      expectedRev = TRI_ExtractRevisionId(newSlice);
    }
    int res = checkRevision(trx, expectedRev, prevRev);
    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  if (newSlice.length() <= 1) {
    // shortcut. no need to do anything
    previous.clone(mdr);

    TRI_ASSERT(!mdr.empty());

    if (_logicalCollection->waitForSync()) {
      trx->state()->waitForSync(true);
      options.waitForSync = true;
    }
    return TRI_ERROR_NO_ERROR;
  }

  // merge old and new values
  transaction::BuilderLeaser builder(trx);
  mergeObjectsForUpdate(trx, oldDoc, newSlice, isEdgeCollection,
                        TRI_RidToString(revisionId), options.mergeObjects,
                        options.keepNull, *builder.get());

  if (trx->state()->isDBServer()) {
    // Need to check that no sharding keys have changed:
    if (arangodb::shardKeysChanged(_logicalCollection->dbName(),
                                   trx->resolver()->getCollectionNameCluster(
                                       _logicalCollection->planId()),
                                   oldDoc, builder->slice(), false)) {
      return TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES;
    }
  }

  RocksDBSavePoint guard(rocksTransaction(trx));

  VPackSlice const newDoc(builder->slice());

  res = updateDocument(trx, oldRevisionId, oldDoc, revisionId, newDoc,
                       options.waitForSync);

  if (res == TRI_ERROR_NO_ERROR) {
    Result result = lookupRevisionVPack(revisionId, trx, mdr);

    if (!result.ok()) {
      return result.errorNumber();
    }

    TRI_ASSERT(!mdr.empty());

    static_cast<RocksDBTransactionState*>(trx->state())
        ->addOperation(_logicalCollection->cid(),
                       TRI_VOC_DOCUMENT_OPERATION_UPDATE, newDoc.byteSize());
    guard.commit();
  }

  return res;
}

int RocksDBCollection::replace(
    transaction::Methods* trx, arangodb::velocypack::Slice const newSlice,
    ManagedDocumentResult& mdr, OperationOptions& options,
    TRI_voc_tick_t& resultMarkerTick, bool /*lock*/, TRI_voc_rid_t& prevRev,
    ManagedDocumentResult& previous, TRI_voc_rid_t const revisionId,
    arangodb::velocypack::Slice const fromSlice,
    arangodb::velocypack::Slice const toSlice) {
  resultMarkerTick = 0;

  bool const isEdgeCollection =
      (_logicalCollection->type() == TRI_COL_TYPE_EDGE);

  // get the previous revision
  VPackSlice key = newSlice.get(StaticStrings::KeyString);
  if (key.isNone()) {
    return TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
  }

  // get the previous revision
  int res = lookupDocument(trx, key, previous);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  TRI_ASSERT(!previous.empty());

  VPackSlice oldDoc(previous.vpack());
  TRI_voc_rid_t oldRevisionId =
      transaction::helpers::extractRevFromDocument(oldDoc);
  prevRev = oldRevisionId;

  // Check old revision:
  if (!options.ignoreRevs) {
    TRI_voc_rid_t expectedRev = 0;
    if (newSlice.isObject()) {
      expectedRev = TRI_ExtractRevisionId(newSlice);
    }
    int res = checkRevision(trx, expectedRev, prevRev);
    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  // merge old and new values
  transaction::BuilderLeaser builder(trx);
  newObjectForReplace(trx, oldDoc, newSlice, fromSlice, toSlice,
                      isEdgeCollection, TRI_RidToString(revisionId),
                      *builder.get());

  if (trx->state()->isDBServer()) {
    // Need to check that no sharding keys have changed:
    if (arangodb::shardKeysChanged(_logicalCollection->dbName(),
                                   trx->resolver()->getCollectionNameCluster(
                                       _logicalCollection->planId()),
                                   oldDoc, builder->slice(), false)) {
      return TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES;
    }
  }

  RocksDBSavePoint guard(rocksTransaction(trx));

  res = updateDocument(trx, oldRevisionId, oldDoc, revisionId,
                       VPackSlice(builder->slice()), options.waitForSync);

  if (res == TRI_ERROR_NO_ERROR) {
    Result result = lookupRevisionVPack(revisionId, trx, mdr);

    if (!result.ok()) {
      return result.errorNumber();
    }

    TRI_ASSERT(!mdr.empty());

    static_cast<RocksDBTransactionState*>(trx->state())
        ->addOperation(_logicalCollection->cid(),
                       TRI_VOC_DOCUMENT_OPERATION_REPLACE,
                       VPackSlice(builder->slice()).byteSize());
    guard.commit();
  }

  return res;
}

int RocksDBCollection::remove(arangodb::transaction::Methods* trx,
                              arangodb::velocypack::Slice const slice,
                              arangodb::ManagedDocumentResult& previous,
                              OperationOptions& options,
                              TRI_voc_tick_t& resultMarkerTick, bool /*lock*/,
                              TRI_voc_rid_t const& revisionId,
                              TRI_voc_rid_t& prevRev) {
  // store the tick that was used for writing the document
  // note that we don't need it for this engine
  resultMarkerTick = 0;
  prevRev = 0;

  transaction::BuilderLeaser builder(trx);
  newObjectForRemove(trx, slice, TRI_RidToString(revisionId), *builder.get());

  VPackSlice key;
  if (slice.isString()) {
    key = slice;
  } else {
    key = slice.get(StaticStrings::KeyString);
  }
  TRI_ASSERT(!key.isNone());

  // get the previous revision
  int res = lookupDocument(trx, key, previous);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  TRI_ASSERT(!previous.empty());

  VPackSlice oldDoc(previous.vpack());
  TRI_voc_rid_t oldRevisionId =
      arangodb::transaction::helpers::extractRevFromDocument(oldDoc);
  prevRev = oldRevisionId;

  // Check old revision:
  if (!options.ignoreRevs && slice.isObject()) {
    TRI_voc_rid_t expectedRevisionId = TRI_ExtractRevisionId(slice);
    int res = checkRevision(trx, expectedRevisionId, oldRevisionId);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  RocksDBSavePoint guard(rocksTransaction(trx));

  res = removeDocument(trx, oldRevisionId, oldDoc, options.waitForSync);

  if (res == TRI_ERROR_NO_ERROR) {
    static_cast<RocksDBTransactionState*>(trx->state())
        ->addOperation(_logicalCollection->cid(),
                       TRI_VOC_DOCUMENT_OPERATION_REMOVE, oldDoc.byteSize());
    guard.commit();
  }

  return res;
}

void RocksDBCollection::deferDropCollection(
    std::function<bool(LogicalCollection*)> /*callback*/) {
  // nothing to do here
}

/// @brief return engine-specific figures
void RocksDBCollection::figuresSpecific(
    std::shared_ptr<arangodb::velocypack::Builder>&) {
  // no specific figures yet
}

/// @brief creates the initial indexes for the collection
void RocksDBCollection::createInitialIndexes() {
  if (!_indexes.empty()) {
    return;
  }

  std::vector<std::shared_ptr<arangodb::Index>> systemIndexes;
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  IndexFactory const* idxFactory = engine->indexFactory();
  TRI_ASSERT(idxFactory != nullptr);

  idxFactory->fillSystemIndexes(_logicalCollection, systemIndexes);
  for (auto const& it : systemIndexes) {
    addIndex(it);
  }
}

void RocksDBCollection::addIndex(std::shared_ptr<arangodb::Index> idx) {
  // primary index must be added at position 0
  TRI_ASSERT(idx->type() != arangodb::Index::TRI_IDX_TYPE_PRIMARY_INDEX ||
             _indexes.empty());

  auto const id = idx->id();
  for (auto const& it : _indexes) {
    if (it->id() == id) {
      // already have this particular index. do not add it again
      return;
    }
  }

  TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(id));

  _indexes.emplace_back(idx);
}

void RocksDBCollection::addIndexCoordinator(
    std::shared_ptr<arangodb::Index> idx) {
  auto const id = idx->id();
  for (auto const& it : _indexes) {
    if (it->id() == id) {
      // already have this particular index. do not add it again
      return;
    }
  }

  _indexes.emplace_back(idx);
}

int RocksDBCollection::saveIndex(transaction::Methods* trx,
                                 std::shared_ptr<arangodb::Index> idx) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  // we cannot persist primary or edge indexes
  TRI_ASSERT(idx->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX);
  TRI_ASSERT(idx->type() != Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX);

  Result res = fillIndexes(trx, idx);
  if (!res.ok()) {
    return res.errorNumber();
  }
  
  std::shared_ptr<VPackBuilder> builder = idx->toVelocyPack(false);
  auto vocbase = _logicalCollection->vocbase();
  auto collectionId = _logicalCollection->cid();
  VPackSlice data = builder->slice();

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  engine->createIndex(vocbase, collectionId, idx->id(), data);

  return TRI_ERROR_NO_ERROR;
}

arangodb::Result RocksDBCollection::fillIndexes(transaction::Methods* trx,
                                                std::shared_ptr<arangodb::Index> added) {
  
  ManagedDocumentResult mmr;
  std::unique_ptr<IndexIterator> iter(primaryIndex()->allIterator(trx, &mmr, false));
  int res = TRI_ERROR_NO_ERROR;
  
  auto cb = [&](DocumentIdentifierToken token) {
    if (res == TRI_ERROR_NO_ERROR && this->readDocument(trx, token, mmr)) {
        RocksDBIndex *ridx = static_cast<RocksDBIndex*>(added.get());
        res = ridx->insert(trx, mmr.lastRevisionId(), VPackSlice(mmr.vpack()), false);
    }
  };
  while (iter->next(cb, 1000) && res == TRI_ERROR_NO_ERROR) {
    if (_logicalCollection->deleted()) {
      return Result(TRI_ERROR_INTERNAL);
    }
  }
  return Result(res);
}

// @brief return the primary index
// WARNING: Make sure that this LogicalCollection Instance
// is somehow protected. If it goes out of all scopes
// or it's indexes are freed the pointer returned will get invalidated.
arangodb::RocksDBPrimaryIndex* RocksDBCollection::primaryIndex() const {
  // The primary index always has iid 0
  auto primary = PhysicalCollection::lookupIndex(0);
  TRI_ASSERT(primary != nullptr);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  if (primary->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "got invalid indexes for collection '" << _logicalCollection->name()
        << "'";
    for (auto const& it : _indexes) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "- " << it.get();
    }
  }
#endif
  TRI_ASSERT(primary->type() == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX);
  // the primary index must be the index at position #0
  return static_cast<arangodb::RocksDBPrimaryIndex*>(primary.get());
}

int RocksDBCollection::insertDocument(arangodb::transaction::Methods* trx,
                                      TRI_voc_rid_t revisionId,
                                      VPackSlice const& doc,
                                      bool& waitForSync) {
  // Coordinator doesn't know index internals
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  TRI_ASSERT(trx->state()->isRunning());

  RocksDBKey key(RocksDBKey::Document(_objectId, revisionId));
  RocksDBValue value(RocksDBValue::Document(doc));

  rocksdb::Transaction* rtrx = rocksTransaction(trx);

  /*
    LOG_TOPIC(ERR, Logger::ENGINE)
        << "INSERT DOCUMENT. COLLECTION '" << _logicalCollection->name()
        << "', OBJECTID: " << _objectId << ", REVISIONID: " << revisionId;

  */
  rocksdb::Status status = rtrx->Put(key.string(), value.string());

  if (!status.ok()) {
    auto converted =
        rocksutils::convertStatus(status, rocksutils::StatusHint::document);
    return converted.errorNumber();
  }

  auto indexes = _indexes;
  size_t const n = indexes.size();

  int result = TRI_ERROR_NO_ERROR;

  for (size_t i = 0; i < n; ++i) {
    auto& idx = indexes[i];

    int res = idx->insert(trx, revisionId, doc, false);

    // in case of no-memory, return immediately
    if (res == TRI_ERROR_OUT_OF_MEMORY) {
      return res;
    }
    if (res != TRI_ERROR_NO_ERROR) {
      if (res == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED ||
          result == TRI_ERROR_NO_ERROR) {
        // "prefer" unique constraint violated
        result = res;
      }
    }
  }

  if (result == TRI_ERROR_NO_ERROR) {
    if (_logicalCollection->waitForSync()) {
      waitForSync = true;  // output parameter (by ref)
    }

    if (waitForSync) {
      trx->state()->waitForSync(true);
    }
  }

  return result;
}

int RocksDBCollection::removeDocument(arangodb::transaction::Methods* trx,
                                      TRI_voc_rid_t revisionId,
                                      VPackSlice const& doc,
                                      bool& waitForSync) {
  // Coordinator doesn't know index internals
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  TRI_ASSERT(trx->state()->isRunning());

  auto key = RocksDBKey::Document(_objectId, revisionId);

  rocksdb::Transaction* rtrx = rocksTransaction(trx);

  rtrx->Delete(key.string());

  auto indexes = _indexes;
  size_t const n = indexes.size();

  int result = TRI_ERROR_NO_ERROR;

  for (size_t i = 0; i < n; ++i) {
    auto idx = indexes[i];

    int res = idx->remove(trx, revisionId, doc, false);

    // in case of no-memory, return immediately
    if (res == TRI_ERROR_OUT_OF_MEMORY) {
      return res;
    }
  }

  if (result == TRI_ERROR_NO_ERROR) {
    if (_logicalCollection->waitForSync()) {
      waitForSync = true;
    }

    if (waitForSync) {
      trx->state()->waitForSync(true);
    }
  }

  return result;
}

/// @brief looks up a document by key, low level worker
/// the key must be a string slice, no revision check is performed
int RocksDBCollection::lookupDocument(transaction::Methods* trx, VPackSlice key,
                                      ManagedDocumentResult& mdr) {
  if (!key.isString()) {
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
  }

  RocksDBToken token = primaryIndex()->lookupKey(trx, key, mdr);
  TRI_voc_rid_t revisionId = token.revisionId();

  if (revisionId > 0) {
    Result result = lookupRevisionVPack(revisionId, trx, mdr);
    return result.errorNumber();
  }

  return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
}

int RocksDBCollection::updateDocument(transaction::Methods* trx,
                                      TRI_voc_rid_t oldRevisionId,
                                      VPackSlice const& oldDoc,
                                      TRI_voc_rid_t newRevisionId,
                                      VPackSlice const& newDoc,
                                      bool& waitForSync) {
  // Coordinator doesn't know index internals
  TRI_ASSERT(trx->state()->isRunning());
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  /*
    LOG_TOPIC(ERR, Logger::FIXME)
        << "UPDATE DOCUMENT. COLLECTION '" << _logicalCollection->name()
        << "', OBJECTID: " << _objectId << ", OLDREVISIONID: " << oldRevisionId
        << ", NEWREVISIONID: " << newRevisionId;
  */
  int res = removeDocument(trx, oldRevisionId, oldDoc, waitForSync);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  res = insertDocument(trx, newRevisionId, newDoc, waitForSync);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  return TRI_ERROR_NO_ERROR;
}

Result RocksDBCollection::lookupDocumentToken(transaction::Methods* trx,
                                              arangodb::StringRef key,
                                              RocksDBToken& outToken) {
  // TODO fix as soon as we got a real primary index
  outToken = primaryIndex()->lookupKey(trx, key);
  return outToken.revisionId() > 0
             ? Result()
             : Result(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
}

arangodb::Result RocksDBCollection::lookupRevisionVPack(
    TRI_voc_rid_t revisionId, transaction::Methods* trx,
    arangodb::ManagedDocumentResult& mdr) {
  TRI_ASSERT(trx->state()->isRunning());

  auto key = RocksDBKey::Document(_objectId, revisionId);
  std::string value;
  auto* state = toRocksTransactionState(trx);
  rocksdb::Status status = state->rocksTransaction()->Get(state->readOptions(),
                                                          key.string(), &value);
  TRI_ASSERT(value.data());
  auto result = convertStatus(status);
  if (result.ok()) {
    /*
    LOG_TOPIC(ERR, Logger::FIXME)
        << "LOOKUPREVISIONVPACK. COLLECTION '" << _logicalCollection->name()
        << "', OBJECTID: " << _objectId << ", REVISIONID: " << revisionId
        << " -> FOUND";
    */
    mdr.setManaged(std::move(value), revisionId);
  } else {
    /*
    LOG_TOPIC(ERR, Logger::FIXME)
        << "LOOKUPREVISIONVPACK. COLLECTION '" << _logicalCollection->name()
        << "', OBJECTID: " << _objectId << ", REVISIONID: " << revisionId
        << " -> NOT FOUND";
    */
    mdr.reset();
  }
  return result;
}

void RocksDBCollection::adjustNumberDocuments(int64_t adjustment) {
  if (adjustment < 0) {
    _numberDocuments -= static_cast<uint64_t>(-adjustment);
  } else if (adjustment > 0) {
    _numberDocuments += static_cast<uint64_t>(adjustment);
  }
}
