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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"  // required for RocksDBColumnFamily.h
#include "IResearch/IResearchFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBColumnFamily.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"

#include "IResearchRocksDBLink.h"
#include "IResearchLinkHelper.h"

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

IResearchRocksDBLink::IResearchRocksDBLink(
    TRI_idx_iid_t iid,
    arangodb::LogicalCollection& collection
)
    : RocksDBIndex(iid, collection, IResearchLinkHelper::emptyIndexSlice(),
                   RocksDBColumnFamily::invalid(), false),
      IResearchLink(iid, collection) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  _unique = false;  // cannot be unique since multiple fields are indexed
  _sparse = true;   // always sparse
}

IResearchRocksDBLink::~IResearchRocksDBLink() {
  // NOOP
}

/*static*/ IResearchRocksDBLink::ptr IResearchRocksDBLink::make(
    arangodb::LogicalCollection& collection,
    arangodb::velocypack::Slice const& definition,
    TRI_idx_iid_t id,
    bool isClusterConstructor
) noexcept {
  try {
    PTR_NAMED(IResearchRocksDBLink, ptr, id, collection);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto* link =
        dynamic_cast<arangodb::iresearch::IResearchRocksDBLink*>(ptr.get());
#else
    auto* link =
        static_cast<arangodb::iresearch::IResearchRocksDBLink*>(ptr.get());
#endif

    return link && link->init(definition) ? ptr : nullptr;
  } catch (arangodb::basics::Exception& e) {
    LOG_TOPIC(WARN, Logger::DEVEL)
      << "caught exception while creating IResearch view RocksDB link '" << id << "': " << e.code() << " " << e.what();
    IR_LOG_EXCEPTION();
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, Logger::DEVEL)
      << "caught exception while creating IResearch view RocksDB link '" << id << "': " << e.what();
    IR_LOG_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, Logger::DEVEL)
      << "caught exception while creating IResearch view RocksDB link '" << id << "'";
    IR_LOG_EXCEPTION();
  }

  return nullptr;
}

void IResearchRocksDBLink::toVelocyPack(arangodb::velocypack::Builder& builder,
                                        bool withFigures,
                                        bool forPersistence) const {
  TRI_ASSERT(!builder.isOpenObject());
  builder.openObject();
  bool success = json(builder, forPersistence);
  TRI_ASSERT(success);

  if (withFigures) {
    VPackBuilder figuresBuilder;

    figuresBuilder.openObject();
    toVelocyPackFigures(figuresBuilder);
    figuresBuilder.close();
    builder.add("figures", figuresBuilder.slice());
  }

  builder.close();
}

void IResearchRocksDBLink::writeRocksWalMarker() {
  RocksDBLogValue logValue = RocksDBLogValue::IResearchLinkDrop(
    Index::_collection.vocbase().id(),
    Index::_collection.id(),
    view() ? view()->id() : 0, // 0 == invalid TRI_voc_cid_t according to transaction::Methods
    Index::_iid
  );
  rocksdb::WriteBatch batch;
  rocksdb::WriteOptions wo;  // TODO: check which options would make sense
  auto db = rocksutils::globalRocksDB();

  batch.PutLogData(logValue.slice());

  auto status = rocksutils::convertStatus(db->Write(wo, &batch));

  if (!status.ok()) {
    THROW_ARANGO_EXCEPTION(status.errorNumber());
  }
}

NS_END      // iresearch
NS_END  // arangodb

// -----------------------------------------------------------------------------
// --SECTION-- END-OF-FILE
// -----------------------------------------------------------------------------