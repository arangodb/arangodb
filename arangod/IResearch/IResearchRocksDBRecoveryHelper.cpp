////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include "ApplicationFeatures/ApplicationServer.h"
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
#include "Basics/DownCast.h"

namespace arangodb::transaction {
class Context;
}

namespace {

std::shared_ptr<arangodb::LogicalCollection> lookupCollection(
    arangodb::DatabaseFeature& db, arangodb::RocksDBEngine& engine,
    uint64_t objectId) {
  auto pair = engine.mapObjectToCollection(objectId);
  auto vocbase = db.useDatabase(pair.first);

  return vocbase ? vocbase->lookupCollection(pair.second) : nullptr;
}

std::vector<std::shared_ptr<arangodb::Index>> lookupLinks(
    arangodb::LogicalCollection& coll) {
  auto indexes = coll.getIndexes();

  // filter out non iresearch links
  const auto it = std::remove_if(
      indexes.begin(), indexes.end(),
      [](std::shared_ptr<arangodb::Index> const& idx) {
        return idx->type() !=
               arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK;
      });
  indexes.erase(it, indexes.end());

  return indexes;
}
}  // namespace

namespace arangodb {
namespace iresearch {

IResearchRocksDBRecoveryHelper::IResearchRocksDBRecoveryHelper(
    ArangodServer& server)
    : _server(server) {}

void IResearchRocksDBRecoveryHelper::prepare() {
  _dbFeature = &_server.getFeature<DatabaseFeature>();
  _engine =
      &_server.getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
  _documentCF = RocksDBColumnFamilyManager::get(
                    RocksDBColumnFamilyManager::Family::Documents)
                    ->GetID();
}

void IResearchRocksDBRecoveryHelper::PutCF(uint32_t column_family_id,
                                           const rocksdb::Slice& key,
                                           const rocksdb::Slice& value,
                                           rocksdb::SequenceNumber /*tick*/) {
  if (column_family_id != _documentCF) {
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
  auto doc = RocksDBValue::data(value);

  transaction::StandaloneContext ctx(coll->vocbase());

  SingleCollectionTransaction trx(std::shared_ptr<transaction::Context>(
                                      std::shared_ptr<transaction::Context>(),
                                      &ctx),  // aliasing ctor
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
    basics::downCast<IResearchRocksDBLink>(*link).insert(trx, nullptr, docId,
                                                         doc, {}, false);
  }

  res = trx.commit();

  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

// common implementation for DeleteCF / SingleDeleteCF
void IResearchRocksDBRecoveryHelper::handleDeleteCF(
    uint32_t column_family_id, const rocksdb::Slice& key,
    rocksdb::SequenceNumber /*tick*/) {
  if (column_family_id != _documentCF) {
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

  transaction::StandaloneContext ctx(coll->vocbase());

  SingleCollectionTransaction trx(std::shared_ptr<transaction::Context>(
                                      std::shared_ptr<transaction::Context>(),
                                      &ctx),  // aliasing ctor
                                  *coll, arangodb::AccessMode::Type::WRITE);

  Result res = trx.begin();

  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  for (std::shared_ptr<arangodb::Index> const& link : links) {
    IResearchLink& impl = basics::downCast<IResearchRocksDBLink>(*link);
    impl.remove(trx, docId, false);
  }

  res = trx.commit();

  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

void IResearchRocksDBRecoveryHelper::LogData(const rocksdb::Slice& blob,
                                             rocksdb::SequenceNumber tick) {
  RocksDBLogType const type = RocksDBLogValue::type(blob);

  switch (type) {
    case RocksDBLogType::IndexCreate: {
      // Intentional NOOP. Index is committed upon creation.
      // So if this marker was written  - index was persisted already.
    } break;
    case RocksDBLogType::CollectionTruncate: {
      TRI_ASSERT(_dbFeature);
      TRI_ASSERT(_engine);
      uint64_t objectId = RocksDBLogValue::objectId(blob);
      auto coll = lookupCollection(*_dbFeature, *_engine, objectId);

      if (coll != nullptr) {
        auto const links = lookupLinks(*coll);
        for (auto const& link : links) {
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
