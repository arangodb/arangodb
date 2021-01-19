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
/// @author Heiko Kernbach
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "./ClusterProvider.h"

#include "Aql/QueryContext.h"
#include "Transaction/Helpers.h"

#include "Futures/Future.h"
#include "Futures/Utilities.h"

#include <vector>

using namespace arangodb;
using namespace arangodb::graph;

namespace arangodb {
namespace graph {
auto operator<<(std::ostream& out, ClusterProvider::Step const& step) -> std::ostream& {
  out << step._vertex.getID();
  return out;
}
}  // namespace graph
}  // namespace arangodb

/*
IndexAccessor::IndexAccessor(transaction::Methods::IndexHandle idx,
                             aql::AstNode* condition, std::optional<size_t> memberToUpdate)
    : _idx(idx), _indexCondition(condition), _memberToUpdate(memberToUpdate) {}

aql::AstNode* IndexAccessor::getCondition() const { return _indexCondition; }
transaction::Methods::IndexHandle IndexAccessor::indexHandle() const {
  return _idx;
}
std::optional<size_t> IndexAccessor::getMemberToUpdate() const {
  return _memberToUpdate;
}*/

ClusterProvider::Step::Step(VertexType v) : _vertex(v), _edge() {}

ClusterProvider::Step::Step(VertexType v, EdgeDocumentToken edge, size_t prev)
    : BaseStep(prev), _vertex(v), _edge(std::move(edge)) {}

ClusterProvider::Step::~Step() = default;

VertexType const& ClusterProvider::Step::Vertex::getID() const {
  return _vertex;
}
EdgeDocumentToken const& ClusterProvider::Step::Edge::getID() const {
  return _token;
}
bool ClusterProvider::Step::Edge::isValid() const {
  return getID().localDocumentId() != DataSourceId::none();
};

void ClusterProvider::addEdgeToBuilder(Step::Edge const& edge,
                                       arangodb::velocypack::Builder& builder) {
  insertEdgeIntoResult(edge.getID(), builder);
};

void ClusterProvider::Step::Edge::addToBuilder(ClusterProvider& provider,
                                               arangodb::velocypack::Builder& builder) const {
  provider.insertEdgeIntoResult(getID(), builder);
}

ClusterProvider::ClusterProvider(arangodb::aql::QueryContext& queryContext,
                                 ClusterBaseProviderOptions opts,
                                 arangodb::ResourceMonitor& resourceMonitor)
    : _trx(std::make_unique<arangodb::transaction::Methods>(queryContext.newTrxContext())),
      _query(&queryContext),
      _resourceMonitor(&resourceMonitor),
      _stringHeap(resourceMonitor, 4096),
      _opts(std::move(opts)),
      _stats{} {
  _cursor = buildCursor();
}

std::unique_ptr<RefactoredClusterEdgeCursor> ClusterProvider::buildCursor() {
  return std::make_unique<RefactoredClusterEdgeCursor>(trx(), _opts.getExpressionContext(), _opts.getCache());
}

auto ClusterProvider::startVertex(VertexType vertex) -> Step {
  LOG_TOPIC("78156", TRACE, Logger::GRAPHS) << "<ClusterProvider> Start Vertex:" << vertex;

  // Create default initial step
  // Note: Refactor naming, Strings in our cache here are not allowed to be removed.
  LOG_DEVEL << "Cluster Provider set startVertex to:" << vertex.toString();
  return Step(persistString(vertex));
}

auto ClusterProvider::fetch(std::vector<Step*> const& looseEnds)
    -> futures::Future<std::vector<Step*>> {
  LOG_TOPIC("c9160", TRACE, Logger::GRAPHS) << "<ClusterProvider> Fetching...";
  std::vector<Step*> result{};
  result.reserve(looseEnds.size());

  LOG_DEVEL << "Loose ends are: ";
  for (auto le : looseEnds) {
    LOG_DEVEL << " -> " << le->toString();
  }

  return futures::makeFuture(std::move(result));
}

auto ClusterProvider::expand(Step const& step, size_t previous,
                             std::function<void(Step)> const& callback) -> void {
  TRI_ASSERT(!step.isLooseEnd());
  auto const& vertex = step.getVertex();

  // _query->vpackOptions()


  LOG_DEVEL << "expanding: " << vertex.getID().toString();

  /* NOTE: OLD SingleServer Variant
  TRI_ASSERT(_cursor != nullptr);
  _cursor->rearm(vertex.getID(), 0);
  _cursor->readAll(_stats, [&](EdgeDocumentToken&& eid, VPackSlice edge, size_t) -> void {
    VertexType id = _cache.persistString(([&]() -> auto {
      if (edge.isString()) {
        return VertexType(edge);
      } else {
        VertexType other(transaction::helpers::extractFromFromDocument(edge));
        if (other == vertex.getID()) {  // TODO: Check getId - discuss
          other = VertexType(transaction::helpers::extractToFromDocument(edge));
        }
        return other;
      }
    })());
    callback(Step{id, std::move(eid), previous});
  });*/
}

void ClusterProvider::addVertexToBuilder(Step::Vertex const& vertex,
                                         arangodb::velocypack::Builder& builder){
    // NOTE: OLD SingleServer Variant
    //_cache.insertVertexIntoResult(_stats, vertex.getID(), builder);
};

void ClusterProvider::insertEdgeIntoResult(EdgeDocumentToken edge,
                                           arangodb::velocypack::Builder& builder) {
  // NOTE: OLD SingleServer Variant
  //_cache.insertEdgeIntoResult(_stats, edge, builder);
}

arangodb::transaction::Methods* ClusterProvider::trx() { return _trx.get(); }

arangodb::ResourceMonitor* ClusterProvider::resourceMonitor() {
  return _resourceMonitor;
}

arangodb::aql::QueryContext* ClusterProvider::query() const { return _query; }

arangodb::aql::TraversalStats ClusterProvider::stealStats() {
  auto t = _stats;
  // Placement new of stats, do not reallocate space.
  _stats.~TraversalStats();
  new (&_stats) aql::TraversalStats{};
  return t;
}

arangodb::velocypack::HashedStringRef ClusterProvider::persistString(arangodb::velocypack::HashedStringRef idString) {
  auto it = _persistedStrings.find(idString);
  if (it != _persistedStrings.end()) {
    return *it;
  }
  auto res = _stringHeap.registerString(idString);
  _persistedStrings.emplace(res);
  LOG_DEVEL << "Persisted size: " << _persistedStrings.size();
  return res;
}