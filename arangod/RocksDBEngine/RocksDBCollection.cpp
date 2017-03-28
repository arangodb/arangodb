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
/// @author Jan-Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "Aql/PlanCache.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterMethods.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBCollection.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBPrimaryMockIndex.h"
#include "RocksDBEngine/RocksDBToken.h"
#include "RocksDBEngine/RocksDBValue.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/TransactionState.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Helpers.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/OperationOptions.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"

#include <rocksdb/utilities/transaction.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::rocksutils;

static std::string const Empty;
 
rocksdb::TransactionDB* db() {
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  return static_cast<RocksDBEngine*>(engine)->db();
}

RocksDBCollection::RocksDBCollection(LogicalCollection* collection,
                                     VPackSlice const& info)
    : PhysicalCollection(collection, info),
      _objectId(basics::VelocyPackHelper::stringUInt64(info, "objectId")) {
  TRI_ASSERT(_objectId != 0);
}

RocksDBCollection::RocksDBCollection(LogicalCollection* collection,
                                     PhysicalCollection* physical)
    : PhysicalCollection(collection, VPackSlice::emptyObjectSlice()),
      _objectId(static_cast<RocksDBCollection*>(physical)->_objectId) {
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
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return arangodb::Result{};
}

PhysicalCollection* RocksDBCollection::clone(LogicalCollection* logical,
                                             PhysicalCollection* physical) {
  return new RocksDBCollection(logical, physical);
}

TRI_voc_rid_t RocksDBCollection::revision() const {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return 0;
}

int64_t RocksDBCollection::initialCount() const {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return 0;
}

void RocksDBCollection::updateCount(int64_t) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
}

void RocksDBCollection::getPropertiesVPack(velocypack::Builder& result) const {
  TRI_ASSERT(result.isOpenObject());
  result.add("objectId", VPackValue(std::to_string(_objectId)));
}

void RocksDBCollection::getPropertiesVPackCoordinator(
    velocypack::Builder& result) const {
  getPropertiesVPack(result);
}

/// @brief closes an open collection
int RocksDBCollection::close() {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return TRI_ERROR_NO_ERROR;
}

uint64_t RocksDBCollection::numberDocuments() const {
  // TODO
  return 0;
}

/// @brief report extra memory used by indexes etc.
size_t RocksDBCollection::memory() const {
  // TODO
  return 0;
}

void RocksDBCollection::open(bool ignoreErrors) {
  // doesn't need to be opened
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
  if (_indexes[0]->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX) {
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
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return false;
}

std::unique_ptr<IndexIterator> RocksDBCollection::getAllIterator(
    transaction::Methods* trx, ManagedDocumentResult* mdr, bool reverse) {
  return std::unique_ptr<IndexIterator>(
      primaryIndex()->allIterator(trx, mdr, reverse));
}

std::unique_ptr<IndexIterator> RocksDBCollection::getAnyIterator(
    transaction::Methods* trx, ManagedDocumentResult* mdr) {
  return std::unique_ptr<IndexIterator>(primaryIndex()->anyIterator(trx, mdr));
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
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
}

int RocksDBCollection::read(transaction::Methods*,
                            arangodb::velocypack::Slice const key,
                            ManagedDocumentResult& result, bool) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return 0;
}

bool RocksDBCollection::readDocument(transaction::Methods* trx,
                                     DocumentIdentifierToken const& token,
                                     ManagedDocumentResult& result) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return false;
}

bool RocksDBCollection::readDocumentConditional(
    transaction::Methods* trx, DocumentIdentifierToken const& token,
    TRI_voc_tick_t maxTick, ManagedDocumentResult& result) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return false;
}

int RocksDBCollection::insert(arangodb::transaction::Methods* trx,
                              arangodb::velocypack::Slice const slice,
                              arangodb::ManagedDocumentResult& result,
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
  VPackSlice newSlice;
  int res = TRI_ERROR_NO_ERROR;
  if (options.recoveryData == nullptr) {
    res = newObjectForInsert(trx, slice, fromSlice, toSlice, isEdgeCollection,
                             *builder.get(), options.isRestore);
    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
    newSlice = builder->slice();
  } else {
    TRI_ASSERT(slice.isObject());
    // we can get away with the fast hash function here, as key values are
    // restricted to strings
    newSlice = slice;
  }

  TRI_voc_rid_t revisionId =
      transaction::helpers::extractRevFromDocument(newSlice);

  res = insertDocument(trx, revisionId, newSlice, options.waitForSync);

  if (res == TRI_ERROR_NO_ERROR) {
    lookupRevisionVPack(revisionId, trx, result);
  }
  return res;
}

int RocksDBCollection::update(arangodb::transaction::Methods* trx,
                              arangodb::velocypack::Slice const newSlice,
                              arangodb::ManagedDocumentResult& result,
                              OperationOptions& options,
                              TRI_voc_tick_t& resultMarkerTick, bool lock,
                              TRI_voc_rid_t& prevRev,
                              ManagedDocumentResult& previous,
                              TRI_voc_rid_t const& revisionId,
                              arangodb::velocypack::Slice const key) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return 0;
}

int RocksDBCollection::replace(
    transaction::Methods* trx, arangodb::velocypack::Slice const newSlice,
    ManagedDocumentResult& result, OperationOptions& options,
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

  uint8_t const* vpack = previous.vpack();
  VPackSlice oldDoc(vpack);
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

  res = updateDocument(trx, oldRevisionId, oldDoc, revisionId,
                       VPackSlice(builder->slice()), options.waitForSync);
  lookupRevisionVPack(revisionId, trx, result);
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

  uint8_t const* vpack = previous.vpack();
  VPackSlice oldDoc(vpack);
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

  res = removeDocument(trx, oldRevisionId, oldDoc, options.waitForSync);

  return res;
}

void RocksDBCollection::deferDropCollection(
    std::function<bool(LogicalCollection*)> callback) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
}

/// @brief return engine-specific figures
void RocksDBCollection::figuresSpecific(
    std::shared_ptr<arangodb::velocypack::Builder>&) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
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
  // we cannot persist PrimaryMockIndex
  TRI_ASSERT(idx->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX);
  std::vector<std::shared_ptr<arangodb::Index>> indexListLocal;
  indexListLocal.emplace_back(idx);

  /* TODO
    int res = fillIndexes(trx, indexListLocal, false);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  */
  std::shared_ptr<VPackBuilder> builder = idx->toVelocyPack(false);
  auto vocbase = _logicalCollection->vocbase();
  auto collectionId = _logicalCollection->cid();
  VPackSlice data = builder->slice();

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  engine->createIndex(vocbase, collectionId, idx->id(), data);

  return TRI_ERROR_NO_ERROR;
}

// @brief return the primary index
// WARNING: Make sure that this LogicalCollection Instance
// is somehow protected. If it goes out of all scopes
// or it's indexes are freed the pointer returned will get invalidated.
arangodb::RocksDBPrimaryMockIndex* RocksDBCollection::primaryIndex() const {
  // The primary index always has iid 0
  auto primary = _logicalCollection->lookupIndex(0);
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
  return static_cast<arangodb::RocksDBPrimaryMockIndex*>(primary.get());
}

int RocksDBCollection::insertDocument(arangodb::transaction::Methods* trx,
                                      TRI_voc_rid_t revisionId,
                                      VPackSlice const& doc,
                                      bool& waitForSync) {
  // Coordinator doesn't know index internals
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  RocksDBKey key(RocksDBKey::Document(_objectId, revisionId));
  RocksDBValue value(RocksDBValue::Document(doc));

  rocksdb::WriteBatch writeBatch;
  writeBatch.Put(key.string(), value.value());

  auto indexes = _indexes;
  size_t const n = indexes.size();

  int result = TRI_ERROR_NO_ERROR;

  for (size_t i = 0; i < n; ++i) {
    auto idx = indexes[i];

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

  if (result != TRI_ERROR_NO_ERROR) {
    rocksdb::WriteOptions writeOptions;

    if (_logicalCollection->waitForSync()) {
      waitForSync = true;
    }

    if (waitForSync) {
      trx->state()->waitForSync(true);

      // handle waitForSync for single operations here
      if (trx->state()->isSingleOperation()) {
        writeOptions.sync = true;
      }
    }

    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    rocksdb::TransactionDB* db = static_cast<RocksDBEngine*>(engine)->db();
    db->Write(writeOptions, &writeBatch);
  }

  return result;
}

int RocksDBCollection::removeDocument(arangodb::transaction::Methods* trx,
                                      TRI_voc_rid_t revisionId,
                                      VPackSlice const& doc,
                                      bool& waitForSync) {
  // Coordinator doesn't know index internals
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  auto key = RocksDBKey::Document(_objectId, revisionId);

  rocksdb::WriteBatch writeBatch;
  writeBatch.Delete(key.string());

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

  if (result != TRI_ERROR_NO_ERROR) {
    rocksdb::WriteOptions writeOptions;

    if (_logicalCollection->waitForSync()) {
      waitForSync = true;
    }

    if (waitForSync) {
      trx->state()->waitForSync(true);

      // handle waitForSync for single operations here
      if (trx->state()->isSingleOperation()) {
        writeOptions.sync = true;
      }
    }

    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    rocksdb::TransactionDB* db = static_cast<RocksDBEngine*>(engine)->db();
    db->Write(writeOptions, &writeBatch);
  }

  return result;
}

/// @brief looks up a document by key, low level worker
/// the key must be a string slice, no revision check is performed
int RocksDBCollection::lookupDocument(transaction::Methods* trx, VPackSlice key,
                                      ManagedDocumentResult& result) {
  if (!key.isString()) {
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
  }

  RocksDBToken token = primaryIndex()->lookupKey(trx, key, result);
  TRI_voc_rid_t revisionId = token.revisionId();

  if (revisionId > 0) {
    lookupRevisionVPack(revisionId, trx, result);
    return TRI_ERROR_NO_ERROR;
  }

  return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
}

int RocksDBCollection::updateDocument(transaction::Methods* trx,
                                      TRI_voc_rid_t oldRevisionId,
                                      VPackSlice const& oldDoc,
                                      TRI_voc_rid_t newRevisionId,
                                      VPackSlice const& newDoc,
                                      bool& waitForSync) {
  // TODO
  return TRI_ERROR_NO_ERROR;
}

Result RocksDBCollection::lookupDocumentToken(transaction::Methods* trx,
                           arangodb::StringRef key, RocksDBToken &outToken) {
  // TODO fix as soon as we got a real primary index
  outToken = primaryIndex()->lookupKey(trx, key);
  return outToken.revisionId() > 0 ? Result() : Result(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
}

void RocksDBCollection::lookupRevisionVPack(TRI_voc_rid_t revisionId, transaction::Methods* trx,arangodb::ManagedDocumentResult& result){
  auto key = RocksDBKey::Document(_objectId,revisionId);
  std::string value;
  TRI_ASSERT(value.data());
  auto* state = toRocksTransactionState(trx);
  state->rocksTransaction()->Get(rocksdb::ReadOptions(), rocksdb::Slice(key.string()), &value);
  result.setManaged(reinterpret_cast<uint8_t const * >(value.data()), revisionId);
}
