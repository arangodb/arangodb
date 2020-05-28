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

#include "TraverserCacheFactory.h"

#include "Cache/Cache.h"
#include "Cache/CacheManagerFeature.h"
#include "Cluster/ServerState.h"
#include "Graph/ClusterTraverserCache.h"
#include "Graph/TraverserCache.h"
#include "Graph/TraverserDocumentCache.h"

using namespace arangodb;
using namespace arangodb::graph;
using namespace arangodb::traverser;
using namespace arangodb::graph::CacheFactory;

TraverserCache* CacheFactory::CreateCache(
    arangodb::aql::QueryContext& query, bool activateDocumentCache,
    std::unordered_map<ServerID, aql::EngineId> const* engines, BaseOptions* opts) {
  if (ServerState::instance()->isCoordinator()) {
    return new ClusterTraverserCache(query, engines, opts);
  }
  if (activateDocumentCache) {
    auto cacheManager = CacheManagerFeature::MANAGER;
    if (cacheManager != nullptr) {
      std::shared_ptr<arangodb::cache::Cache> cache = cacheManager->createCache(cache::CacheType::Plain);
      if (cache != nullptr) {
        return new TraverserDocumentCache(query, std::move(cache), opts);
      }
    }
    // fallthrough intentional
  }
  return new TraverserCache(query, opts);
}
