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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchRocksDBInvertedIndex.h"
#include "IResearchRocksDBEncryption.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb {
namespace iresearch {

IResearchRocksDBInvertedIndexFactory::IResearchRocksDBInvertedIndexFactory(
    application_features::ApplicationServer& server)
    : IndexTypeFactory(server) {}

bool IResearchRocksDBInvertedIndexFactory::equal(
    velocypack::Slice lhs, velocypack::Slice rhs,
    std::string const& dbname) const {
  IResearchInvertedIndexMeta lhsFieldsMeta;
  std::string errField;
  if (!lhsFieldsMeta.init(_server, lhs, true, errField, dbname)) {
    LOG_TOPIC("79384", ERR, iresearch::TOPIC)
        << (errField.empty()
                ? (std::string(
                       "failed to initialize index fields from definition: ") +
                   lhs.toString())
                : (std::string("failed to initialize index fields from "
                               "definition, error in attribute '") +
                   errField + "': " + lhs.toString()));
    return false;
  }

  IResearchInvertedIndexMeta rhsFieldsMeta;
  if (!rhsFieldsMeta.init(_server, rhs, true, errField, dbname)) {
    LOG_TOPIC("31eaf", ERR, iresearch::TOPIC)
        << (errField.empty()
                ? (std::string(
                       "failed to initialize index fields from definition: ") +
                   rhs.toString())
                : (std::string("failed to initialize index fields from "
                               "definition, error in attribute '") +
                   errField + "': " + rhs.toString()));
    return false;
  }

  return lhsFieldsMeta == rhsFieldsMeta;
}

std::shared_ptr<Index> IResearchRocksDBInvertedIndexFactory::instantiate(
    LogicalCollection& collection, velocypack::Slice definition, IndexId id,
    bool isClusterConstructor) const {
  auto const clusterWideIndex =
      collection.id() == collection.planId() && collection.isAStub();
  IResearchViewMeta indexMeta;
  std::string errField;
  if (!indexMeta.init(definition, errField)) {
    LOG_TOPIC("d2b8a", ERR, iresearch::TOPIC)
        << (errField.empty()
                ? (std::string(
                       "failed to initialize index meta from definition: ") +
                   definition.toString())
                : (std::string("failed to initialize index meta from "
                               "definition, error in attribute '") +
                   errField + "': " + definition.toString()));
    return nullptr;
  }
  IResearchInvertedIndexMeta fieldsMeta;
  if (!fieldsMeta.init(_server, definition, true, errField,
                       collection.vocbase().name())) {
    LOG_TOPIC("18c17", ERR, iresearch::TOPIC)
        << (errField.empty()
                ? (std::string(
                       "failed to initialize index fields from definition: ") +
                   definition.toString())
                : (std::string("failed to initialize index fields from "
                               "definition, error in attribute '") +
                   errField + "': " + definition.toString()));
    return nullptr;
  }
  auto nameSlice = definition.get(arangodb::StaticStrings::IndexName);
  std::string indexName;
  if (!nameSlice.isNone()) {
    if (!nameSlice.isString() || nameSlice.getStringLength() == 0) {
      LOG_TOPIC("91ebd", ERR, iresearch::TOPIC)
          << "failed to initialize index from definition, error in attribute "
             "'" +
                 arangodb::StaticStrings::IndexName +
                 "': " + definition.toString();
      return nullptr;
    }
    indexName = nameSlice.copyString();
  }

  auto objectId = basics::VelocyPackHelper::stringUInt64(
      definition, arangodb::StaticStrings::ObjectId);
  auto index = std::make_shared<IResearchRocksDBInvertedIndex>(
      id, collection, objectId, indexName, std::move(fieldsMeta));
  if (!clusterWideIndex) {
    auto initRes = index->init([this]() -> irs::directory_attributes {
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

    if (initRes.fail()) {
      LOG_TOPIC("9c9ac", ERR, iresearch::TOPIC)
          << "failed to do an init iresearch data store: "
          << initRes.errorMessage();
      return nullptr;
    }

    // separate call as here object should be fully constructed and could be
    // safely used by async tasks
    initRes = index->properties(indexMeta);
    if (initRes.fail()) {
      LOG_TOPIC("9c949", ERR, iresearch::TOPIC)
          << "failed to initialize index from definition, error setting index "
             "properties: "
          << initRes.errorMessage();
      return nullptr;
    }
  }
  return index;
}

Result IResearchRocksDBInvertedIndexFactory::normalize(
    velocypack::Builder& normalized, velocypack::Slice definition,
    bool isCreation, TRI_vocbase_t const& vocbase) const {
  TRI_ASSERT(normalized.isOpenObject());

  auto res = IndexFactory::validateFieldsDefinition(
      definition, arangodb::StaticStrings::IndexFields, 1, SIZE_MAX, true);
  if (res.fail()) {
    return res;
  }

  IResearchViewMeta tmpMeta;
  std::string errField;
  if (!tmpMeta.init(definition, errField)) {
    return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        errField.empty()
            ? (std::string("failed to normalize index meta from definition: ") +
               definition.toString())
            : (std::string("failed to normalize index meta from definition, "
                           "error in attribute '") +
               errField + "': " + definition.toString()));
  }
  if (!tmpMeta.json(normalized)) {
    return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        std::string("failed to write normalized index meta from definition: ") +
            definition.toString());
  }
  IResearchInvertedIndexMeta tmpLinkMeta;
  if (!tmpLinkMeta.init(_server, definition,
                        arangodb::ServerState::instance()->isDBServer(),
                        errField, vocbase.name())) {
    return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        errField.empty()
            ? (std::string(
                   "failed to normalize index fields from definition: ") +
               definition.toString())
            : (std::string("failed to normalize index fields from definition, "
                           "error in attribute '") +
               errField + "': " + definition.toString()));
  }
  if (!tmpLinkMeta.json(_server, normalized, isCreation, &vocbase)) {
    return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        std::string(
            "failed to write normalized index fields from definition: ") +
            definition.toString());
  }
  auto nameSlice = definition.get(arangodb::StaticStrings::IndexName);
  if (nameSlice.isString() && nameSlice.getStringLength() > 0) {
    normalized.add(arangodb::StaticStrings::IndexName, nameSlice);
  } else if (!nameSlice.isNone()) {
    return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        std::string(
            "failed to normalize index from definition, error in attribute '") +
            arangodb::StaticStrings::IndexName + "': " + definition.toString());
  }

  normalized.add(arangodb::StaticStrings::IndexType,
                 arangodb::velocypack::Value(arangodb::Index::oldtypeName(
                     arangodb::Index::TRI_IDX_TYPE_INVERTED_INDEX)));

  if (isCreation && !arangodb::ServerState::instance()->isCoordinator() &&
      !definition.hasKey(arangodb::StaticStrings::ObjectId)) {
    normalized.add(arangodb::StaticStrings::ObjectId,
                   VPackValue(std::to_string(TRI_NewTickServer())));
  }

  normalized.add(arangodb::StaticStrings::IndexSparse,
                 arangodb::velocypack::Value(true));
  normalized.add(arangodb::StaticStrings::IndexUnique,
                 arangodb::velocypack::Value(false));

  bool bck = basics::VelocyPackHelper::getBooleanValue(
      definition, arangodb::StaticStrings::IndexInBackground, false);
  normalized.add(arangodb::StaticStrings::IndexInBackground, VPackValue(bck));
  return res;
}

IResearchRocksDBInvertedIndex::IResearchRocksDBInvertedIndex(
    IndexId id, LogicalCollection& collection, uint64_t objectId,
    std::string const& name, IResearchInvertedIndexMeta&& m)
    : IResearchInvertedIndex(id, collection,
                             std::forward<IResearchInvertedIndexMeta>(m)),
      RocksDBIndex(id, collection, name, IResearchInvertedIndex::fields(meta()),
                   false, true,
                   RocksDBColumnFamilyManager::get(
                       RocksDBColumnFamilyManager::Family::Invalid),
                   objectId, false) {}

void IResearchRocksDBInvertedIndex::toVelocyPack(
    VPackBuilder& builder,
    std::underlying_type<Index::Serialize>::type flags) const {
  auto const forPersistence =
      Index::hasFlag(flags, Index::Serialize::Internals);
  VPackObjectBuilder objectBuilder(&builder);
  IResearchInvertedIndex::toVelocyPack(
      IResearchDataStore::collection().vocbase().server(),
      &IResearchDataStore::collection().vocbase(), builder, forPersistence);
  if (forPersistence) {
    TRI_ASSERT(objectId() != 0);  // If we store it, it cannot be 0
    builder.add(arangodb::StaticStrings::ObjectId,
                VPackValue(std::to_string(objectId())));
  }
  // can't use Index::toVelocyPack as it will try to output 'fields'
  // but we have custom storage format
  builder.add(arangodb::StaticStrings::IndexId,
              arangodb::velocypack::Value(std::to_string(_iid.id())));
  builder.add(arangodb::StaticStrings::IndexType,
              arangodb::velocypack::Value(oldtypeName(type())));
  builder.add(arangodb::StaticStrings::IndexName,
              arangodb::velocypack::Value(name()));
  builder.add(arangodb::StaticStrings::IndexUnique, VPackValue(unique()));
  builder.add(arangodb::StaticStrings::IndexSparse, VPackValue(sparse()));
}

Result IResearchRocksDBInvertedIndex::drop() { return deleteDataStore(); }

void IResearchRocksDBInvertedIndex::unload() {
  auto res = shutdownDataStore();
  if (!res.ok()) {
    LOG_TOPIC("0617f", ERR, iresearch::TOPIC)
        << "failed to unload arangosearch rockdb inverted index: "
        << res.errorNumber() << " " << res.errorMessage();
  }
}

bool IResearchRocksDBInvertedIndex::matchesDefinition(
    arangodb::velocypack::Slice const& other) const {
  TRI_ASSERT(other.isObject());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto typeSlice = other.get(arangodb::StaticStrings::IndexType);
  TRI_ASSERT(typeSlice.isString());
  std::string_view typeStr = typeSlice.stringView();
  TRI_ASSERT(typeStr == oldtypeName());
#endif
  auto value = other.get(arangodb::StaticStrings::IndexId);

  if (!value.isNone()) {
    // We already have an id.
    if (!value.isString()) {
      // Invalid ID
      return false;
    }
    // Short circuit. If id is correct the index is identical.
    std::string_view idRef = value.stringView();
    return idRef == std::to_string(IResearchDataStore::id().id());
  }
  return IResearchInvertedIndex::matchesFieldsDefinition(other);
}
}  // namespace iresearch
}  // namespace arangodb
