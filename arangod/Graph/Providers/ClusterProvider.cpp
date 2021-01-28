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
#include "Futures/Future.h"
#include "Futures/Utilities.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"
#include "Transaction/Helpers.h"

#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"

#include <vector>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::futures;
using namespace arangodb::graph;

using Helper = arangodb::basics::VelocyPackHelper;

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

ClusterProvider::Step::Step(VertexType v)
    : _vertex(v), _edge(), _fetched(false) {}

ClusterProvider::Step::Step(VertexType v, EdgeDocumentToken edge, size_t prev)
    : BaseStep(prev), _vertex(v), _edge(std::move(edge)), _fetched(false) {}

ClusterProvider::Step::~Step() = default;

VertexType const& ClusterProvider::Step::Vertex::getID() const {
  return _vertex;
}

VPackSlice const& ClusterProvider::Step::Vertex::getData() const {
  return _data;
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

void ClusterProvider::fetchVerticesFromEngines(std::vector<Step*> const& looseEnds,
                                               std::vector<Step*>& result) {
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

  size_t counter = 0;
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

    for (auto pair : VPackObjectIterator(resSlice, /*sequential*/ true)) {
      LOG_DEVEL << "Vertex in Cluster Provider: " << pair.key.toString();
      LOG_DEVEL << "WE Need the ID here, not the key!";  // TODO
      VertexType vertexKey(pair.key);
      /*if (ADB_UNLIKELY(vertexIds.erase(key) == 0)) {
       // TODO: Check!
        // We either found the same vertex twice,
        // or found a vertex we did not request.
        // Anyways something somewhere went seriously wrong
        THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_GOT_CONTRADICTING_ANSWERS);
      }*/

      if (!_opts.getCache()->isVertexCached(vertexKey)) {
        _opts.getCache()->datalake().add(std::move(payload));
        // Protected by datalake (Cache)
        _opts.getCache()->cacheVertex(std::move(vertexKey), pair.value);
      }

      // TODO check if counter is valid
      looseEnds.at(counter)->setFetched();
      result.emplace_back(std::move(looseEnds.at(counter)));
    }

    /* TODO: Check!
    bool forShortestPath = true;
    if (!forShortestPath) {
      // Fill everything we did not find with NULL
      for (auto const& v : vertexIds) {
        result.try_emplace(v, VPackSlice::nullSlice());
      }
    vertexIds.clear();
    }
    */

    counter++;
  }
}

Result ClusterProvider::fetchEdgesFromEnginesWithVariables(VertexType const& vertexId,
                                                           size_t& depth) {
  auto const* engines = _opts.getCache()->engines();
  auto& cache = _opts.getCache()->cache();
  size_t& filtered = _opts.getCache()->filteredDocuments();
  size_t& read = _opts.getCache()->insertedDocuments();

  // TODO map id => ServerID if possible
  // And go fast-path
  transaction::BuilderLeaser leased(trx());
  leased->openObject(true);
  leased->add("depth", VPackValue(depth));
  leased->add("keys", VPackValuePair(vertexId.data(), vertexId.length(),
                                     VPackValueType::String));
  leased->add(VPackValue("variables"));
  {
    leased->openArray();
    _opts.getExpressionContext().serializeAllVariables(trx()->vpackOptions(),
                                                       *(leased.get()));
    leased->close();
  }
  leased->close();

  auto* pool = trx()->vocbase().server().getFeature<NetworkFeature>().pool();

  network::RequestOptions reqOpts;
  reqOpts.database = trx()->vocbase().name();
  reqOpts.skipScheduler = true;  // hack to avoid scheduler queue

  std::vector<Future<network::Response>> futures;
  futures.reserve(engines->size());

  for (auto const& engine : *engines) {
    futures.emplace_back(
        network::sendRequestRetry(pool, "server:" + engine.first, fuerte::RestVerb::Put,
                                  ::edgeUrl + StringUtils::itoa(engine.second),
                                  leased->bufferRef(), reqOpts));
  }

  for (Future<network::Response>& f : futures) {
    network::Response const& r = f.get();

    if (r.fail()) {
      return network::fuerteToArangoErrorCode(r);
    }

    auto payload = r.response().stealPayload();
    VPackSlice resSlice(payload->data());
    if (!resSlice.isObject()) {
      // Response has invalid format
      return TRI_ERROR_HTTP_CORRUPTED_JSON;
    }

    Result res = network::resultFromBody(resSlice, TRI_ERROR_NO_ERROR);
    if (res.fail()) {
      return res;
    }
    filtered += Helper::getNumericValue<size_t>(resSlice, "filtered", 0);
    read += Helper::getNumericValue<size_t>(resSlice, "readIndex", 0);

    VPackSlice edges = resSlice.get("edges");
    bool allCached = true;
    VPackArrayIterator allEdges(edges);

    for (VPackSlice e : allEdges) {
      VPackSlice id = e.get(StaticStrings::IdString);
      if (!id.isString()) {
        // invalid id type
        LOG_TOPIC("a23b5", ERR, Logger::GRAPHS)
            << "got invalid edge id type: " << id.typeName();
        continue;
      }

      arangodb::velocypack::HashedStringRef idRef(id);
      auto resE = cache.try_emplace(idRef, e);
      if (resE.second) {
        // This edge is not yet cached.
        allCached = false;
      }
    }
    if (!allCached) {
      _opts.getCache()->datalake().add(std::move(payload));
    }
  }
  return {};
}

Result ClusterProvider::fetchEdgesFromEngines(VertexType const& vertex) {
  auto const* engines = _opts.getCache()->engines();
  auto& cache = _opts.getCache()->cache();
  // TODO map id => ServerID if possible
  // And go fast-path

  // TODO: adjust comment or implement
  // This function works for one specific vertex
  // or for a list of vertices.
  // TRI_ASSERT(vertexId.isString() || vertexId.isArray());

  transaction::BuilderLeaser leased(trx());
  leased->openObject(true);
  leased->add("backward", VPackValue(_opts.isBackward()));
  leased->add("keys", VPackValue(vertex.toString()));
  leased->close();

  auto* pool = trx()->vocbase().server().getFeature<NetworkFeature>().pool();

  network::RequestOptions reqOpts;
  reqOpts.database = trx()->vocbase().name();
  reqOpts.skipScheduler = true;  // hack to avoid scheduler queue

  std::vector<Future<network::Response>> futures;
  futures.reserve(engines->size());

  for (auto const& engine : *engines) {
    futures.emplace_back(
        network::sendRequestRetry(pool, "server:" + engine.first, fuerte::RestVerb::Put,
                                  ::edgeUrl + StringUtils::itoa(engine.second),
                                  leased->bufferRef(), reqOpts));
  }

  for (Future<network::Response>& f : futures) {
    network::Response const& r = f.get();

    if (r.fail()) {
      return network::fuerteToArangoErrorCode(r);
    }

    auto payload = r.response().stealPayload();
    VPackSlice resSlice(payload->data());
    if (!resSlice.isObject()) {
      // Response has invalid format
      return TRI_ERROR_HTTP_CORRUPTED_JSON;
    }
    Result res = network::resultFromBody(resSlice, TRI_ERROR_NO_ERROR);
    if (res.fail()) {
      return res;
    }
    // TODO CHECK: read += Helper::getNumericValue<size_t>(resSlice, "readIndex", 0);
    _opts.getCache()->insertedDocuments() +=
        Helper::getNumericValue<size_t>(resSlice, "readIndex", 0);

    bool allCached = true;
    VPackSlice edges = resSlice.get("edges");
    for (VPackSlice e : VPackArrayIterator(edges)) {
      VPackSlice id = e.get(StaticStrings::IdString);
      if (!id.isString()) {
        // invalid id type
        LOG_TOPIC("da49d", ERR, Logger::GRAPHS)
            << "got invalid edge id type: " << id.typeName();
        continue;
      }

      arangodb::velocypack::HashedStringRef edgeIdRef(id);
      auto resE = cache.try_emplace(edgeIdRef, e);  // TODO: add new cache entry method

      if (!_opts.getCache()->isEdgeCached(edgeIdRef)) {
        allCached = false;
        _opts.getCache()->cacheEdge(vertex, edgeIdRef, e);
      }
    }

    if (!allCached) {
      _opts.getCache()->datalake().add(std::move(payload));
    }
  }
}

void ClusterProvider::fetchEdgesAndRearm(Step* const& vertex) {
  Result res = fetchEdgesFromEngines(vertex->getVertex().getID());
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }
  // add http stats
  _stats.addHttpRequests(_opts.getCache()->engines()->size());  // TODO: Do this somewhere else at the right place.
  _cursor->rearm();  // TODO: Check if we can get rid of this completely
}

void ClusterProvider::fetchEdges(std::vector<Step*> const& vertices) {
  TRI_ASSERT(_cursor != nullptr);

  for (auto const& vertexStep : vertices) {
    // fetch edges and re-arm cursor
    // TODO: think about just passing that vertices vector to the function directly.
    fetchEdgesAndRearm(vertexStep);
  }
}

auto ClusterProvider::fetch(std::vector<Step*> const& looseEnds)
    -> futures::Future<std::vector<Step*>> {
  LOG_TOPIC("c9160", TRACE, Logger::GRAPHS) << "<ClusterProvider> Fetching...";
  std::vector<Step*> result{};

  if (looseEnds.size() > 0) {
    result.reserve(looseEnds.size());
    fetchVerticesFromEngines(looseEnds, result);

    // TODO: Check get rid of result object at all
    for (auto const& step : result) {
      fetchEdgesFromEngines(step->getVertex().getID());  // TODO: Check depth
    }
  }

  // TODO: This needs to be changed!
  return futures::makeFuture(std::move(result));
}

/*
 * Start: 1x looseEnd
 *  - 1. Request Vertex holen - Save: VertexRef -> VertexData
 *
 *  Logik:
 *  V1:
 *    Fetch -> Vertices + Edges => persist in datalake (cache)
 *      map: Step -> Vector<EdgeDocumentTokens> evtl. besser Vector<EdgeDocumentToken, VertexRef>
 *    Expand -> Read Steps out of cache (map entry) -> Remove used entry out of list/vector (Target -> No network communication)
 *
 *  V2: New Cache class
 *  - We do have a datalake
 *  - Map<VertexRef, Slice> (ref to vertex data)
 *  - Map<VertexRef, vector<EdgeId, VertexRef>> (vertex ref to all connected edges incl. target vertex)
 *  - Map<EdgeId, Slice> (ref to edge data)
 *
 *    -> Fetch as in V1 => populate maps
 *    -> Expand read data out of cache (connected edges)
 *    -> appendData(edge,vertex) => Read out of cache (what's needed)
 *    isCached(VertexRef)
 *
 *  Rearm:
 *   -
 *
 */

auto ClusterProvider::expand(Step const& step, size_t previous,
                             std::function<void(Step)> const& callback) -> void {
  TRI_ASSERT(!step.isLooseEnd());
  auto const& vertex = step.getVertex();

  //for (auto const& results: )
  /*
  _cursor->readAll(_stats, [&](EdgeDocumentToken&& eid, VPackSlice edge, size_t) -> void {
      VertexType id = _opts.getCache()->persistString(([&]() -> auto {
        if (edge.isString()) {
          return VertexType(edge);
        } else {
          VertexType other(transaction::helpers::extractFromFromDocument(edge));
          if (other == vertexStep->getVertex().getID()) {  // TODO: Check getId - discuss
            other = VertexType(transaction::helpers::extractToFromDocument(edge));
          }
          return other;
        }
      })());
      // previous pointing to origin v
      callback(Step{id, std::move(eid), previous});
    });
          */

  LOG_DEVEL << "expanding: " << vertex.getID().toString();
}

void ClusterProvider::addVertexToBuilder(Step::Vertex const& vertex,
                                         arangodb::velocypack::Builder& builder) {
  // NOTE: OLD SingleServer Variant
  //_cache.insertVertexIntoResult(_stats, vertex.getID(), builder);
  LOG_DEVEL << "trying to add vertex into result:";
  TRI_ASSERT(!vertex.getData().isNull());  // TODO CHECK better assert (e.g. _fetched <- maybe move to vertex from parent step element)
  builder.add(vertex.getData());
  /*
  bool test = _opts.getCache()->appendVertex(vertex.getID().stringRef(), builder);
  if (test) {
    LOG_DEVEL << "Could find entry in cache.";
  } else {
    LOG_DEVEL << "Could NOT find entry in cache.";
  }*/
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