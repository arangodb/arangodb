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
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchRocksDBRecoveryHelper.h"
#include "IResearch/IResearchLink.h"
#include "Indexes/Index.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBCollection.h"
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

namespace {

std::shared_ptr<arangodb::LogicalCollection> lookupCollection(
    arangodb::DatabaseFeature& db,
    arangodb::RocksDBEngine& engine,
    uint64_t objectId
) {
  auto pair = engine.mapObjectToCollection(objectId);
  auto vocbase = db.useDatabase(pair.first);

  return vocbase ? vocbase->lookupCollection(pair.second) : nullptr;
}

std::vector<std::shared_ptr<arangodb::Index>> lookupLinks(
    arangodb::LogicalCollection& coll
) {
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

std::shared_ptr<arangodb::iresearch::IResearchLink> lookupLink(
    TRI_vocbase_t& vocbase,
    TRI_voc_cid_t cid,
    TRI_idx_iid_t iid
) {
  auto col = vocbase.lookupCollection(cid);

  if (!col) {
    // invalid cid
    return nullptr;
  }

  for (auto& index: col->getIndexes()) {
    if (!index || arangodb::Index::TRI_IDX_TYPE_IRESEARCH_LINK != index->type()) {
      continue; // not an IRresearch Link
    }

    // TODO FIXME find a better way to retrieve an iResearch Link
    // cannot use static_cast/reinterpret_cast since Index is not related to IResearchLink
    auto link =
      std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);

    if (link && link->id() == iid) {
      return link; // found required link
    }
  }

  return nullptr;
}

void ensureLink(
    arangodb::DatabaseFeature& db,
    std::set<arangodb::iresearch::IResearchRocksDBRecoveryHelper::IndexId>& recoveredIndexes,
    TRI_voc_tick_t dbId,
    TRI_voc_cid_t cid,
    arangodb::velocypack::Slice indexSlice
) {
  if (!indexSlice.isObject()) {
    LOG_TOPIC(WARN, arangodb::Logger::ENGINES)
        << "Cannot recover index for the collection '" << cid
        << "' in the database '" << dbId << "' : invalid marker";
    return;
  }

  // ensure null terminated string
  auto const indexTypeSlice =
    indexSlice.get(arangodb::StaticStrings::IndexType);
  auto const indexTypeStr = indexTypeSlice.copyString();
  auto const indexType = arangodb::Index::type(indexTypeStr.c_str());

  if (arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK != indexType) {
    // skip non iresearch link indexes
    return;
  }

  TRI_idx_iid_t iid;
  auto const idSlice = indexSlice.get("id");

  if (idSlice.isString()) {
    iid = static_cast<TRI_idx_iid_t>(std::stoull(idSlice.copyString()));
  } else if (idSlice.isNumber()) {
    iid = idSlice.getNumber<TRI_idx_iid_t>();
  } else {
    LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
        << "Cannot recover index for the collection '" << cid
        << "' in the database '" << dbId
        << "' : invalid value for attribute 'id', expected 'String' or 'Number', got '"
        << idSlice.typeName() << "'";
    return;
  }

  if (!recoveredIndexes.emplace(dbId, cid, iid).second) {
    // already there
    LOG_TOPIC(TRACE, arangodb::iresearch::TOPIC)
        << "Index of type 'IResearchLink' with id `" << iid
        << "' in the collection '" << cid
        << "' in the database '" << dbId
        << "' already exists: skipping create marker";
    return;
  }

  TRI_vocbase_t* vocbase = db.useDatabase(dbId);

  if (!vocbase) {
    // if the underlying database is gone, we can go on
    LOG_TOPIC(TRACE, arangodb::iresearch::TOPIC)
        << "Cannot create index for the collection '" << cid
        << "' in the database '" << dbId << "' : "
        << TRI_errno_string(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    return;
  }

  auto col = vocbase->lookupCollection(cid);

  if (!col) {
    // if the underlying collection gone, we can go on
    LOG_TOPIC(TRACE, arangodb::iresearch::TOPIC)
        << "Cannot create index for the collection '" << cid
        << "' in the database '" << dbId << "' : "
        << TRI_errno_string(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    return;
  }

  auto link = lookupLink(*vocbase, cid, iid);

  if (!link) {
    LOG_TOPIC(TRACE, arangodb::iresearch::TOPIC)
        << "Collection '" << cid
        << "' in the database '" << dbId
        << "' does not contain index of type 'IResearchLink' with id '" << iid << "': skip create marker";
    return;
  }

  LOG_TOPIC(TRACE, arangodb::iresearch::TOPIC)
      << "found create index marker, databaseId: '" << dbId
      << "', collectionId: '" << cid << "'";

  arangodb::velocypack::Builder json;

  json.openObject();

  if (!link->json(json)) {
    LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
        << "Failed to generate jSON definition for link '" << iid
        << "' to the collection '" << cid
        << "' in the database '" << dbId;
    return;
  }

  json.close();

  bool created;
  // re-insert link
  if (!col->dropIndex(link->id())
      || !col->createIndex(json.slice(), created)
      || !created) {
    LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
        << "Failed to recreate the link '" << iid
        << "' to the collection '" << cid
        << "' in the database '" << dbId;
  }
}

void dropCollectionFromAllViews(
    arangodb::DatabaseFeature& db,
    TRI_voc_tick_t dbId,
    TRI_voc_cid_t collectionId
) {
  auto* vocbase = db.useDatabase(dbId);
  if (!vocbase) {
    return;
  }
  TRI_DEFER(vocbase->release());

  // iterate over vocbase views
  arangodb::LogicalView::enumerate(
    *vocbase,
    [collectionId](arangodb::LogicalView::ptr const& view)->bool {
      #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
        auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(view.get());
      #else
        auto* impl = static_cast<arangodb::iresearch::IResearchView*>(view.get());
      #endif

      if (!impl) {
        LOG_TOPIC(TRACE, arangodb::iresearch::TOPIC)
            << "error finding view: '" << view->name() << "': not an arangosearch view";

        return true;
      }

      LOG_TOPIC(TRACE, arangodb::iresearch::TOPIC)
        << "Removing all documents belonging to view '" << view->name() << "' sourced from collection " << collectionId;

      impl->drop(collectionId);

      return true;
    }
  );
}

void dropCollectionFromView(
    arangodb::DatabaseFeature& db,
    TRI_voc_tick_t dbId,
    TRI_voc_cid_t collectionId,
    TRI_idx_iid_t indexId,
    TRI_voc_cid_t viewId
) {
  auto* vocbase = db.useDatabase(dbId);

  if (vocbase) {
    auto logicalCollection = vocbase->lookupCollection(collectionId);

    if (!logicalCollection) {
      LOG_TOPIC(TRACE, arangodb::iresearch::TOPIC)
          << "error looking up collection '" << collectionId << "': no such collection";
      return;
    }

    auto link = lookupLink(*vocbase, collectionId, indexId);

    if (link) {
      // don't remove the link if it's there
      LOG_TOPIC(TRACE, arangodb::iresearch::TOPIC)
          << "found link '" << indexId
          << "' of type arangosearch in collection '" << collectionId
          << "' in database '" << dbId << "': skipping drop marker";
      return;
    }

    auto logicalView = vocbase->lookupView(viewId);

    if (!logicalView
        || arangodb::iresearch::DATA_SOURCE_TYPE != logicalView->type()) {
      LOG_TOPIC(TRACE, arangodb::iresearch::TOPIC)
          << "error looking up view '" << viewId << "': no such view";
      return;
    }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
#else
    auto* view = static_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
#endif

    if (!view) {
      LOG_TOPIC(TRACE, arangodb::iresearch::TOPIC)
          << "error finding view: '" << viewId << "': not an iresearch view";
      return;
    }

    LOG_TOPIC(TRACE, arangodb::iresearch::TOPIC)
        << "Removing all documents belonging to view " << viewId
        << " sourced from collection " << collectionId;

    view->drop(collectionId);
  }
}

}

namespace arangodb {
namespace iresearch {

void IResearchRocksDBRecoveryHelper::prepare() {
  _dbFeature = DatabaseFeature::DATABASE,
  _engine = static_cast<RocksDBEngine*>(EngineSelectorFeature::ENGINE),
  _documentCF = RocksDBColumnFamily::documents()->GetID();
}

void IResearchRocksDBRecoveryHelper::PutCF(uint32_t column_family_id,
                                           const rocksdb::Slice& key,
                                           const rocksdb::Slice& value) {
  if (column_family_id == _documentCF) {
    auto coll =
      lookupCollection(*_dbFeature, *_engine, RocksDBKey::objectId(key));

    if (coll == nullptr) {
      return;
    }

    auto const links = lookupLinks(*coll);

    if (links.empty()) {
      return;
    }

    auto docId = RocksDBKey::documentId(key);
    auto doc = RocksDBValue::data(value);
    SingleCollectionTransaction trx(
      transaction::StandaloneContext::Create(coll->vocbase()),
      *coll,
      arangodb::AccessMode::Type::WRITE
    );

    trx.begin();

    for (auto link : links) {
      IndexId indexId(coll->vocbase().id(), coll->id(), link->id());

      // optimization: avoid insertion of recovered documents twice,
      //               first insertion done during index creation
      if (!link || _recoveredIndexes.find(indexId) != _recoveredIndexes.end()) {
        continue; // index was already populated when it was created
      }

      link->insert(
        &trx,
        docId,
        doc,
        Index::OperationMode::internal
      );
    }

    trx.commit();

    return;
  }
}

// common implementation for DeleteCF / SingleDeleteCF
void IResearchRocksDBRecoveryHelper::handleDeleteCF(uint32_t column_family_id,
                                                    const rocksdb::Slice& key) {
  if (column_family_id == _documentCF) {
    return;
  }
  auto coll =
    lookupCollection(*_dbFeature, *_engine, RocksDBKey::objectId(key));

  if (coll == nullptr) {
    return;
  }

  auto const links = lookupLinks(*coll);

  if (links.empty()) {
    return;
  }

  auto docId = RocksDBKey::documentId(key);
  SingleCollectionTransaction trx(
    transaction::StandaloneContext::Create(coll->vocbase()),
    *coll,
    arangodb::AccessMode::Type::WRITE
  );

  trx.begin();

  for (auto link : links) {
    link->remove(
      &trx,
      docId,
      arangodb::velocypack::Slice::emptyObjectSlice(),
      Index::OperationMode::internal
    );
  }

  trx.commit();
}

void IResearchRocksDBRecoveryHelper::DeleteRangeCF(uint32_t column_family_id,
                                                   const rocksdb::Slice& end_key,
                                                   const rocksdb::Slice& begin_key) {
  // not needed for anything atm
}

void IResearchRocksDBRecoveryHelper::LogData(const rocksdb::Slice& blob) {
  TRI_ASSERT(_dbFeature);

  RocksDBLogType const type = RocksDBLogValue::type(blob);

  switch (type) {
    case RocksDBLogType::CollectionDrop: {
      // find database, iterate over all extant views and drop collection
      TRI_voc_tick_t const dbId = RocksDBLogValue::databaseId(blob);
      TRI_voc_cid_t const collectionId = RocksDBLogValue::collectionId(blob);
      dropCollectionFromAllViews(*_dbFeature, dbId, collectionId);
    } break;
    case RocksDBLogType::IndexCreate: {
      TRI_voc_tick_t const dbId = RocksDBLogValue::databaseId(blob);
      TRI_voc_cid_t const collectionId = RocksDBLogValue::collectionId(blob);
      auto const indexSlice = RocksDBLogValue::indexSlice(blob);
      ensureLink(*_dbFeature, _recoveredIndexes, dbId, collectionId, indexSlice);
    } break;
    case RocksDBLogType::IResearchLinkDrop: {
      // check if view still exists, if not ignore
      TRI_voc_tick_t const dbId = RocksDBLogValue::databaseId(blob);
      TRI_voc_cid_t const collectionId = RocksDBLogValue::collectionId(blob);
      TRI_voc_cid_t const viewId = RocksDBLogValue::viewId(blob);
      TRI_idx_iid_t const indexId = RocksDBLogValue::indexId(blob);
      dropCollectionFromView(*_dbFeature, dbId, collectionId, indexId, viewId);
    } break;
    case RocksDBLogType::CollectionTruncate: {
      uint64_t objectId = RocksDBLogValue::objectId(blob);
      auto coll = lookupCollection(*_dbFeature, *_engine, objectId);

      if (coll != nullptr) {
        auto const links = lookupLinks(*coll);
        for (auto link : links) {
          link->afterTruncate(/*tick*/0); // tick isn't used for links
        }
      }

      break;
    }
    default: break; // shut up the compiler
  }
}

} // iresearch
} // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------