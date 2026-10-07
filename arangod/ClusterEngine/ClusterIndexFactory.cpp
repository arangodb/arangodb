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
#include "IResearch/IResearchInvertedIndexDefinition.h"
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

ClusterIndexFactory::ClusterIndexFactory(
    application_features::ApplicationServer& server, ClusterEngine& engine,
    IndexTypeCatalog const& catalog)
    : IndexFactory(server, catalog),
      _engine(engine),
      _edge(&own<EdgeIndexFactory>(server, engine, catalog.edge())),
      _fulltext(&own<ClusterIndexFactoryT<FulltextIndexDefinition>>(
          server, engine, catalog.fulltext())),
      _geo(&own<ClusterIndexFactoryT<GeoIndexDefinition>>(server, engine,
                                                          catalog.geo())),
      _geo1(&own<ClusterIndexFactoryT<Geo1IndexDefinition>>(server, engine,
                                                            catalog.geo1())),
      _geo2(&own<ClusterIndexFactoryT<Geo2IndexDefinition>>(server, engine,
                                                            catalog.geo2())),
      _hash(&own<ClusterIndexFactoryT<SecondaryIndexDefinition>>(
          server, engine, catalog.hash())),
      _persistent(&own<ClusterIndexFactoryT<SecondaryIndexDefinition>>(
          server, engine, catalog.persistent())),
      _primary(&own<PrimaryIndexFactory>(server, engine, catalog.primary())),
      _skiplist(&own<ClusterIndexFactoryT<SecondaryIndexDefinition>>(
          server, engine, catalog.skiplist())),
      _ttl(&own<ClusterIndexFactoryT<TtlIndexDefinition>>(server, engine,
                                                          catalog.ttl())),
      _zkd(&own<ClusterIndexFactoryT<MdiIndexDefinition>>(server, engine,
                                                          catalog.zkd())),
      _mdi(&own<ClusterIndexFactoryT<MdiIndexDefinition>>(server, engine,
                                                          catalog.mdi())),
      _mdiPrefixed(&own<ClusterIndexFactoryT<MdiPrefixedIndexDefinition>>(
          server, engine, catalog.mdiPrefixed())),
      _inverted(&own<IResearchInvertedIndexClusterFactory>(server, engine,
                                                           catalog.inverted())),
      _vector(&own<ClusterIndexFactoryT<VectorIndexDefinition>>(
          server, engine, catalog.vector())) {}

IndexTypeFactory const& ClusterIndexFactory::factoryFor(
    IndexType type) const noexcept {
  switch (type) {
    case IndexType::Primary:
      return *_primary;
    case IndexType::Edge:
      return *_edge;
    case IndexType::Geo:
      return *_geo;
    case IndexType::Geo1:
      return *_geo1;
    case IndexType::Geo2:
      return *_geo2;
    case IndexType::Hash:
      return *_hash;
    case IndexType::Persistent:
      return *_persistent;
    case IndexType::Skiplist:
      return *_skiplist;
    case IndexType::TTL:
      return *_ttl;
    case IndexType::Fulltext:
      return *_fulltext;
    case IndexType::Zkd:
      return *_zkd;
    case IndexType::MDI:
      return *_mdi;
    case IndexType::MDIPrefixed:
      return *_mdiPrefixed;
    case IndexType::Vector:
      return *_vector;
    case IndexType::Inverted:
      return *_inverted;
    case IndexType::IResearchLink:
      return linkFactory();
    case IndexType::Unknown:
    case IndexType::NoAccess:
      return invalidFactory();
  }
  return invalidFactory();
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
