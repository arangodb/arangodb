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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/PlanCache.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/hashes.h"
#include "Cache/CacheManagerFeature.h"
#include "Cache/Common.h"
#include "Cache/Manager.h"
#include "Cache/TransactionalCache.h"
#include "Cluster/ClusterMethods.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBBuilderIndex.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBComparator.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBIterators.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBPrimaryIndex.h"
#include "RocksDBEngine/RocksDBReplicationIterator.h"
#include "RocksDBEngine/RocksDBReplicationManager.h"
#include "RocksDBEngine/RocksDBSettingsManager.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Hints.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionGuard.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/Events.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/Identifiers/LocalDocumentId.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/ticks.h"
#include "VocBase/voc-types.h"

#include <rocksdb/utilities/transaction.h>
#include <rocksdb/utilities/transaction_db.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

namespace {
arangodb::LocalDocumentId generateDocumentId(arangodb::LogicalCollection const& collection,
                                             TRI_voc_rid_t revisionId) {
  bool useRev = collection.usesRevisionsAsDocumentIds();
  return useRev ? arangodb::LocalDocumentId::create(revisionId)
                : arangodb::LocalDocumentId::create();
}

template <typename F>
void reverseIdxOps(arangodb::PhysicalCollection::IndexContainerType const& indexes,
                   arangodb::PhysicalCollection::IndexContainerType::const_iterator& it,
                   F&& op) {
  while (it != indexes.begin()) {
    it--;
    auto* rIdx = static_cast<arangodb::RocksDBIndex*>(it->get());
    if (rIdx->needsReversal()) {
      if (std::forward<F>(op)(rIdx).fail()) {
        // best effort for reverse failed. Let`s trigger full rollback
        // or we will end up with inconsistent storage and indexes
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "Failed to reverse index operation.");
      }
    }
  }
}

using ObjectIdTransformer =
    std::function<std::pair<std::uint64_t, std::uint64_t>(std::uint64_t, std::uint64_t)>;

using IndicesMap =
    std::unordered_map<std::shared_ptr<arangodb::Index>, std::pair<std::uint64_t, std::uint64_t>>;

std::pair<std::uint64_t, std::uint64_t> injectNewTemporaryObjectId(std::uint64_t objectId,
                                                                   std::uint64_t tempObjectId) {
  return {objectId, TRI_NewTickServer()};
}

std::pair<std::uint64_t, std::uint64_t> swapObjectIds(std::uint64_t objectId,
                                                      std::uint64_t tempObjectId) {
  return {tempObjectId, objectId};
}

std::pair<std::uint64_t, std::uint64_t> clearTemporaryObjectId(std::uint64_t objectId,
                                                               std::uint64_t tempObjectId) {
  return {objectId, 0};
}

void handlePropertiesEntryForObjectIdUpdate(
    arangodb::LogicalCollection& collection, arangodb::velocypack::Builder& builder,
    ::ObjectIdTransformer idFunc, bool setUpgradedProperties, ::IndicesMap& indicesMap,
    arangodb::velocypack::Slice key, arangodb::velocypack::Slice value) {
  using arangodb::StaticStrings;
  using arangodb::basics::VelocyPackHelper;

  if (key.isEqualString("indexes")) {
    arangodb::velocypack::ArrayBuilder collectionIndicesArrayGuard(&builder, StaticStrings::Indexes);
    for (auto idxSlice : arangodb::velocypack::ArrayIterator(value)) {
      auto idx = collection.lookupIndex(idxSlice);
      if (!idx || idx->type() == arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK) {
        builder.add(idxSlice);
        continue;
      }
      arangodb::velocypack::ObjectBuilder indexObjectGuard(&builder);
      std::uint64_t objectIdIdx =
          VelocyPackHelper::stringUInt64(idxSlice, StaticStrings::ObjectId);
      std::uint64_t tempObjectIdIdx =
          VelocyPackHelper::stringUInt64(idxSlice, StaticStrings::TempObjectId);
      auto outputPairIdx = idFunc(objectIdIdx, tempObjectIdIdx);
      for (auto idxPair : arangodb::velocypack::ObjectIterator(idxSlice)) {
        if (idxPair.key.isEqualString(StaticStrings::ObjectId) ||
            idxPair.key.isEqualString(StaticStrings::TempObjectId)) {
          continue;
        }
        builder.add(idxPair.key);
        builder.add(idxPair.value);
      }
      builder.add(StaticStrings::ObjectId,
                  arangodb::velocypack::Value(std::to_string(outputPairIdx.first)));
      builder.add(StaticStrings::TempObjectId,
                  arangodb::velocypack::Value(std::to_string(outputPairIdx.second)));
      indicesMap.emplace(idx, outputPairIdx);
    }
    return;
  }

  if (key.isEqualString(StaticStrings::ObjectId) ||
      key.isEqualString(StaticStrings::TempObjectId)) {
    return;
  }

  if (setUpgradedProperties && key.isEqualString(StaticStrings::Version)) {
    builder.add(StaticStrings::Version,
                arangodb::velocypack::Value(static_cast<std::uint32_t>(
                    arangodb::LogicalCollection::Version::v37)));
  } else if (setUpgradedProperties && key.isEqualString(StaticStrings::SyncByRevision)) {
    builder.add(StaticStrings::SyncByRevision, arangodb::velocypack::Value(true));
  } else if (setUpgradedProperties &&
             key.isEqualString(StaticStrings::UsesRevisionsAsDocumentIds)) {
    builder.add(StaticStrings::UsesRevisionsAsDocumentIds,
                arangodb::velocypack::Value(true));
  } else {
    builder.add(key);
    builder.add(value);
  }
}

arangodb::Result setObjectIdsForCollection(arangodb::LogicalCollection& collection,
                                           std::pair<std::uint64_t, std::uint64_t> const& outputPair,
                                           ::IndicesMap const& indicesMap) {
  arangodb::Result res;

  auto rcoll = static_cast<arangodb::RocksDBMetaCollection*>(collection.getPhysical());
  TRI_ASSERT(rcoll);
  res = rcoll->setObjectIds(outputPair.first, outputPair.second);
  if (res.fail()) {
    return res;
  }
  for (auto const& idxPair : indicesMap) {
    auto ridx = static_cast<arangodb::RocksDBIndex*>(idxPair.first.get());
    res = ridx->setObjectIds(idxPair.second.first, idxPair.second.second);
    if (res.fail()) {
      return res;
    }
  }

  return res;
}

void setUpgradedPropertiesForCollection(arangodb::LogicalCollection& collection) {
  collection.setVersion(arangodb::LogicalCollection::Version::v37);
  collection.setUsesRevisionsAsDocumentIds(true);
  collection.setSyncByRevision(true);
}

arangodb::Result updateObjectIdsForCollection(rocksdb::DB& db,
                                              arangodb::LogicalCollection& collection,
                                              ::ObjectIdTransformer idFunc,
                                              bool setUpgradedProperties) {
  using arangodb::StaticStrings;
  using arangodb::basics::VelocyPackHelper;

  arangodb::Result res;
  rocksdb::WriteOptions wo;
  rocksdb::WriteBatch batch;

  // methods need lock
  arangodb::RocksDBKey key;  // read collection info from database
  key.constructCollection(collection.vocbase().id(), collection.id());
  rocksdb::PinnableSlice ps;
  rocksdb::Status s =
      db.Get(rocksdb::ReadOptions(),
             arangodb::RocksDBColumnFamily::definitions(), key.string(), &ps);
  if (!s.ok()) {
    res.reset(arangodb::rocksutils::convertStatus(s));
    return res;
  }
  arangodb::velocypack::Slice oldProps = arangodb::RocksDBValue::data(ps);

  arangodb::velocypack::Builder builder;
  std::pair<std::uint64_t, std::uint64_t> outputPair;
  ::IndicesMap indicesMap;
  {
    arangodb::velocypack::ObjectBuilder collectionObjectGuard(&builder);
    std::uint64_t objectId =
        VelocyPackHelper::stringUInt64(oldProps, StaticStrings::ObjectId);
    std::uint64_t tempObjectId =
        VelocyPackHelper::stringUInt64(oldProps, StaticStrings::TempObjectId);
    outputPair = idFunc(objectId, tempObjectId);
    for (auto pair : arangodb::velocypack::ObjectIterator(oldProps)) {
      ::handlePropertiesEntryForObjectIdUpdate(collection, builder, idFunc, setUpgradedProperties,
                                               indicesMap, pair.key, pair.value);
    }
    builder.add(StaticStrings::ObjectId,
                arangodb::velocypack::Value(std::to_string(outputPair.first)));
    builder.add(StaticStrings::TempObjectId,
                arangodb::velocypack::Value(std::to_string(outputPair.second)));
  }
  auto value = arangodb::RocksDBValue::Collection(builder.slice());
  batch.Put(arangodb::RocksDBColumnFamily::definitions(), key.string(), value.string());
  res = arangodb::rocksutils::convertStatus(db.Write(wo, &batch));
  if (res.fail()) {
    return res;
  }
  auto cleanup = arangodb::scopeGuard([oldProps, &db, &key]() -> void {
    rocksdb::WriteOptions wo;
    rocksdb::WriteBatch batch;
    auto value = arangodb::RocksDBValue::Collection(oldProps);
    batch.Put(arangodb::RocksDBColumnFamily::definitions(), key.string(), value.string());
    db.Write(wo, &batch);
  });

  res = ::setObjectIdsForCollection(collection, outputPair, indicesMap);
  if (res.fail()) {
    return res;
  }
  ::setUpgradedPropertiesForCollection(collection);

  cleanup.cancel();  // succeeded, no cleanup needed

  return res;
}

arangodb::Result commitBatch(rocksdb::WriteOptions& wo,
                             rocksdb::WriteBatch& batch, rocksdb::DB& db) {
  rocksdb::Status r = db.Write(wo, &batch);
  arangodb::Result res = arangodb::rocksutils::convertStatus(r);
  if (res.ok()) {
    batch.Clear();
  }
  return res;
}

void rewriteDocument(rocksdb::WriteBatch& batch, arangodb::RocksDBKey& key,
                     arangodb::RocksDBKeyBounds& bounds,
                     rocksdb::Slice oldValue, std::uint64_t tempObjectId) {
  arangodb::velocypack::Slice document = arangodb::RocksDBValue::data(oldValue);
  arangodb::velocypack::Builder builder;
  arangodb::velocypack::Slice revSlice = document.get(arangodb::StaticStrings::RevString);
  TRI_ASSERT(revSlice.isString());
  arangodb::velocypack::ValueLength l;
  char const* p = revSlice.getString(l);
  TRI_voc_rid_t revision = TRI_StringToRid(p, l, false);

  arangodb::LocalDocumentId newDocumentId{revision};

  key.constructDocument(tempObjectId, newDocumentId);
  batch.Put(bounds.columnFamily(), key.string(), oldValue);
}

arangodb::Result copyCollectionToNewObjectIdSpace(rocksdb::DB& db,
                                                  arangodb::LogicalCollection& collection) {
  arangodb::Result res;
  arangodb::RocksDBKey key;

  auto rcoll = static_cast<arangodb::RocksDBMetaCollection*>(collection.getPhysical());
  TRI_ASSERT(rcoll);
  std::uint64_t objectId = rcoll->objectId();
  std::uint64_t tempObjectId = rcoll->tempObjectId();

  rocksdb::ReadOptions ro;
  auto bounds = arangodb::RocksDBKeyBounds::CollectionDocuments(objectId);
  ro.prefix_same_as_start = true;
  auto iterateBound = bounds.end();
  ro.iterate_upper_bound = &iterateBound;

  std::unique_ptr<rocksdb::Iterator> iter{db.NewIterator(ro, bounds.columnFamily())};
  if (!iter) {
    res.reset(TRI_ERROR_INTERNAL, "could not acquire iterator");
    return res;
  }
  auto cmp = bounds.columnFamily()->GetComparator();

  rocksdb::WriteOptions wo;
  rocksdb::WriteBatch batch;
  for (iter->Seek(bounds.start());
       iter->Valid() && cmp->Compare(iter->key(), bounds.end()) <= 0; iter->Next()) {
    ::rewriteDocument(batch, key, bounds, iter->value(), tempObjectId);

    if (batch.Count() >= 5000) {
      res = commitBatch(wo, batch, db);
      if (res.fail()) {
        return res;
      }
    }
  }

  return commitBatch(wo, batch, db);
}

arangodb::LocalDocumentId extractDocumentIdFromIndexEntry(arangodb::RocksDBIndex& ridx,
                                                          rocksdb::Slice key,
                                                          rocksdb::Slice value) {
  arangodb::LocalDocumentId id = arangodb::LocalDocumentId::none();

  switch (ridx.type()) {
    case arangodb::RocksDBIndex::TRI_IDX_TYPE_PRIMARY_INDEX:
      id = arangodb::RocksDBValue::documentId(value);
      break;
    case arangodb::RocksDBIndex::TRI_IDX_TYPE_EDGE_INDEX:
      id = arangodb::RocksDBKey::edgeDocumentId(value);
      break;
    case arangodb::RocksDBIndex::TRI_IDX_TYPE_HASH_INDEX:
    case arangodb::RocksDBIndex::TRI_IDX_TYPE_SKIPLIST_INDEX:
    case arangodb::RocksDBIndex::TRI_IDX_TYPE_TTL_INDEX:
    case arangodb::RocksDBIndex::TRI_IDX_TYPE_PERSISTENT_INDEX:
      if (ridx.unique()) {
        id = arangodb::RocksDBValue::documentId(value);
        break;
      }
      id = arangodb::RocksDBKey::indexDocumentId(key);
      break;
    case arangodb::RocksDBIndex::TRI_IDX_TYPE_FULLTEXT_INDEX:
    case arangodb::RocksDBIndex::TRI_IDX_TYPE_GEO1_INDEX:
    case arangodb::RocksDBIndex::TRI_IDX_TYPE_GEO2_INDEX:
    case arangodb::RocksDBIndex::TRI_IDX_TYPE_GEO_INDEX:
      id = arangodb::RocksDBKey::indexDocumentId(key);
      break;
    case arangodb::RocksDBIndex::TRI_IDX_TYPE_IRESEARCH_LINK:
    case arangodb::RocksDBIndex::TRI_IDX_TYPE_UNKNOWN:
    default:
      break;
  }

  return id;
}

TRI_voc_rid_t getRevisionFromOldDocumentId(rocksdb::DB& db,
                                           arangodb::RocksDBMetaCollection& rcoll,
                                           arangodb::LocalDocumentId oldId) {
  arangodb::RocksDBKey key;
  key.constructDocument(rcoll.objectId(), oldId);
  rocksdb::PinnableSlice ps;
  rocksdb::Status s =
      db.Get(rocksdb::ReadOptions(), arangodb::RocksDBColumnFamily::documents(),
             key.string(), &ps);
  if (!s.ok()) {
    return TRI_voc_rid_t{0};
  }

  arangodb::velocypack::Slice doc = arangodb::RocksDBValue::data(ps);
  return arangodb::transaction::helpers::extractRevFromDocument(doc);
}

void rewritePrimaryIndexEntry(arangodb::RocksDBIndex& ridx, arangodb::RocksDBKey& key,
                              arangodb::RocksDBValue& buffer, rocksdb::Slice& value,
                              rocksdb::Slice oldKey, rocksdb::Slice oldValue,
                              arangodb::LocalDocumentId const& newId) {
  arangodb::velocypack::StringRef docKey = arangodb::RocksDBKey::primaryKey(oldKey);
  TRI_voc_rid_t revision = arangodb::RocksDBValue::revisionId(oldValue);
  key.constructPrimaryIndexValue(ridx.tempObjectId(), docKey);
  buffer = arangodb::RocksDBValue::PrimaryIndexValue(newId, revision);
  value = {buffer.string().data(), buffer.string().size()};
}

void rewriteEdgeIndexEntry(arangodb::RocksDBIndex& ridx, arangodb::RocksDBKey& key,
                           rocksdb::Slice oldKey, arangodb::LocalDocumentId const& newId) {
  arangodb::velocypack::StringRef vertexId = arangodb::RocksDBKey::vertexId(oldKey);
  key.constructEdgeIndexValue(ridx.tempObjectId(), vertexId, newId);
}

void rewriteVPackIndexEntry(arangodb::RocksDBIndex& ridx, arangodb::RocksDBKey& key,
                            arangodb::RocksDBValue& buffer, rocksdb::Slice& value,
                            rocksdb::Slice oldKey, arangodb::LocalDocumentId const& newId) {
  arangodb::velocypack::Slice indexedValues = arangodb::RocksDBKey::indexedVPack(oldKey);
  if (ridx.unique()) {
    key.constructUniqueVPackIndexValue(ridx.tempObjectId(), indexedValues);
    buffer = arangodb::RocksDBValue::UniqueVPackIndexValue(newId);
    value = {buffer.string().data(), buffer.string().size()};
    return;
  }
  key.constructVPackIndexValue(ridx.tempObjectId(), indexedValues, newId);
}

void rewriteFulltextIndexEntry(arangodb::RocksDBIndex& ridx,
                               arangodb::RocksDBKey& key, rocksdb::Slice oldKey,
                               arangodb::LocalDocumentId const& newId) {
  arangodb::velocypack::Slice indexedValues = arangodb::RocksDBKey::indexedVPack(oldKey);
  key.constructVPackIndexValue(ridx.tempObjectId(), indexedValues, newId);
}

void rewriteGeoIndexEntry(arangodb::RocksDBIndex& ridx, arangodb::RocksDBKey& key,
                          rocksdb::Slice oldKey, arangodb::LocalDocumentId const& newId) {
  uint64_t geoValue = arangodb::RocksDBKey::geoValue(oldKey);
  key.constructGeoIndexValue(ridx.tempObjectId(), geoValue, newId);
}

arangodb::Result rewriteIndexEntry(rocksdb::DB& db, rocksdb::WriteBatch& batch,
                                   arangodb::RocksDBMetaCollection& rcoll,
                                   arangodb::RocksDBIndex& ridx,
                                   rocksdb::Slice oldKey, rocksdb::Slice oldValue) {
  arangodb::LocalDocumentId oldId =
      ::extractDocumentIdFromIndexEntry(ridx, oldKey, oldValue);
  TRI_voc_rid_t revision = ::getRevisionFromOldDocumentId(db, rcoll, oldId);
  arangodb::LocalDocumentId newId = arangodb::LocalDocumentId::create(revision);
  if (!newId.isSet()) {
    return arangodb::Result(TRI_ERROR_INTERNAL,
                            "could not get revision for document " +
                                std::to_string(oldId.id()));
  }

  arangodb::Result res;
  arangodb::RocksDBKey key;
  arangodb::RocksDBValue buffer =
      arangodb::RocksDBValue::Empty(arangodb::RocksDBEntryType::Placeholder);
  rocksdb::Slice value = oldValue;

  switch (ridx.type()) {
    case arangodb::RocksDBIndex::TRI_IDX_TYPE_PRIMARY_INDEX:
      ::rewritePrimaryIndexEntry(ridx, key, buffer, value, oldKey, oldValue, newId);
      break;
    case arangodb::RocksDBIndex::TRI_IDX_TYPE_EDGE_INDEX:
      ::rewriteEdgeIndexEntry(ridx, key, oldKey, newId);
      break;
    case arangodb::RocksDBIndex::TRI_IDX_TYPE_HASH_INDEX:
    case arangodb::RocksDBIndex::TRI_IDX_TYPE_SKIPLIST_INDEX:
    case arangodb::RocksDBIndex::TRI_IDX_TYPE_TTL_INDEX:
    case arangodb::RocksDBIndex::TRI_IDX_TYPE_PERSISTENT_INDEX:
      ::rewriteVPackIndexEntry(ridx, key, buffer, value, oldKey, newId);
      break;
    case arangodb::RocksDBIndex::TRI_IDX_TYPE_FULLTEXT_INDEX:
      ::rewriteFulltextIndexEntry(ridx, key, oldKey, newId);
      break;
    case arangodb::RocksDBIndex::TRI_IDX_TYPE_GEO_INDEX:
      ::rewriteGeoIndexEntry(ridx, key, oldKey, newId);
      break;
    case arangodb::RocksDBIndex::TRI_IDX_TYPE_GEO1_INDEX:  // deprecated
    case arangodb::RocksDBIndex::TRI_IDX_TYPE_GEO2_INDEX:  // deprecated
    case arangodb::RocksDBIndex::TRI_IDX_TYPE_IRESEARCH_LINK:
    case arangodb::RocksDBIndex::TRI_IDX_TYPE_UNKNOWN:
    default:
      res.reset(TRI_ERROR_INTERNAL, "encountered unexpected index type");
      break;
  }

  batch.Put(ridx.columnFamily(), key.string(), value);
  return res;
}

arangodb::Result copyIndexToNewObjectIdSpace(rocksdb::DB& db,
                                             arangodb::LogicalCollection& collection,
                                             arangodb::Index& index) {
  arangodb::Result res;
  arangodb::RocksDBKey key;

  auto rcoll = static_cast<arangodb::RocksDBMetaCollection*>(collection.getPhysical());
  TRI_ASSERT(rcoll);
  auto& ridx = static_cast<arangodb::RocksDBIndex&>(index);

  rocksdb::ReadOptions ro;
  auto bounds = ridx.getBounds();
  ro.prefix_same_as_start = index.type() != arangodb::Index::TRI_IDX_TYPE_EDGE_INDEX;
  auto iterateBound = bounds.end();
  ro.iterate_upper_bound = &iterateBound;

  std::unique_ptr<rocksdb::Iterator> iter{db.NewIterator(ro, bounds.columnFamily())};
  if (!iter) {
    res.reset(TRI_ERROR_INTERNAL, "could not acquire iterator");
    return res;
  }
  auto cmp = bounds.columnFamily()->GetComparator();

  rocksdb::WriteOptions wo;
  rocksdb::WriteBatch batch;
  for (iter->Seek(bounds.start());
       iter->Valid() && cmp->Compare(iter->key(), bounds.end()) <= 0; iter->Next()) {
    res = ::rewriteIndexEntry(db, batch, *rcoll, ridx, iter->key(), iter->value());
    if (res.fail()) {
      return res;
    }

    if (batch.Count() >= 5000) {
      res = commitBatch(wo, batch, db);
      if (res.fail()) {
        return res;
      }
    }
  }

  return commitBatch(wo, batch, db);
}

arangodb::Result cleanupOldIdSpaces(rocksdb::DB& db, arangodb::RocksDBMetaCollection& rcoll) {
  arangodb::Result res;

  if (rcoll.tempObjectId() != 0) {
    auto bounds = arangodb::RocksDBKeyBounds::CollectionDocuments(rcoll.tempObjectId());
    res = arangodb::rocksutils::removeLargeRange(&db, bounds, true, true);
    if (res.fail()) {
      return res;
    }
  }

  std::vector<std::shared_ptr<arangodb::Index>> indices = rcoll.getIndexes();
  for (auto& idx : indices) {
    auto& ridx = static_cast<arangodb::RocksDBIndex&>(*idx);
    if (ridx.tempObjectId() != 0) {
      auto bounds = ridx.getBounds(ridx.tempObjectId());
      res = arangodb::rocksutils::removeLargeRange(&db, bounds, ridx.type() != arangodb::Index::TRI_IDX_TYPE_EDGE_INDEX, true);
      if (res.fail()) {
        return res;
      }
    }
  }

  return res;
}
}  // namespace

namespace arangodb {

RocksDBCollection::RocksDBCollection(LogicalCollection& collection,
                                     arangodb::velocypack::Slice const& info)
    : RocksDBMetaCollection(collection, info),
      _primaryIndex(nullptr),
      _cacheEnabled(
          !collection.system() &&
          basics::VelocyPackHelper::getBooleanValue(info, StaticStrings::CacheEnabled, false) &&
          CacheManagerFeature::MANAGER != nullptr),
      _numIndexCreations(0) {
  TRI_ASSERT(_logicalCollection.isAStub() || objectId() != 0);
  if (_cacheEnabled) {
    createCache();
  }
}

RocksDBCollection::RocksDBCollection(LogicalCollection& collection,
                                     PhysicalCollection const* physical)
    : RocksDBMetaCollection(collection, VPackSlice::emptyObjectSlice()),
      _primaryIndex(nullptr),
      _cacheEnabled(static_cast<RocksDBCollection const*>(physical)->_cacheEnabled &&
                    CacheManagerFeature::MANAGER != nullptr),
      _numIndexCreations(0) {
  if (_cacheEnabled) {
    createCache();
  }
}

RocksDBCollection::~RocksDBCollection() {
  if (useCache()) {
    try {
      destroyCache();
    } catch (...) {
    }
  }
}

Result RocksDBCollection::updateProperties(VPackSlice const& slice, bool doSync) {
  auto isSys = _logicalCollection.system();

  _cacheEnabled =
      !isSys &&
      basics::VelocyPackHelper::getBooleanValue(slice, StaticStrings::CacheEnabled, _cacheEnabled) &&
      CacheManagerFeature::MANAGER != nullptr;
  primaryIndex()->setCacheEnabled(_cacheEnabled);

  if (_cacheEnabled) {
    createCache();
    primaryIndex()->createCache();
  } else {
    // will do nothing if cache is not present
    destroyCache();
    primaryIndex()->destroyCache();
    TRI_ASSERT(_cache.get() == nullptr);
  }

  // nothing else to do
  return {};
}

PhysicalCollection* RocksDBCollection::clone(LogicalCollection& logical) const {
  return new RocksDBCollection(logical, this);
}

/// @brief export properties
void RocksDBCollection::getPropertiesVPack(velocypack::Builder& result) const {
  TRI_ASSERT(result.isOpenObject());
  result.add(StaticStrings::ObjectId, VPackValue(std::to_string(objectId())));
  result.add(StaticStrings::TempObjectId, VPackValue(std::to_string(tempObjectId())));
  result.add(StaticStrings::CacheEnabled, VPackValue(_cacheEnabled));
  TRI_ASSERT(result.isOpenObject());
}

/// @brief closes an open collection
int RocksDBCollection::close() {
  READ_LOCKER(guard, _indexesLock);
  for (auto it : _indexes) {
    it->unload();
  }
  return TRI_ERROR_NO_ERROR;
}

void RocksDBCollection::load() {
  if (_cacheEnabled) {
    createCache();
    if (_cache) {
      uint64_t numDocs = _meta.numberDocuments();
      if (numDocs > 0) {
        _cache->sizeHint(static_cast<uint64_t>(0.3 * numDocs));
      }
    }
  }
  READ_LOCKER(guard, _indexesLock);
  for (auto it : _indexes) {
    it->load();
  }
}

void RocksDBCollection::unload() {
  WRITE_LOCKER(guard, _exclusiveLock);
  if (useCache()) {
    destroyCache();
    TRI_ASSERT(_cache.get() == nullptr);
  }
  READ_LOCKER(indexGuard, _indexesLock);
  for (auto it : _indexes) {
    it->unload();
  }
}

/// return bounds for all documents
RocksDBKeyBounds RocksDBCollection::bounds() const  {
  return RocksDBKeyBounds::CollectionDocuments(objectId());
}

void RocksDBCollection::prepareIndexes(arangodb::velocypack::Slice indexesSlice) {
  TRI_ASSERT(indexesSlice.isArray());

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  std::vector<std::shared_ptr<Index>> indexes;
  {
    READ_LOCKER(guard, _indexesLock);  // link creation needs read-lock too
    if (indexesSlice.length() == 0 && _indexes.empty()) {
      engine->indexFactory().fillSystemIndexes(_logicalCollection, indexes);
    } else {
      engine->indexFactory().prepareIndexes(_logicalCollection, indexesSlice, indexes);
    }
  }

  WRITE_LOCKER(guard, _indexesLock);
  TRI_ASSERT(_indexes.empty());
  for (std::shared_ptr<Index>& idx : indexes) {
    TRI_ASSERT(idx != nullptr);
    auto const id = idx->id();
    for (auto const& it : _indexes) {
      TRI_ASSERT(it != nullptr);
      if (it->id() == id) {  // index is there twice
        idx.reset();
        break;
      }
    }

    if (idx) {
      TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(id.id()));
      _indexes.emplace(idx);
      if (idx->type() == Index::TRI_IDX_TYPE_PRIMARY_INDEX) {
        TRI_ASSERT(idx->id().isPrimary());
        _primaryIndex = static_cast<RocksDBPrimaryIndex*>(idx.get());
      }
    }
  }

  auto it = _indexes.cbegin();
  if ((*it)->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX ||
      (TRI_COL_TYPE_EDGE == _logicalCollection.type() &&
       (_indexes.size() < 3 ||
        ((*++it)->type() != Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX ||
         (*++it)->type() != Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX)))) {
    std::string msg =
        "got invalid indexes for collection '" + _logicalCollection.name() + "'";
    LOG_TOPIC("0ef34", ERR, arangodb::Logger::ENGINES) << msg;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    for (auto it : _indexes) {
      LOG_TOPIC("19e0b", ERR, arangodb::Logger::ENGINES) << "- " << it->context();
    }
#endif
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, msg);
  }

  TRI_ASSERT(!_indexes.empty());
}

std::shared_ptr<Index> RocksDBCollection::createIndex(VPackSlice const& info,
                                                      bool restore, bool& created) {
  TRI_ASSERT(info.isObject());

  // Step 0. Lock all the things
  TRI_vocbase_t& vocbase = _logicalCollection.vocbase();
  if (!vocbase.use()) { // someone dropped the database
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  std::unique_ptr<CollectionGuard> guard;
  try {
    guard = std::make_unique<CollectionGuard>(&vocbase, _logicalCollection.id());
  } catch (...) {
    vocbase.release();
    throw;
  }

  _numIndexCreations.fetch_add(1, std::memory_order_release);
  auto colGuard = scopeGuard([&] {
    _numIndexCreations.fetch_sub(1, std::memory_order_release);
    vocbase.release();
  });

  RocksDBBuilderIndex::Locker locker(this);
  if (!locker.lock()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_LOCK_TIMEOUT);
  }

  std::shared_ptr<Index> idx;
  {  // Step 1. Check for matching index
    READ_LOCKER(guard, _indexesLock);
    if ((idx = findIndex(info, _indexes)) != nullptr) {
      // We already have this index.
      if (idx->type() == arangodb::Index::TRI_IDX_TYPE_TTL_INDEX) {
        // special handling for TTL indexes
        // if there is exactly the same index present, we return it
        if (idx->matchesDefinition(info)) {
          created = false;
          return idx;
        }
        // if there is another TTL index already, we make things abort here
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_BAD_PARAMETER,
            "there can only be one ttl index per collection");
      }

      created = false;
      return idx;
    }
  }

  RocksDBEngine* engine = static_cast<RocksDBEngine*>(EngineSelectorFeature::ENGINE);

  // Step 2. We are sure that we do not have an index of this type.
  // We also hold the lock. Create it
  bool const generateKey = !restore;
  try {
    idx = engine->indexFactory().prepareIndexFromSlice(info, generateKey,
                                                       _logicalCollection, false);
  } catch (std::exception const& ex) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_INDEX_CREATION_FAILED, ex.what());
  }

  // we cannot persist primary or edge indexes
  TRI_ASSERT(idx->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX);
  TRI_ASSERT(idx->type() != Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX);

  {
    READ_LOCKER(guard, _indexesLock);
    for (auto const& other : _indexes) {  // conflicting index exists
      if (other->id() == idx->id() || other->name() == idx->name()) {
        // definition shares an identifier with an existing index with a
        // different definition
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
        VPackBuilder builder;
        other->toVelocyPack(builder, static_cast<std::underlying_type<Index::Serialize>::type>(
                                         Index::Serialize::Basics));
        LOG_TOPIC("29d1c", WARN, Logger::ENGINES)
            << "attempted to create index '" << info.toJson()
            << "' but found conflicting index '" << builder.slice().toJson() << "'";
#endif
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER,
                                       "duplicate value for `" + StaticStrings::IndexId +
                                           "` or `" + StaticStrings::IndexName + "`");
      }
    }
  }

  Result res;

  do {

    // Step 3. add index to collection entry (for removal after a crash)
    auto buildIdx =
    std::make_shared<RocksDBBuilderIndex>(std::static_pointer_cast<RocksDBIndex>(idx));
    if (!engine->inRecovery()) {  // manually modify collection entry, other
      // methods need lock
      RocksDBKey key;             // read collection info from database
      key.constructCollection(_logicalCollection.vocbase().id(),
                              _logicalCollection.id());
      rocksdb::PinnableSlice ps;
      rocksdb::Status s = engine->db()->Get(rocksdb::ReadOptions(),
                                            RocksDBColumnFamily::definitions(),
                                            key.string(), &ps);
      if (!s.ok()) {
        res.reset(rocksutils::convertStatus(s));
        break;
      }

      VPackBuilder builder;
      builder.openObject();
      for (auto pair : VPackObjectIterator(RocksDBValue::data(ps))) {
        if (pair.key.isEqualString("indexes")) {  // append new index
          VPackArrayBuilder arrGuard(&builder, "indexes");
          builder.add(VPackArrayIterator(pair.value));
          buildIdx->toVelocyPack(builder, Index::makeFlags(Index::Serialize::Internals));
          continue;
        }
        builder.add(pair.key);
        builder.add(pair.value);
      }
      builder.close();
      res = engine->writeCreateCollectionMarker(_logicalCollection.vocbase().id(),
                                                _logicalCollection.id(), builder.slice(),
                                                RocksDBLogValue::Empty());
      if (res.fail()) {
        break;
      }
    }

    // Step 4. fill index
    bool const inBackground =
        basics::VelocyPackHelper::getBooleanValue(info, StaticStrings::IndexInBackground, false);
    if (inBackground) {  // allow concurrent inserts into index
      {
        WRITE_LOCKER(guard, _indexesLock);
        _indexes.emplace(buildIdx);
      }
      res = buildIdx->fillIndexBackground(locker);
    } else {
      res = buildIdx->fillIndexForeground();
    }
    if (res.fail()) {
      break;
    }
    locker.lock(); // always lock to avoid inconsistencies

    // Step 5. register in index list
    WRITE_LOCKER(guard, _indexesLock);
    if (inBackground) {  // swap in actual index
      for (auto& it : _indexes) {
        if (it->id() == buildIdx->id()) {
          _indexes.erase(it);
          _indexes.emplace(idx);
          break;
        }
      }
    } else {
      _indexes.emplace(idx);
    }
    guard.unlock();
#if USE_PLAN_CACHE
    arangodb::aql::PlanCache::instance()->invalidate(_logicalCollection.vocbase());
#endif

    // inBackground index might not recover selectivity estimate w/o sync
    if (inBackground && !idx->unique() && idx->hasSelectivityEstimate()) {
      engine->settingsManager()->sync(false);
    }

    // Step 6. persist in rocksdb
    if (!engine->inRecovery()) {  // write new collection marker
      auto builder = _logicalCollection.toVelocyPackIgnore(
          {"path", "statusString"},
          LogicalDataSource::Serialization::PersistenceWithInProgress);
      VPackBuilder indexInfo;
      idx->toVelocyPack(indexInfo, Index::makeFlags(Index::Serialize::Internals));
      res = engine->writeCreateCollectionMarker(_logicalCollection.vocbase().id(),
                                                _logicalCollection.id(), builder.slice(),
                                                RocksDBLogValue::IndexCreate(_logicalCollection.vocbase().id(),
                                                                             _logicalCollection.id(), indexInfo.slice()));
    }
  } while(false);

  // cleanup routine
  if (res.fail()) { // We could not create the index. Better abort
    WRITE_LOCKER(guard, _indexesLock);
    auto it = _indexes.begin();
    while (it != _indexes.end()) {
      if ((*it)->id() == idx->id()) {
        _indexes.erase(it);
        break;
      }
      it++;
    }
    guard.unlock();
    idx->drop();
    THROW_ARANGO_EXCEPTION(res);
  }

  created = true;
  return idx;
}

/// @brief Drop an index with the given iid.
bool RocksDBCollection::dropIndex(IndexId iid) {
  // usually always called when _exclusiveLock is held
  if (iid.empty() || iid.isPrimary()) {
    return true;
  }

  std::shared_ptr<arangodb::Index> toRemove;
  {
    WRITE_LOCKER(guard, _indexesLock);
    for (auto& it : _indexes) {
      if (iid == it->id()) {
        toRemove = it;
        _indexes.erase(it);
        break;
      }
    }
  }

  if (!toRemove) {  // index not found
    // We tried to remove an index that does not exist
    events::DropIndex(_logicalCollection.vocbase().name(), _logicalCollection.name(),
                      std::to_string(iid.id()), TRI_ERROR_ARANGO_INDEX_NOT_FOUND);
    return false;
  }

  READ_LOCKER(guard, _indexesLock);

  RocksDBIndex* cindex = static_cast<RocksDBIndex*>(toRemove.get());
  TRI_ASSERT(cindex != nullptr);

  Result res = cindex->drop();

  if (!res.ok()) {
    return false;
  }

  events::DropIndex(_logicalCollection.vocbase().name(), _logicalCollection.name(),
                    std::to_string(iid.id()), TRI_ERROR_NO_ERROR);

  cindex->compact(); // trigger compaction before deleting the object

  auto* engine = static_cast<RocksDBEngine*>(EngineSelectorFeature::ENGINE);

  if (!engine || engine->inRecovery()) {
    return true; // skip writing WAL marker if inRecovery()
  }

  auto builder =  // RocksDB path
      _logicalCollection.toVelocyPackIgnore(
          {"path", "statusString"},
          LogicalDataSource::Serialization::PersistenceWithInProgress);

  // log this event in the WAL and in the collection meta-data
  res = engine->writeCreateCollectionMarker( // write marker
    _logicalCollection.vocbase().id(), // vocbase id
    _logicalCollection.id(), // collection id
    builder.slice(), // RocksDB path
    RocksDBLogValue::IndexDrop( // marker
      _logicalCollection.vocbase().id(), _logicalCollection.id(), iid // args
    )
  );

  return res.ok();
}

std::unique_ptr<IndexIterator> RocksDBCollection::getAllIterator(transaction::Methods* trx) const {
  return std::make_unique<RocksDBAllIndexIterator>(&_logicalCollection, trx);
}

std::unique_ptr<IndexIterator> RocksDBCollection::getAnyIterator(transaction::Methods* trx) const {
  return std::make_unique<RocksDBAnyIndexIterator>(&_logicalCollection, trx);
}

std::unique_ptr<ReplicationIterator> RocksDBCollection::getReplicationIterator(
    ReplicationIterator::Ordering order, uint64_t batchId) {
  if (order != ReplicationIterator::Ordering::Revision) {
    // not supported
    return nullptr;
  }

  EngineSelectorFeature& selector =
      _logicalCollection.vocbase().server().getFeature<EngineSelectorFeature>();
  RocksDBEngine& engine = selector.engine<RocksDBEngine>();
  RocksDBReplicationManager* manager = engine.replicationManager();
  RocksDBReplicationContext* ctx = batchId == 0 ? nullptr : manager->find(batchId);
  auto guard = scopeGuard([manager, ctx]() -> void {
    if (ctx) {
      manager->release(ctx);
    }
  });
  rocksdb::Snapshot const* snapshot = ctx ? ctx->snapshot() : nullptr;

  return std::make_unique<RocksDBRevisionReplicationIterator>(_logicalCollection, snapshot);
}

std::unique_ptr<ReplicationIterator> RocksDBCollection::getReplicationIterator(
    ReplicationIterator::Ordering order, transaction::Methods& trx) {
  if (order != ReplicationIterator::Ordering::Revision) {
    // not supported
    return nullptr;
  }

  return std::make_unique<RocksDBRevisionReplicationIterator>(_logicalCollection, trx);
}

////////////////////////////////////
// -- SECTION DML Operations --
///////////////////////////////////

Result RocksDBCollection::truncate(transaction::Methods& trx, OperationOptions& options) {
  TRI_ASSERT(objectId() != 0);
  auto state = RocksDBTransactionState::toState(&trx);
  RocksDBMethods* mthds = state->rocksdbMethods();

  if (state->isOnlyExclusiveTransaction() &&
      state->hasHint(transaction::Hints::Hint::ALLOW_RANGE_DELETE) &&
      this->canUseRangeDeleteInWal() && _meta.numberDocuments() >= 32 * 1024) {
    // non-transactional truncate optimization. We perform a bunch of
    // range deletes and circumvent the normal rocksdb::Transaction.
    // no savepoint needed here
    TRI_ASSERT(!state->hasOperations());  // not allowed

    TRI_IF_FAILURE("RocksDBRemoveLargeRangeOn") {
      return Result(TRI_ERROR_DEBUG);
    }

    RocksDBEngine* engine = rocksutils::globalRocksEngine();
    rocksdb::DB* db = engine->db()->GetRootDB();

    TRI_IF_FAILURE("RocksDBCollection::truncate::forceSync") {
      engine->settingsManager()->sync(false);
    }

    // pre commit sequence needed to place a blocker
    rocksdb::SequenceNumber seq = rocksutils::latestSequenceNumber();
    auto guard = scopeGuard([&] {  // remove blocker afterwards
      _meta.removeBlocker(state->id());
    });
    _meta.placeBlocker(state->id(), seq);

    rocksdb::WriteBatch batch;
    // delete documents
    RocksDBKeyBounds bounds = RocksDBKeyBounds::CollectionDocuments(objectId());
    rocksdb::Status s =
    batch.DeleteRange(bounds.columnFamily(), bounds.start(), bounds.end());
    if (!s.ok()) {
      return rocksutils::convertStatus(s);
    }

    // delete indexes, place estimator blockers
    {
      READ_LOCKER(idxGuard, _indexesLock);
      for (std::shared_ptr<Index> const& idx : _indexes) {
        RocksDBIndex* ridx = static_cast<RocksDBIndex*>(idx.get());
        bounds = ridx->getBounds();
        s = batch.DeleteRange(bounds.columnFamily(), bounds.start(), bounds.end());
        if (!s.ok()) {
          return rocksutils::convertStatus(s);
        }
      }
    }

    // add the log entry so we can recover the correct count
    auto log = RocksDBLogValue::CollectionTruncate(trx.vocbase().id(),
                                                   _logicalCollection.id(), objectId());

    s = batch.PutLogData(log.slice());

    if (!s.ok()) {
      return rocksutils::convertStatus(s);
    }

    rocksdb::WriteOptions wo;

    s = db->Write(wo, &batch);

    if (!s.ok()) {
      return rocksutils::convertStatus(s);
    }

    seq = db->GetLatestSequenceNumber() - 1;  // post commit sequence

    uint64_t numDocs = _meta.numberDocuments();
    _meta.adjustNumberDocuments(seq, /*revision*/ newRevisionId(),
                                -static_cast<int64_t>(numDocs));

    {
      READ_LOCKER(idxGuard, _indexesLock);
      for (std::shared_ptr<Index> const& idx : _indexes) {
        idx->afterTruncate(seq, &trx);  // clears caches / clears links (if applicable)
      }
    }
    bufferTruncate(seq);

    guard.fire();  // remove blocker

    TRI_ASSERT(!state->hasOperations());  // not allowed
    return Result{};
  }

  TRI_IF_FAILURE("RocksDBRemoveLargeRangeOff") {
    return Result(TRI_ERROR_DEBUG);
  }

  // normal transactional truncate
  RocksDBKeyBounds documentBounds = RocksDBKeyBounds::CollectionDocuments(objectId());
  rocksdb::Comparator const* cmp = RocksDBColumnFamily::documents()->GetComparator();
  // intentionally copy the read options so we can modify them
  rocksdb::ReadOptions ro = mthds->iteratorReadOptions();
  rocksdb::Slice const end = documentBounds.end();
  ro.iterate_upper_bound = &end;
  // we are going to blow away all data anyway. no need to blow up the cache
  ro.fill_cache = false;

  TRI_ASSERT(ro.snapshot);

  // avoid OOM error for truncate by committing earlier
  uint64_t const prvICC = state->options().intermediateCommitCount;
  state->options().intermediateCommitCount = std::min<uint64_t>(prvICC, 10000);

  uint64_t found = 0;
  VPackBuilder docBuffer;
  auto iter = mthds->NewIterator(ro, documentBounds.columnFamily());
  for (iter->Seek(documentBounds.start());
       iter->Valid() && cmp->Compare(iter->key(), end) < 0;
       iter->Next()) {

    ++found;
    TRI_ASSERT(objectId() == RocksDBKey::objectId(iter->key()));
    VPackSlice document(reinterpret_cast<uint8_t const*>(iter->value().data()));
    TRI_ASSERT(document.isObject());

    // tmp may contain a pointer into rocksdb::WriteBuffer::_rep. This is
    // a 'std::string' which might be realloc'ed on any Put/Delete operation
    docBuffer.clear();
    docBuffer.add(document);

    // To print the WAL we need key and RID
    VPackSlice key;
    TRI_voc_rid_t rid = 0;
    transaction::helpers::extractKeyAndRevFromDocument(document, key, rid);
    TRI_ASSERT(key.isString());
    TRI_ASSERT(rid != 0);

    RocksDBSavePoint guard(&trx, TRI_VOC_DOCUMENT_OPERATION_REMOVE);
    state->prepareOperation(_logicalCollection.id(),
                            rid,  // actual revision ID!!
                            TRI_VOC_DOCUMENT_OPERATION_REMOVE);

    LocalDocumentId const docId = RocksDBKey::documentId(iter->key());
    auto res = removeDocument(&trx, docId, docBuffer.slice(), options);

    if (res.fail()) {  // Failed to remove document in truncate.
      return res;
    }

    bool hasPerformedIntermediateCommit = false;
    res = state->addOperation(_logicalCollection.id(), docId.id(), TRI_VOC_DOCUMENT_OPERATION_REMOVE,
                              hasPerformedIntermediateCommit);

    if (res.fail()) {  // This should never happen...
      return res;
    }
    guard.finish(hasPerformedIntermediateCommit);

    trackWaitForSync(&trx, options);

  }

  // reset to previous value after truncate is finished
  state->options().intermediateCommitCount = prvICC;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  if (state->numCommits() == 0) {
    // check IN TRANSACTION if documents have been deleted
    if (mthds->countInBounds(RocksDBKeyBounds::CollectionDocuments(objectId()), true)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "deletion check in collection truncate "
                                     "failed - not all documents have been "
                                     "deleted");
    }
  }
#endif

  TRI_IF_FAILURE("FailAfterAllCommits") { return Result(TRI_ERROR_DEBUG); }
  TRI_IF_FAILURE("SegfaultAfterAllCommits") {
    TRI_TerminateDebugging("SegfaultAfterAllCommits");
  }
  return Result{};
}

Result RocksDBCollection::lookupKey(transaction::Methods* trx,
                                    VPackStringRef key,
                                    std::pair<LocalDocumentId, TRI_voc_rid_t>& result) const {
  result.first = LocalDocumentId::none();
  result.second = 0;
  
  // lookup the revision id in the primary index
  if (!primaryIndex()->lookupRevision(trx, key, result.first, result.second)) {
    // document not found
    TRI_ASSERT(!result.first.isSet());
    TRI_ASSERT(result.second == 0);
    return Result(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
  }

  // document found, but revisionId may not have been present in the primary
  // index. this can happen for "older" collections
  TRI_ASSERT(result.first.isSet());
  TRI_ASSERT(result.second != 0);
  return Result();
}

bool RocksDBCollection::lookupRevision(transaction::Methods* trx, VPackSlice const& key,
                                       TRI_voc_rid_t& revisionId) const {
  TRI_ASSERT(key.isString());
  LocalDocumentId documentId;
  revisionId = 0;
  // lookup the revision id in the primary index
  if (!primaryIndex()->lookupRevision(trx, arangodb::velocypack::StringRef(key),
                                      documentId, revisionId)) {
    // document not found
    TRI_ASSERT(revisionId == 0);
    return false;
  }

  // document found, and we have a valid revisionId
  TRI_ASSERT(documentId.isSet());
  TRI_ASSERT(revisionId != 0);
  return true;
}

Result RocksDBCollection::read(transaction::Methods* trx,
                               arangodb::velocypack::StringRef const& key,
                               ManagedDocumentResult& result) {
  Result res;
  do {
    LocalDocumentId const documentId = primaryIndex()->lookupKey(trx, key);
    if (!documentId.isSet()) {
      res.reset(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
      break;
    }  // else found

    std::string* buffer = result.setManaged();
    rocksdb::PinnableSlice ps(buffer);
    res = lookupDocumentVPack(trx, documentId, ps, /*readCache*/true, /*fillCache*/true);
    if (res.ok()) {
      if (ps.IsPinned()) {
        buffer->assign(ps.data(), ps.size());
      } // else value is already assigned
      result.setRevisionId(); // extracts id from buffer
    }
  } while (res.is(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) &&
           RocksDBTransactionState::toState(trx)->setSnapshotOnReadOnly());
  return res;
}

// read using a token!
bool RocksDBCollection::readDocument(transaction::Methods* trx,
                                     LocalDocumentId const& documentId,
                                     ManagedDocumentResult& result) const {
  if (documentId.isSet()) {
    std::string* buffer = result.setManaged();
    rocksdb::PinnableSlice ps(buffer);
    Result res = lookupDocumentVPack(trx, documentId, ps, /*readCache*/true, /*fillCache*/true);
    if (res.ok()) {
      if (ps.IsPinned()) {
        buffer->assign(ps.data(), ps.size());
      } // else value is already assigned
      return true;
    }
  }
  return false;
}

// read using a token!
bool RocksDBCollection::readDocumentWithCallback(transaction::Methods* trx,
                                                 LocalDocumentId const& documentId,
                                                 IndexIterator::DocumentCallback const& cb) const {
  if (documentId.isSet()) {
    return lookupDocumentVPack(trx, documentId, cb, /*withCache*/true);
  }
  return false;
}

Result RocksDBCollection::insert(arangodb::transaction::Methods* trx,
                                 arangodb::velocypack::Slice const slice,
                                 arangodb::ManagedDocumentResult& resultMdr,
                                 OperationOptions& options) {
  bool const isEdgeCollection = (TRI_COL_TYPE_EDGE == _logicalCollection.type());

  transaction::BuilderLeaser builder(trx);
  TRI_voc_tick_t revisionId;
  Result res(newObjectForInsert(trx, slice, isEdgeCollection, *builder.get(),
                                options.isRestore, revisionId));
  if (res.fail()) {
    return res;
  }

  VPackSlice newSlice = builder->slice();

  if (options.validate && 
      !options.isRestore && 
      options.isSynchronousReplicationFrom.empty()) {
    // only do schema validation when we are not restoring/replicating
    res = _logicalCollection.validate(newSlice, trx->transactionContextPtr()->getVPackOptions());

    if (res.fail()) {
      return res;
    }
  }
    
  
  int r = transaction::Methods::validateSmartJoinAttribute(_logicalCollection, newSlice);

  if (r != TRI_ERROR_NO_ERROR) {
    res.reset(r);
    return res;
  }
        

  LocalDocumentId const documentId = ::generateDocumentId(_logicalCollection, revisionId);

  RocksDBSavePoint guard(trx, TRI_VOC_DOCUMENT_OPERATION_INSERT);

  auto* state = RocksDBTransactionState::toState(trx);
  state->prepareOperation(_logicalCollection.id(), revisionId, TRI_VOC_DOCUMENT_OPERATION_INSERT);

  res = insertDocument(trx, documentId, newSlice, options);

  if (res.ok()) {
    trackWaitForSync(trx, options);

    if (options.returnNew) {
      resultMdr.setManaged(newSlice.begin());
      TRI_ASSERT(resultMdr.revisionId() == revisionId);
    } else if (!options.silent) {  //  need to pass revId manually
      transaction::BuilderLeaser keyBuilder(trx);
      keyBuilder->openObject(/*unindexed*/true);
      keyBuilder->add(StaticStrings::KeyString, transaction::helpers::extractKeyFromDocument(newSlice));
      keyBuilder->close();
      resultMdr.setManaged()->assign(reinterpret_cast<char const*>(keyBuilder->start()),
                                     keyBuilder->size());
      resultMdr.setRevisionId(revisionId);
    }

    bool hasPerformedIntermediateCommit = false;
    res = state->addOperation(_logicalCollection.id(), revisionId,
                              TRI_VOC_DOCUMENT_OPERATION_INSERT,
                              hasPerformedIntermediateCommit);

    guard.finish(hasPerformedIntermediateCommit);
  }

  return res;
}

Result RocksDBCollection::update(transaction::Methods* trx,
                                 velocypack::Slice newSlice,
                                 ManagedDocumentResult& resultMdr, OperationOptions& options,
                                 ManagedDocumentResult& previousMdr) {

  VPackSlice keySlice = newSlice.get(StaticStrings::KeyString);
  if (keySlice.isNone()) {
    return TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
  } else if (!keySlice.isString()) {
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
  }

  auto const oldDocumentId = primaryIndex()->lookupKey(trx, VPackStringRef(keySlice));
  if (!oldDocumentId.isSet()) {
    return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }
  std::string* prevBuffer = previousMdr.setManaged();
  // uses either prevBuffer or avoids memcpy (if read hits block cache)
  rocksdb::PinnableSlice previousPS(prevBuffer);
  Result res = lookupDocumentVPack(trx, oldDocumentId, previousPS,
                                   /*readCache*/true, /*fillCache*/false);
  if (res.fail()) {
    return res;
  }

  TRI_ASSERT(previousPS.size() > 0);
  VPackSlice const oldDoc(reinterpret_cast<uint8_t const*>(previousPS.data()));
  previousMdr.setRevisionId(transaction::helpers::extractRevFromDocument(oldDoc));
  TRI_ASSERT(previousMdr.revisionId() != 0);

  if (!options.ignoreRevs) {  // Check old revision:
    TRI_voc_rid_t expectedRev = TRI_ExtractRevisionId(newSlice);
    int result = checkRevision(trx, expectedRev, previousMdr.revisionId());
    if (result != TRI_ERROR_NO_ERROR) {
      return res.reset(result);
    }
  }

  if (newSlice.length() <= 1) {  // TODO move above ?!
    // shortcut. no need to do anything
    resultMdr.setManaged(oldDoc.begin());
    TRI_ASSERT(!resultMdr.empty());

    trackWaitForSync(trx, options);
    return res;
  }

  // merge old and new values
  TRI_voc_rid_t revisionId;
  auto isEdgeCollection = (TRI_COL_TYPE_EDGE == _logicalCollection.type());

  transaction::BuilderLeaser builder(trx);
  res = mergeObjectsForUpdate(trx, oldDoc, newSlice, isEdgeCollection,
                              options.mergeObjects, options.keepNull,
                              *builder.get(), options.isRestore, revisionId);
  if (res.fail()) {
    return res;
  }
  LocalDocumentId const newDocumentId = ::generateDocumentId(_logicalCollection, revisionId);

  if (_isDBServer) {
    // Need to check that no sharding keys have changed:
    if (arangodb::shardKeysChanged(_logicalCollection, oldDoc, builder->slice(), true)) {
      return res.reset(TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES);
    }
    if (arangodb::smartJoinAttributeChanged(_logicalCollection, oldDoc,
                                            builder->slice(), true)) {
      return res.reset(TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SMART_JOIN_ATTRIBUTE);
    }
  }

  if (options.validate && options.isSynchronousReplicationFrom.empty()) {
    res = _logicalCollection.validate(builder->slice(), oldDoc,
                                      trx->transactionContextPtr()->getVPackOptions());
    if (res.fail()) {
      return res;
    }
  }

  VPackSlice const newDoc(builder->slice());
  RocksDBSavePoint guard(trx, TRI_VOC_DOCUMENT_OPERATION_UPDATE);

  auto* state = RocksDBTransactionState::toState(trx);
  // add possible log statement under guard
  state->prepareOperation(_logicalCollection.id(), revisionId, TRI_VOC_DOCUMENT_OPERATION_UPDATE);
  res = updateDocument(trx, oldDocumentId, oldDoc, newDocumentId, newDoc, options);

  if (res.ok()) {
    trackWaitForSync(trx, options);

    if (options.returnNew) {
      resultMdr.setManaged(newDoc.begin());
      TRI_ASSERT(!resultMdr.empty());
    } else {  //  need to pass revId manually
      resultMdr.setRevisionId(revisionId);
    }
    if (options.returnOld) {
      if (previousPS.IsPinned()) { // value was not copied
        prevBuffer->assign(previousPS.data(), previousPS.size());
      }  // else value is already assigned
      TRI_ASSERT(!previousMdr.empty());
    } else {
      previousMdr.clearData();
    }

    bool hasPerformedIntermediateCommit = false;
    auto result = state->addOperation(_logicalCollection.id(), revisionId,
                                      TRI_VOC_DOCUMENT_OPERATION_UPDATE,
                                      hasPerformedIntermediateCommit);

    if (result.fail()) {
      THROW_ARANGO_EXCEPTION(result);
    }

    guard.finish(hasPerformedIntermediateCommit);
  }

  return res;
}

Result RocksDBCollection::replace(transaction::Methods* trx,
                                  velocypack::Slice newSlice,
                                  ManagedDocumentResult& resultMdr, OperationOptions& options,
                                  ManagedDocumentResult& previousMdr) {

  VPackSlice keySlice = newSlice.get(StaticStrings::KeyString);
  if (keySlice.isNone()) {
    return TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
  } else if (!keySlice.isString()) {
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
  }

  auto const oldDocumentId = primaryIndex()->lookupKey(trx, VPackStringRef(keySlice));
  if (!oldDocumentId.isSet()) {
    return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }
  std::string* prevBuffer = previousMdr.setManaged();
  // uses either prevBuffer or avoids memcpy (if read hits block cache)
  rocksdb::PinnableSlice previousPS(prevBuffer);
  Result res = lookupDocumentVPack(trx, oldDocumentId, previousPS,
                                   /*readCache*/ true, /*fillCache*/ false);
  if (res.fail()) {
    return res;
  }

  TRI_ASSERT(previousPS.size() > 0);
  VPackSlice const oldDoc(reinterpret_cast<uint8_t const*>(previousPS.data()));
  previousMdr.setRevisionId(transaction::helpers::extractRevFromDocument(oldDoc));
  TRI_ASSERT(previousMdr.revisionId() != 0);

  if (!options.ignoreRevs) {  // Check old revision:
    TRI_voc_rid_t expectedRev = TRI_ExtractRevisionId(newSlice);
    res = checkRevision(trx, expectedRev, previousMdr.revisionId());
    if (res.fail()) {
      return res;
    }
  }

  // merge old and new values
  TRI_voc_rid_t revisionId;
  bool const isEdgeCollection = (TRI_COL_TYPE_EDGE == _logicalCollection.type());

  transaction::BuilderLeaser builder(trx);
  res = newObjectForReplace(trx, oldDoc, newSlice, isEdgeCollection,
                            *builder.get(), options.isRestore, revisionId);
  if (res.fail()) {
    return res;
  }
  LocalDocumentId const newDocumentId = ::generateDocumentId(_logicalCollection, revisionId);

  if (_isDBServer) {
    // Need to check that no sharding keys have changed:
    if (arangodb::shardKeysChanged(_logicalCollection, oldDoc, builder->slice(), false)) {
      return res.reset(TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES);
    }
    if (arangodb::smartJoinAttributeChanged(_logicalCollection, oldDoc,
                                            builder->slice(), false)) {
      return Result(TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SMART_JOIN_ATTRIBUTE);
    }
  }

  VPackSlice const newDoc(builder->slice());

  if (options.validate && options.isSynchronousReplicationFrom.empty()) {
    res = _logicalCollection.validate(newDoc, oldDoc,
                                      trx->transactionContextPtr()->getVPackOptions());
    if (res.fail()) {
      return res;
    }
  }

  RocksDBSavePoint guard(trx, TRI_VOC_DOCUMENT_OPERATION_REPLACE);

  auto* state = RocksDBTransactionState::toState(trx);
  // add possible log statement under guard
  state->prepareOperation(_logicalCollection.id(), revisionId, TRI_VOC_DOCUMENT_OPERATION_REPLACE);
  res = updateDocument(trx, oldDocumentId, oldDoc, newDocumentId, newDoc, options);

  if (res.ok()) {
    trackWaitForSync(trx, options);

    if (options.returnNew) {
      resultMdr.setManaged(newDoc.begin());
      TRI_ASSERT(!resultMdr.empty());
    } else {  //  need to pass revId manually
      resultMdr.setRevisionId(revisionId);
    }
    if (options.returnOld) {
      if (previousPS.IsPinned()) {  // value was not copied
        prevBuffer->assign(previousPS.data(), previousPS.size());
      }  // else value is already assigned
      TRI_ASSERT(!previousMdr.empty());
    } else {
      previousMdr.clearData();
    }

    bool hasPerformedIntermediateCommit = false;
    auto result = state->addOperation(_logicalCollection.id(), revisionId,
                                      TRI_VOC_DOCUMENT_OPERATION_REPLACE,
                                      hasPerformedIntermediateCommit);

    if (result.fail()) {
      THROW_ARANGO_EXCEPTION(result);
    }

    guard.finish(hasPerformedIntermediateCommit);
  }

  return res;
}

Result RocksDBCollection::remove(transaction::Methods& trx, velocypack::Slice slice,
                                 ManagedDocumentResult& previousMdr,
                                 OperationOptions& options) {
  VPackSlice keySlice;
  if (slice.isString()) {
    keySlice = slice;
  } else {
    keySlice = slice.get(StaticStrings::KeyString);
  }
  TRI_ASSERT(!keySlice.isNone());
  if (!keySlice.isString()) {
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
  }

  auto const documentId = primaryIndex()->lookupKey(&trx, VPackStringRef(keySlice));
  if (!documentId.isSet()) {
    return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }

  // Check old revision:
  LocalDocumentId expectedId;
  if (!options.ignoreRevs && slice.isObject()) {
    expectedId = LocalDocumentId::create(TRI_ExtractRevisionId(slice));
  }

  return remove(trx, documentId, expectedId, previousMdr, options);
}

Result RocksDBCollection::remove(transaction::Methods& trx, LocalDocumentId documentId,
                                 ManagedDocumentResult& previousMdr,
                                 OperationOptions& options) {
  return remove(trx, documentId, LocalDocumentId(), previousMdr, options);
}

Result RocksDBCollection::remove(transaction::Methods& trx, LocalDocumentId documentId,
                                 LocalDocumentId expectedId, ManagedDocumentResult& previousMdr,
                                 OperationOptions& options) {
  if (!documentId.isSet()) {
    return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }
  std::string* prevBuffer = previousMdr.setManaged();
  // uses either prevBuffer or avoids memcpy (if read hits block cache)
  rocksdb::PinnableSlice previousPS(prevBuffer);
  Result res = lookupDocumentVPack(&trx, documentId, previousPS,
                                   /*readCache*/ true, /*fillCache*/ false);
  if (res.fail()) {
    return res;
  }

  TRI_ASSERT(previousPS.size() > 0);
  VPackSlice const oldDoc(reinterpret_cast<uint8_t const*>(previousPS.data()));
  previousMdr.setRevisionId(transaction::helpers::extractRevFromDocument(oldDoc));
  TRI_ASSERT(previousMdr.revisionId() != 0);

  // Check old revision:
  if (!options.ignoreRevs && expectedId.isSet()) {
    res = checkRevision(&trx, expectedId.id(), previousMdr.revisionId());
    if (res.fail()) {
      return res;
    }
  }

  auto state = RocksDBTransactionState::toState(&trx);
  RocksDBSavePoint guard(&trx, TRI_VOC_DOCUMENT_OPERATION_REMOVE);

  // add possible log statement under guard
  state->prepareOperation(_logicalCollection.id(), previousMdr.revisionId(),
                          TRI_VOC_DOCUMENT_OPERATION_REMOVE);
  res = removeDocument(&trx, documentId, oldDoc, options);

  if (res.ok()) {
    trackWaitForSync(&trx, options);

    if (options.returnOld) {
      if (previousPS.IsPinned()) {  // value was not copied
        prevBuffer->assign(previousPS.data(), previousPS.size());
      }  // else value is already assigned
      TRI_ASSERT(!previousMdr.empty());
    } else {
      previousMdr.clearData();
    }

    bool hasPerformedIntermediateCommit = false;
    res = state->addOperation(_logicalCollection.id(), newRevisionId(),
                              TRI_VOC_DOCUMENT_OPERATION_REMOVE,
                              hasPerformedIntermediateCommit);

    guard.finish(hasPerformedIntermediateCommit);
  }

  return res;
}

void RocksDBCollection::adjustNumberDocuments(transaction::Methods& trx, int64_t diff) {
  auto seq = rocksutils::latestSequenceNumber();
  meta().adjustNumberDocuments(seq, /*revId*/ 0, diff);
}

Result RocksDBCollection::upgrade() {
  Result res{};
  if (_logicalCollection.version() >= LogicalCollection::Version::v37) {
    return res;
  }

  auto& server = _logicalCollection.vocbase().server();
  auto& selector = server.getFeature<EngineSelectorFeature>();
  RocksDBEngine& engine = selector.engine<RocksDBEngine>();

  auto cleanupGuard = arangodb::scopeGuard(
      [this]() -> void { [[maybe_unused]] Result res = cleanupAfterUpgrade(); });

  res = ::updateObjectIdsForCollection(*engine.db(), _logicalCollection,
                                       ::injectNewTemporaryObjectId, false);
  if (res.fail()) {
    LOG_TOPIC("ad41c", WARN, Logger::ENGINES)
        << "failed to allocate temporary id for writing while upgrading '"
        << _logicalCollection.name() << "': " << res.errorMessage();
    return res;
  }

  {
    // start an exclusive transaction to block access to the collection
    std::shared_ptr<transaction::Context> context =
        transaction::StandaloneContext::Create(_logicalCollection.vocbase());
    SingleCollectionTransaction trx(context, _logicalCollection, AccessMode::Type::EXCLUSIVE);
    res = trx.begin();
    if (res.fail()) {
      LOG_TOPIC("ad51c", WARN, Logger::ENGINES)
          << "failed to lock collection for writing while upgrading '"
          << _logicalCollection.name() << "': " << res.errorMessage();
      return res;
    }

    res = ::copyCollectionToNewObjectIdSpace(*engine.db(), _logicalCollection);
    if (res.fail()) {
      LOG_TOPIC("af51c", WARN, Logger::ENGINES)
          << "failed to upgrade collection data while upgrading '"
          << _logicalCollection.name() << "': " << res.errorMessage();
      return res;
    }

    std::vector<std::shared_ptr<Index>> indices = getIndexes();
    for (auto const& index : indices) {
      res = ::copyIndexToNewObjectIdSpace(*engine.db(), _logicalCollection, *index);
      if (res.fail()) {
        LOG_TOPIC("af61c", WARN, Logger::ENGINES)
            << "failed to upgrade index data while upgrading '"
            << _logicalCollection.name() << "': " << res.errorMessage();
        return res;
      }
    }

    res = ::updateObjectIdsForCollection(*engine.db(), _logicalCollection,
                                         ::swapObjectIds, true);
    if (res.fail()) {
      LOG_TOPIC("af62c", WARN, Logger::ENGINES)
          << "failed to finalize upgrade while upgrading '"
          << _logicalCollection.name() << "': " << res.errorMessage();
      return res;
    }

    res = rebuildRevisionTree();
    if (res.fail()) {
      LOG_TOPIC("af82c", WARN, Logger::ENGINES)
          << "failed to rebuild revision tree while upgrading '"
          << _logicalCollection.name() << "': " << res.errorMessage();
      return res;
    }
  }

  cleanupGuard.cancel();
  return cleanupAfterUpgrade();
}

bool RocksDBCollection::didPartialUpgrade() {
  if (tempObjectId() != 0) {
    return true;
  }

  for (auto& idx : getIndexes()) {
    if (static_cast<RocksDBIndex&>(*idx).tempObjectId() != 0) {
      return true;
    }
  }

  return false;
}

Result RocksDBCollection::cleanupAfterUpgrade() {
  auto& server = _logicalCollection.vocbase().server();
  auto& selector = server.getFeature<EngineSelectorFeature>();
  RocksDBEngine& engine = selector.engine<RocksDBEngine>();

  auto& rcoll =
      static_cast<arangodb::RocksDBMetaCollection&>(*_logicalCollection.getPhysical());
  Result res = ::cleanupOldIdSpaces(*engine.db(), rcoll);
  if (res.fail()) {
    return res;
  }

  return ::updateObjectIdsForCollection(*engine.db(), _logicalCollection,
                                        ::clearTemporaryObjectId, false);
}

bool RocksDBCollection::hasDocuments() {
  rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
  RocksDBKeyBounds bounds = RocksDBKeyBounds::CollectionDocuments(objectId());
  return rocksutils::hasKeys(db, bounds, true);
}

/// @brief return engine-specific figures
void RocksDBCollection::figuresSpecific(bool details, arangodb::velocypack::Builder& builder) {
  rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
  RocksDBKeyBounds bounds = RocksDBKeyBounds::CollectionDocuments(objectId());
  rocksdb::Range r(bounds.start(), bounds.end());

  uint64_t out = 0;
  db->GetApproximateSizes(RocksDBColumnFamily::documents(), &r, 1, &out,
                          static_cast<uint8_t>(
                              rocksdb::DB::SizeApproximationFlags::INCLUDE_MEMTABLES |
                              rocksdb::DB::SizeApproximationFlags::INCLUDE_FILES));

  builder.add("documentsSize", VPackValue(out));
  bool cacheInUse = useCache();
  builder.add("cacheInUse", VPackValue(cacheInUse));
  if (cacheInUse) {
    builder.add("cacheSize", VPackValue(_cache->size()));
    builder.add("cacheUsage", VPackValue(_cache->usage()));
    auto hitRates = _cache->hitRates();
    double rate = hitRates.first;
    rate = std::isnan(rate) ? 0.0 : rate;
    builder.add("cacheLifeTimeHitRate", VPackValue(rate));
    rate = hitRates.second;
    rate = std::isnan(rate) ? 0.0 : rate;
    builder.add("cacheWindowedHitRate", VPackValue(rate));
  } else {
    builder.add("cacheSize", VPackValue(0));
    builder.add("cacheUsage", VPackValue(0));
  }

  if (details) {
    // engine-specific stuff here
    rocksdb::DB* db = rocksutils::globalRocksDB()->GetRootDB();

    builder.add("engine", VPackValue(VPackValueType::Object));
    
    builder.add("documents", VPackValue(rocksutils::countKeyRange(db, RocksDBKeyBounds::CollectionDocuments(objectId()), true)));
    builder.add("indexes", VPackValue(VPackValueType::Array));
    {
      READ_LOCKER(guard, _indexesLock);
      for (auto it : _indexes) {
        auto type = it->type();
        if (type == Index::TRI_IDX_TYPE_UNKNOWN ||
            type == Index::TRI_IDX_TYPE_IRESEARCH_LINK ||
            type == Index::TRI_IDX_TYPE_NO_ACCESS_INDEX) {
          continue;
        }
        
        builder.openObject();
        builder.add("type", VPackValue(it->typeName()));
        builder.add("id", VPackValue(it->id().id()));
        
        RocksDBIndex const* rix = static_cast<RocksDBIndex const*>(it.get());
        size_t count = 0;
        switch (type) {
          case Index::TRI_IDX_TYPE_PRIMARY_INDEX:
            count = rocksutils::countKeyRange(db, RocksDBKeyBounds::PrimaryIndex(rix->objectId()), true);
            break;
          case Index::TRI_IDX_TYPE_GEO_INDEX:
          case Index::TRI_IDX_TYPE_GEO1_INDEX:
          case Index::TRI_IDX_TYPE_GEO2_INDEX: 
            count = rocksutils::countKeyRange(db, RocksDBKeyBounds::GeoIndex(rix->objectId()), true);
            break;
          case Index::TRI_IDX_TYPE_HASH_INDEX:
          case Index::TRI_IDX_TYPE_SKIPLIST_INDEX:
          case Index::TRI_IDX_TYPE_TTL_INDEX:
          case Index::TRI_IDX_TYPE_PERSISTENT_INDEX: 
            if (it->unique()) {
              count = rocksutils::countKeyRange(db, RocksDBKeyBounds::UniqueVPackIndex(rix->objectId(), false), true);
            } else {
              count = rocksutils::countKeyRange(db, RocksDBKeyBounds::VPackIndex(rix->objectId(), false), true);
            }
            break;
          case Index::TRI_IDX_TYPE_EDGE_INDEX: 
            count = rocksutils::countKeyRange(db, RocksDBKeyBounds::EdgeIndex(rix->objectId()), false);
            break;
          case Index::TRI_IDX_TYPE_FULLTEXT_INDEX: 
            count = rocksutils::countKeyRange(db, RocksDBKeyBounds::FulltextIndex(rix->objectId()), true);
            break;
          default: 
            // we should not get here
            TRI_ASSERT(false);
        }

        builder.add("count", VPackValue(count));
        builder.close();
      }
    }
    builder.close(); // "indexes" array
    builder.close(); // "engine" object
  }
}

Result RocksDBCollection::insertDocument(arangodb::transaction::Methods* trx,
                                         LocalDocumentId const& documentId,
                                         VPackSlice const& doc,
                                         OperationOptions& options) const {
  // Coordinator doesn't know index internals
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  TRI_ASSERT(trx->state()->isRunning());
  Result res;

  RocksDBKeyLeaser key(trx);
  key->constructDocument(objectId(), documentId);

  RocksDBTransactionState* state = RocksDBTransactionState::toState(trx);
  if (state->hasHint(transaction::Hints::Hint::GLOBAL_MANAGED)) {
    // blacklist new document to avoid caching without committing first
    blackListKey(key.ref());
  }

  RocksDBMethods* mthds = state->rocksdbMethods();
  // disable indexing in this transaction if we are allowed to
  IndexingDisabler disabler(mthds, state->isSingleOperation());

  TRI_ASSERT(key->containsLocalDocumentId(documentId));
  rocksdb::Status s =
      mthds->PutUntracked(RocksDBColumnFamily::documents(), key.ref(),
                          rocksdb::Slice(doc.startAs<char>(),
                                         static_cast<size_t>(doc.byteSize())));
  if (!s.ok()) {
    return res.reset(rocksutils::convertStatus(s, rocksutils::document));
  }

  READ_LOCKER(guard, _indexesLock);

  bool needReversal = false;
  for (auto it = _indexes.begin(); it != _indexes.end(); it++) {
    RocksDBIndex* rIdx = static_cast<RocksDBIndex*>(it->get());
    res = rIdx->insert(*trx, mthds, documentId, doc, options);
    needReversal = needReversal || rIdx->needsReversal();
    if (res.fail()) {
      if (needReversal && !state->isSingleOperation()) {
        ::reverseIdxOps(_indexes, it, [mthds, trx, &documentId, &doc](RocksDBIndex* rid) {
          return rid->remove(*trx, mthds, documentId, doc, Index::OperationMode::rollback);
        });
      }
      break;
    }
  }

  if (res.ok()) {
    RocksDBTransactionState::toState(trx)->trackInsert(_logicalCollection.id(),
                                                       documentId.id());
  }

  return res;
}

Result RocksDBCollection::removeDocument(arangodb::transaction::Methods* trx,
                                         LocalDocumentId const& documentId,
                                         VPackSlice const& doc,
                                         OperationOptions& options) const {
  // Coordinator doesn't know index internals
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  TRI_ASSERT(trx->state()->isRunning());
  TRI_ASSERT(objectId() != 0);
  Result res;

  RocksDBKeyLeaser key(trx);
  key->constructDocument(objectId(), documentId);

  blackListKey(key.ref());

  RocksDBMethods* mthds = RocksDBTransactionState::toMethods(trx);

  // disable indexing in this transaction if we are allowed to
  IndexingDisabler disabler(mthds, trx->isSingleOperationTransaction());

  rocksdb::Status s = mthds->SingleDelete(RocksDBColumnFamily::documents(), key.ref());
  if (!s.ok()) {
    return res.reset(rocksutils::convertStatus(s, rocksutils::document));
  }

  /*LOG_TOPIC("17502", ERR, Logger::ENGINES)
      << "Delete rev: " << revisionId << " trx: " << trx->state()->id()
      << " seq: " << mthds->sequenceNumber()
      << " objectID " << objectId() << " name: " << _logicalCollection.name();*/

  READ_LOCKER(guard, _indexesLock);
  bool needReversal = false;
  for (auto it = _indexes.begin(); it != _indexes.end(); it++) {
    RocksDBIndex* rIdx = static_cast<RocksDBIndex*>(it->get());
    res = rIdx->remove(*trx, mthds, documentId, doc, options.indexOperationMode);
    needReversal = needReversal || rIdx->needsReversal();
    if (res.fail()) {
      if (needReversal && !trx->isSingleOperationTransaction()) {
        ::reverseIdxOps(_indexes, it, [mthds, trx, &documentId, &doc](RocksDBIndex* rid) {
          OperationOptions options;
          options.indexOperationMode = Index::OperationMode::rollback;
          return rid->insert(*trx, mthds, documentId, doc, options);
        });
      }
      break;
    }
  }

  if (res.ok()) {
    RocksDBTransactionState::toState(trx)->trackRemove(_logicalCollection.id(),
                                                       documentId.id());
  }

  return res;
}

Result RocksDBCollection::updateDocument(transaction::Methods* trx,
                                         LocalDocumentId const& oldDocumentId,
                                         VPackSlice const& oldDoc,
                                         LocalDocumentId const& newDocumentId,
                                         VPackSlice const& newDoc,
                                         OperationOptions& options) const {
  // Coordinator doesn't know index internals
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  TRI_ASSERT(trx->state()->isRunning());
  TRI_ASSERT(objectId() != 0);
  Result res;

  RocksDBTransactionState* state = RocksDBTransactionState::toState(trx);
  RocksDBMethods* mthds = state->rocksdbMethods();
  // disable indexing in this transaction if we are allowed to
  IndexingDisabler disabler(mthds, trx->isSingleOperationTransaction());

  RocksDBKeyLeaser key(trx);
  key->constructDocument(objectId(), oldDocumentId);
  TRI_ASSERT(key->containsLocalDocumentId(oldDocumentId));
  blackListKey(key.ref());

  rocksdb::Status s = mthds->SingleDelete(RocksDBColumnFamily::documents(), key.ref());
  if (!s.ok()) {
    return res.reset(rocksutils::convertStatus(s, rocksutils::document));
  }

  key->constructDocument(objectId(), newDocumentId);
  TRI_ASSERT(key->containsLocalDocumentId(newDocumentId));
  s = mthds->PutUntracked(RocksDBColumnFamily::documents(), key.ref(),
                          rocksdb::Slice(newDoc.startAs<char>(),
                                         static_cast<size_t>(newDoc.byteSize())));
  if (!s.ok()) {
    return res.reset(rocksutils::convertStatus(s, rocksutils::document));
  }

  if (state->hasHint(transaction::Hints::Hint::GLOBAL_MANAGED)) {
    // blacklist new document to avoid caching without committing first
    blackListKey(key.ref());
  }

  READ_LOCKER(guard, _indexesLock);
  bool needReversal = false;
  for (auto it = _indexes.begin(); it != _indexes.end(); it++) {
    auto rIdx = static_cast<RocksDBIndex*>(it->get());
    res = rIdx->update(*trx, mthds, oldDocumentId, oldDoc, newDocumentId,
                       newDoc, options.indexOperationMode);
    needReversal = needReversal || rIdx->needsReversal();
    if (!res.ok()) {
      if (needReversal && !trx->isSingleOperationTransaction()) {
        ::reverseIdxOps(_indexes, it,
                        [mthds, trx, &newDocumentId, &newDoc, &oldDocumentId,
                         &oldDoc](RocksDBIndex* rid) {
                          return rid->update(*trx, mthds, newDocumentId, newDoc, oldDocumentId,
                                             oldDoc, Index::OperationMode::rollback);
                        });
      }
      break;
    }
  }

  if (res.ok()) {
    RocksDBTransactionState::toState(trx)->trackRemove(_logicalCollection.id(),
                                                       oldDocumentId.id());
    RocksDBTransactionState::toState(trx)->trackInsert(_logicalCollection.id(),
                                                       newDocumentId.id());
  }

  return res;
}

/// @brief lookup document in cache and / or rocksdb
/// @param readCache attempt to read from cache
/// @param fillCache fill cache with found document
arangodb::Result RocksDBCollection::lookupDocumentVPack(transaction::Methods* trx,
                                                        LocalDocumentId const& documentId,
                                                        rocksdb::PinnableSlice& ps,
                                                        bool readCache, bool fillCache) const {
  TRI_ASSERT(trx->state()->isRunning());
  TRI_ASSERT(objectId() != 0);
  Result res;

  RocksDBKeyLeaser key(trx);
  key->constructDocument(objectId(), documentId);

  bool lockTimeout = false;
  if (readCache && useCache()) {
    TRI_ASSERT(_cache != nullptr);
    // check cache first for fast path
    auto f = _cache->find(key->string().data(),
                          static_cast<uint32_t>(key->string().size()));
    if (f.found()) {  // copy finding into buffer
      ps.PinSelf(rocksdb::Slice(reinterpret_cast<char const*>(f.value()->value()),
                                f.value()->valueSize()));
      // TODO we could potentially use the PinSlice method ?!
      return res;  // all good
    }
    if (f.result().errorNumber() == TRI_ERROR_LOCK_TIMEOUT) {
      // assuming someone is currently holding a write lock, which
      // is why we cannot access the TransactionalBucket.
      lockTimeout = true;  // we skip the insert in this case
    }
  }

  RocksDBMethods* mthd = RocksDBTransactionState::toMethods(trx);
  rocksdb::Status s = mthd->Get(RocksDBColumnFamily::documents(), key->string(), &ps);

  if (!s.ok()) {
    LOG_TOPIC("f63dd", DEBUG, Logger::ENGINES)
        << "NOT FOUND rev: " << documentId.id() << " trx: " << trx->state()->id()
        << " objectID " << objectId() << " name: " << _logicalCollection.name();
    return res.reset(rocksutils::convertStatus(s, rocksutils::document));
  }

  if (fillCache && useCache() && !lockTimeout) {
    TRI_ASSERT(_cache != nullptr);
    // write entry back to cache
    auto entry =
        cache::CachedValue::construct(key->string().data(),
                                      static_cast<uint32_t>(key->string().size()),
                                      ps.data(), static_cast<uint64_t>(ps.size()));
    if (entry) {
      auto status = _cache->insert(entry);
      if (status.errorNumber() == TRI_ERROR_LOCK_TIMEOUT) {
        // the writeLock uses cpu_relax internally, so we can try yield
        std::this_thread::yield();
        status = _cache->insert(entry);
      }
      if (status.fail()) {
        delete entry;
      }
    }
  }

  return res;
}

bool RocksDBCollection::lookupDocumentVPack(transaction::Methods* trx,
                                            LocalDocumentId const& documentId,
                                            IndexIterator::DocumentCallback const& cb,
                                            bool withCache) const {
  TRI_ASSERT(trx->state()->isRunning());
  TRI_ASSERT(objectId() != 0);

  RocksDBKeyLeaser key(trx);
  key->constructDocument(objectId(), documentId);

  if (withCache && useCache()) {
    TRI_ASSERT(_cache != nullptr);
    // check cache first for fast path
    auto f = _cache->find(key->string().data(),
                          static_cast<uint32_t>(key->string().size()));
    if (f.found()) {
      cb(documentId, VPackSlice(reinterpret_cast<uint8_t const*>(f.value()->value())));
      return true;
    }
  }

  transaction::StringLeaser buffer(trx);
  rocksdb::PinnableSlice ps(buffer.get());

  RocksDBMethods* mthd = RocksDBTransactionState::toMethods(trx);
  rocksdb::Status s = mthd->Get(RocksDBColumnFamily::documents(), key->string(), &ps);

  if (!s.ok()) {
    return false;
  }

  TRI_ASSERT(ps.size() > 0);
  cb(documentId, VPackSlice(reinterpret_cast<uint8_t const*>(ps.data())));

  if (withCache && useCache()) {
    TRI_ASSERT(_cache != nullptr);
    // write entry back to cache
    auto entry =
        cache::CachedValue::construct(key->string().data(),
                                      static_cast<uint32_t>(key->string().size()),
                                      ps.data(), static_cast<uint64_t>(ps.size()));
    if (entry) {
      auto status = _cache->insert(entry);
      if (status.errorNumber() == TRI_ERROR_LOCK_TIMEOUT) {
        // the writeLock uses cpu_relax internally, so we can try yield
        std::this_thread::yield();
        status = _cache->insert(entry);
      }
      if (status.fail()) {
        delete entry;
      }
    }
  }

  return true;
}

void RocksDBCollection::createCache() const {
  if (!_cacheEnabled || _cache || _logicalCollection.isAStub() ||
      ServerState::instance()->isCoordinator()) {
    // we leave this if we do not need the cache
    // or if cache already created
    return;
  }

  TRI_ASSERT(_cacheEnabled);
  TRI_ASSERT(_cache.get() == nullptr);
  TRI_ASSERT(CacheManagerFeature::MANAGER != nullptr);
  LOG_TOPIC("f5df2", DEBUG, Logger::CACHE) << "Creating document cache";
  _cache = CacheManagerFeature::MANAGER->createCache(cache::CacheType::Transactional);
  TRI_ASSERT(_cacheEnabled);
}

void RocksDBCollection::destroyCache() const {
  if (!_cache) {
    return;
  }
  TRI_ASSERT(CacheManagerFeature::MANAGER != nullptr);
  // must have a cache...
  TRI_ASSERT(_cache.get() != nullptr);
  LOG_TOPIC("7137b", DEBUG, Logger::CACHE) << "Destroying document cache";
  CacheManagerFeature::MANAGER->destroyCache(_cache);
  _cache.reset();
}

// blacklist given key from transactional cache
void RocksDBCollection::blackListKey(RocksDBKey const& k) const {
  if (useCache()) {
    TRI_ASSERT(_cache != nullptr);
    bool blacklisted = false;
    while (!blacklisted) {
      auto status = _cache->blacklist(k.buffer()->data(),
                                      static_cast<uint32_t>(k.buffer()->size()));
      if (status.ok()) {
        blacklisted = true;
      } else if (status.errorNumber() == TRI_ERROR_SHUTTING_DOWN) {
        destroyCache();
        break;
      }
    }
  }
}

/// @brief can use non transactional range delete in write ahead log
bool RocksDBCollection::canUseRangeDeleteInWal() const {
  if (ServerState::instance()->isSingleServer()) {
    // disableWalFilePruning is used by createIndex
    return _numIndexCreations.load(std::memory_order_acquire) == 0;
  }
  return false;
}

}  // namespace arangodb
