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
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationFeatures/ApplicationServer.h"
#include "ClusterIndexFactory.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"
#include "ClusterEngine/ClusterEngine.h"
#include "ClusterEngine/ClusterIndex.h"
#include "Indexes/Index.h"
#include "Indexes/IndexDefinitions.h"
#include "Indexes/IndexTypeCatalog.h"
#include "IResearch/IResearchInvertedIndex.h"
#include "IResearch/IResearchInvertedClusterIndex.h"
#include "IResearch/IResearchRocksDBInvertedIndex.h"
#include "IResearch/IResearchViewMeta.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

namespace {

using namespace arangodb;
using namespace arangodb::iresearch;

// Definition is a reference into IndexTypeCatalog, the same instance
// RocksDBIndexFactory uses; only instantiate() differs per engine
template<typename Definition>
struct ClusterIndexFactoryT : public DelegatingIndexFactory<Definition> {
  template<typename... Args>
  explicit ClusterIndexFactoryT(application_features::ApplicationServer& server,
                                ClusterEngine& engine, Args&&... args)
      : DelegatingIndexFactory<Definition>(server, std::forward<Args>(args)...),
        _engine(engine) {}

  std::shared_ptr<Index> instantiate(
      LogicalCollection& collection, velocypack::Slice definition, IndexId id,
      bool /* isClusterConstructor */) const override {
    return std::make_shared<ClusterIndex>(id, collection, _engine.engineType(),
                                          this->_definition._type, definition);
  }

 protected:
  ClusterEngine& _engine;
};

struct EdgeIndexFactory : public ClusterIndexFactoryT<EdgeIndexDefinition> {
  using ClusterIndexFactoryT::ClusterIndexFactoryT;

  std::shared_ptr<Index> instantiate(LogicalCollection& collection,
                                     velocypack::Slice definition, IndexId id,
                                     bool isClusterConstructor) const override {
    if (!isClusterConstructor) {
      // this index type cannot be created directly
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "cannot create edge index");
    }

    return std::make_shared<ClusterIndex>(id, collection, _engine.engineType(),
                                          IndexType::Edge, definition);
  }
};

struct PrimaryIndexFactory
    : public ClusterIndexFactoryT<PrimaryIndexDefinition> {
  using ClusterIndexFactoryT::ClusterIndexFactoryT;

  std::shared_ptr<Index> instantiate(LogicalCollection& collection,
                                     velocypack::Slice definition,
                                     IndexId /*id*/,
                                     bool isClusterConstructor) const override {
    if (!isClusterConstructor) {
      // this index type cannot be created directly
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "cannot create primary index");
    }

    return std::make_shared<ClusterIndex>(IndexId::primary(), collection,
                                          _engine.engineType(),
                                          IndexType::Primary, definition);
  }
};

struct IResearchInvertedIndexClusterFactory
    : public ClusterIndexFactoryT<IResearchInvertedIndexDefinition> {
  using ClusterIndexFactoryT::ClusterIndexFactoryT;

  std::shared_ptr<Index> instantiate(LogicalCollection& collection,
                                     velocypack::Slice definition, IndexId id,
                                     bool isClusterConstructor) const override {
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
    auto index = std::make_shared<IResearchInvertedClusterIndex>(
        id, objectId, collection, indexName);
    bool pathExists = false;
    if (index->init(definition, pathExists).fail()) {
      return nullptr;
    }
    index->initFields();
    return index;
  }
};
}  // namespace

namespace arangodb {

void ClusterIndexFactory::linkIndexFactories(ClusterEngine& engine) {
  auto const& catalog = _catalog;
  auto const& edgeIndexFactory =
      own<EdgeIndexFactory>(_server, engine, catalog.edge());
  auto const& fulltextIndexFactory =
      own<ClusterIndexFactoryT<FulltextIndexDefinition>>(_server, engine,
                                                         catalog.fulltext());
  auto const& geoIndexFactory = own<ClusterIndexFactoryT<GeoIndexDefinition>>(
      _server, engine, catalog.geo());
  auto const& geo1IndexFactory = own<ClusterIndexFactoryT<Geo1IndexDefinition>>(
      _server, engine, catalog.geo1());
  auto const& geo2IndexFactory = own<ClusterIndexFactoryT<Geo2IndexDefinition>>(
      _server, engine, catalog.geo2());
  auto const& hashIndexFactory =
      own<ClusterIndexFactoryT<SecondaryIndexDefinition>>(_server, engine,
                                                          catalog.hash());
  auto const& persistentIndexFactory =
      own<ClusterIndexFactoryT<SecondaryIndexDefinition>>(_server, engine,
                                                          catalog.persistent());
  auto const& primaryIndexFactory =
      own<PrimaryIndexFactory>(_server, engine, catalog.primary());
  auto const& skiplistIndexFactory =
      own<ClusterIndexFactoryT<SecondaryIndexDefinition>>(_server, engine,
                                                          catalog.skiplist());
  auto const& ttlIndexFactory = own<ClusterIndexFactoryT<TtlIndexDefinition>>(
      _server, engine, catalog.ttl());
  auto const& mdiIndexFactory = own<ClusterIndexFactoryT<MdiIndexDefinition>>(
      _server, engine, catalog.mdi());
  auto const& zkdIndexFactory = own<ClusterIndexFactoryT<MdiIndexDefinition>>(
      _server, engine, catalog.zkd());
  auto const& mdiPrefixedIndexFactory =
      own<ClusterIndexFactoryT<MdiPrefixedIndexDefinition>>(
          _server, engine, catalog.mdiPrefixed());
  auto const& invertedIndexFactory = own<IResearchInvertedIndexClusterFactory>(
      _server, engine, catalog.inverted());
  auto const& vectorIndexFactory =
      own<ClusterIndexFactoryT<VectorIndexDefinition>>(_server, engine,
                                                       catalog.vector());

  emplace("edge", edgeIndexFactory);
  emplace("fulltext", fulltextIndexFactory);
  emplace("geo", geoIndexFactory);
  emplace("geo1", geo1IndexFactory);
  emplace("geo2", geo2IndexFactory);
  emplace("hash", hashIndexFactory);
  emplace("persistent", persistentIndexFactory);
  emplace("rocksdb", persistentIndexFactory);
  emplace("primary", primaryIndexFactory);
  emplace("skiplist", skiplistIndexFactory);
  emplace("ttl", ttlIndexFactory);
  emplace("zkd", zkdIndexFactory);
  emplace("mdi", mdiIndexFactory);
  emplace("mdi-prefixed", mdiPrefixedIndexFactory);
  emplace(IRESEARCH_INVERTED_INDEX_TYPE.data(), invertedIndexFactory);
  emplace("vector", vectorIndexFactory);
}

ClusterIndexFactory::ClusterIndexFactory(
    application_features::ApplicationServer& server, ClusterEngine& engine,
    IndexTypeCatalog const& catalog)
    : IndexFactory(server, catalog), _engine(engine) {
  linkIndexFactories(engine);
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
  ClusterEngineType ct = _engine.engineType();

  systemIndexes.emplace_back(std::make_shared<ClusterIndex>(
      IndexId::primary(), col, ct, IndexType::Primary, input.slice()));

  // create edges indexes
  if (col.type() == TRI_COL_TYPE_EDGE) {
    // first edge index
    input.clear();
    input.openObject();
    input.add(StaticStrings::IndexType,
              VPackValue(Index::oldtypeName(IndexType::Edge)));
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
        IndexId::edgeFrom(), col, ct, IndexType::Edge, input.slice()));

    // second edge index
    if (ct == ClusterEngineType::RocksDBEngine) {
      input.clear();
      input.openObject();
      input.add(StaticStrings::IndexType,
                VPackValue(Index::oldtypeName(IndexType::Edge)));
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
          IndexId::edgeTo(), col, ct, IndexType::Edge, input.slice()));
    }
  }
}

void ClusterIndexFactory::prepareIndexes(
    LogicalCollection& col, velocypack::Slice indexesSlice,
    std::vector<std::shared_ptr<Index>>& indexes) const {
  TRI_ASSERT(indexesSlice.isArray());

  for (VPackSlice v : VPackArrayIterator(indexesSlice)) {
    if (!validateFieldsDefinition(v, StaticStrings::IndexFields, 0, SIZE_MAX,
                                  /*allowSubAttributes*/ true,
                                  /*allowIdAttribute*/ false)
             .ok()) {
      // We have an error here. Do not add.
      continue;
    }

    if (basics::VelocyPackHelper::getBooleanValue(
            v, StaticStrings::IndexIsBuilding, false) &&
        !(basics::VelocyPackHelper::getStringView(v, StaticStrings::IndexType,
                                                  {}) ==
          iresearch::StaticStrings::ViewArangoSearchType)) {
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
