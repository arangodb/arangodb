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
/// @author Daniel Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <rocksdb/db.h>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include "IResearch/IResearchRocksDBRecoveryHelper.h"

#include "Basics/Exceptions.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/debugging.h"
#include "Basics/error.h"
#include "Basics/voc-errors.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchLink.h"
#include "IResearch/IResearchLinkHelper.h"
#include "IResearch/IResearchRocksDBLink.h"
#include "Indexes/Index.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "RocksDBEngine/RocksDBValue.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/AccessMode.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

namespace arangodb::transaction {
class Context;
}

namespace {

std::shared_ptr<arangodb::LogicalCollection> lookupCollection(
    arangodb::DatabaseFeature& db, arangodb::RocksDBEngine& engine, uint64_t objectId) {
  auto pair = engine.mapObjectToCollection(objectId);
  auto vocbase = db.useDatabase(pair.first);

  return vocbase ? vocbase->lookupCollection(pair.second) : nullptr;
}

std::vector<std::shared_ptr<arangodb::Index>> lookupLinks(arangodb::LogicalCollection& coll) {
  auto indexes = coll.getIndexes();

  // filter out non iresearch links
  const auto it = std::remove_if(indexes.begin(), indexes.end(),
                                 [](std::shared_ptr<arangodb::Index> const& idx) {
                                   return idx->type() != arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK;
                                 });
  indexes.erase(it, indexes.end());

  return indexes;
}

std::shared_ptr<arangodb::iresearch::IResearchLink> lookupLink(TRI_vocbase_t& vocbase,
                                                               arangodb::DataSourceId cid,
                                                               arangodb::IndexId iid) {
  auto col = vocbase.lookupCollection(cid);

  if (!col) {
    // invalid cid
    return nullptr;
  }

  return arangodb::iresearch::IResearchLinkHelper::find(*col, iid);
}

void ensureLink(arangodb::DatabaseFeature& db,
                std::set<arangodb::iresearch::IResearchRocksDBRecoveryHelper::IndexId>& recoveredIndexes,
                TRI_voc_tick_t dbId, arangodb::DataSourceId cid,
                arangodb::velocypack::Slice indexSlice) {
  if (!indexSlice.isObject()) {
    LOG_TOPIC("67422", WARN, arangodb::Logger::ENGINES)
        << "Cannot recover index for the collection '" << cid.id()
        << "' in the database '" << dbId << "' : invalid marker";
    return;
  }

  // ensure null terminated string
  auto const indexTypeSlice = indexSlice.get(arangodb::StaticStrings::IndexType);
  auto const indexTypeStr = indexTypeSlice.copyString();
  auto const indexType = arangodb::Index::type(indexTypeStr);

  if (arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK != indexType) {
    // skip non iresearch link indexes
    return;
  }

  arangodb::IndexId iid;
  auto const idSlice = indexSlice.get("id");

  if (idSlice.isString()) {
    iid = arangodb::IndexId{
        static_cast<arangodb::IndexId::BaseType>(std::stoull(idSlice.copyString()))};
  } else if (idSlice.isNumber()) {
    iid = arangodb::IndexId{idSlice.getNumber<arangodb::IndexId::BaseType>()};
  } else {
    LOG_TOPIC("96bc8", ERR, arangodb::iresearch::TOPIC)
        << "Cannot recover index for the collection '" << cid.id()
        << "' in the database '" << dbId
        << "' : invalid value for attribute 'id', expected 'String' or "
           "'Number', got '"
        << idSlice.typeName() << "'";
    return;
  }

  TRI_vocbase_t* vocbase = db.useDatabase(dbId);

  if (!vocbase) {
    // if the underlying database is gone, we can go on
    LOG_TOPIC("3c21a", TRACE, arangodb::iresearch::TOPIC)
        << "Cannot create index for the collection '" << cid.id() << "' in the database '"
        << dbId << "' : " << TRI_errno_string(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    return;
  }

  auto col = vocbase->lookupCollection(cid);

  if (!col) {
    // if the underlying collection gone, we can go on
    LOG_TOPIC("43f99", TRACE, arangodb::iresearch::TOPIC)
        << "Cannot create index for the collection '" << cid.id()
        << "' in the database '" << dbId
        << "' : " << TRI_errno_string(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    return;
  }

  if (!recoveredIndexes.emplace(dbId, cid, iid).second) {
    // already there
    LOG_TOPIC("3dcb4", TRACE, arangodb::iresearch::TOPIC)
        << "Index of type 'IResearchLink' with id `" << iid.id()
        << "' in the collection '" << cid.id() << "' in the database '" << dbId
        << "' already exists: skipping create marker";
    return;
  }

  auto link = lookupLink(*vocbase, cid, iid);

  if (!link) {
    LOG_TOPIC("e9142", TRACE, arangodb::iresearch::TOPIC)
        << "Collection '" << cid.id() << "' in the database '" << dbId
        << "' does not contain index of type 'IResearchLink' with id '"
        << iid.id() << "': skip create marker";
    return;
  }

  LOG_TOPIC("29bea", TRACE, arangodb::iresearch::TOPIC)
      << "found create index marker, databaseId: '" << dbId
      << "', collectionId: '" << cid.id() << "', link '" << iid.id() << "'";

  arangodb::velocypack::Builder json;

  json.openObject();

  if (!link->properties(json, true).ok()) { // link definition used for recreation and persistence
    LOG_TOPIC("15f11", ERR, arangodb::iresearch::TOPIC)
        << "Failed to generate jSON definition for link '" << iid.id()
        << "' to the collection '" << cid.id() << "' in the database '" << dbId;
    return;
  }
  // we need to keep objectId
  if (indexSlice.hasKey(arangodb::StaticStrings::ObjectId)) {
    json.add(arangodb::StaticStrings::ObjectId, indexSlice.get(arangodb::StaticStrings::ObjectId));
  } else {
    LOG_TOPIC("ed031", WARN, arangodb::iresearch::TOPIC)
        << "Missing objectId in jSON definition for link '" << iid.id()
        << "' to the collection '" << cid.id() << "' in the database '" << dbId
        << "'. ObjectId will be regenerated";
  }
  
  json.close();

  bool created;

  // re-insert link
  if (!col->dropIndex(link->id()) // index drop failure
      || !col->createIndex(json.slice(), created) // index creation failure
      || !created) { // index not created
    LOG_TOPIC("44a02", ERR, arangodb::iresearch::TOPIC)
        << "Failed to recreate an arangosearch link '" << iid.id()
        << "' to the collection '" << cid.id() << "' in the database '" << dbId;

    return;
  }
}

}  // namespace

namespace arangodb {
namespace iresearch {

IResearchRocksDBRecoveryHelper::IResearchRocksDBRecoveryHelper(application_features::ApplicationServer& server)
    : _server(server) {}

void IResearchRocksDBRecoveryHelper::prepare() {
  _dbFeature = &_server.getFeature<DatabaseFeature>();
  _engine = &_server.getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
  _documentCF = RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::Documents)
                    ->GetID();
}

void IResearchRocksDBRecoveryHelper::PutCF(
    uint32_t column_family_id,
    const rocksdb::Slice& key,
    const rocksdb::Slice& value,
    rocksdb::SequenceNumber /*tick*/) {
  if (column_family_id != _documentCF) {
    return;
  }

  auto coll = lookupCollection(*_dbFeature, *_engine, RocksDBKey::objectId(key));

  if (coll == nullptr) {
    return;
  }

  auto const links = lookupLinks(*coll);

  if (links.empty()) {
    return;
  }

  auto docId = RocksDBKey::documentId(key);
  auto doc = RocksDBValue::data(value);

  transaction::StandaloneContext ctx(coll->vocbase());

  SingleCollectionTransaction trx(
    std::shared_ptr<transaction::Context>(
      std::shared_ptr<transaction::Context>(),
      &ctx), // aliasing ctor
    *coll, arangodb::AccessMode::Type::WRITE);

  Result res = trx.begin();

  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  for (std::shared_ptr<arangodb::Index> const& link : links) {
    IndexId indexId(coll->vocbase().id(), coll->id(), link->id());

    // optimization: avoid insertion of recovered documents twice,
    //               first insertion done during index creation
    if (!link || _recoveredIndexes.find(indexId) != _recoveredIndexes.end()) {
      continue;  // index was already populated when it was created
    }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    IResearchLink& impl = dynamic_cast<IResearchRocksDBLink&>(*link);
#else
    IResearchLink& impl = static_cast<IResearchRocksDBLink&>(*link);
#endif

    impl.insert(trx, docId, doc);
  }

  res = trx.commit();

  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

// common implementation for DeleteCF / SingleDeleteCF
void IResearchRocksDBRecoveryHelper::handleDeleteCF(
    uint32_t column_family_id,
    const rocksdb::Slice& key,
    rocksdb::SequenceNumber /*tick*/) {
  if (column_family_id != _documentCF) {
    return;
  }

  auto coll = lookupCollection(*_dbFeature, *_engine, RocksDBKey::objectId(key));

  if (coll == nullptr) {
    return;
  }

  auto const links = lookupLinks(*coll);

  if (links.empty()) {
    return;
  }

  auto docId = RocksDBKey::documentId(key);

  transaction::StandaloneContext ctx(coll->vocbase());

  SingleCollectionTransaction trx(
    std::shared_ptr<transaction::Context>(
      std::shared_ptr<transaction::Context>(),
      &ctx), // aliasing ctor
    *coll, arangodb::AccessMode::Type::WRITE);

  Result res = trx.begin();

  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  for (std::shared_ptr<arangodb::Index> const& link : links) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    IResearchLink& impl = dynamic_cast<IResearchRocksDBLink&>(*link);
#else
    IResearchLink& impl = static_cast<IResearchRocksDBLink&>(*link);
#endif

    impl.remove(trx, docId,
                arangodb::velocypack::Slice::emptyObjectSlice());
  }

  res = trx.commit();

  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

void IResearchRocksDBRecoveryHelper::LogData(
    const rocksdb::Slice& blob,
    rocksdb::SequenceNumber tick) {
  RocksDBLogType const type = RocksDBLogValue::type(blob);

  switch (type) {
    case RocksDBLogType::IndexCreate: {
      TRI_ASSERT(_dbFeature);
      TRI_ASSERT(_engine);
      TRI_voc_tick_t const dbId = RocksDBLogValue::databaseId(blob);
      DataSourceId const collectionId = RocksDBLogValue::collectionId(blob);
      auto const indexSlice = RocksDBLogValue::indexSlice(blob);
      ensureLink(*_dbFeature, _recoveredIndexes, dbId, collectionId, indexSlice);
    } break;
    case RocksDBLogType::CollectionTruncate: {
      TRI_ASSERT(_dbFeature);
      TRI_ASSERT(_engine);
      uint64_t objectId = RocksDBLogValue::objectId(blob);
      auto coll = lookupCollection(*_dbFeature, *_engine, objectId);

      if (coll != nullptr) {
        auto const links = lookupLinks(*coll);
        for (auto link : links) {
          link->afterTruncate(tick, nullptr);
        }
      }

      break;
    }
    default:
      break;  // shut up the compiler
  }
}

}  // namespace iresearch
}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
