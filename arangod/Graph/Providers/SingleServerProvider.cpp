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

IndexAccessor::IndexAccessor(transaction::Methods::IndexHandle idx,
                             aql::AstNode* condition, std::optional<size_t> memberToUpdate)
    : _idx(idx), _indexCondition(condition), _memberToUpdate(memberToUpdate) {}

aql::AstNode* IndexAccessor::getCondition() const { return _indexCondition; }
transaction::Methods::IndexHandle IndexAccessor::indexHandle() const {
  return _idx;
}
std::optional<size_t> IndexAccessor::getMemberToUpdate() const {
  return _memberToUpdate;
}

BaseProviderOptions::BaseProviderOptions(aql::Variable const* tmpVar,
                                         std::vector<IndexAccessor> indexInfo)
    : _temporaryVariable(tmpVar), _indexInformation(std::move(indexInfo)) {}

aql::Variable const* BaseProviderOptions::tmpVar() const {
  return _temporaryVariable;
}

std::vector<IndexAccessor> const& BaseProviderOptions::indexInformations() const {
  return _indexInformation;
}

SingleServerProvider::Step::Step(VertexType v)
    : _vertex(v), _edge(std::nullopt) {}

SingleServerProvider::Step::Step(VertexType v, EdgeDocumentToken edge, size_t prev)
    : BaseStep(prev), _vertex(v), _edge(std::in_place, std::move(edge)) {}

SingleServerProvider::Step::~Step() = default;

VertexType SingleServerProvider::Step::Vertex::data() const { return _vertex; }

SingleServerProvider::SingleServerProvider(arangodb::aql::QueryContext& queryContext,
                                           BaseProviderOptions opts)
    : _trx{queryContext.newTrxContext()},
      _query(queryContext),
      _cache(RefactoredTraverserCache{&_trx, &queryContext}),
      _opts(std::move(opts)) {
  // activateCache(false); // TODO CHECK RefactoredTraverserCache
  _cursor = buildCursor();
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

auto SingleServerProvider::startVertex(VertexType vertex) -> Step {
  LOG_TOPIC("78156", TRACE, Logger::GRAPHS)
      << "<MockGraphProvider> Start Vertex:" << vertex;
  // Should not clear anything: clearCursor(vertex);

  // Create default initial step

  // TODO Implement minimal variant of _cache in the Provider
  // This should only handle the HashedStringRef storage (string heap, and Lookup List)
  return Step(_cache.persistString(vertex));
}

auto SingleServerProvider::fetch(std::vector<Step*> const& looseEnds)
    -> futures::Future<std::vector<Step*>> {
  LOG_TOPIC("78156", TRACE, Logger::GRAPHS)
      << "<MockGraphProvider> Fetching...";
  std::vector<Step*> result{};  // TODO: Question: Modify inplace or build up new result vector
  result.reserve(looseEnds.size());
  // TODO do we need to do something at all?
  // We may want to cache the Vertex Data
  return futures::makeFuture(std::move(result));
}

auto SingleServerProvider::expand(Step const& step, size_t previous) -> std::vector<Step> {
  TRI_ASSERT(!step.isLooseEnd());
  std::vector<Step> result{};
  auto const& vertex = step.getVertex();
  TRI_ASSERT(_cursor != nullptr);
  _cursor->rearm(vertex.data(), 0);
  _cursor->readAll([&](EdgeDocumentToken&& eid, VPackSlice edge, size_t /*cursorIdx*/) -> void {
    VertexType id = _cache.persistString(([&]() -> auto {
      if (edge.isString()) {
        return VertexType(edge);
      } else {
        VertexType other(transaction::helpers::extractFromFromDocument(edge));
        if (other == vertex.data()) {  // TODO: Check getId
          other = VertexType(transaction::helpers::extractToFromDocument(edge));
        }
        return other;
      }

      result.emplace_back(id, std::move(eid), previous);
    })());
  });
  return result;
}

std::unique_ptr<RefactoredSingleServerEdgeCursor> SingleServerProvider::buildCursor() {
  return std::make_unique<RefactoredSingleServerEdgeCursor>(trx(), query(),
                                                            _opts.tmpVar(),
                                                            _opts.indexInformations());
}

arangodb::transaction::Methods* SingleServerProvider::trx() { return &_trx; }

arangodb::aql::QueryContext* SingleServerProvider::query() const {
  return &_query;
}
