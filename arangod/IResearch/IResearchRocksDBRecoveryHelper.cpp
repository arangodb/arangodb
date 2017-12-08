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
    auto pair = lookupDatabaseAndCollection(RocksDBKey::objectId(key));
    TRI_vocbase_t* vocbase = pair.first;
    LogicalCollection* coll = pair.second;
    if (coll == nullptr) {
      return;
    }

    std::vector<IResearchLink*> links = lookupLinks(coll);
    if (links.size() == 0) {
      return;
    }

    auto rev = RocksDBKey::revisionId(RocksDBEntryType::Document, key);
    auto doc = RocksDBValue::data(value);

    SingleCollectionTransaction trx(
        transaction::StandaloneContext::Create(vocbase), coll->cid(),
        arangodb::AccessMode::Type::WRITE);
    for (auto link : links) {
      link->insert(&trx, LocalDocumentId(rev), doc,
                   Index::OperationMode::internal);
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
    auto pair = lookupDatabaseAndCollection(RocksDBKey::objectId(key));
    TRI_vocbase_t* vocbase = pair.first;
    LogicalCollection* coll = pair.second;
    if (coll == nullptr) {
      return;
    }

    std::vector<IResearchLink*> links = lookupLinks(coll);
    if (links.size() == 0) {
      return;
    }

    auto rev = RocksDBKey::revisionId(RocksDBEntryType::Document, key);

    SingleCollectionTransaction trx(
        transaction::StandaloneContext::Create(vocbase), coll->cid(),
        arangodb::AccessMode::Type::WRITE);
    for (auto link : links) {
      link->remove(&trx, LocalDocumentId(rev), Index::OperationMode::internal);
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
  RocksDBLogType type = RocksDBLogValue::type(blob);
  if (type == RocksDBLogType::IResearchLinkDrop) {
    // check if view still exists, if not ignore
    TRI_voc_tick_t dbId = RocksDBLogValue::databaseId(blob);
    TRI_voc_cid_t collectionId = RocksDBLogValue::collectionId(blob);
    TRI_voc_cid_t viewId = RocksDBLogValue::viewId(blob);
    dropCollectionFromView(dbId, collectionId, viewId);
  } else if (type == RocksDBLogType::CollectionDrop) {
    // find database, iterate over all extant views and drop collection
    TRI_voc_tick_t dbId = RocksDBLogValue::databaseId(blob);
    TRI_voc_cid_t collectionId = RocksDBLogValue::collectionId(blob);
    dropCollectionFromAllViews(dbId, collectionId);
  }
}

std::pair<TRI_vocbase_t*, LogicalCollection*>
IResearchRocksDBRecoveryHelper::lookupDatabaseAndCollection(
    uint64_t objectId) const {
  auto pair = _engine->mapObjectToCollection(objectId);

  auto vocbase = _dbFeature->useDatabase(pair.first);
  if (vocbase == nullptr) {
    return std::make_pair(nullptr, nullptr);
  }

  return std::make_pair(vocbase, vocbase->lookupCollection(pair.second));
}

std::vector<IResearchLink*> IResearchRocksDBRecoveryHelper::lookupLinks(
    LogicalCollection* coll) const {
  std::vector<IResearchLink*> links{};

  for (auto idx : coll->getIndexes()) {
    if (idx->type() == Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK) {
      links.emplace_back(dynamic_cast<IResearchLink*>(idx.get()));
    }
  }

  return links;
}

void IResearchRocksDBRecoveryHelper::dropCollectionFromAllViews(
    TRI_voc_tick_t dbId, TRI_voc_cid_t collectionId) {
  auto vocbase = _dbFeature->useDatabase(dbId);
  if (vocbase) {
    // iterate over vocbase views
    for (auto logicalView : vocbase->views()) {
      if (IResearchView::type() != logicalView->type()) {
        continue;
      }
      auto* view =
          static_cast<IResearchView*>(logicalView->getImplementation());
      LOG_TOPIC(TRACE, arangodb::iresearch::IResearchFeature::IRESEARCH)
          << "Removing all documents belonging to view " << view->id()
          << " sourced from collection " << collectionId;
      view->drop(collectionId);
    }
  }
}

void IResearchRocksDBRecoveryHelper::dropCollectionFromView(
    TRI_voc_tick_t dbId, TRI_voc_cid_t collectionId, TRI_voc_cid_t viewId) {
  auto vocbase = _dbFeature->useDatabase(dbId);
  if (vocbase) {
    auto logicalView = vocbase->lookupView(viewId);

    if (!logicalView || IResearchView::type() != logicalView->type()) {
      LOG_TOPIC(TRACE, arangodb::iresearch::IResearchFeature::IRESEARCH)
          << "error looking up view '" << viewId << "': no such view";
      return;
    }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto* view = dynamic_cast<IResearchView*>(logicalView->getImplementation());
#else
    auto* view = static_cast<IResearchView*>(logicalView->getImplementation());
#endif

    if (!view) {
      LOG_TOPIC(TRACE, arangodb::iresearch::IResearchFeature::IRESEARCH)
          << "error finding view: '" << viewId << "': not an iresearch view";
      return;
    }

    LOG_TOPIC(TRACE, arangodb::iresearch::IResearchFeature::IRESEARCH)
        << "Removing all documents belonging to view " << viewId
        << " sourced from collection " << collectionId;

    view->drop(collectionId);
  }
}
