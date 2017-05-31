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

#include "Cluster/ServerState.h"
#include "Graph/ClusterTraverserCache.h"
#include "Graph/TraverserCache.h"
#include "Graph/TraverserDocumentCache.h"
#include "Logger/Logger.h"
#include "Transaction/Methods.h"

using namespace arangodb;
using namespace arangodb::graph;
using namespace arangodb::traverser;
using namespace arangodb::graph::cacheFactory;

TraverserCache* cacheFactory::CreateCache(
    arangodb::transaction::Methods* trx, bool activateDocumentCache,
    std::unordered_map<ServerID, traverser::TraverserEngineID> const* engines) {
  if (ServerState::instance()->isCoordinator()) {
    return new ClusterTraverserCache(trx, engines);
  }
  if (activateDocumentCache) {
    return new TraverserDocumentCache(trx);
  }
  return new TraverserCache(trx);
}
