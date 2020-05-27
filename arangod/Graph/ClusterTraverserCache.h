////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_GRAPH_CLUSTER_TRAVERSER_CACHE_H
#define ARANGOD_GRAPH_CLUSTER_TRAVERSER_CACHE_H 1

#include "Aql/types.h"
#include "Cluster/ClusterInfo.h"
#include "Graph/TraverserCache.h"
#include <velocypack/StringRef.h>

namespace arangodb {

namespace aql {
struct AqlValue;
}

namespace transaction {
class Methods;
}

namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

namespace graph {
struct BaseOptions;

class ClusterTraverserCache : public TraverserCache {
 public:
  ClusterTraverserCache(aql::QueryContext& query,
                        std::unordered_map<ServerID, aql::EngineId> const* engines,
                        BaseOptions*);

  ~ClusterTraverserCache() = default;

  /// @brief will convert the EdgeDocumentToken to a slice
  arangodb::velocypack::Slice lookupToken(EdgeDocumentToken const& token) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Inserts the real document stored within the token
  ///        into the given builder. No need for actual lookup
  //////////////////////////////////////////////////////////////////////////////
  void insertEdgeIntoResult(graph::EdgeDocumentToken const& idToken,
                            arangodb::velocypack::Builder& builder) override;

  /// Lookup document in cache and add it into the builder
  void insertVertexIntoResult(arangodb::velocypack::StringRef idString, velocypack::Builder& builder) override;
  /// Lookup document in cache and transform it to an AqlValue
  aql::AqlValue fetchVertexAqlResult(arangodb::velocypack::StringRef idString) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Return AQL value containing the result
  ///        The document will either be fetched from storage or looked up in
  ///        the datalake (on the coordinator)
  //////////////////////////////////////////////////////////////////////////////
  aql::AqlValue fetchEdgeAqlResult(graph::EdgeDocumentToken const& idToken) override;

  std::unordered_map<ServerID, aql::EngineId> const* engines() const {
    return _engines;
  }

  /// Map of already fetched vertices and edges (raw _id attribute)
  std::unordered_map<arangodb::velocypack::StringRef, arangodb::velocypack::Slice>& cache() {
    return _cache;
  }

  std::vector<std::shared_ptr<arangodb::velocypack::UInt8Buffer>>& datalake() {
    return _datalake;
  }

  size_t& insertedDocuments() { return _insertedDocuments; }

  size_t& filteredDocuments() { return _filteredDocuments; }

 private:
  /// @brief link by _id into our data dump
  std::unordered_map<arangodb::velocypack::StringRef, arangodb::velocypack::Slice> _cache;
  /// @brief dump for our edge and vertex documents
  std::vector<std::shared_ptr<arangodb::velocypack::UInt8Buffer>> _datalake;
  std::unordered_map<ServerID, aql::EngineId> const* _engines;
};

}  // namespace graph
}  // namespace arangodb
#endif
