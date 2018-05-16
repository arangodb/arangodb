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
#include "ClusterEngine/ClusterIndex.h"
#include "Indexes/Index.h"
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

                   auto fields = definition.get("fields");
                   TRI_ASSERT(fields.isArray() && fields.length() == 1);
                   auto direction = fields.at(0).copyString();
                   TRI_ASSERT(direction == StaticStrings::FromString ||
                              direction == StaticStrings::ToString);

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

void ClusterIndexFactory::fillSystemIndexes(
    arangodb::LogicalCollection* col,
    std::vector<std::shared_ptr<arangodb::Index>>& systemIndexes) const {
  // create primary index
  VPackBuilder builder;
  builder.openObject();
  builder.close();

  systemIndexes.emplace_back(
      std::make_shared<arangodb::ClusterIndex>(0, col, Index::TRI_IDX_TYPE_PRIMARY_INDEX, builder.slice()));
  // create edges indexes
  if (col->type() == TRI_COL_TYPE_EDGE) {
    systemIndexes.emplace_back(std::make_shared<arangodb::ClusterIndex>(
        1, col, Index::TRI_IDX_TYPE_EDGE_INDEX, builder.slice()));
    systemIndexes.emplace_back(std::make_shared<arangodb::ClusterIndex>(
        2, col, Index::TRI_IDX_TYPE_EDGE_INDEX, builder.slice()));
  }
#warning FIX for mmfiles
}

std::vector<std::string> ClusterIndexFactory::supportedIndexes() const {
  return std::vector<std::string>{"primary",    "edge", "hash",    "skiplist",
                                  "persistent", "geo",  "fulltext"};
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
