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

namespace arangodb {

ClusterIndexFactory::ClusterIndexFactory() {
  emplaceFactory(
    "edge",
    [](
        LogicalCollection& collection,
        velocypack::Slice const& definition,
        TRI_idx_iid_t id,
        bool isClusterConstructor
    )->std::shared_ptr<Index> {
      if (!isClusterConstructor) {
        // this indexes cannot be created directly
        THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL, "cannot create edge index"
        );
      }

      auto* ce = static_cast<ClusterEngine*>(EngineSelectorFeature::ENGINE);
      auto ct = ce->engineType();

      return std::make_shared<ClusterIndex>(
        id, collection, ct, Index::TRI_IDX_TYPE_EDGE_INDEX, definition
      );
    }
  );

  emplaceFactory(
    "primary",
    [](
        LogicalCollection& collection,
        velocypack::Slice const& definition,
        TRI_idx_iid_t id,
        bool isClusterConstructor
    )->std::shared_ptr<Index> {
      if (!isClusterConstructor) {
        // this indexes cannot be created directly
        THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL, "cannot create primary index"
        );
      }

      auto* ce = static_cast<ClusterEngine*>(EngineSelectorFeature::ENGINE);
      auto ct = ce->engineType();

      return std::make_shared<ClusterIndex>(
        0, collection, ct, Index::TRI_IDX_TYPE_PRIMARY_INDEX, definition
      );
    }
  );

  // both engines support all types right now
  static const std::vector<std::string> supported = {
    "fulltext",
    "geo",
    "geo1",
    "geo2",
    "hash",
    "persistent",
    "skiplist"
  };

  for (auto& typeStr: supported) {
    auto type = Index::type(typeStr);

    emplaceFactory(
      typeStr,
      [type](
          LogicalCollection& collection,
          velocypack::Slice const& definition,
          TRI_idx_iid_t id,
          bool isClusterConstructor
      )->std::shared_ptr<Index> {
        auto* ce = static_cast<ClusterEngine*>(EngineSelectorFeature::ENGINE);
        auto ct = ce->engineType();

        return std::make_shared<ClusterIndex>(
          id, collection, ct, type, definition
        );
      }
    );
  }
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
      // We have an error here.
      // Do not add index.
      continue;
    }

    if (basics::VelocyPackHelper::getBooleanValue(v, "isBuilding", false)) {
      // We have an error here.
      // Do not add index.
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
