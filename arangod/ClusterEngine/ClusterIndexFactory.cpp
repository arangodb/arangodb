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

using namespace arangodb;

ClusterIndexFactory::ClusterIndexFactory() {
  emplaceFactory("edge",
                 [](LogicalCollection* collection,
                    velocypack::Slice const& definition, TRI_idx_iid_t id,
                    bool isClusterConstructor) -> std::shared_ptr<Index> {
                   if (!isClusterConstructor) {
                     // this indexes cannot be created directly
                     THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                                    "cannot create edge index");
                   }

                   return std::make_shared<ClusterIndex>(
                    id, collection, Index::TRI_IDX_TYPE_EDGE_INDEX, definition);
                 });

  emplaceFactory("fulltext",
                 [](LogicalCollection* collection,
                    velocypack::Slice const& definition, TRI_idx_iid_t id,
                    bool isClusterConstructor) -> std::shared_ptr<Index> {
                   return std::make_shared<ClusterIndex>(id, collection, Index::TRI_IDX_TYPE_FULLTEXT_INDEX, definition);
                 });

  emplaceFactory("geo1",
                 [](LogicalCollection* collection,
                    velocypack::Slice const& definition, TRI_idx_iid_t id,
                    bool isClusterConstructor) -> std::shared_ptr<Index> {
                   return std::make_shared<ClusterIndex>(id, collection, Index::TRI_IDX_TYPE_GEO1_INDEX, definition);
                 });

  emplaceFactory("geo2",
                 [](LogicalCollection* collection,
                    velocypack::Slice const& definition, TRI_idx_iid_t id,
                    bool isClusterConstructor) -> std::shared_ptr<Index> {
                   return std::make_shared<ClusterIndex>(id, collection, Index::TRI_IDX_TYPE_GEO2_INDEX, definition);
                 });

  emplaceFactory("geo",
                 [](LogicalCollection* collection,
                    velocypack::Slice const& definition, TRI_idx_iid_t id,
                    bool isClusterConstructor) -> std::shared_ptr<Index> {
                   return std::make_shared<ClusterIndex>(id, collection, Index::TRI_IDX_TYPE_GEO_INDEX, definition);
                 });

  emplaceFactory("hash",
                 [](LogicalCollection* collection,
                    velocypack::Slice const& definition, TRI_idx_iid_t id,
                    bool isClusterConstructor) -> std::shared_ptr<Index> {
                   return std::make_shared<ClusterIndex>(id, collection, Index::TRI_IDX_TYPE_HASH_INDEX, definition);
                 });

  emplaceFactory("persistent",
                 [](LogicalCollection* collection,
                    velocypack::Slice const& definition, TRI_idx_iid_t id,
                    bool isClusterConstructor) -> std::shared_ptr<Index> {
                   return std::make_shared<ClusterIndex>(
                       id, collection, Index::TRI_IDX_TYPE_PERSISTENT_INDEX, definition);
                 });

  emplaceFactory("primary",
                 [](LogicalCollection* collection,
                    velocypack::Slice const& definition, TRI_idx_iid_t id,
                    bool isClusterConstructor) -> std::shared_ptr<Index> {
                   if (!isClusterConstructor) {
                     // this indexes cannot be created directly
                     THROW_ARANGO_EXCEPTION_MESSAGE(
                         TRI_ERROR_INTERNAL, "cannot create primary index");
                   }

                   return std::make_shared<ClusterIndex>(0, collection, Index::TRI_IDX_TYPE_PRIMARY_INDEX, definition);
                 });

  emplaceFactory("skiplist",
                 [](LogicalCollection* collection,
                    velocypack::Slice const& definition, TRI_idx_iid_t id,
                    bool isClusterConstructor) -> std::shared_ptr<Index> {
                   return std::make_shared<ClusterIndex>(id, collection, Index::TRI_IDX_TYPE_SKIPLIST_INDEX, definition);
                 });
}

Result ClusterIndexFactory::enhanceIndexDefinition(
        velocypack::Slice const definition,
        velocypack::Builder& normalized,
        bool isCreation,
        bool isCoordinator
      ) const {
  ClusterEngine* ce = static_cast<ClusterEngine*>(EngineSelectorFeature::ENGINE);
  return ce->actualEngine()->indexFactory().enhanceIndexDefinition(definition, normalized, isCreation, isCoordinator);
}

void ClusterIndexFactory::fillSystemIndexes(
    arangodb::LogicalCollection* col,
    std::vector<std::shared_ptr<arangodb::Index>>& systemIndexes) const {
  // create primary index
  VPackBuilder input;
  input.openObject();
  input.add("type", VPackValue("primary"));
  input.add("id", VPackValue("0"));
  input.add("fields", VPackValue(VPackValueType::Array));
  input.add(VPackValue(StaticStrings::KeyString));
  input.close();
  input.close();

  systemIndexes.emplace_back(
      std::make_shared<arangodb::ClusterIndex>(0, col, Index::TRI_IDX_TYPE_PRIMARY_INDEX, input.slice()));
  // create edges indexes
  if (col->type() == TRI_COL_TYPE_EDGE) {
    ClusterEngine* ce = static_cast<ClusterEngine*>(EngineSelectorFeature::ENGINE);

    // first edge index
    input.clear();
    input.openObject();
    input.add("type", VPackValue(Index::oldtypeName(Index::TRI_IDX_TYPE_EDGE_INDEX)));
    input.add("id", VPackValue("1"));
    input.add("fields", VPackValue(VPackValueType::Array));
    input.add(VPackValue(StaticStrings::FromString));
    if (ce->isMMFiles()) {
      input.add(VPackValue(StaticStrings::ToString));
    }
    input.close();
    input.close();
    systemIndexes.emplace_back(std::make_shared<arangodb::ClusterIndex>(
        1, col, Index::TRI_IDX_TYPE_EDGE_INDEX, input.slice()));
    
    // second edge index
    if (ce->isRocksDB()) {
      input.clear();
      input.openObject();
      input.add("type", VPackValue(Index::oldtypeName(Index::TRI_IDX_TYPE_EDGE_INDEX)));
      input.add("id", VPackValue("2"));
      input.add("fields", VPackValue(VPackValueType::Array));
      input.add(VPackValue(StaticStrings::ToString));
      input.close();
      input.close();
      systemIndexes.emplace_back(std::make_shared<arangodb::ClusterIndex>(
        2, col, Index::TRI_IDX_TYPE_EDGE_INDEX, input.slice()));
    }
  }
}

void ClusterIndexFactory::prepareIndexes(LogicalCollection* col, VPackSlice const& indexesSlice,
                                         std::vector<std::shared_ptr<arangodb::Index>>& indexes) const {
  TRI_ASSERT(indexesSlice.isArray());
  
  for (auto const& v : VPackArrayIterator(indexesSlice)) {
    if (basics::VelocyPackHelper::getBooleanValue(v, "error", false)) {
      // We have an error here.
      // Do not add index.
      continue;
    }
    
    auto idx = prepareIndexFromSlice(v, false, col, true);
    if (!idx) {
      LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
      << "error creating index from definition '"
      << indexesSlice.toString() << "'";
      continue;
    }
    indexes.emplace_back(std::move(idx));
  }
}

std::vector<std::string> ClusterIndexFactory::supportedIndexes() const {
  return std::vector<std::string>{"primary",    "edge", "hash",    "skiplist",
                                  "persistent", "geo",  "fulltext"};
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
