////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_CLUSTER_QUERY_H
#define ARANGOD_AQL_CLUSTER_QUERY_H 1

#include "Aql/Query.h"
#include "Cluster/ClusterTypes.h"
#include "Futures/Future.h"

namespace arangodb {
namespace aql {

// additonally contains TraversalEngines
class ClusterQuery final : public arangodb::aql::Query {
 public:
  
  /// Used to construct a full query
  ClusterQuery(std::shared_ptr<arangodb::transaction::Context> const& ctx,
               QueryOptions&& options);
  ~ClusterQuery();
  
  traverser::GraphEngineList const& traversers() const {
    return _traversers;
  }
  
  void prepareClusterQuery(arangodb::velocypack::Slice querySlice,
                           arangodb::velocypack::Slice collections,
                           arangodb::velocypack::Slice variables,
                           arangodb::velocypack::Slice snippets,
                           arangodb::velocypack::Slice traversals,
                           arangodb::velocypack::Builder& answer,
                           arangodb::QueryAnalyzerRevisions const& analyzersRevision);
  
  arangodb::futures::Future<Result> finalizeClusterQuery(ErrorCode errorCode);

 private:
  /// @brief first one should be the local one
  arangodb::traverser::GraphEngineList _traversers;
};

}  // namespace aql
}  // namespace arangodb

#endif // ARANGOD_AQL_CLUSTER_QUERY_H
