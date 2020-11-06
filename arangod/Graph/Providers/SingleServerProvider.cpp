////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "./SingleServerProvider.h"

#include "Aql/QueryContext.h"
#include "Graph/Cursors/RefactoredSingleServerEdgeCursor.h"
#include "Transaction/Helpers.h"
//#include "Graph/TraverserCacheFactory.h" // TODO: remove
//#include "Graph/TraverserDocumentCache.h" // TODO: remove
//#include "Cache/CacheManagerFeature.h" // TODO: remove

#include "Futures/Future.h"
#include "Futures/Utilities.h"

#include <vector>

using namespace arangodb;
using namespace arangodb::graph;

SingleServerProvider::SingleServerProvider(arangodb::transaction::Methods* trx,
                                           arangodb::aql::QueryContext* queryContext)
    : _trx(trx), _query(queryContext), _cache(RefactoredTraverserCache{trx, queryContext}) {
  // activateCache(false); // TODO CHECK RefactoredTraverserCache
  buildCursor();
}

SingleServerProvider::~SingleServerProvider() = default;

void SingleServerProvider::activateCache(bool enableDocumentCache) {
  // Do not call this twice.
  // TRI_ASSERT(_cache == nullptr);
  // TODO: enableDocumentCache check + opts check + cacheManager check
  /*
  if (enableDocumentCache) {
    auto cacheManager = CacheManagerFeature::MANAGER;
    if (cacheManager != nullptr) {
      // TODO CHECK: cacheManager functionality
      //  std::shared_ptr<arangodb::cache::Cache> cache = cacheManager->createCache(cache::CacheType::Plain);
      if (cache != nullptr) {
        TraverserCache(query, options) return new TraverserCache(query, std::move(cache));
      }
    }
    // fallthrough intentional
  }*/
  //  _cache = new RefactoredTraverserCache(query());
}

auto SingleServerProvider::startVertex(arangodb::velocypack::StringRef vertex) -> Step {
  LOG_TOPIC("78156", TRACE, Logger::GRAPHS)
      << "<MockGraphProvider> Start Vertex:" << vertex;
  // Should not clear anything: clearCursor(vertex);

  // Create default initial step
  return Step(vertex);
}

auto SingleServerProvider::fetch(std::vector<Step*> const& looseEnds)
    -> futures::Future<std::vector<Step*>> {
  LOG_TOPIC("78156", TRACE, Logger::GRAPHS)
      << "<MockGraphProvider> Fetching...";
  std::vector<Step*> result{}; // TODO: Question: Modify inplace or build up new result vector
  result.reserve(looseEnds.size());
  for (auto* step : looseEnds) {
    TRI_ASSERT(step->isLooseEnd());
    auto const& vertex = step->getVertex();
    _cursor->rearm(vertex.getId(), 0);
    _cursor->readAll([&](EdgeDocumentToken&& eid, VPackSlice edge, size_t /*cursorIdx*/) -> void {
      VertexRef id = _cache.persistString(([&]() -> auto {
        if (edge.isString()) {
          return VertexRef(edge);
        } else {
          VertexRef other(transaction::helpers::extractFromFromDocument(edge));
          if (other == vertex.getId()) { // TODO: Check getId
            other = VertexRef(transaction::helpers::extractToFromDocument(edge));
          }
          return other;
        }
      })());

      LOG_DEVEL << id; // TODO: remove me - right now just for compilation

      step->setLooseEnd(false);
      // TODO: Where to emplace results? Should be already part of the cache? Right?
      // TODO IMPORTANT: Otherwise, what is the best method to store the data itself.
      //VertexIdentifier match{id, _searchIndex, std::move(eid)};
      //_shell.emplace(std::move(match));
    });
    result.emplace_back(step);
  }
  return futures::makeFuture(std::move(result));
}

std::unique_ptr<RefactoredSingleServerEdgeCursor> SingleServerProvider::buildCursor() {
  return std::make_unique<RefactoredSingleServerEdgeCursor>(trx(), query());
}

arangodb::transaction::Methods* SingleServerProvider::trx() const {
  return _trx;
}

arangodb::aql::QueryContext* SingleServerProvider::query() const {
  return _query;
}