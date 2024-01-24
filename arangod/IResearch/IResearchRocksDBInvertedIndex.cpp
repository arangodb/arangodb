////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "IResearch/IResearchRocksDBInvertedIndex.h"
#include "IResearch/IResearchRocksDBEncryption.h"
#include "IResearch/IResearchMetricStats.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/VelocyPackHelper.h"
#include "Logger/LogMacros.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/LogicalCollection.h"

#include <absl/strings/str_cat.h>

namespace arangodb {
namespace iresearch {

IResearchRocksDBInvertedIndexFactory::IResearchRocksDBInvertedIndexFactory(
    ArangodServer& server)
    : IndexTypeFactory(server) {}

bool IResearchRocksDBInvertedIndexFactory::equal(
    velocypack::Slice lhs, velocypack::Slice rhs,
    std::string const& dbname) const {
  IResearchInvertedIndexMeta lhsFieldsMeta;
  std::string errField;
  if (!lhsFieldsMeta.init(_server, lhs, true, errField, dbname)) {
    LOG_TOPIC("79384", ERR, iresearch::TOPIC)
        << "failed to initialize index fields from definition"
        << (errField.empty() ? absl::StrCat(": ", lhs.toString())
                             : absl::StrCat(", error in attribute '", errField,
                                            "': ", lhs.toString()));
    return false;
  }

  IResearchInvertedIndexMeta rhsFieldsMeta;
  if (!rhsFieldsMeta.init(_server, rhs, true, errField, dbname)) {
    LOG_TOPIC("31eaf", ERR, iresearch::TOPIC)
        << (errField.empty()
                ? absl::StrCat(
                      "failed to initialize index fields from definition: ",
                      rhs.toString())
                : absl::StrCat("failed to initialize index fields from "
                               "definition, error in attribute '",
                               errField, "': ", rhs.toString()));
    return false;
  }

  return lhsFieldsMeta == rhsFieldsMeta;
}

std::shared_ptr<Index> IResearchRocksDBInvertedIndexFactory::instantiate(
    LogicalCollection& collection, velocypack::Slice definition, IndexId id,
    bool isClusterConstructor) const {
  auto const clusterWideIndex =
      collection.id() == collection.planId() && collection.isAStub();
  auto nameSlice = definition.get(arangodb::StaticStrings::IndexName);
  std::string indexName;
  if (!nameSlice.isNone()) {
    if (!nameSlice.isString() || nameSlice.getStringLength() == 0) {
      LOG_TOPIC("91ebd", ERR, iresearch::TOPIC)
          << "failed to initialize index from definition, error in attribute '"
          << arangodb::StaticStrings::IndexName
          << "': " << definition.toString();
      return nullptr;
    }
    indexName = nameSlice.stringView();
  }

  auto objectId = basics::VelocyPackHelper::stringUInt64(
      definition, arangodb::StaticStrings::ObjectId);
  auto index = std::make_shared<IResearchRocksDBInvertedIndex>(
      id, collection, objectId, indexName);
  if (!clusterWideIndex) {
    bool pathExists = false;
    auto cleanup = scopeGuard([&]() noexcept {
      if (pathExists) {
        index->unload();
      } else {
        index->drop();
      }
    });

    auto initRes = index->init(
        definition, pathExists, [this]() -> irs::directory_attributes {
          auto& selector = _server.getFeature<EngineSelectorFeature>();
          TRI_ASSERT(selector.isRocksDB());
          auto& engine = selector.engine<RocksDBEngine>();
          auto* encryption = engine.encryptionProvider();
          if (encryption) {
            return irs::directory_attributes{
                std::make_unique<RocksDBEncryptionProvider>(
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
    index->initFields();
    cleanup.cancel();
  }
  return index;
}

Result IResearchRocksDBInvertedIndexFactory::normalize(
    velocypack::Builder& normalized, velocypack::Slice definition,
    bool isCreation, TRI_vocbase_t const& vocbase) const {
  TRI_ASSERT(normalized.isOpenObject());

  auto res = IndexFactory::validateFieldsDefinition(
      definition, arangodb::StaticStrings::IndexFields, 0, SIZE_MAX,
      /*allowSubAttributes*/ true, /*allowIdAttribute*/ false);
  if (res.fail()) {
    return res;
  }

  std::string errField;
  IResearchInvertedIndexMeta tmpLinkMeta;
  if (!tmpLinkMeta.init(_server, definition,
                        ServerState::instance()->isDBServer(), errField,
                        vocbase.name())) {
    return {
        TRI_ERROR_BAD_PARAMETER,
        errField.empty()
            ? absl::StrCat("failed to normalize index fields from definition: ",
                           definition.toString())
            : absl::StrCat("failed to normalize index fields from definition, "
                           "error in attribute '",
                           errField, "': ", definition.toString())};
  }
  if (!tmpLinkMeta.json(_server, normalized, isCreation, &vocbase)) {
    return {TRI_ERROR_BAD_PARAMETER,
            absl::StrCat(
                "failed to write normalized index fields from definition: ",
                definition.toString())};
  }
  auto nameSlice = definition.get(arangodb::StaticStrings::IndexName);
  if (nameSlice.isString() && nameSlice.getStringLength() > 0) {
    normalized.add(arangodb::StaticStrings::IndexName, nameSlice);
  } else if (!nameSlice.isNone()) {
    return {
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat(
            "failed to normalize index from definition, error in attribute '",
            arangodb::StaticStrings::IndexName, "': ", definition.toString())};
  }

  normalized.add(arangodb::StaticStrings::IndexType,
                 velocypack::Value(
                     Index::oldtypeName(Index::TRI_IDX_TYPE_INVERTED_INDEX)));

  if (isCreation && !ServerState::instance()->isCoordinator() &&
      definition.get(arangodb::StaticStrings::ObjectId).isNone()) {
    normalized.add(arangodb::StaticStrings::ObjectId,
                   VPackValue(absl::AlphaNum{TRI_NewTickServer()}.Piece()));
  }

  normalized.add(arangodb::StaticStrings::IndexSparse, velocypack::Value(true));
  normalized.add(arangodb::StaticStrings::IndexUnique,
                 velocypack::Value(false));

  IndexFactory::processIndexInBackground(definition, normalized);
  IndexFactory::processIndexParallelism(definition, normalized);

  return res;
}

IResearchRocksDBInvertedIndex::IResearchRocksDBInvertedIndex(
    IndexId id, LogicalCollection& collection, uint64_t objectId,
    std::string const& name)
    : RocksDBIndex{id,
                   collection,
                   name,
                   {},
                   false,
                   true,
                   RocksDBColumnFamilyManager::get(
                       RocksDBColumnFamilyManager::Family::Invalid),
                   objectId,
                   /*useCache*/ false,
                   /*cacheManager*/ nullptr,
                   /*engine*/
                   collection.vocbase()
                       .server()
                       .getFeature<EngineSelectorFeature>()
                       .engine<RocksDBEngine>()},
      IResearchInvertedIndex{collection.vocbase().server(), collection} {}

namespace {

template<typename T>
T getMetric(IResearchRocksDBInvertedIndex const& index) {
  T metric;
  metric.addLabel("db", index.getDbName());
  metric.addLabel("index", index.name());
  metric.addLabel("collection", index.getCollectionName());
  metric.addLabel("index_id", std::to_string(index.id().id()));
  metric.addLabel("shard", index.getShardName());
  return metric;
}

std::string getLabels(IResearchRocksDBInvertedIndex const& index) {
  return absl::StrCat(  // clang-format off
      "db=\"", index.getDbName(), "\","
      "index=\"", index.name(), "\","
      "collection=\"", index.getCollectionName(), "\","
      "index_id=\"", index.id().id(), "\","
      "shard=\"", index.getShardName(), "\"");  // clang-format on
}

}  // namespace

std::string IResearchRocksDBInvertedIndex::getCollectionName() const {
  if (ServerState::instance()->isSingleServer()) {
    return std::to_string(Index::_collection.id().id());
  }
  return meta()._collectionName;
}

std::string const& IResearchRocksDBInvertedIndex::getShardName()
    const noexcept {
  if (ServerState::instance()->isDBServer()) {
    return Index::_collection.name();
  }
  return arangodb::StaticStrings::Empty;
}

void IResearchRocksDBInvertedIndex::insertMetrics() {
  auto& metric = Index::_collection.vocbase()
                     .server()
                     .getFeature<metrics::MetricsFeature>();
  _writersMemory =
      &metric.add(getMetric<arangodb_search_writers_memory_usage>(*this));
  _readersMemory =
      &metric.add(getMetric<arangodb_search_readers_memory_usage>(*this));
  _consolidationsMemory = &metric.add(
      getMetric<arangodb_search_consolidations_memory_usage>(*this));
  _fileDescriptorsCount =
      &metric.add(getMetric<arangodb_search_file_descriptors>(*this));
  _mappedMemory = &metric.add(getMetric<arangodb_search_mapped_memory>(*this));
  _numFailedCommits =
      &metric.add(getMetric<arangodb_search_num_failed_commits>(*this));
  _numFailedCleanups =
      &metric.add(getMetric<arangodb_search_num_failed_cleanups>(*this));
  _numFailedConsolidations =
      &metric.add(getMetric<arangodb_search_num_failed_consolidations>(*this));
  _avgCommitTimeMs = &metric.add(getMetric<arangodb_search_commit_time>(*this));
  _avgCleanupTimeMs =
      &metric.add(getMetric<arangodb_search_cleanup_time>(*this));
  _avgConsolidationTimeMs =
      &metric.add(getMetric<arangodb_search_consolidation_time>(*this));
  _metricStats = &metric.batchAdd<MetricStats>(kSearchStats, getLabels(*this));
}

void IResearchRocksDBInvertedIndex::removeMetrics() {
  auto& metric = Index::_collection.vocbase()
                     .server()
                     .getFeature<metrics::MetricsFeature>();
  if (_writersMemory != &irs::IResourceManager::kNoop) {
    _writersMemory = &irs::IResourceManager::kNoop;
    metric.remove(getMetric<arangodb_search_writers_memory_usage>(*this));
  }
  if (_readersMemory != &irs::IResourceManager::kNoop) {
    _readersMemory = &irs::IResourceManager::kNoop;
    metric.remove(getMetric<arangodb_search_readers_memory_usage>(*this));
  }
  if (_consolidationsMemory != &irs::IResourceManager::kNoop) {
    _consolidationsMemory = &irs::IResourceManager::kNoop;
    metric.remove(
        getMetric<arangodb_search_consolidations_memory_usage>(*this));
  }
  if (_fileDescriptorsCount != &irs::IResourceManager::kNoop) {
    _fileDescriptorsCount = &irs::IResourceManager::kNoop;
    metric.remove(getMetric<arangodb_search_file_descriptors>(*this));
  }
  if (_mappedMemory) {
    _mappedMemory = nullptr;
    metric.remove(getMetric<arangodb_search_mapped_memory>(*this));
  }
  if (_numFailedCommits) {
    _numFailedCommits = nullptr;
    metric.remove(getMetric<arangodb_search_num_failed_commits>(*this));
  }
  if (_numFailedCleanups) {
    _numFailedCleanups = nullptr;
    metric.remove(getMetric<arangodb_search_num_failed_cleanups>(*this));
  }
  if (_numFailedConsolidations) {
    _numFailedConsolidations = nullptr;
    metric.remove(getMetric<arangodb_search_num_failed_consolidations>(*this));
  }
  if (_avgCommitTimeMs) {
    _avgCommitTimeMs = nullptr;
    metric.remove(getMetric<arangodb_search_commit_time>(*this));
  }
  if (_avgCleanupTimeMs) {
    _avgCleanupTimeMs = nullptr;
    metric.remove(getMetric<arangodb_search_cleanup_time>(*this));
  }
  if (_avgConsolidationTimeMs) {
    _avgConsolidationTimeMs = nullptr;
    metric.remove(getMetric<arangodb_search_consolidation_time>(*this));
  }
  if (_metricStats) {
    _metricStats = nullptr;
    metric.batchRemove(kSearchStats, getLabels(*this));
  }
}

void IResearchRocksDBInvertedIndex::toVelocyPack(
    VPackBuilder& builder,
    std::underlying_type_t<Index::Serialize> flags) const {
  bool const forPersistence =
      Index::hasFlag(flags, Index::Serialize::Internals);
  bool const forInventory = Index::hasFlag(flags, Index::Serialize::Inventory);
  VPackObjectBuilder objectBuilder(&builder);
  IResearchInvertedIndex::toVelocyPack(collection().vocbase().server(),
                                       &collection().vocbase(), builder,
                                       forPersistence || forInventory);
  if (forPersistence) {
    TRI_ASSERT(objectId() != 0);  // If we store it, it cannot be 0
    builder.add(arangodb::StaticStrings::ObjectId,
                VPackValue(absl::AlphaNum{objectId()}.Piece()));
  }
  // can't use Index::toVelocyPack as it will try to output 'fields'
  // but we have custom storage format
  builder.add(arangodb::StaticStrings::IndexId,
              velocypack::Value(absl::AlphaNum{_iid.id()}.Piece()));
  builder.add(arangodb::StaticStrings::IndexType,
              velocypack::Value(oldtypeName(type())));
  builder.add(arangodb::StaticStrings::IndexName, velocypack::Value(name()));
  builder.add(arangodb::StaticStrings::IndexUnique, VPackValue(unique()));
  builder.add(arangodb::StaticStrings::IndexSparse, VPackValue(sparse()));

  if (Index::hasFlag(flags, Index::Serialize::Figures)) {
    builder.add("figures", VPackValue(VPackValueType::Object));
    toVelocyPackFigures(builder);
    builder.close();
  }
}

bool IResearchRocksDBInvertedIndex::matchesDefinition(
    velocypack::Slice const& other) const {
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
    return idRef == absl::AlphaNum{id().id()}.Piece();
  }
  return IResearchInvertedIndex::matchesDefinition(other,
                                                   collection().vocbase());
}

}  // namespace iresearch
}  // namespace arangodb
