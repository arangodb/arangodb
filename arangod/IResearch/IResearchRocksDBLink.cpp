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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"  // required for RocksDBColumnFamilyManager.h
#include "IResearchLinkHelper.h"
#include "IResearchView.h"
#include "Indexes/IndexFactory.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"

#include "IResearchRocksDBLink.h"
#include "IResearchRocksDBEncryption.h"

namespace arangodb {
namespace iresearch {

IResearchRocksDBLink::IResearchRocksDBLink(IndexId iid,
                                           LogicalCollection& collection,
                                           uint64_t objectId)
    : RocksDBIndex(iid, collection,
                   IResearchLinkHelper::emptyIndexSlice(objectId).slice(),
                   RocksDBColumnFamilyManager::get(
                       RocksDBColumnFamilyManager::Family::Invalid),
                   false),
      IResearchLink(iid, collection) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  _unique = false;  // cannot be unique since multiple fields are indexed
  _sparse = true;   // always sparse
}

void IResearchRocksDBLink::toVelocyPack(
    VPackBuilder& builder,
    std::underlying_type<Index::Serialize>::type flags) const {
  if (builder.isOpenObject()) {
    THROW_ARANGO_EXCEPTION(Result(  // result
        TRI_ERROR_BAD_PARAMETER,    // code
        std::string("failed to generate link definition for arangosearch view "
                    "RocksDB link '") +
            std::to_string(Index::id().id()) + "'"));
  }

  auto forPersistence = Index::hasFlag(flags, Index::Serialize::Internals);

  builder.openObject();

  if (!IResearchLink::properties(builder, forPersistence).ok()) {
    THROW_ARANGO_EXCEPTION(Result(
        TRI_ERROR_INTERNAL,
        std::string("failed to generate link definition for arangosearch view "
                    "RocksDB link '") +
            std::to_string(Index::id().id()) + "'"));
  }

  if (Index::hasFlag(flags, Index::Serialize::Internals)) {
    TRI_ASSERT(objectId() != 0);  // If we store it, it cannot be 0
    builder.add("objectId", VPackValue(std::to_string(objectId())));
  }

  if (Index::hasFlag(flags, Index::Serialize::Figures)) {
    builder.add("figures", VPackValue(VPackValueType::Object));
    toVelocyPackFigures(builder);
    builder.close();
  }

  builder.close();
}

IResearchRocksDBLink::IndexFactory::IndexFactory(
    application_features::ApplicationServer& server)
    : IndexTypeFactory(server) {}

bool IResearchRocksDBLink::IndexFactory::equal(
    VPackSlice lhs, VPackSlice rhs, std::string const& dbname) const {
  return IResearchLinkHelper::equal(_server, lhs, rhs, dbname);
}

std::shared_ptr<Index> IResearchRocksDBLink::IndexFactory::instantiate(
    LogicalCollection& collection, VPackSlice definition, IndexId id,
    bool /*isClusterConstructor*/) const {
  uint64_t objectId = basics::VelocyPackHelper::stringUInt64(
      definition, arangodb::StaticStrings::ObjectId);
  auto link = std::make_shared<IResearchRocksDBLink>(id, collection, objectId);

  auto const res =
      link->init(definition, [this]() -> irs::directory_attributes {
        auto& selector = _server.getFeature<EngineSelectorFeature>();
        TRI_ASSERT(selector.isRocksDB());
        auto& engine = selector.engine<RocksDBEngine>();
        auto* encryption = engine.encryptionProvider();
        if (encryption) {
          return irs::directory_attributes{
              0, std::make_unique<RocksDBEncryptionProvider>(
                     *encryption, engine.rocksDBOptions())};
        }
        return irs::directory_attributes{};
      });

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  return link;
}

Result IResearchRocksDBLink::IndexFactory::normalize(
    VPackBuilder& normalized, VPackSlice definition, bool isCreation,
    TRI_vocbase_t const& vocbase) const {
  // no attribute set in a definition -> old version
  constexpr LinkVersion defaultVersion = LinkVersion::MIN;

  return IResearchLinkHelper::normalize(normalized, definition, isCreation,
                                        vocbase, defaultVersion);
}

std::shared_ptr<IResearchRocksDBLink::IndexFactory>
IResearchRocksDBLink::createFactory(
    application_features::ApplicationServer& server) {
  return std::shared_ptr<IResearchRocksDBLink::IndexFactory>(
      new IResearchRocksDBLink::IndexFactory(server));
}
}  // namespace iresearch
}  // namespace arangodb
