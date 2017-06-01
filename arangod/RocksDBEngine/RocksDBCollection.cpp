////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/ReadLocker.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Cache/CacheManagerFeature.h"
#include "Cache/Common.h"
#include "Cache/Manager.h"
#include "Cache/TransactionalCache.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/CollectionLockState.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBComparator.h"
#include "RocksDBEngine/RocksDBCounterManager.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBIterators.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBPrimaryIndex.h"
#include "RocksDBEngine/RocksDBToken.h"
#include "RocksDBEngine/RocksDBTransactionCollection.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "RocksDBEngine/RocksDBValue.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Helpers.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/Events.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "VocBase/voc-types.h"

#include <rocksdb/db.h>
#include <rocksdb/utilities/transaction.h>
#include <rocksdb/utilities/write_batch_with_index.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::rocksutils;

namespace {

static std::string const Empty;

}  // namespace

RocksDBCollection::RocksDBCollection(LogicalCollection* collection,
                                     VPackSlice const& info)
    : PhysicalCollection(collection, info),
      _objectId(basics::VelocyPackHelper::stringUInt64(info, "objectId")),
      _numberDocuments(0),
      _revisionId(0),
      _needToPersistIndexEstimates(false),
      _hasGeoIndex(false),
      _cache(nullptr),
      _cachePresent(false),
      _useCache(false) {
  addCollectionMapping(_objectId, _logicalCollection->vocbase()->id(),
                       _logicalCollection->cid());
  if (_useCache) {
    createCache();
  }
}

RocksDBCollection::RocksDBCollection(LogicalCollection* collection,
                                     PhysicalCollection* physical)
    : PhysicalCollection(collection, VPackSlice::emptyObjectSlice()),
      _objectId(static_cast<RocksDBCollection*>(physical)->_objectId),
      _numberDocuments(0),
      _revisionId(0),
      _needToPersistIndexEstimates(false),
      _hasGeoIndex(false),
      _cache(nullptr),
      _cachePresent(false),
      _useCache(false) {
  addCollectionMapping(_objectId, _logicalCollection->vocbase()->id(),
                       _logicalCollection->cid());
  if (_useCache) {
    createCache();
  }
}

RocksDBCollection::~RocksDBCollection() {
  if (useCache()) {
    try {
      TRI_ASSERT(_cache != nullptr);
      TRI_ASSERT(CacheManagerFeature::MANAGER != nullptr);
      CacheManagerFeature::MANAGER->destroyCache(_cache);
    } catch (...) {
    }
  }
}

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
  // only code path calling this causes these properties to be
  // already written in RocksDBEngine::changeCollection()
  return arangodb::Result{};
}

PhysicalCollection* RocksDBCollection::clone(LogicalCollection* logical,
                                             PhysicalCollection* physical) {
  return new RocksDBCollection(logical, physical);
}

void RocksDBCollection::getPropertiesVPack(velocypack::Builder& result) const {
  // objectId might be undefined on the coordinator
  TRI_ASSERT(result.isOpenObject());
  result.add("objectId", VPackValue(std::to_string(_objectId)));
  TRI_ASSERT(result.isOpenObject());
}

void RocksDBCollection::getPropertiesVPackCoordinator(
    velocypack::Builder& result) const {
  getPropertiesVPack(result);
}

/// @brief closes an open collection
int RocksDBCollection::close() {
  READ_LOCKER(guard, _indexesLock);
  for (auto it : _indexes) {
    it->unload();
  }
  return TRI_ERROR_NO_ERROR;
}

TRI_voc_rid_t RocksDBCollection::revision() const { return _revisionId; }

TRI_voc_rid_t RocksDBCollection::revision(transaction::Methods* trx) const {
  RocksDBTransactionState* state = toRocksTransactionState(trx);

  auto trxCollection = static_cast<RocksDBTransactionCollection*>(
      state->findCollection(_logicalCollection->cid()));
  TRI_ASSERT(trxCollection != nullptr);

  return trxCollection->revision();
}

uint64_t RocksDBCollection::numberDocuments() const { return _numberDocuments; }

uint64_t RocksDBCollection::numberDocuments(transaction::Methods* trx) const {
  RocksDBTransactionState* state = toRocksTransactionState(trx);

  auto trxCollection = static_cast<RocksDBTransactionCollection*>(
      state->findCollection(_logicalCollection->cid()));
  TRI_ASSERT(trxCollection != nullptr);

  return trxCollection->numberDocuments();
}

/// @brief report extra memory used by indexes etc.
size_t RocksDBCollection::memory() const { return 0; }

void RocksDBCollection::open(bool ignoreErrors) {
  TRI_ASSERT(_objectId != 0);

  // set the initial number of documents
  RocksDBEngine* engine =
      static_cast<RocksDBEngine*>(EngineSelectorFeature::ENGINE);
  auto counterValue = engine->counterManager()->loadCounter(this->objectId());
  _numberDocuments = counterValue.added() - counterValue.removed();
  _revisionId = counterValue.revisionId();

  READ_LOCKER(guard, _indexesLock);
  for (std::shared_ptr<Index> it : _indexes) {
    if (it->type() == Index::TRI_IDX_TYPE_GEO1_INDEX ||
        it->type() == Index::TRI_IDX_TYPE_GEO2_INDEX) {
      _hasGeoIndex = true;
    }
  }
}

void RocksDBCollection::prepareIndexes(
    arangodb::velocypack::Slice indexesSlice) {
  WRITE_LOCKER(guard, _indexesLock);
  TRI_ASSERT(indexesSlice.isArray());
  if (indexesSlice.length() == 0) {
    createInitialIndexes();
  }

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  IndexFactory const* idxFactory = engine->indexFactory();
  TRI_ASSERT(idxFactory != nullptr);
  bool splitEdgeIndex = false;
  TRI_idx_iid_t last = 0;
  for (auto const& v : VPackArrayIterator(indexesSlice)) {
    if (arangodb::basics::VelocyPackHelper::getBooleanValue(v, "error",
                                                            false)) {
      // We have an error here.
      // Do not add index.
      // TODO Handle Properly
      continue;
    }

    bool alreadyHandled = false;
    // check for combined edge index from MMFiles; must split!
    auto value = v.get("type");
    if (value.isString()) {
      std::string tmp = value.copyString();
      arangodb::Index::IndexType const type =
          arangodb::Index::type(tmp.c_str());
      if (type == Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX) {
        VPackSlice fields = v.get("fields");
        if (fields.isArray() && fields.length() == 2) {
          VPackBuilder from;
          from.openObject();
          for (auto const& f : VPackObjectIterator(v)) {
            if (arangodb::StringRef(f.key) == "fields") {
              from.add(VPackValue("fields"));
              from.openArray();
              from.add(VPackValue(StaticStrings::FromString));
              from.close();
            } else {
              from.add(f.key);
              from.add(f.value);
            }
          }
          from.close();

          VPackBuilder to;
          to.openObject();
          for (auto const& f : VPackObjectIterator(v)) {
            if (arangodb::StringRef(f.key) == "fields") {
              to.add(VPackValue("fields"));
              to.openArray();
              to.add(VPackValue(StaticStrings::ToString));
              to.close();
            } else if (arangodb::StringRef(f.key) == "id") {
              auto iid = basics::StringUtils::uint64(f.value.copyString()) + 1;
              last = iid;
              to.add("id", VPackValue(std::to_string(iid)));
            } else {
              to.add(f.key);
              to.add(f.value);
            }
          }
          to.close();

          auto idxFrom = idxFactory->prepareIndexFromSlice(
              from.slice(), false, _logicalCollection, true);

          if (ServerState::instance()->isRunningInCluster()) {
            addIndexCoordinator(idxFrom);
          } else {
            addIndex(idxFrom);
          }

          auto idxTo = idxFactory->prepareIndexFromSlice(
              to.slice(), false, _logicalCollection, true);

          if (ServerState::instance()->isRunningInCluster()) {
            addIndexCoordinator(idxTo);
          } else {
            addIndex(idxTo);
          }

          alreadyHandled = true;
          splitEdgeIndex = true;
        }
      } else if (splitEdgeIndex) {
        VPackBuilder b;
        b.openObject();
        for (auto const& f : VPackObjectIterator(v)) {
          if (arangodb::StringRef(f.key) == "id") {
            last++;
            b.add("id", VPackValue(std::to_string(last)));
          } else {
            b.add(f.key);
            b.add(f.value);
          }
        }
        b.close();

        auto idx = idxFactory->prepareIndexFromSlice(b.slice(), false,
                                                     _logicalCollection, true);

        if (ServerState::instance()->isRunningInCluster()) {
          addIndexCoordinator(idx);
        } else {
          addIndex(idx);
        }

        alreadyHandled = true;
      }
    }

    if (!alreadyHandled) {
      auto idx =
          idxFactory->prepareIndexFromSlice(v, false, _logicalCollection, true);

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
    for (auto it : _indexes) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "- " << it.get();
    }
  }
#endif
}

static std::shared_ptr<Index> findIndex(
    velocypack::Slice const& info,
    std::vector<std::shared_ptr<Index>> const& indexes) {
  TRI_ASSERT(info.isObject());

  // extract type
  VPackSlice value = info.get("type");

  if (!value.isString()) {
    // Compatibility with old v8-vocindex.
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  std::string tmp = value.copyString();
  arangodb::Index::IndexType const type = arangodb::Index::type(tmp.c_str());

  for (auto const& idx : indexes) {
    if (idx->type() == type) {
      // Only check relevant indexes
      if (idx->matchesDefinition(info)) {
        // We found an index for this definition.
        return idx;
      }
    }
  }
  return nullptr;
}

/// @brief Find index by definition
std::shared_ptr<Index> RocksDBCollection::lookupIndex(
    velocypack::Slice const& info) const {
  READ_LOCKER(guard, _indexesLock);
  return findIndex(info, _indexes);
}

std::shared_ptr<Index> RocksDBCollection::createIndex(
    transaction::Methods* trx, arangodb::velocypack::Slice const& info,
    bool& created) {
  // prevent concurrent dropping
  bool isLocked =
      trx->isLocked(_logicalCollection, AccessMode::Type::EXCLUSIVE);
  CONDITIONAL_WRITE_LOCKER(guard, _exclusiveLock, !isLocked);
  std::shared_ptr<Index> idx;
  {
    WRITE_LOCKER(guard, _indexesLock);
    idx = findIndex(info, _indexes);
    if (idx) {
      created = false;
      // We already have this index.
      return idx;
    }
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
  {
    WRITE_LOCKER(guard, _indexesLock);
    addIndex(idx);
  }
  VPackBuilder builder = _logicalCollection->toVelocyPackIgnore(
      {"path", "statusString"}, true, /*forPersistence*/ true);

  VPackBuilder indexInfo;
  idx->toVelocyPack(indexInfo, false, true);
  res = static_cast<RocksDBEngine*>(engine)->writeCreateCollectionMarker(
      _logicalCollection->vocbase()->id(), _logicalCollection->cid(),
      builder.slice(), RocksDBLogValue::IndexCreate(
                           _logicalCollection->vocbase()->id(),
                           _logicalCollection->cid(), indexInfo.slice()));
  if (res != TRI_ERROR_NO_ERROR) {
    // We could not persist the index creation. Better abort
    // Remove the Index in the local list again.
    size_t i = 0;
    WRITE_LOCKER(guard, _indexesLock);
    for (auto index : _indexes) {
      if (index == idx) {
        _indexes.erase(_indexes.begin() + i);
        break;
      }
      ++i;
    }
    THROW_ARANGO_EXCEPTION(res);
  }
  created = true;
  return idx;
}

/// @brief Restores an index from VelocyPack.
int RocksDBCollection::restoreIndex(transaction::Methods* trx,
                                    velocypack::Slice const& info,
                                    std::shared_ptr<Index>& idx) {
  // The coordinator can never get into this state!
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  idx.reset();  // Clear it to make sure.
  if (!info.isObject()) {
    return TRI_ERROR_INTERNAL;
  }

  // We create a new Index object to make sure that the index
  // is not handed out except for a successful case.
  std::shared_ptr<Index> newIdx;
  try {
    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    IndexFactory const* idxFactory = engine->indexFactory();
    TRI_ASSERT(idxFactory != nullptr);
    newIdx = idxFactory->prepareIndexFromSlice(info, false, _logicalCollection,
                                               false);
  } catch (arangodb::basics::Exception const& e) {
    // Something with index creation went wrong.
    // Just report.
    return e.code();
  }
  TRI_ASSERT(newIdx != nullptr);

  auto const id = newIdx->id();

  TRI_UpdateTickServer(id);

  for (auto& it : _indexes) {
    if (it->id() == id) {
      // index already exists
      idx = it;
      return TRI_ERROR_NO_ERROR;
    }
  }

  TRI_ASSERT(newIdx.get()->type() !=
             Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX);

  Result res = fillIndexes(trx, newIdx);

  if (!res.ok()) {
    return res.errorNumber();
  }

  addIndex(newIdx);
  {
    VPackBuilder builder = _logicalCollection->toVelocyPackIgnore(
        {"path", "statusString"}, true, /*forPersistence*/ true);
    VPackBuilder indexInfo;
    newIdx->toVelocyPack(indexInfo, false, true);

    RocksDBEngine* engine =
        static_cast<RocksDBEngine*>(EngineSelectorFeature::ENGINE);
    TRI_ASSERT(engine != nullptr);
    int res = engine->writeCreateCollectionMarker(
        _logicalCollection->vocbase()->id(), _logicalCollection->cid(),
        builder.slice(), RocksDBLogValue::IndexCreate(
                             _logicalCollection->vocbase()->id(),
                             _logicalCollection->cid(), indexInfo.slice()));
    if (res != TRI_ERROR_NO_ERROR) {
      // We could not persist the index creation. Better abort
      // Remove the Index in the local list again.
      size_t i = 0;
      WRITE_LOCKER(guard, _indexesLock);
      for (auto index : _indexes) {
        if (index == newIdx) {
          _indexes.erase(_indexes.begin() + i);
          break;
        }
        ++i;
      }
      return res;
    }
  }

  idx = newIdx;
  // We need to write the IndexMarker

  return TRI_ERROR_NO_ERROR;
}

/// @brief Drop an index with the given iid.
bool RocksDBCollection::dropIndex(TRI_idx_iid_t iid) {
  // usually always called when _exclusiveLock is held
  if (iid == 0) {
    // invalid index id or primary index
    return true;
  }

  size_t i = 0;
  WRITE_LOCKER(guard, _indexesLock);
  for (auto index : _indexes) {
    RocksDBIndex* cindex = static_cast<RocksDBIndex*>(index.get());
    TRI_ASSERT(cindex != nullptr);

    if (iid == cindex->id()) {
      int rv = cindex->drop();

      if (rv == TRI_ERROR_NO_ERROR) {
        // trigger compaction before deleting the object
        cindex->cleanup();

        _indexes.erase(_indexes.begin() + i);
        events::DropIndex("", std::to_string(iid), TRI_ERROR_NO_ERROR);
        // toVelocyPackIgnore will take a read lock and we don't need the
        // lock anymore, we will always return
        guard.unlock();

        VPackBuilder builder = _logicalCollection->toVelocyPackIgnore(
            {"path", "statusString"}, true, true);
        StorageEngine* engine = EngineSelectorFeature::ENGINE;

        int res =
            static_cast<RocksDBEngine*>(engine)->writeCreateCollectionMarker(
                _logicalCollection->vocbase()->id(), _logicalCollection->cid(),
                builder.slice(),
                RocksDBLogValue::IndexDrop(_logicalCollection->vocbase()->id(),
                                           _logicalCollection->cid(), iid));
        return res == TRI_ERROR_NO_ERROR;
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
    transaction::Methods* trx, ManagedDocumentResult* mdr, bool reverse) const {
  return std::unique_ptr<IndexIterator>(new RocksDBAllIndexIterator(
      _logicalCollection, trx, mdr, primaryIndex(), reverse));
}

std::unique_ptr<IndexIterator> RocksDBCollection::getAnyIterator(
    transaction::Methods* trx, ManagedDocumentResult* mdr) const {
  return std::unique_ptr<IndexIterator>(new RocksDBAnyIndexIterator(
      _logicalCollection, trx, mdr, primaryIndex()));
}

std::unique_ptr<IndexIterator> RocksDBCollection::getSortedAllIterator(
    transaction::Methods* trx, ManagedDocumentResult* mdr) const {
  return std::unique_ptr<RocksDBSortedAllIterator>(new RocksDBSortedAllIterator(
      _logicalCollection, trx, mdr, primaryIndex()));
}

void RocksDBCollection::invokeOnAllElements(
    transaction::Methods* trx,
    std::function<bool(DocumentIdentifierToken const&)> callback) {
  ManagedDocumentResult mmdr;
  std::unique_ptr<IndexIterator> cursor(
      this->getAllIterator(trx, &mmdr, false));
  bool cnt = true;
  auto cb = [&](DocumentIdentifierToken token) {
    if (cnt) {
      cnt = callback(token);
    }
  };
  while (cursor->next(cb, 1000) && cnt) {
  }
}

////////////////////////////////////
// -- SECTION DML Operations --
///////////////////////////////////

void RocksDBCollection::truncate(transaction::Methods* trx,
                                 OperationOptions& options) {
  // TODO FIXME -- improve transaction size
  TRI_ASSERT(_objectId != 0);
  TRI_voc_cid_t cid = _logicalCollection->cid();
  RocksDBTransactionState* state = rocksutils::toRocksTransactionState(trx);
  // delete documents
  RocksDBMethods* mthd;
  mthd = state->rocksdbMethods();

  RocksDBKeyBounds documentBounds =
      RocksDBKeyBounds::CollectionDocuments(this->objectId());

  rocksdb::Comparator const* cmp =
      RocksDBColumnFamily::documents()->GetComparator();
  rocksdb::ReadOptions ro = mthd->readOptions();
  rocksdb::Slice const end = documentBounds.end();
  ro.iterate_upper_bound = &end;

  std::unique_ptr<rocksdb::Iterator> iter =
      mthd->NewIterator(ro, RocksDBColumnFamily::documents());
  iter->Seek(documentBounds.start());

  while (iter->Valid() && cmp->Compare(iter->key(), end) < 0) {
    TRI_ASSERT(_objectId == RocksDBKey::objectId(iter->key()));

    TRI_voc_rid_t revisionId = RocksDBKey::revisionId(iter->key());
    VPackSlice key =
        VPackSlice(iter->value().data()).get(StaticStrings::KeyString);
    TRI_ASSERT(key.isString());

    blackListKey(iter->key().data(), static_cast<uint32_t>(iter->key().size()));

    // add possible log statement
    state->prepareOperation(cid, revisionId, StringRef(key),
                            TRI_VOC_DOCUMENT_OPERATION_REMOVE);
    Result r = mthd->Delete(RocksDBColumnFamily::documents(), iter->key());
    if (!r.ok()) {
      THROW_ARANGO_EXCEPTION(r);
    }
    // report size of key
    RocksDBOperationResult result =
        state->addOperation(cid, revisionId, TRI_VOC_DOCUMENT_OPERATION_REMOVE,
                            0, iter->key().size());

    // transaction size limit reached -- fail
    if (result.fail()) {
      THROW_ARANGO_EXCEPTION(result);
    }

    iter->Next();
  }

  // delete index items
  READ_LOCKER(guard, _indexesLock);
  for (std::shared_ptr<Index> const& index : _indexes) {
    RocksDBIndex* rindex = static_cast<RocksDBIndex*>(index.get());
    rindex->truncate(trx);
  }
  _needToPersistIndexEstimates = true;
}

DocumentIdentifierToken RocksDBCollection::lookupKey(transaction::Methods* trx,
                                                     VPackSlice const& key) {
  TRI_ASSERT(key.isString());
  return primaryIndex()->lookupKey(trx, StringRef(key));
}

int RocksDBCollection::read(transaction::Methods* trx,
                            arangodb::velocypack::Slice const key,
                            ManagedDocumentResult& result, bool) {
  TRI_ASSERT(key.isString());
  RocksDBToken token = primaryIndex()->lookupKey(trx, StringRef(key));

  if (token.revisionId()) {
    if (readDocument(trx, token, result)) {
      // found
      return TRI_ERROR_NO_ERROR;
    }
  }

  // not found
  return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
}

// read using a token!
bool RocksDBCollection::readDocument(transaction::Methods* trx,
                                     DocumentIdentifierToken const& token,
                                     ManagedDocumentResult& result) {
  // TODO: why do we have read(), readDocument() and lookupKey()?
  RocksDBToken const* tkn = static_cast<RocksDBToken const*>(&token);
  TRI_voc_rid_t revisionId = tkn->revisionId();
  if (revisionId != 0) {
    auto res = lookupRevisionVPack(revisionId, trx, result, true);
    return res.ok();
  }
  return false;
}

// read using a token, bypassing the cache
bool RocksDBCollection::readDocumentNoCache(
    transaction::Methods* trx, DocumentIdentifierToken const& token,
    ManagedDocumentResult& result) {
  // TODO: why do we have read(), readDocument() and lookupKey()?
  auto tkn = static_cast<RocksDBToken const*>(&token);
  TRI_voc_rid_t revisionId = tkn->revisionId();
  auto res = lookupRevisionVPack(revisionId, trx, result, false);
  return res.ok();
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

  RocksDBOperationResult res;
  bool const isEdgeCollection =
      (_logicalCollection->type() == TRI_COL_TYPE_EDGE);

  if (isEdgeCollection) {
    // _from:
    fromSlice = slice.get(StaticStrings::FromString);
    if (!fromSlice.isString()) {
      res.reset(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE);
      return res.errorNumber();
    }
    VPackValueLength len;
    char const* docId = fromSlice.getString(len);
    size_t split;
    if (!TRI_ValidateDocumentIdKeyGenerator(docId, static_cast<size_t>(len),
                                            &split)) {
      res.reset(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE);
      return res.errorNumber();
    }
    // _to:
    toSlice = slice.get(StaticStrings::ToString);
    if (!toSlice.isString()) {
      res.reset(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE);
      return res.errorNumber();
    }
    docId = toSlice.getString(len);
    if (!TRI_ValidateDocumentIdKeyGenerator(docId, static_cast<size_t>(len),
                                            &split)) {
      res.reset(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE);
      return res.errorNumber();
    }
  }

  transaction::BuilderLeaser builder(trx);
  res.reset(newObjectForInsert(trx, slice, fromSlice, toSlice, isEdgeCollection,
                               *builder.get(), options.isRestore));
  if (res.fail()) {
    return res.errorNumber();
  }
  VPackSlice newSlice = builder->slice();

  TRI_voc_rid_t revisionId =
      transaction::helpers::extractRevFromDocument(newSlice);

  RocksDBTransactionState* state = toRocksTransactionState(trx);

  RocksDBSavePoint guard(rocksutils::toRocksMethods(trx),
                         trx->isSingleOperationTransaction(),
                         [&state]() { state->resetLogState(); });

  state->prepareOperation(_logicalCollection->cid(), revisionId, StringRef(),
                          TRI_VOC_DOCUMENT_OPERATION_INSERT);

  res = insertDocument(trx, revisionId, newSlice, options.waitForSync);
  if (res.ok()) {
    Result lookupResult = lookupRevisionVPack(revisionId, trx, mdr, false);

    if (lookupResult.fail()) {
      return lookupResult.errorNumber();
    }

    // report document and key size
    RocksDBOperationResult result = state->addOperation(
        _logicalCollection->cid(), revisionId,
        TRI_VOC_DOCUMENT_OPERATION_INSERT, newSlice.byteSize(), res.keySize());

    // transaction size limit reached -- fail
    if (result.fail()) {
      THROW_ARANGO_EXCEPTION(result);
    }

    guard.commit();
  }

  return res.errorNumber();
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
  RocksDBOperationResult res = lookupDocument(trx, key, previous);

  if (res.fail()) {
    return res.errorNumber();
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
    int result = checkRevision(trx, expectedRev, prevRev);
    if (result != TRI_ERROR_NO_ERROR) {
      return result;
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
  RocksDBTransactionState* state = toRocksTransactionState(trx);
  if (state->isDBServer()) {
    // Need to check that no sharding keys have changed:
    if (arangodb::shardKeysChanged(_logicalCollection->dbName(),
                                   trx->resolver()->getCollectionNameCluster(
                                       _logicalCollection->planId()),
                                   oldDoc, builder->slice(), false)) {
      return TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES;
    }
  }

  VPackSlice const newDoc(builder->slice());

  RocksDBSavePoint guard(rocksutils::toRocksMethods(trx),
                         trx->isSingleOperationTransaction(),
                         [&state]() { state->resetLogState(); });

  // add possible log statement under guard
  state->prepareOperation(_logicalCollection->cid(), revisionId, StringRef(),
                          TRI_VOC_DOCUMENT_OPERATION_UPDATE);
  res = updateDocument(trx, oldRevisionId, oldDoc, revisionId, newDoc,
                       options.waitForSync);

  if (res.ok()) {
    mdr.setManaged(newDoc.begin(), revisionId);
    TRI_ASSERT(!mdr.empty());

    // report document and key size
    RocksDBOperationResult result = state->addOperation(
        _logicalCollection->cid(), revisionId,
        TRI_VOC_DOCUMENT_OPERATION_UPDATE, newDoc.byteSize(), res.keySize());

    // transaction size limit reached -- fail
    if (result.fail()) {
      THROW_ARANGO_EXCEPTION(result);
    }

    guard.commit();
  }

  return res.errorNumber();
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
  Result res = lookupDocument(trx, key, previous).errorNumber();

  if (res.fail()) {
    return res.errorNumber();
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

  RocksDBTransactionState* state = toRocksTransactionState(trx);
  if (state->isDBServer()) {
    // Need to check that no sharding keys have changed:
    if (arangodb::shardKeysChanged(_logicalCollection->dbName(),
                                   trx->resolver()->getCollectionNameCluster(
                                       _logicalCollection->planId()),
                                   oldDoc, builder->slice(), false)) {
      return TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES;
    }
  }

  RocksDBSavePoint guard(rocksutils::toRocksMethods(trx),
                         trx->isSingleOperationTransaction(),
                         [&state]() { state->resetLogState(); });

  // add possible log statement under guard
  state->prepareOperation(_logicalCollection->cid(), revisionId, StringRef(),
                          TRI_VOC_DOCUMENT_OPERATION_REPLACE);

  VPackSlice const newDoc(builder->slice());

  RocksDBOperationResult opResult = updateDocument(
      trx, oldRevisionId, oldDoc, revisionId, newDoc, options.waitForSync);
  if (opResult.ok()) {
    mdr.setManaged(newDoc.begin(), revisionId);
    TRI_ASSERT(!mdr.empty());

    // report document and key size
    RocksDBOperationResult result =
        state->addOperation(_logicalCollection->cid(), revisionId,
                            TRI_VOC_DOCUMENT_OPERATION_REPLACE,
                            newDoc.byteSize(), opResult.keySize());

    // transaction size limit reached -- fail
    if (result.fail()) {
      THROW_ARANGO_EXCEPTION(result);
    }

    guard.commit();
  }

  return opResult.errorNumber();
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
  RocksDBOperationResult res = lookupDocument(trx, key, previous);

  if (res.fail()) {
    return res.errorNumber();
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

  RocksDBTransactionState* state = toRocksTransactionState(trx);
  RocksDBSavePoint guard(rocksutils::toRocksMethods(trx),
                         trx->isSingleOperationTransaction(),
                         [&state]() { state->resetLogState(); });

  // add possible log statement under guard
  state->prepareOperation(_logicalCollection->cid(), revisionId, StringRef(key),
                          TRI_VOC_DOCUMENT_OPERATION_REMOVE);
  res = removeDocument(trx, oldRevisionId, oldDoc, false, options.waitForSync);
  if (res.ok()) {
    // report key size
    res = state->addOperation(_logicalCollection->cid(), revisionId,
                              TRI_VOC_DOCUMENT_OPERATION_REMOVE, 0,
                              res.keySize());
    // transaction size limit reached -- fail
    if (res.fail()) {
      THROW_ARANGO_EXCEPTION(res);
    }

    guard.commit();
  }

  return res.errorNumber();
}

void RocksDBCollection::deferDropCollection(
    std::function<bool(LogicalCollection*)> /*callback*/) {
  // nothing to do here
}

/// @brief return engine-specific figures
void RocksDBCollection::figuresSpecific(
    std::shared_ptr<arangodb::velocypack::Builder>& builder) {
  rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
  RocksDBKeyBounds bounds = RocksDBKeyBounds::CollectionDocuments(_objectId);
  rocksdb::Range r(bounds.start(), bounds.end());

  uint64_t out = 0;
  db->GetApproximateSizes(&r, 1, &out, true);

  builder->add("documentsSize", VPackValue(out));
}

/// @brief creates the initial indexes for the collection
void RocksDBCollection::createInitialIndexes() {
  // LOCKED from the outside
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
  // LOCKED from the outside
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
  if (idx->type() == Index::TRI_IDX_TYPE_GEO1_INDEX ||
      idx->type() == Index::TRI_IDX_TYPE_GEO2_INDEX) {
    _hasGeoIndex = true;
  }
}

void RocksDBCollection::addIndexCoordinator(
    std::shared_ptr<arangodb::Index> idx) {
  // LOCKED from the outside
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
  // LOCKED from the outside
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

/// non-transactional: fill index with existing documents
/// from this collection
arangodb::Result RocksDBCollection::fillIndexes(
    transaction::Methods* trx, std::shared_ptr<arangodb::Index> added) {
  // LOCKED from the outside, can't use lookupIndex
  RocksDBPrimaryIndex* primIndex = nullptr;
  for (std::shared_ptr<Index> idx : _indexes) {
    if (idx->type() == Index::TRI_IDX_TYPE_PRIMARY_INDEX) {
      primIndex = static_cast<RocksDBPrimaryIndex*>(idx.get());
      break;
    }
  }
  TRI_ASSERT(primIndex);

  ManagedDocumentResult mmdr;
  RocksDBIndex* ridx = static_cast<RocksDBIndex*>(added.get());
  RocksDBTransactionState* state = rocksutils::toRocksTransactionState(trx);
  std::unique_ptr<IndexIterator> it(new RocksDBAllIndexIterator(
      _logicalCollection, trx, &mmdr, primaryIndex(), false));

  rocksdb::TransactionDB* db = globalRocksDB();
  uint64_t numDocsWritten = 0;
  // write batch will be reset each 5000 documents
  rocksdb::WriteBatchWithIndex batch(db->DefaultColumnFamily()->GetComparator(),
                                     32 * 1024 * 1024);
  RocksDBBatchedMethods batched(state, &batch);

  int res = TRI_ERROR_NO_ERROR;
  auto cb = [&](ManagedDocumentResult const& mdr) {
    if (res == TRI_ERROR_NO_ERROR) {
      res = ridx->insertRaw(&batched, mdr.lastRevisionId(),
                            VPackSlice(mdr.vpack()));
      if (res == TRI_ERROR_NO_ERROR) {
        numDocsWritten++;
      }
    }
  };

  rocksdb::WriteOptions writeOpts;
  Result r;
  bool hasMore = true;
  while (hasMore) {
    hasMore = it->nextDocument(cb, 250);
    if (_logicalCollection->status() == TRI_VOC_COL_STATUS_DELETED ||
        _logicalCollection->deleted()) {
      res = TRI_ERROR_INTERNAL;
    }
    if (res != TRI_ERROR_NO_ERROR) {
      r = Result(res);
      break;
    }
    rocksdb::Status s = db->Write(writeOpts, batch.GetWriteBatch());
    if (!s.ok()) {
      r = rocksutils::convertStatus(s, rocksutils::StatusHint::index);
      break;
    }
    batch.Clear();
  }

  // we will need to remove index elements created before an error
  // occured, this needs to happen since we are non transactional
  if (!r.ok()) {
    it->reset();
    batch.Clear();

    res = TRI_ERROR_NO_ERROR;
    auto removeCb = [&](DocumentIdentifierToken token) {
      if (res == TRI_ERROR_NO_ERROR && numDocsWritten > 0 &&
          this->readDocument(trx, token, mmdr)) {
        // we need to remove already inserted documents up to numDocsWritten
        res = ridx->removeRaw(&batched, mmdr.lastRevisionId(),
                              VPackSlice(mmdr.vpack()));
        if (res == TRI_ERROR_NO_ERROR) {
          numDocsWritten--;
        }
      }
    };

    hasMore = true;
    while (hasMore && numDocsWritten > 0) {
      hasMore = it->next(removeCb, 5000);
    }
    // TODO: if this fails, do we have any recourse?
    // Simon: Don't think so
    rocksdb::WriteOptions writeOpts;
    db->Write(writeOpts, batch.GetWriteBatch());
  }
  if (numDocsWritten > 0) {
    _needToPersistIndexEstimates = true;
  }

  return r;
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
    READ_LOCKER(guard, _indexesLock);
    for (auto const& it : _indexes) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "- " << it.get();
    }
  }
#endif
  TRI_ASSERT(primary->type() == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX);
  // the primary index must be the index at position #0
  return static_cast<arangodb::RocksDBPrimaryIndex*>(primary.get());
}

RocksDBOperationResult RocksDBCollection::insertDocument(
    arangodb::transaction::Methods* trx, TRI_voc_rid_t revisionId,
    VPackSlice const& doc, bool& waitForSync) const {
  RocksDBOperationResult res;
  // Coordinator doesn't know index internals
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  TRI_ASSERT(trx->state()->isRunning());

  RocksDBKey key(RocksDBKey::Document(_objectId, revisionId));
  RocksDBValue value(RocksDBValue::Document(doc));

  blackListKey(key.string().data(), static_cast<uint32_t>(key.string().size()));

  RocksDBMethods* mthd = rocksutils::toRocksMethods(trx);
  res = mthd->Put(RocksDBColumnFamily::documents(), key, value.string());
  if (!res.ok()) {
    // set keysize that is passed up to the crud operations
    res.keySize(key.string().size());
    return res;
  }

  /*LOG_TOPIC(ERR, Logger::FIXME)
      << "PUT rev: " << revisionId << " trx: " << trx->state()->id()
      << " seq: " << mthd->readOptions().snapshot->GetSequenceNumber()
      << " objectID " << _objectId << " name: " << _logicalCollection->name();*/

  RocksDBOperationResult innerRes;
  READ_LOCKER(guard, _indexesLock);
  for (std::shared_ptr<Index> const& idx : _indexes) {
    innerRes.reset(idx->insert(trx, revisionId, doc, false));

    // in case of no-memory, return immediately
    if (innerRes.is(TRI_ERROR_OUT_OF_MEMORY)) {
      return innerRes;
    }

    if (innerRes.fail()) {
      // "prefer" unique constraint violated over other errors
      if (innerRes.is(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) ||
          res.ok()) {
        res = innerRes;
      }
    }
  }

  if (res.ok()) {
    if (_logicalCollection->waitForSync()) {
      waitForSync = true;  // output parameter (by ref)
    }

    if (waitForSync) {
      trx->state()->waitForSync(true);
    }
    _needToPersistIndexEstimates = true;
  }

  return res;
}

RocksDBOperationResult RocksDBCollection::removeDocument(
    arangodb::transaction::Methods* trx, TRI_voc_rid_t revisionId,
    VPackSlice const& doc, bool isUpdate, bool& waitForSync) const {
  // Coordinator doesn't know index internals
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  TRI_ASSERT(trx->state()->isRunning());
  TRI_ASSERT(_objectId != 0);

  auto key = RocksDBKey::Document(_objectId, revisionId);

  blackListKey(key.string().data(), static_cast<uint32_t>(key.string().size()));

  // prepare operation which adds log statements is called
  // from the outside. We do not need to DELETE a document from the
  // document store, if the doc is overwritten with PUT
  // Simon: actually we do, because otherwise the counter recovery is broken
  // if (!isUpdate) {
  RocksDBMethods* mthd = rocksutils::toRocksMethods(trx);
  RocksDBOperationResult res =
      mthd->Delete(RocksDBColumnFamily::documents(), key);
  if (!res.ok()) {
    return res;
  }
  //}

  /*LOG_TOPIC(ERR, Logger::FIXME)
      << "Delete rev: " << revisionId << " trx: " << trx->state()->id()
      << " seq: " << mthd->readOptions().snapshot->GetSequenceNumber()
      << " objectID " << _objectId << " name: " << _logicalCollection->name();*/

  RocksDBOperationResult resInner;
  READ_LOCKER(guard, _indexesLock);
  for (std::shared_ptr<Index> const& idx : _indexes) {
    int tmpres = idx->remove(trx, revisionId, doc, false);
    resInner.reset(tmpres);

    // in case of no-memory, return immediately
    if (resInner.is(TRI_ERROR_OUT_OF_MEMORY)) {
      return resInner;
    }

    // for other errors, set result
    if (resInner.fail()) {
      res = resInner;
    }
  }

  if (res.ok()) {
    if (_logicalCollection->waitForSync()) {
      waitForSync = true;
    }

    if (waitForSync) {
      trx->state()->waitForSync(true);
    }
    _needToPersistIndexEstimates = true;
  }

  return res;
}

/// @brief looks up a document by key, low level worker
/// the key must be a string slice, no revision check is performed
RocksDBOperationResult RocksDBCollection::lookupDocument(
    transaction::Methods* trx, VPackSlice key,
    ManagedDocumentResult& mdr) const {
  RocksDBOperationResult res;
  if (!key.isString()) {
    res.reset(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD);
    return res;
  }

  RocksDBToken token = primaryIndex()->lookupKey(trx, StringRef(key));
  TRI_voc_rid_t revisionId = token.revisionId();

  if (revisionId > 0) {
    res = lookupRevisionVPack(revisionId, trx, mdr, true);
  } else {
    res.reset(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
  }
  return res;
}

RocksDBOperationResult RocksDBCollection::updateDocument(
    transaction::Methods* trx, TRI_voc_rid_t oldRevisionId,
    VPackSlice const& oldDoc, TRI_voc_rid_t newRevisionId,
    VPackSlice const& newDoc, bool& waitForSync) const {
  // keysize in return value is set by insertDocument

  // Coordinator doesn't know index internals
  TRI_ASSERT(trx->state()->isRunning());
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  RocksDBOperationResult res =
      removeDocument(trx, oldRevisionId, oldDoc, true, waitForSync);
  if (res.fail()) {
    return res;
  }

  res = insertDocument(trx, newRevisionId, newDoc, waitForSync);
  return res;
}

Result RocksDBCollection::lookupDocumentToken(transaction::Methods* trx,
                                              arangodb::StringRef key,
                                              RocksDBToken& outToken) const {
  TRI_ASSERT(_objectId != 0);

  // TODO fix as soon as we got a real primary index
  outToken = primaryIndex()->lookupKey(trx, key);
  return outToken.revisionId() > 0
             ? Result()
             : Result(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
}

arangodb::Result RocksDBCollection::lookupRevisionVPack(
    TRI_voc_rid_t revisionId, transaction::Methods* trx,
    arangodb::ManagedDocumentResult& mdr, bool withCache) const {
  TRI_ASSERT(trx->state()->isRunning());
  TRI_ASSERT(_objectId != 0);

  auto key = RocksDBKey::Document(_objectId, revisionId);
  std::string value;

  if (withCache && useCache()) {
    TRI_ASSERT(_cache != nullptr);
    // check cache first for fast path
    auto f = _cache->find(key.string().data(),
                          static_cast<uint32_t>(key.string().size()));
    if (f.found()) {
      value.append(reinterpret_cast<char const*>(f.value()->value()),
                   static_cast<size_t>(f.value()->valueSize));
      mdr.setManaged(std::move(value), revisionId);
      return {TRI_ERROR_NO_ERROR};
    }
  }

  RocksDBMethods* mthd = rocksutils::toRocksMethods(trx);
  Result res = mthd->Get(RocksDBColumnFamily::documents(), key, &value);
  TRI_ASSERT(value.data());
  if (res.ok()) {
    if (withCache && useCache()) {
      TRI_ASSERT(_cache != nullptr);
      // write entry back to cache
      auto entry = cache::CachedValue::construct(
          key.string().data(), static_cast<uint32_t>(key.string().size()),
          value.data(), static_cast<uint64_t>(value.size()));
      bool cached = _cache->insert(entry);
      if (!cached) {
        delete entry;
      }
    }

    mdr.setManaged(std::move(value), revisionId);
  } else {
    LOG_TOPIC(ERR, Logger::FIXME)
        << "NOT FOUND rev: " << revisionId << " trx: " << trx->state()->id()
        << " seq: " << mthd->readOptions().snapshot->GetSequenceNumber()
        << " objectID " << _objectId << " name: " << _logicalCollection->name();
    mdr.reset();
  }
  return res;
}

void RocksDBCollection::setRevision(TRI_voc_rid_t revisionId) {
  _revisionId = revisionId;
}

void RocksDBCollection::adjustNumberDocuments(int64_t adjustment) {
  if (adjustment < 0) {
    _numberDocuments -= static_cast<uint64_t>(-adjustment);
  } else if (adjustment > 0) {
    _numberDocuments += static_cast<uint64_t>(adjustment);
  }
}

/// @brief write locks a collection, with a timeout
int RocksDBCollection::lockWrite(double timeout) {
  uint64_t waitTime = 0;  // indicates that time is uninitialized
  double startTime = 0.0;

  while (true) {
    TRY_WRITE_LOCKER(locker, _exclusiveLock);

    if (locker.isLocked()) {
      // keep lock and exit loop
      locker.steal();
      return TRI_ERROR_NO_ERROR;
    }

    double now = TRI_microtime();

    if (waitTime == 0) {  // initialize times
      // set end time for lock waiting
      if (timeout <= 0.0) {
        timeout = defaultLockTimeout;
      }
      startTime = now;
      waitTime = 1;
    }

    if (now > startTime + timeout) {
      LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
          << "timed out after " << timeout
          << " s waiting for write-lock on collection '"
          << _logicalCollection->name() << "'";
      return TRI_ERROR_LOCK_TIMEOUT;
    }

    if (now - startTime < 0.001) {
      std::this_thread::yield();
    } else {
      usleep(static_cast<TRI_usleep_t>(waitTime));
      if (waitTime < 32) {
        waitTime *= 2;
      }
    }
  }
}

/// @brief write unlocks a collection
int RocksDBCollection::unlockWrite() {
  _exclusiveLock.unlockWrite();

  return TRI_ERROR_NO_ERROR;
}

/// @brief read locks a collection, with a timeout
int RocksDBCollection::lockRead(double timeout) {
  uint64_t waitTime = 0;  // indicates that time is uninitialized
  double startTime = 0.0;

  while (true) {
    TRY_READ_LOCKER(locker, _exclusiveLock);

    if (locker.isLocked()) {
      // keep lock and exit loop
      locker.steal();
      return TRI_ERROR_NO_ERROR;
    }

    double now = TRI_microtime();

    if (waitTime == 0) {  // initialize times
      // set end time for lock waiting
      if (timeout <= 0.0) {
        timeout = defaultLockTimeout;
      }
      startTime = now;
      waitTime = 1;
    }

    if (now > startTime + timeout) {
      LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
          << "timed out after " << timeout
          << " s waiting for read-lock on collection '"
          << _logicalCollection->name() << "'";
      return TRI_ERROR_LOCK_TIMEOUT;
    }

    if (now - startTime < 0.001) {
      std::this_thread::yield();
    } else {
      usleep(static_cast<TRI_usleep_t>(waitTime));
      if (waitTime < 32) {
        waitTime *= 2;
      }
    }
  }
}

/// @brief read unlocks a collection
int RocksDBCollection::unlockRead() {
  _exclusiveLock.unlockRead();
  return TRI_ERROR_NO_ERROR;
}

// rescans the collection to update document count
uint64_t RocksDBCollection::recalculateCounts() {
  // start transaction to get a collection lock
  arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(
          _logicalCollection->vocbase()),
      _logicalCollection->cid(), AccessMode::Type::EXCLUSIVE);
  auto res = trx.begin();
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  rocksdb::ReadOptions readOptions;
  readOptions.fill_cache = false;

  // count documents
  auto documentBounds = RocksDBKeyBounds::CollectionDocuments(_objectId);
  _numberDocuments = rocksutils::countKeyRange(globalRocksDB(), readOptions,
                                               RocksDBColumnFamily::documents(),
                                               documentBounds);

  // update counter manager value
  res = globalRocksEngine()->counterManager()->setAbsoluteCounter(
      _objectId, _numberDocuments);
  if (res.ok()) {
    // in case of fail the counter has never been written and hence does not
    // need correction. The value is not changed and does not need to be synced
    globalRocksEngine()->counterManager()->sync(true);
  }
  trx.commit();

  return _numberDocuments;
}

void RocksDBCollection::compact() {
  rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
  rocksdb::CompactRangeOptions opts;
  RocksDBKeyBounds bounds = RocksDBKeyBounds::CollectionDocuments(_objectId);
  rocksdb::Slice b = bounds.start(), e = bounds.end();
  db->CompactRange(opts, &b, &e);

  READ_LOCKER(guard, _indexesLock);
  for (std::shared_ptr<Index> i : _indexes) {
    RocksDBIndex* index = static_cast<RocksDBIndex*>(i.get());
    index->cleanup();
  }
}

void RocksDBCollection::estimateSize(velocypack::Builder& builder) {
  TRI_ASSERT(!builder.isOpenObject() && !builder.isOpenArray());

  rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
  RocksDBKeyBounds bounds = RocksDBKeyBounds::CollectionDocuments(_objectId);
  rocksdb::Range r(bounds.start(), bounds.end());
  uint64_t out = 0, total = 0;
  db->GetApproximateSizes(&r, 1, &out, true);
  total += out;

  builder.openObject();
  builder.add("documents", VPackValue(out));
  builder.add("indexes", VPackValue(VPackValueType::Object));

  READ_LOCKER(guard, _indexesLock);
  for (std::shared_ptr<Index> i : _indexes) {
    RocksDBIndex* index = static_cast<RocksDBIndex*>(i.get());
    out = index->memory();
    builder.add(std::to_string(index->id()), VPackValue(out));
    total += out;
  }
  builder.close();
  builder.add("total", VPackValue(total));
  builder.close();
}

arangodb::Result RocksDBCollection::serializeIndexEstimates(
    rocksdb::Transaction* rtrx) const {
  if (!_needToPersistIndexEstimates) {
    return {TRI_ERROR_NO_ERROR};
  }
  _needToPersistIndexEstimates = false;
  std::string output;
  rocksdb::TransactionDB* tdb = rocksutils::globalRocksDB();
  for (auto index : getIndexes()) {
    output.clear();
    RocksDBIndex* cindex = static_cast<RocksDBIndex*>(index.get());
    TRI_ASSERT(cindex != nullptr);
    rocksutils::uint64ToPersistent(
        output, static_cast<uint64_t>(tdb->GetLatestSequenceNumber()));
    cindex->serializeEstimate(output);
    if (output.size() > sizeof(uint64_t)) {
      RocksDBKey key = RocksDBKey::IndexEstimateValue(cindex->objectId());
      rocksdb::Slice value(output);
      rocksdb::Status s = rtrx->Put(key.string(), value);

      if (!s.ok()) {
        LOG_TOPIC(WARN, Logger::ENGINES) << "writing index estimates failed";
        rtrx->Rollback();
        return rocksutils::convertStatus(s);
      }
    }
  }
  return {TRI_ERROR_NO_ERROR};
}

void RocksDBCollection::deserializeIndexEstimates(RocksDBCounterManager* mgr) {
  std::vector<std::shared_ptr<Index>> toRecalculate;
  for (auto const& it : getIndexes()) {
    auto idx = static_cast<RocksDBIndex*>(it.get());
    if (!idx->deserializeEstimate(mgr)) {
      toRecalculate.push_back(it);
    }
  }
  if (!toRecalculate.empty()) {
    recalculateIndexEstimates(toRecalculate);
  }
}

void RocksDBCollection::recalculateIndexEstimates() {
  auto idxs = getIndexes();
  recalculateIndexEstimates(idxs);
}

void RocksDBCollection::recalculateIndexEstimates(
    std::vector<std::shared_ptr<Index>>& indexes) {
  // start transaction to get a collection lock
  arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(
          _logicalCollection->vocbase()),
      _logicalCollection->cid(), AccessMode::Type::EXCLUSIVE);
  auto res = trx.begin();
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  for (auto const& it : indexes) {
    auto idx = static_cast<RocksDBIndex*>(it.get());
    idx->recalculateEstimates();
  }
  _needToPersistIndexEstimates = true;
  trx.commit();
}

void RocksDBCollection::createCache() const {
  if (!_useCache || _cachePresent) {
    // we leave this if we do not need the cache
    // or if cache already created
    return;
  }

  TRI_ASSERT(_useCache);
  TRI_ASSERT(_cache.get() == nullptr);
  TRI_ASSERT(CacheManagerFeature::MANAGER != nullptr);
  _cache = CacheManagerFeature::MANAGER->createCache(
      cache::CacheType::Transactional);
  _cachePresent = (_cache.get() != nullptr);
  TRI_ASSERT(_useCache);
}

void RocksDBCollection::disableCache() const {
  if (!_cachePresent) {
    return;
  }
  TRI_ASSERT(CacheManagerFeature::MANAGER != nullptr);
  // must have a cache...
  TRI_ASSERT(_useCache);
  TRI_ASSERT(_cachePresent);
  TRI_ASSERT(_cache.get() != nullptr);
  CacheManagerFeature::MANAGER->destroyCache(_cache);
  _cache.reset();
  _cachePresent = false;
  TRI_ASSERT(_useCache);
}

// blacklist given key from transactional cache
void RocksDBCollection::blackListKey(char const* data, std::size_t len) const {
  if (useCache()) {
    TRI_ASSERT(_cache != nullptr);
    bool blacklisted = false;
    uint64_t attempts = 0;
    while (!blacklisted) {
      blacklisted = _cache->blacklist(data, static_cast<uint32_t>(len));
      if (attempts++ % 10 == 0) {
        if (_cache->isShutdown()) {
          disableCache();
          break;
        }
      }
    }
  }
}
