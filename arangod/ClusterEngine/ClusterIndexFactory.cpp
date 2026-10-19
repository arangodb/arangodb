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
#include "Indexes/IndexTypeCatalog.h"
#include "IResearch/IResearchInvertedIndex.h"
#include "IResearch/IResearchInvertedClusterIndex.h"
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

namespace arangodb {

ClusterIndexFactory::ClusterIndexFactory(
    application_features::ApplicationServer& server, ClusterEngine& engine,
    IndexTypeCatalog const& catalog)
    : IndexFactory(server, catalog), _engine(engine) {}

std::shared_ptr<Index> ClusterIndexFactory::createGeneric(
    IndexType type, LogicalCollection& collection, velocypack::Slice definition,
    IndexId id) const {
  return std::make_shared<ClusterIndex>(id, collection, _engine.engineType(),
                                        type, definition);
}

std::shared_ptr<Index> ClusterIndexFactory::createIndex(
    IndexType type, IndexCreationContext const& ctx) const {
  auto const& [collection, definition, id, isClusterConstructor] = ctx;
  switch (type) {
    case IndexType::Primary:
      if (!isClusterConstructor) {
        // this index type cannot be created directly
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "cannot create primary index");
      }
      return createGeneric(IndexType::Primary, collection, definition,
                           IndexId::primary());
    case IndexType::Edge:
      if (!isClusterConstructor) {
        // this index type cannot be created directly
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "cannot create edge index");
      }
      return createGeneric(IndexType::Edge, collection, definition, id);
    case IndexType::Geo:
    case IndexType::Geo1:
    case IndexType::Geo2:
    case IndexType::Hash:
    case IndexType::Persistent:
    case IndexType::Skiplist:
    case IndexType::TTL:
    case IndexType::Fulltext:
    case IndexType::Zkd:
    case IndexType::MDI:
    case IndexType::MDIPrefixed:
    case IndexType::Vector:
      return createGeneric(type, collection, definition, id);
    case IndexType::Inverted: {
      using namespace arangodb::iresearch;
      auto nameSlice = definition.get(arangodb::StaticStrings::IndexName);
      std::string indexName;
      if (!nameSlice.isNone()) {
        if (!nameSlice.isString() || nameSlice.getStringLength() == 0) {
          LOG_TOPIC("91ebe", ERR, TOPIC)
              << "failed to initialize index from definition, error in "
                 "attribute '" +
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
    default:
      TRI_ASSERT(false) << "ClusterIndexFactory::createIndex called for "
                        << int(type);
      return nullptr;
  }
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
