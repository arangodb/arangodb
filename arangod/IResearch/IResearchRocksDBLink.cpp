////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Common.h"  // required for RocksDBColumnFamilyManager.h
#include "IResearch/IResearchLinkHelper.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchRocksDBLink.h"
#include "IResearch/IResearchRocksDBEncryption.h"
#include "Indexes/IndexFactory.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "IResearch/IResearchFeature.h"

#include <absl/strings/str_cat.h>

namespace arangodb::iresearch {

IResearchRocksDBLink::IResearchRocksDBLink(IndexId iid,
                                           LogicalCollection& collection,
                                           uint64_t objectId)
    : RocksDBIndex{iid, collection,
                   IResearchLinkHelper::emptyIndexSlice(objectId).slice(),
                   RocksDBColumnFamilyManager::get(
                       RocksDBColumnFamilyManager::Family::Invalid),
                   /*useCache*/ false,
                   /*cacheManager*/ nullptr,
                   /*engine*/
                   collection.vocbase().engine<RocksDBEngine>()},
      IResearchLink{collection.vocbase().server(), collection} {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  _unique = false;  // cannot be unique since multiple fields are indexed
  _sparse = true;   // always sparse
}

void IResearchRocksDBLink::toVelocyPack(
    VPackBuilder& builder,
    std::underlying_type_t<Index::Serialize> flags) const {
  if (builder.isOpenObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat("failed to generate link definition for arangosearch view "
                     "RocksDB link '",
                     id().id(), "'"));
  }

  auto forPersistence = Index::hasFlag(flags, Index::Serialize::Internals);

  builder.openObject();

  if (!IResearchLink::properties(builder, forPersistence).ok()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, absl::StrCat("failed to generate link definition "
                                         "for arangosearch view RocksDB link '",
                                         id().id(), "'"));
  }

  if (Index::hasFlag(flags, Index::Serialize::Internals)) {
    TRI_ASSERT(objectId() != 0);  // If we store it, it cannot be 0
    builder.add("objectId", VPackValue(absl::AlphaNum{objectId()}.Piece()));
  }

  if (Index::hasFlag(flags, Index::Serialize::Figures)) {
    builder.add("figures", VPackValue(VPackValueType::Object));
    toVelocyPackFigures(builder);
    builder.close();
  }

  builder.close();
}

IResearchRocksDBLink::IndexFactory::IndexFactory(ArangodServer& server)
    : IndexTypeFactory(server) {}

bool IResearchRocksDBLink::IndexFactory::equal(
    VPackSlice lhs, VPackSlice rhs, std::string const& dbname) const {
  return IResearchLinkHelper::equal(_server, lhs, rhs, dbname);
}

std::shared_ptr<Index> IResearchRocksDBLink::IndexFactory::instantiate(
    LogicalCollection& collection, VPackSlice definition, IndexId id,
    bool isOpening) const {
  uint64_t objectId = basics::VelocyPackHelper::stringUInt64(
      definition, arangodb::StaticStrings::ObjectId);
  auto link = std::make_shared<IResearchRocksDBLink>(id, collection, objectId);
  bool pathExists = false;
  auto cleanup = scopeGuard([&]() noexcept {
    if (pathExists) {
      try {
        link->unload();  // TODO(MBkkt) unload should be implicit noexcept?
      } catch (...) {
      }
    } else {
      link->drop();
    }
  });
  if (!isOpening) {
    link->setBuilding(true);
  }
  auto const res = link->init(
      definition, pathExists, [&collection]() -> irs::directory_attributes {
        auto& engine = collection.vocbase().engine<RocksDBEngine>();
        auto* encryption = engine.encryptionProvider();
        if (encryption) {
          return irs::directory_attributes{
              std::make_unique<RocksDBEncryptionProvider>(
                  *encryption, engine.rocksDBOptions())};
        }
        return irs::directory_attributes{};
      });
  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }
  cleanup.cancel();
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
IResearchRocksDBLink::createFactory(ArangodServer& server) {
  return std::shared_ptr<IResearchRocksDBLink::IndexFactory>(
      new IResearchRocksDBLink::IndexFactory(server));
}
}  // namespace arangodb::iresearch
