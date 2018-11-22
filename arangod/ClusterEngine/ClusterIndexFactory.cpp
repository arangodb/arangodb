////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace {

struct DefaultIndexFactory: public arangodb::IndexTypeFactory {
  std::string const _type;

  DefaultIndexFactory(std::string const& type): _type(type) {}

  virtual bool equal(
      arangodb::velocypack::Slice const& lhs,
      arangodb::velocypack::Slice const& rhs
  ) const override {
    auto* clusterEngine = static_cast<arangodb::ClusterEngine*>(
      arangodb::EngineSelectorFeature::ENGINE
    );

    if (!clusterEngine) {
      THROW_ARANGO_EXCEPTION(arangodb::Result(
        TRI_ERROR_INTERNAL,
        "cannot find cluster engine while normalizing index"
      ));
    }

    auto* engine = clusterEngine->actualEngine();

    if (!engine) {
      THROW_ARANGO_EXCEPTION(arangodb::Result(
        TRI_ERROR_INTERNAL,
        "cannot find storage engine while normalizing index"
      ));
    }

    return engine->indexFactory().factory(_type).equal(lhs, rhs);
  }

  virtual arangodb::Result instantiate(
      std::shared_ptr<arangodb::Index>& index,
      arangodb::LogicalCollection& collection,
      arangodb::velocypack::Slice const& definition,
      TRI_idx_iid_t id,
      bool //isClusterConstructor
  ) const override {
    auto* clusterEngine = static_cast<arangodb::ClusterEngine*>(
      arangodb::EngineSelectorFeature::ENGINE
    );

    if (!clusterEngine) {
      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        "cannot find cluster engine while creating index"
      );
    }

    auto ct = clusterEngine->engineType();

    index = std::make_shared<arangodb::ClusterIndex>(
      id, collection, ct, arangodb::Index::type(_type), definition
    );

    return arangodb::Result();
  }

  virtual arangodb::Result normalize(
      arangodb::velocypack::Builder& normalized,
      arangodb::velocypack::Slice definition,
      bool isCreation
  ) const override {
    auto* clusterEngine = static_cast<arangodb::ClusterEngine*>(
      arangodb::EngineSelectorFeature::ENGINE
    );

    if (!clusterEngine) {
      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        "cannot find cluster engine while normalizing index"
      );
    }

    auto* engine = clusterEngine->actualEngine();

    if (!engine) {
      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        "cannot find storage engine while normalizing index"
      );
    }

    return engine->indexFactory().factory(_type).normalize(
      normalized, definition, isCreation
    );
  }
};

struct EdgeIndexFactory: public DefaultIndexFactory {
  EdgeIndexFactory(std::string const& type): DefaultIndexFactory(type) {}

  virtual arangodb::Result instantiate(
      std::shared_ptr<arangodb::Index>& index,
      arangodb::LogicalCollection& collection,
      arangodb::velocypack::Slice const& definition,
      TRI_idx_iid_t id,
      bool isClusterConstructor
  ) const override {
    if (!isClusterConstructor) {
      // this indexes cannot be created directly
      return arangodb::Result(TRI_ERROR_INTERNAL, "cannot create edge index");
    }

    auto* clusterEngine = static_cast<arangodb::ClusterEngine*>(
      arangodb::EngineSelectorFeature::ENGINE
    );

    if (!clusterEngine) {
      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        "cannot find storage engine while creating index"
      );
    }

    auto ct = clusterEngine->engineType();

    index = std::make_shared<arangodb::ClusterIndex>(
      id, collection, ct, arangodb::Index::TRI_IDX_TYPE_EDGE_INDEX, definition
    );

    return arangodb::Result();
  }
};

struct PrimaryIndexFactory: public DefaultIndexFactory {
  PrimaryIndexFactory(std::string const& type): DefaultIndexFactory(type) {}

  virtual arangodb::Result instantiate(
      std::shared_ptr<arangodb::Index>& index,
      arangodb::LogicalCollection& collection,
      arangodb::velocypack::Slice const& definition,
      TRI_idx_iid_t id,
      bool isClusterConstructor
  ) const override {
    if (!isClusterConstructor) {
      // this indexes cannot be created directly
      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        "cannot create primary index"
      );
    }

    auto* clusterEngine = static_cast<arangodb::ClusterEngine*>(
      arangodb::EngineSelectorFeature::ENGINE
    );

    if (!clusterEngine) {
      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        "cannot find storage engine while creating index"
      );
    }

    auto ct = clusterEngine->engineType();

    index = std::make_shared<arangodb::ClusterIndex>(
      0, collection, ct, arangodb::Index::TRI_IDX_TYPE_PRIMARY_INDEX, definition
    );

    return arangodb::Result();
  }
};

}

namespace arangodb {

ClusterIndexFactory::ClusterIndexFactory() {
  static const EdgeIndexFactory edgeIndexFactory("edge");
  static const DefaultIndexFactory fulltextIndexFactory("fulltext");
  static const DefaultIndexFactory geoIndexFactory("geo");
  static const DefaultIndexFactory geo1IndexFactory("geo1");
  static const DefaultIndexFactory geo2IndexFactory("geo2");
  static const DefaultIndexFactory hashIndexFactory("hash");
  static const DefaultIndexFactory persistentIndexFactory("persistent");
  static const PrimaryIndexFactory primaryIndexFactory("primary");
  static const DefaultIndexFactory skiplistIndexFactory("skiplist");

  emplace(edgeIndexFactory._type, edgeIndexFactory);
  emplace(fulltextIndexFactory._type, fulltextIndexFactory);
  emplace(geoIndexFactory._type, geoIndexFactory);
  emplace(geo1IndexFactory._type, geo1IndexFactory);
  emplace(geo2IndexFactory._type, geo2IndexFactory);
  emplace(hashIndexFactory._type, hashIndexFactory);
  emplace(persistentIndexFactory._type, persistentIndexFactory);
  emplace(primaryIndexFactory._type, primaryIndexFactory);
  emplace(skiplistIndexFactory._type, skiplistIndexFactory);
}

Result ClusterIndexFactory::enhanceIndexDefinition(VPackSlice const definition,
                                                   VPackBuilder& normalized,
                                                   bool isCreation,
                                                   bool isCoordinator) const {
  auto* ce = static_cast<ClusterEngine*>(EngineSelectorFeature::ENGINE);
  
  if (!ce) {
    return TRI_ERROR_INTERNAL;
  }
  auto* ae = ce->actualEngine();
  if (!ae) {
    return TRI_ERROR_INTERNAL;
  }
  return ae->indexFactory().enhanceIndexDefinition(definition, normalized,
                                                   isCreation, isCoordinator);
}

void ClusterIndexFactory::fillSystemIndexes(
    arangodb::LogicalCollection& col,
    std::vector<std::shared_ptr<arangodb::Index>>& systemIndexes
) const {
  // create primary index
  VPackBuilder input;
  input.openObject();
  input.add(StaticStrings::IndexType, VPackValue("primary"));
  input.add(StaticStrings::IndexId, VPackValue("0"));
  input.add(StaticStrings::IndexFields, VPackValue(VPackValueType::Array));
  input.add(VPackValue(StaticStrings::KeyString));
  input.close();
  input.add(StaticStrings::IndexUnique, VPackValue(true));
  input.add(StaticStrings::IndexSparse, VPackValue(false));
  input.close();

  // get the storage engine type
  ClusterEngine* ce =
      static_cast<ClusterEngine*>(EngineSelectorFeature::ENGINE);
  ClusterEngineType ct = ce->engineType();

  systemIndexes.emplace_back(std::make_shared<arangodb::ClusterIndex>(
      0, col, ct, Index::TRI_IDX_TYPE_PRIMARY_INDEX, input.slice()));

  // create edges indexes
  if (col.type() == TRI_COL_TYPE_EDGE) {
    // first edge index
    input.clear();
    input.openObject();
    input.add(StaticStrings::IndexType,
              VPackValue(Index::oldtypeName(Index::TRI_IDX_TYPE_EDGE_INDEX)));
    input.add(StaticStrings::IndexId, VPackValue("1"));
    input.add(StaticStrings::IndexFields, VPackValue(VPackValueType::Array));
    input.add(VPackValue(StaticStrings::FromString));

    if (ct == ClusterEngineType::MMFilesEngine) {
      input.add(VPackValue(StaticStrings::ToString));
    }

    input.close();
    input.add(StaticStrings::IndexUnique, VPackValue(false));
    input.add(StaticStrings::IndexSparse, VPackValue(false));
    input.close();
    systemIndexes.emplace_back(std::make_shared<arangodb::ClusterIndex>(
        1, col, ct, Index::TRI_IDX_TYPE_EDGE_INDEX, input.slice()));

    // second edge index
    if (ct == ClusterEngineType::RocksDBEngine) {
      input.clear();
      input.openObject();
      input.add(StaticStrings::IndexType,
                VPackValue(Index::oldtypeName(Index::TRI_IDX_TYPE_EDGE_INDEX)));
      input.add(StaticStrings::IndexId, VPackValue("2"));
      input.add(StaticStrings::IndexFields, VPackValue(VPackValueType::Array));
      input.add(VPackValue(StaticStrings::ToString));
      input.close();
      input.add(StaticStrings::IndexUnique, VPackValue(false));
      input.add(StaticStrings::IndexSparse, VPackValue(false));
      input.close();
      systemIndexes.emplace_back(std::make_shared<arangodb::ClusterIndex>(
          2, col, ct, Index::TRI_IDX_TYPE_EDGE_INDEX, input.slice()));
    }
  }
}

void ClusterIndexFactory::prepareIndexes(
    LogicalCollection& col,
    arangodb::velocypack::Slice const& indexesSlice,
    std::vector<std::shared_ptr<arangodb::Index>>& indexes
) const {
  TRI_ASSERT(indexesSlice.isArray());

  for (auto const& v : VPackArrayIterator(indexesSlice)) {
    if (basics::VelocyPackHelper::getBooleanValue(v, "error", false)) {
      // We have an error here. Do not add.
      continue;
    }

    if (basics::VelocyPackHelper::getBooleanValue(v, "isBuilding", false)) {
      // This index is still being built. Do not add.
      continue;
    }

    auto idx = prepareIndexFromSlice(v, false, col, true);

    if (!idx) {
      LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
        << "error creating index from definition '" << v.toString() << "'";

      continue;
    }

    indexes.emplace_back(std::move(idx));
  }
}

} // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------