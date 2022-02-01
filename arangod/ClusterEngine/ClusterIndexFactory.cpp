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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "ClusterIndexFactory.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"
#include "ClusterEngine/ClusterEngine.h"
#include "ClusterEngine/ClusterIndex.h"
#include "Indexes/Index.h"
#include "IResearch/IResearchInvertedIndex.h"
#include "IResearch/IResearchViewMeta.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace {

using namespace arangodb;
using namespace arangodb::iresearch;

struct DefaultIndexFactory : public IndexTypeFactory {
  std::string const _type;

  explicit DefaultIndexFactory(application_features::ApplicationServer& server,
                               std::string const& type)
      : IndexTypeFactory(server), _type(type) {}

  bool equal(velocypack::Slice lhs, velocypack::Slice rhs,
             std::string const& dbname) const override {
    auto& clusterEngine =
        _server.getFeature<EngineSelectorFeature>().engine<ClusterEngine>();
    auto* engine = clusterEngine.actualEngine();

    if (!engine) {
      THROW_ARANGO_EXCEPTION(
          Result(TRI_ERROR_INTERNAL,
                 "cannot find storage engine while normalizing index"));
    }

    return engine->indexFactory().factory(_type).equal(lhs, rhs, dbname);
  }

  std::shared_ptr<Index> instantiate(
      LogicalCollection& collection, velocypack::Slice definition, IndexId id,
      bool /* isClusterConstructor */) const override {
    auto& clusterEngine =
        _server.getFeature<EngineSelectorFeature>().engine<ClusterEngine>();
    auto ct = clusterEngine.engineType();

    return std::make_shared<ClusterIndex>(id, collection, ct,
                                          Index::type(_type), definition);
  }

  virtual Result normalize(velocypack::Builder& normalized,
                           velocypack::Slice definition, bool isCreation,
                           TRI_vocbase_t const& vocbase) const override {
    auto& clusterEngine =
        _server.getFeature<EngineSelectorFeature>().engine<ClusterEngine>();
    auto* engine = clusterEngine.actualEngine();

    if (!engine) {
      return Result(TRI_ERROR_INTERNAL,
                    "cannot find storage engine while normalizing index");
    }

    return engine->indexFactory().factory(_type).normalize(
        normalized, definition, isCreation, vocbase);
  }
};

struct EdgeIndexFactory : public DefaultIndexFactory {
  explicit EdgeIndexFactory(application_features::ApplicationServer& server,
                            std::string const& type)
      : DefaultIndexFactory(server, type) {}

  std::shared_ptr<Index> instantiate(LogicalCollection& collection,
                                     velocypack::Slice definition, IndexId id,
                                     bool isClusterConstructor) const override {
    if (!isClusterConstructor) {
      // this index type cannot be created directly
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "cannot create edge index");
    }

    auto& clusterEngine =
        _server.getFeature<EngineSelectorFeature>().engine<ClusterEngine>();
    auto ct = clusterEngine.engineType();

    return std::make_shared<ClusterIndex>(
        id, collection, ct, Index::TRI_IDX_TYPE_EDGE_INDEX, definition);
  }
};

struct PrimaryIndexFactory : public DefaultIndexFactory {
  explicit PrimaryIndexFactory(application_features::ApplicationServer& server,
                               std::string const& type)
      : DefaultIndexFactory(server, type) {}

  std::shared_ptr<Index> instantiate(LogicalCollection& collection,
                                     velocypack::Slice definition,
                                     IndexId /*id*/,
                                     bool isClusterConstructor) const override {
    if (!isClusterConstructor) {
      // this index type cannot be created directly
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "cannot create primary index");
    }

    auto& clusterEngine =
        _server.getFeature<EngineSelectorFeature>().engine<ClusterEngine>();
    auto ct = clusterEngine.engineType();

    return std::make_shared<ClusterIndex>(IndexId::primary(), collection, ct,
                                          Index::TRI_IDX_TYPE_PRIMARY_INDEX,
                                          definition);
  }
};

struct IResearchInvertedIndexFactory : public DefaultIndexFactory {
  explicit IResearchInvertedIndexFactory(
      application_features::ApplicationServer& server)
      : DefaultIndexFactory(server, IRESEARCH_INVERTED_INDEX_TYPE.data()) {}

  std::shared_ptr<Index> instantiate(LogicalCollection& collection,
                                     velocypack::Slice definition, IndexId id,
                                     bool isClusterConstructor) const override {
    IResearchViewMeta indexMeta;
    std::string errField;
    if (!indexMeta.init(definition, errField)) {
      LOG_TOPIC("a9cce", ERR, TOPIC)
          << (errField.empty()
                  ? ("failed to initialize index meta from definition: " +
                     definition.toString())
                  : ("failed to initialize index meta from definition, "
                     "error in attribute '" +
                     errField + "': " + definition.toString()));
      return nullptr;
    }
    IResearchInvertedIndexMeta fieldsMeta;
    if (!fieldsMeta.init(_server, definition, true, errField,
                         collection.vocbase().name())) {
      LOG_TOPIC("18c18", ERR, TOPIC)
          << (errField.empty()
                  ? ("failed to initialize index fields from "
                     "definition: " +
                     definition.toString())
                  : ("failed to initialize index fields from definition, "
                     "error in attribute '" +
                     errField + "': " + definition.toString()));
      return nullptr;
    }
    auto nameSlice = definition.get(arangodb::StaticStrings::IndexName);
    std::string indexName;
    if (!nameSlice.isNone()) {
      if (!nameSlice.isString() || nameSlice.getStringLength() == 0) {
        LOG_TOPIC("91ebe", ERR, TOPIC)
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
    return std::make_shared<IResearchInvertedClusterIndex>(
        id, objectId, collection, indexName, std::move(fieldsMeta));
  }
};

}  // namespace

namespace arangodb {

void ClusterIndexFactory::linkIndexFactories(
    application_features::ApplicationServer& server, IndexFactory& factory) {
  static const EdgeIndexFactory edgeIndexFactory(server, "edge");
  static const DefaultIndexFactory fulltextIndexFactory(server, "fulltext");
  static const DefaultIndexFactory geoIndexFactory(server, "geo");
  static const DefaultIndexFactory geo1IndexFactory(server, "geo1");
  static const DefaultIndexFactory geo2IndexFactory(server, "geo2");
  static const DefaultIndexFactory hashIndexFactory(server, "hash");
  static const DefaultIndexFactory persistentIndexFactory(server, "persistent");
  static const PrimaryIndexFactory primaryIndexFactory(server, "primary");
  static const DefaultIndexFactory skiplistIndexFactory(server, "skiplist");
  static const DefaultIndexFactory ttlIndexFactory(server, "ttl");
  static const DefaultIndexFactory zkdIndexFactory(server, "zkd");
  static const IResearchInvertedIndexFactory invertedIndexFactory(server);

  factory.emplace(edgeIndexFactory._type, edgeIndexFactory);
  factory.emplace(fulltextIndexFactory._type, fulltextIndexFactory);
  factory.emplace(geoIndexFactory._type, geoIndexFactory);
  factory.emplace(geo1IndexFactory._type, geo1IndexFactory);
  factory.emplace(geo2IndexFactory._type, geo2IndexFactory);
  factory.emplace(hashIndexFactory._type, hashIndexFactory);
  factory.emplace(persistentIndexFactory._type, persistentIndexFactory);
  factory.emplace(primaryIndexFactory._type, primaryIndexFactory);
  factory.emplace(skiplistIndexFactory._type, skiplistIndexFactory);
  factory.emplace(ttlIndexFactory._type, ttlIndexFactory);
  factory.emplace(zkdIndexFactory._type, zkdIndexFactory);
  factory.emplace(invertedIndexFactory._type, invertedIndexFactory);
}

ClusterIndexFactory::ClusterIndexFactory(
    application_features::ApplicationServer& server)
    : IndexFactory(server) {
  linkIndexFactories(server, *this);
}

/// @brief index name aliases (e.g. "persistent" => "hash", "skiplist" =>
/// "hash") used to display storage engine capabilities
std::unordered_map<std::string, std::string> ClusterIndexFactory::indexAliases()
    const {
  auto& ce =
      _server.getFeature<EngineSelectorFeature>().engine<ClusterEngine>();
  auto* ae = ce.actualEngine();
  if (!ae) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "no actual storage engine for ClusterIndexFactory");
  }
  return ae->indexFactory().indexAliases();
}

Result ClusterIndexFactory::enhanceIndexDefinition(  // normalize definition
    velocypack::Slice const definition,              // source definition
    velocypack::Builder& normalized,  // normalized definition (out-param)
    bool isCreation,                  // definition for index creation
    TRI_vocbase_t const& vocbase      // index vocbase
) const {
  auto& ce = _server.getFeature<EngineSelectorFeature>()
                 .engine<arangodb::ClusterEngine>();

  auto* ae = ce.actualEngine();

  if (!ae) {
    return TRI_ERROR_INTERNAL;
  }

  return ae->indexFactory().enhanceIndexDefinition(definition, normalized,
                                                   isCreation, vocbase);
}

void ClusterIndexFactory::fillSystemIndexes(
    LogicalCollection& col,
    std::vector<std::shared_ptr<Index>>& systemIndexes) const {
  // create primary index
  VPackBuilder input;
  input.openObject();
  input.add(StaticStrings::IndexType, VPackValue("primary"));
  input.add(StaticStrings::IndexId,
            VPackValue(std::to_string(IndexId::primary().id())));
  input.add(StaticStrings::IndexName,
            VPackValue(StaticStrings::IndexNamePrimary));
  input.add(StaticStrings::IndexFields, VPackValue(VPackValueType::Array));
  input.add(VPackValue(StaticStrings::KeyString));
  input.close();
  input.add(StaticStrings::IndexUnique, VPackValue(true));
  input.add(StaticStrings::IndexSparse, VPackValue(false));
  input.close();

  // get the storage engine type
  auto& ce =
      _server.getFeature<EngineSelectorFeature>().engine<ClusterEngine>();
  ClusterEngineType ct = ce.engineType();

  systemIndexes.emplace_back(std::make_shared<ClusterIndex>(
      IndexId::primary(), col, ct, Index::TRI_IDX_TYPE_PRIMARY_INDEX,
      input.slice()));

  // create edges indexes
  if (col.type() == TRI_COL_TYPE_EDGE) {
    // first edge index
    input.clear();
    input.openObject();
    input.add(StaticStrings::IndexType,
              VPackValue(Index::oldtypeName(Index::TRI_IDX_TYPE_EDGE_INDEX)));
    input.add(StaticStrings::IndexId,
              VPackValue(std::to_string(IndexId::edgeFrom().id())));

    input.add(StaticStrings::IndexFields, VPackValue(VPackValueType::Array));
    input.add(VPackValue(StaticStrings::FromString));
    input.close();

    if (ct == ClusterEngineType::RocksDBEngine) {
      input.add(StaticStrings::IndexName,
                VPackValue(StaticStrings::IndexNameEdgeFrom));
    }

    input.add(StaticStrings::IndexUnique, VPackValue(false));
    input.add(StaticStrings::IndexSparse, VPackValue(false));
    input.close();
    systemIndexes.emplace_back(std::make_shared<ClusterIndex>(
        IndexId::edgeFrom(), col, ct, Index::TRI_IDX_TYPE_EDGE_INDEX,
        input.slice()));

    // second edge index
    if (ct == ClusterEngineType::RocksDBEngine) {
      input.clear();
      input.openObject();
      input.add(StaticStrings::IndexType,
                VPackValue(Index::oldtypeName(Index::TRI_IDX_TYPE_EDGE_INDEX)));
      input.add(StaticStrings::IndexId,
                VPackValue(std::to_string(IndexId::edgeTo().id())));
      input.add(StaticStrings::IndexName,
                VPackValue(StaticStrings::IndexNameEdgeTo));
      input.add(StaticStrings::IndexFields, VPackValue(VPackValueType::Array));
      input.add(VPackValue(StaticStrings::ToString));
      input.close();
      input.add(StaticStrings::IndexUnique, VPackValue(false));
      input.add(StaticStrings::IndexSparse, VPackValue(false));
      input.close();
      systemIndexes.emplace_back(std::make_shared<ClusterIndex>(
          IndexId::edgeTo(), col, ct, Index::TRI_IDX_TYPE_EDGE_INDEX,
          input.slice()));
    }
  }
}

void ClusterIndexFactory::prepareIndexes(
    LogicalCollection& col, velocypack::Slice indexesSlice,
    std::vector<std::shared_ptr<Index>>& indexes) const {
  TRI_ASSERT(indexesSlice.isArray());

  for (VPackSlice v : VPackArrayIterator(indexesSlice)) {
    if (!validateFieldsDefinition(v, StaticStrings::IndexFields, 0, SIZE_MAX)
             .ok()) {
      // We have an error here. Do not add.
      continue;
    }

    if (basics::VelocyPackHelper::getBooleanValue(
            v, StaticStrings::IndexIsBuilding, false)) {
      // This index is still being built. Do not add.
      continue;
    }

    try {
      auto idx = prepareIndexFromSlice(v, false, col, true);
      TRI_ASSERT(idx != nullptr);
      indexes.emplace_back(std::move(idx));
    } catch (std::exception const& ex) {
      LOG_TOPIC("7ed52", ERR, Logger::ENGINES)
          << "error creating index from definition '" << v.toString()
          << "': " << ex.what();
    }
  }
}

}  // namespace arangodb
