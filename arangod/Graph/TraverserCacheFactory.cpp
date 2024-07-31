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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "TraverserCacheFactory.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/QueryContext.h"
#include "Basics/ResourceUsage.h"
#include "Cache/BinaryKeyHasher.h"
#include "Cache/Cache.h"
#include "Cache/CacheManagerFeature.h"
#include "Cluster/ServerState.h"
#include "Graph/Cache/RefactoredClusterTraverserCache.h"
#include "Graph/ClusterTraverserCache.h"
#include "Graph/TraverserCache.h"
#include "Graph/TraverserDocumentCache.h"
#include "VocBase/vocbase.h"

namespace arangodb::graph {

TraverserCache* CacheFactory::CreateCache(
    arangodb::aql::QueryContext& query, bool activateDocumentCache,
    std::unordered_map<ServerID, aql::EngineId> const* engines,
    BaseOptions* opts) {
  if (ServerState::instance()->isCoordinator()) {
    return new ClusterTraverserCache(query, engines, opts);
  }
  if (activateDocumentCache) {
    auto cacheManager =
        query.vocbase().server().getFeature<CacheManagerFeature>().manager();
    if (cacheManager != nullptr) {
      std::shared_ptr<arangodb::cache::Cache> cache =
          cacheManager->createCache<cache::BinaryKeyHasher>(
              cache::CacheType::Plain);
      if (cache != nullptr) {
        return new TraverserDocumentCache(query, std::move(cache), opts);
      }
    }
    // fallthrough intentional
  }
  return new TraverserCache(query, opts);
}

}  // namespace arangodb::graph
