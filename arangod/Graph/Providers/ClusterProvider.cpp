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
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"

#include "Basics/StringUtils.h"

#include <vector>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::futures;
using namespace arangodb::graph;

namespace {
std::string const edgeUrl = "/_internal/traverser/edge/";
std::string const vertexUrl = "/_internal/traverser/vertex/";
}  // namespace

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

ClusterProvider::Step::Step(VertexType v)
    : _vertex(v), _edge(), _fetched(false) {}

ClusterProvider::Step::Step(VertexType v, EdgeDocumentToken edge, size_t prev)
    : BaseStep(prev), _vertex(v), _edge(std::move(edge)), _fetched(false) {}

ClusterProvider::Step::~Step() = default;

VertexType const& ClusterProvider::Step::Vertex::getID() const {
  return _vertex;
}
EdgeDocumentToken const& ClusterProvider::Step::Edge::getID() const {
  return _token;
}
bool ClusterProvider::Step::Edge::isValid() const {
  // TODO: Check isValid, when it is actually valid here in cluster case?
  if (getID().vpack() != nullptr) {
    return true;
  }
  return false;
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
      _opts(std::move(opts)),
      _stats{} {
  _cursor = buildCursor();
}

std::unique_ptr<RefactoredClusterEdgeCursor> ClusterProvider::buildCursor() {
  return std::make_unique<RefactoredClusterEdgeCursor>(trx(), _opts.getExpressionContext(),
                                                       _opts.getCache(),
                                                       _opts.isBackward());
}

auto ClusterProvider::startVertex(VertexType vertex) -> Step {
  LOG_TOPIC("78156", TRACE, Logger::GRAPHS) << "<ClusterProvider> Start Vertex:" << vertex;

  // Create default initial step
  // Note: Refactor naming, Strings in our cache here are not allowed to be removed.
  LOG_DEVEL << "Cluster Provider set startVertex to:" << vertex.toString();
  return Step(_opts.getCache()->persistString(vertex));
}

auto ClusterProvider::fetch(std::vector<Step*> const& looseEnds)
    -> futures::Future<std::vector<Step*>> {
  LOG_TOPIC("c9160", TRACE, Logger::GRAPHS) << "<ClusterProvider> Fetching...";
  std::vector<Step*> result{};

  if (looseEnds.size() > 0) {
    result.reserve(looseEnds.size());
    auto const* engines = _opts.getCache()->engines();

    // slow path, sharding not deducable from _id
    transaction::BuilderLeaser leased(trx());
    leased->openObject();
    leased->add("keys", VPackValue(VPackValueType::Array));
    for (auto const& looseEnd : looseEnds) {
      LOG_DEVEL << " -> " << looseEnd->toString();
      leased->add(VPackValuePair(looseEnd->getVertex().getID().data(),
                                 looseEnd->getVertex().getID().length(),
                                 VPackValueType::String));
    }
    leased->close();  // 'keys' Array
    leased->close();  // base object

    auto* pool = trx()->vocbase().server().getFeature<NetworkFeature>().pool();

    network::RequestOptions reqOpts;
    reqOpts.database = trx()->vocbase().name();
    reqOpts.skipScheduler = true;  // hack to avoid scheduler queue

    std::vector<Future<network::Response>> futures;
    futures.reserve(engines->size());

    for (auto const& engine : *engines) {
      futures.emplace_back(
          network::sendRequestRetry(pool, "server:" + engine.first, fuerte::RestVerb::Put,
                                    ::vertexUrl + StringUtils::itoa(engine.second),
                                    leased->bufferRef(), reqOpts));
    }

    for (Future<network::Response>& f : futures) {
      network::Response const& r = f.get();

      if (r.fail()) {
        THROW_ARANGO_EXCEPTION(network::fuerteToArangoErrorCode(r));
      }

      auto payload = r.response().stealPayload();
      VPackSlice resSlice(payload->data());
      if (!resSlice.isObject()) {
        // Response has invalid format
        THROW_ARANGO_EXCEPTION(TRI_ERROR_HTTP_CORRUPTED_JSON);
      }
      if (r.statusCode() != fuerte::StatusOK) {
        // We have an error case here. Throw it.
        THROW_ARANGO_EXCEPTION(network::resultFromBody(resSlice, TRI_ERROR_INTERNAL));
      }

      bool cached = false;
      for (auto pair : VPackObjectIterator(resSlice, /*sequential*/ true)) {
        arangodb::velocypack::HashedStringRef key(pair.key);
        /*if (ADB_UNLIKELY(vertexIds.erase(key) == 0)) {
         // TODO: Check!
          // We either found the same vertex twice,
          // or found a vertex we did not request.
          // Anyways something somewhere went seriously wrong
          THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_GOT_CONTRADICTING_ANSWERS);
        }*/

        // TODO: Check!
        // TRI_ASSERT(result.find(key) == result.end());
        if (!cached) {
          _opts.getCache()->datalake().add(std::move(payload));
          cached = true;
        }

        // Protected by datalake
        LOG_DEVEL << "Found key: " << key.toString();
        LOG_DEVEL << pair.value.toString();

        // TODO: This needs to be optimised!!
        for (auto const& looseEnd : looseEnds) {
          if (looseEnd->getVertex().getID().equals(key)) {
            LOG_DEVEL << "found entry in loose ends";
            looseEnd->setFetched(_opts.getCache()->persistString(key));
            result.emplace_back(std::move(looseEnd));
          }
        }
      }
    }

    /* TODO: Check!
    bool forShortestPath = true;
    if (!forShortestPath) {
      // Fill everything we did not find with NULL
      for (auto const& v : vertexIds) {
        result.try_emplace(v, VPackSlice::nullSlice());
      }
      vertexIds.clear();
      */
  }

  // TODO: This needs to be changed!
  return futures::makeFuture(std::move(result));
}

auto ClusterProvider::expand(Step const& step, size_t previous,
                             std::function<void(Step)> const& callback) -> void {
  TRI_ASSERT(!step.isLooseEnd());
  auto const& vertex = step.getVertex();

  LOG_DEVEL << "expanding: " << vertex.getID().toString();

  TRI_ASSERT(_cursor != nullptr);
  _cursor->rearm(vertex.getID().stringRef(), 0);

  _cursor->readAll(_stats, [&](EdgeDocumentToken&& eid, VPackSlice edge, size_t) -> void {
    VertexType id = _opts.getCache()->persistString(([&]() -> auto {
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
  });
}

void ClusterProvider::addVertexToBuilder(Step::Vertex const& vertex,
                                         arangodb::velocypack::Builder& builder) {
  // NOTE: OLD SingleServer Variant
  //_cache.insertVertexIntoResult(_stats, vertex.getID(), builder);
  LOG_DEVEL << "trying to add vertex into result:";
  bool test = _opts.getCache()->appendVertex(vertex.getID().stringRef(), builder);
  if (test) {
    LOG_DEVEL << "Could find entry in cache.";
  } else {
    LOG_DEVEL << "Could NOT find entry in cache.";
  }
};

void ClusterProvider::insertEdgeIntoResult(EdgeDocumentToken edge,
                                           arangodb::velocypack::Builder& builder) {
  // NOTE: OLD SingleServer Variant
  //_cache.insertEdgeIntoResult(_stats, edge, builder);
  LOG_DEVEL << "trying to add edge into result:";
  _opts.getCache()->insertEdgeIntoResult(edge, builder);
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