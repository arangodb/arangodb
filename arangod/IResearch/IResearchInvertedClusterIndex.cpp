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

#include "IResearch/IResearchInvertedClusterIndex.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Metrics/ClusterMetricsFeature.h"

#include <absl/strings/str_cat.h>

namespace arangodb::iresearch {

void IResearchInvertedClusterIndex::toVelocyPack(
    VPackBuilder& builder,
    std::underlying_type<Index::Serialize>::type flags) const {
  bool const forPersistence =
      Index::hasFlag(flags, Index::Serialize::Internals);
  bool const forInventory = Index::hasFlag(flags, Index::Serialize::Inventory);
  VPackObjectBuilder objectBuilder(&builder);
  auto& vocbase = collection().vocbase();
  IResearchInvertedIndex::toVelocyPack(vocbase.server(), &vocbase, builder,
                                       forPersistence || forInventory);
  // can't use Index::toVelocyPack as it will try to output 'fields'
  // but we have custom storage format
  builder.add(arangodb::StaticStrings::IndexId,
              velocypack::Value(absl::AlphaNum{_iid.id()}.Piece()));
  builder.add(arangodb::StaticStrings::IndexType,
              velocypack::Value(oldtypeName(type())));
  builder.add(arangodb::StaticStrings::IndexName, velocypack::Value(name()));
  builder.add(arangodb::StaticStrings::IndexUnique, VPackValue(unique()));
  builder.add(arangodb::StaticStrings::IndexSparse, VPackValue(sparse()));

  if (isOutOfSync()) {
    // link is out of sync - we need to report that
    builder.add(StaticStrings::LinkError,
                VPackValue(StaticStrings::LinkErrorOutOfSync));
  }

  if (Index::hasFlag(flags, Index::Serialize::Figures)) {
    builder.add("figures", VPackValue(VPackValueType::Object));
    toVelocyPackFigures(builder);
    builder.close();
  }
}

bool IResearchInvertedClusterIndex::matchesDefinition(
    velocypack::Slice const& other) const {
  TRI_ASSERT(other.isObject());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto typeSlice = other.get(arangodb::StaticStrings::IndexType);
  TRI_ASSERT(typeSlice.isString());
  auto typeStr = typeSlice.stringView();
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
    auto idRef = value.stringView();
    return idRef == absl::AlphaNum{id().id()}.Piece();
  }
  return IResearchInvertedIndex::matchesDefinition(other,
                                                   collection().vocbase());
}

std::string IResearchInvertedClusterIndex::getCollectionName() const {
  return collection().name();
}

IResearchDataStore::Stats IResearchInvertedClusterIndex::stats() const {
  auto& cmf = collection()
                  .vocbase()
                  .server()
                  .getFeature<metrics::ClusterMetricsFeature>();
  auto data = cmf.getData();
  if (!data) {
    return {};
  }
  auto& metrics = data->metrics;
  auto labels = absl::StrCat(  // clang-format off
      "db=\"", getDbName(), "\","
      "index=\"", name(), "\","
      "collection=\"", getCollectionName(), "\"");  // clang-format on
  return {
      metrics.get<std::uint64_t>("arangodb_search_num_docs", labels),
      metrics.get<std::uint64_t>("arangodb_search_num_live_docs", labels),
      metrics.get<std::uint64_t>("arangodb_search_num_primary_docs", labels),
      metrics.get<std::uint64_t>("arangodb_search_num_segments", labels),
      metrics.get<std::uint64_t>("arangodb_search_num_files", labels),
      metrics.get<std::uint64_t>("arangodb_search_index_size", labels),
  };
}

IResearchInvertedClusterIndex::IResearchInvertedClusterIndex(
    IndexId iid, uint64_t /*objectId*/, LogicalCollection& collection,
    std::string const& name)
    : Index{iid, collection, name, {}, false, true},
      IResearchInvertedIndex{collection.vocbase().server(), collection} {
  initClusterMetrics();
}

}  // namespace arangodb::iresearch
