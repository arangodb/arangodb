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

#include "Cluster/ClusterInfo.h"
#include "Cluster/TraverserEngineRegistry.h"
#include "Graph/TraverserCache.h"

namespace arangodb {

class StringRef;

namespace aql {
struct AqlValue;
}

namespace transaction {
class Methods;
}

namespace velocypack {
class Builder;
class Slice;
}

namespace graph {

class ClusterTraverserCache : public TraverserCache {
 public:
  ClusterTraverserCache(
      transaction::Methods* trx,
      std::unordered_map<ServerID, traverser::TraverserEngineID> const*
          engines);

  ~ClusterTraverserCache();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Inserts the real document stored within the token
  ///        into the given builder.
  ///        The document will be looked up in the Datalake
  //////////////////////////////////////////////////////////////////////////////

  void insertIntoResult(StringRef idString,
                        arangodb::velocypack::Builder& builder) override;

  void insertIntoResult(graph::EdgeDocumentToken const* idToken,
                        arangodb::velocypack::Builder& builder) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Return AQL value containing the result
  ///        The document will be looked up in the Datalake
  //////////////////////////////////////////////////////////////////////////////

  aql::AqlValue fetchAqlResult(StringRef idString) override;

  aql::AqlValue fetchAqlResult(graph::EdgeDocumentToken const* idToken) override;

  std::unordered_map<ServerID, traverser::TraverserEngineID> const* engines();

  std::unordered_map<StringRef, arangodb::velocypack::Slice>& edges();

  std::vector<std::shared_ptr<arangodb::velocypack::Builder>>& datalake();

  size_t& insertedDocuments();

  size_t& filteredDocuments();

 private:
  std::unordered_map<StringRef, arangodb::velocypack::Slice> _edges;

  std::vector<std::shared_ptr<arangodb::velocypack::Builder>> _datalake;

  std::unordered_map<ServerID, traverser::TraverserEngineID> const* _engines;
};

}  // namespace graph
}  // namespace arangodb
#endif
