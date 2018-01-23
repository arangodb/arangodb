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
/// @author Daniel Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"
#include "IResearch/IResearchRocksDBRecoveryHelper.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLink.h"
#include "Indexes/Index.h"
#include "Logger/Logger.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBColumnFamily.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "RocksDBEngine/RocksDBValue.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LocalDocumentId.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/vocbase.h"

NS_LOCAL

std::pair<TRI_vocbase_t*, arangodb::LogicalCollection*> lookupDatabaseAndCollection(
    arangodb::DatabaseFeature& db,
    arangodb::RocksDBEngine& engine,
    uint64_t objectId
) {
  auto pair = engine.mapObjectToCollection(objectId);

  auto vocbase = db.useDatabase(pair.first);

  if (vocbase == nullptr) {
    return std::make_pair(nullptr, nullptr);
  }

  return std::make_pair(vocbase, vocbase->lookupCollection(pair.second));
}

void dropCollectionFromAllViews(
    arangodb::DatabaseFeature& db,
    TRI_voc_tick_t dbId,
    TRI_voc_cid_t collectionId
) {
  auto vocbase = db.useDatabase(dbId);

  if (vocbase) {
    // iterate over vocbase views
    for (auto logicalView : vocbase->views()) {
      if (arangodb::iresearch::IResearchView::type() != logicalView->type()) {
        continue;
      }
      auto* view = static_cast<arangodb::iresearch::IResearchView*>(logicalView->getImplementation());
      LOG_TOPIC(TRACE, arangodb::iresearch::IResearchFeature::IRESEARCH)
          << "Removing all documents belonging to view " << view->id()
          << " sourced from collection " << collectionId;
      view->drop(collectionId);
    }
  }
}

std::vector<std::shared_ptr<arangodb::Index>> lookupLinks(
    arangodb::LogicalCollection& coll
) {
  // FIXME better to have
  // LogicalCollection::getIndexes(std::vector<>&)
  auto indexes = coll.getIndexes();

  // filter out non iresearch links
  const auto it = std::remove_if(
    indexes.begin(), indexes.end(),
    [](std::shared_ptr<arangodb::Index> const& idx) {
      return idx->type() != arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK;
  });
  indexes.erase(it, indexes.end());

  return indexes;
}

arangodb::iresearch::IResearchLink* lookupLink(
    arangodb::DatabaseFeature& db,
    TRI_voc_tick_t dbId,
    TRI_voc_cid_t cid,
    TRI_idx_iid_t iid
) {
  auto* vocbase = db.useDatabase(dbId);

  if (!vocbase) {
    // invalid dbId
    return nullptr;
  }

  auto* col = vocbase->lookupCollection(cid);

  if (!col) {
    // invalid cid
    return nullptr;
  }

  auto indexes = col->getIndexes();

  auto it = std::find_if(
    indexes.begin(), indexes.end(),
    [iid](std::shared_ptr<arangodb::Index> const& idx) {
      return idx->id() == iid && idx->type() == arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK;
  });

  // TODO FIXME find a better way to retrieve an iResearch Link
  // cannot use static_cast/reinterpret_cast since Index is not related to IResearchLink

  return it == indexes.end()
    ? nullptr
    : dynamic_cast<arangodb::iresearch::IResearchLink*>(it->get());
}

NS_END

using namespace arangodb;
using namespace arangodb::iresearch;

IResearchRocksDBRecoveryHelper::IResearchRocksDBRecoveryHelper() {}

IResearchRocksDBRecoveryHelper::~IResearchRocksDBRecoveryHelper() {}

void IResearchRocksDBRecoveryHelper::prepare() {
  _dbFeature = DatabaseFeature::DATABASE,
  _engine = static_cast<RocksDBEngine*>(EngineSelectorFeature::ENGINE),
  _documentCF = RocksDBColumnFamily::documents()->GetID();
}

void IResearchRocksDBRecoveryHelper::PutCF(uint32_t column_family_id,
                                           const rocksdb::Slice& key,
                                           const rocksdb::Slice& value) {
  if (column_family_id == _documentCF) {
    auto pair = lookupDatabaseAndCollection(*_dbFeature, *_engine, RocksDBKey::objectId(key));
    TRI_vocbase_t* vocbase = pair.first;
    LogicalCollection* coll = pair.second;
    if (coll == nullptr) {
      return;
    }

    auto const links = lookupLinks(*coll);

    if (links.empty()) {
      return;
    }

    auto rev = RocksDBKey::revisionId(RocksDBEntryType::Document, key);
    auto doc = RocksDBValue::data(value);

    SingleCollectionTransaction trx(
      transaction::StandaloneContext::Create(vocbase), coll->cid(),
      arangodb::AccessMode::Type::WRITE
    );

    trx.begin();

    for (auto link : links) {
      link->insert(
        &trx,
        LocalDocumentId(rev),
        doc,
        Index::OperationMode::internal
      );
      // LOG_TOPIC(TRACE, IResearchFeature::IRESEARCH) << "recovery helper
      // inserted: " << doc.toJson();
    }

    trx.commit();

    return;
  }
}

void IResearchRocksDBRecoveryHelper::DeleteCF(uint32_t column_family_id,
                                              const rocksdb::Slice& key) {
  if (column_family_id == _documentCF) {
    auto pair = lookupDatabaseAndCollection(*_dbFeature, *_engine, RocksDBKey::objectId(key));
    TRI_vocbase_t* vocbase = pair.first;
    LogicalCollection* coll = pair.second;
    if (coll == nullptr) {
      return;
    }

    auto const links = lookupLinks(*coll);

    if (links.empty()) {
      return;
    }

    auto rev = RocksDBKey::revisionId(RocksDBEntryType::Document, key);

    SingleCollectionTransaction trx(
      transaction::StandaloneContext::Create(vocbase), coll->cid(),
      arangodb::AccessMode::Type::WRITE
    );

    trx.begin();

    for (auto link : links) {
      link->remove(
        &trx,
        LocalDocumentId(rev),
        arangodb::velocypack::Slice::emptyObjectSlice(),
        Index::OperationMode::internal
      );
      // LOG_TOPIC(TRACE, IResearchFeature::IRESEARCH) << "recovery helper
      // removed: " << rev;
    }

    trx.commit();

    return;
  }
}

void IResearchRocksDBRecoveryHelper::SingleDeleteCF(uint32_t column_family_id,
                                                    const rocksdb::Slice& key) {

}

void IResearchRocksDBRecoveryHelper::LogData(const rocksdb::Slice& blob) {
  TRI_ASSERT(_dbFeature);
  TRI_ASSERT(_engine);

  const RocksDBLogType type = RocksDBLogValue::type(blob);

  switch (type) {
    case RocksDBLogType::CollectionDrop: {
      // find database, iterate over all extant views and drop collection
      TRI_voc_tick_t const dbId = RocksDBLogValue::databaseId(blob);
      TRI_voc_cid_t const collectionId = RocksDBLogValue::collectionId(blob);
      dropCollectionFromAllViews(*_dbFeature, dbId, collectionId);
    } break;
    case RocksDBLogType::IndexCreate: {
//      auto const indexSlice = RocksDBLogValue::indexSlice(blob);
//
//      if (!indexSlice.isObject()) {
//        LOG_TOPIC(WARN, arangodb::Logger::ENGINES)
//            << "cannot recover index for collection: invalid marker";
//        return;
//      }
//
//      // ensure null terminated string
//      auto const indexTypeStr = indexTypeSlice.copyString();
//      auto const indexType = arangodb::Index::type(indexTypeStr.c_str());
//
//      if (arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK != indexType) {
//        // skip non iresearch link indexes
//        return;
//      }
//
//
//      TRI_voc_tick_t const dbId = RocksDBLogValue::databaseId(blob);
//      TRI_voc_cid_t const collectionId = RocksDBLogValue::collectionId(blob);
//
//      TRI_vocbase_t* vocbase = state->useDatabase(databaseId);
//
//      if (!vocbase) {
//        // if the underlying database is gone, we can go on
//        LOG_TOPIC(TRACE, arangodb::Logger::ENGINES)
//            << "cannot create index for collection " << collectionId
//            << " in database " << databaseId << ": "
//            << TRI_errno_string(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
//        return;
//      }
//
//      auto* col = vocbase->lookupCollection(collectionId);
//
//      if (!col) {
//        // if the underlying collection gone, we can go on
//        LOG_TOPIC(TRACE, arangodb::Logger::ENGINES)
//            << "cannot create index for collection " << collectionId
//            << " in database " << databaseId << ": "
//            << TRI_errno_string(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
//        return;
//      }
//
//      TRI_idx_iid_t indexId = numericValue<TRI_idx_iid_t>(payloadSlice, "id");
//
//        if (state->isDropped(databaseId, collectionId)) {
//          return true;
//        }
//
//        LOG_TOPIC(TRACE, arangodb::Logger::ENGINES)
//            << "found create index marker. databaseId: " << databaseId
//            << ", collectionId: " << collectionId;
//
//
//
//        auto physical = static_cast<MMFilesCollection*>(col->getPhysical());
//        TRI_ASSERT(physical != nullptr);
//        MMFilesPersistentIndexFeature::dropIndex(databaseId, collectionId,
//                                                 indexId);
//
//        std::string const indexName("index-" + std::to_string(indexId) +
//                                    ".json");
//        std::string const filename(arangodb::basics::FileUtils::buildFilename(
//            physical->path(), indexName));
//
//        bool const forceSync = state->willBeDropped(databaseId, collectionId);
//        bool ok = arangodb::basics::VelocyPackHelper::velocyPackToFile(
//            filename, payloadSlice, forceSync);
//
//        if (!ok) {
//          LOG_TOPIC(WARN, arangodb::Logger::ENGINES)
//              << "cannot create index " << indexId << ", collection "
//              << collectionId << " in database " << databaseId;
//          ++state->errorCount;
//          return state->canContinue();
//        } else {
//          auto ctx = transaction::StandaloneContext::Create(vocbase);
//          arangodb::SingleCollectionTransaction trx(ctx, collectionId,
//                                                    AccessMode::Type::WRITE);
//          std::shared_ptr<arangodb::Index> unused;
//          int res = physical->restoreIndex(&trx, payloadSlice, unused);
//
//          if (res != TRI_ERROR_NO_ERROR) {
//            LOG_TOPIC(WARN, arangodb::Logger::ENGINES)
//                << "cannot create index " << indexId << ", collection "
//                << collectionId << " in database " << databaseId;
//            ++state->errorCount;
//            return state->canContinue();
//          }
//        }
    } break;
    case RocksDBLogType::IndexDrop: {
      const TRI_voc_tick_t dbId = RocksDBLogValue::databaseId(blob);
      const TRI_voc_cid_t cid = RocksDBLogValue::collectionId(blob);
      const TRI_idx_iid_t iid = RocksDBLogValue::indexId(blob);
      auto* link = lookupLink(*_dbFeature, dbId, cid, iid);

      if (!link) {
        LOG_TOPIC(TRACE, arangodb::iresearch::IResearchFeature::IRESEARCH)
            << "Collection '" << cid << "' does not contain index of type 'IResearchLink'"
            << "', index id '" << iid << "'";
        return;
      }

      // FIXME add non-const view() getter
      auto* view = const_cast<IResearchView*>(link->view());

      // remove link
      arangodb::velocypack::Builder linksBuilder;
      linksBuilder.openObject();
      linksBuilder.add(
        std::to_string(cid),
        arangodb::velocypack::Value(arangodb::velocypack::ValueType::Null)
      );
      linksBuilder.close();

      view->updateProperties(linksBuilder.slice(), true, false);

    } break;
    default: {
      // shut up the compiler
    } break;
  }
}
