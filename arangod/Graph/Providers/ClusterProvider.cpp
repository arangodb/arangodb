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

#include "Basics/ScopeGuard.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"

#include <vector>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::futures;
using namespace arangodb::graph;

using Helper = arangodb::basics::VelocyPackHelper;

namespace {
constexpr size_t costPerVertexOrEdgeType = sizeof(arangodb::velocypack::HashedStringRef);

std::string const edgeUrl = "/_internal/traverser/edge/";
std::string const vertexUrl = "/_internal/traverser/vertex/";

VertexType getEdgeDestination(arangodb::velocypack::Slice edge, VertexType const& origin) {
  if (edge.isString()) {
    return VertexType{edge};
  }

  TRI_ASSERT(edge.isObject());
  auto from = edge.get(arangodb::StaticStrings::FromString);
  TRI_ASSERT(from.isString());
  if (from.stringRef() == origin.stringRef()) {
    auto to = edge.get(arangodb::StaticStrings::ToString);
    TRI_ASSERT(to.isString());
    return VertexType{to};
  }
  return VertexType{from};
}
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

ClusterProvider::Step::Step(VertexType v, EdgeType edge, size_t prev)
    : BaseStep(prev), _vertex(v), _edge(std::move(edge)), _fetched(false) {}

ClusterProvider::Step::Step(VertexType v, EdgeType edge, size_t prev, bool fetched)
    : BaseStep(prev), _vertex(v), _edge(std::move(edge)), _fetched(fetched) {}

ClusterProvider::Step::~Step() = default;

VertexType const& ClusterProvider::Step::Vertex::getID() const {
  return _vertex;
}

EdgeType const& ClusterProvider::Step::Edge::getID() const { return _edge; }
bool ClusterProvider::Step::Edge::isValid() const {
  return !_edge.empty();
};

ClusterProvider::ClusterProvider(arangodb::aql::QueryContext& queryContext,
                                 ClusterBaseProviderOptions opts,
                                 arangodb::ResourceMonitor& resourceMonitor)
    : _trx(std::make_unique<arangodb::transaction::Methods>(queryContext.newTrxContext())),
      _query(&queryContext),
      _resourceMonitor(&resourceMonitor),
      _opts(std::move(opts)),
      _stats{} {}

ClusterProvider::~ClusterProvider() {
  clear();
}

void ClusterProvider::clear() {
  for (auto const& entry : _vertexConnectedEdges) {
    _resourceMonitor->decreaseMemoryUsage(costPerVertexOrEdgeType + (entry.second.size() * (costPerVertexOrEdgeType * 2)));
  }
}

auto ClusterProvider::startVertex(VertexType vertex) -> Step {
  LOG_TOPIC("da308", TRACE, Logger::GRAPHS) << "<ClusterProvider> Start Vertex:" << vertex;
  // Create the default initial step.
  return Step(_opts.getCache()->persistString(vertex));
}

void ClusterProvider::fetchVerticesFromEngines(std::vector<Step*> const& looseEnds,
                                               std::vector<Step*>& result) {
  auto const* engines = _opts.engines();
  // slow path, sharding not deducable from _id
  transaction::BuilderLeaser leased(trx());
  leased->openObject();
  leased->add("keys", VPackValue(VPackValueType::Array));
  for (auto const& looseEnd : looseEnds) {
    TRI_ASSERT(looseEnd->isLooseEnd());
    auto const& vertexId = looseEnd->getVertex().getID();
    if (!_opts.getCache()->isVertexCached(vertexId)) {
      leased->add(VPackValuePair(vertexId.data(), vertexId.length(), VPackValueType::String));
    }
  }
  leased->close();  // 'keys' Array
  leased->close();  // base object

  auto* pool = trx()->vocbase().server().getFeature<NetworkFeature>().pool();

  network::RequestOptions reqOpts;
  reqOpts.database = trx()->vocbase().name();
  reqOpts.skipScheduler = true;  // hack to avoid scheduler queue

  std::vector<Future<network::Response>> futures;
  futures.reserve(engines->size());

  TRI_DEFER({
    for (Future<network::Response>& f : futures) {
      try {
        // TODO: As soon as we switch to the new future library, we need to replace the wait with proper *finally* method.
        f.wait();
      } catch (...) {
      }
    }
  });

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
    bool needToRetainPayload = false;

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
      VertexType vertexKey(pair.key);

      if (!_opts.getCache()->isVertexCached(vertexKey)) {
        // Will be protected by the datalake.
        // We flag to retain the payload.
        _opts.getCache()->cacheVertex(std::move(vertexKey), pair.value);
        needToRetainPayload = true;
      }
    }

    if (needToRetainPayload) {
      // We have stored at least one entry from this payload.
      // Retain it.
      _opts.getCache()->datalake().add(std::move(payload));
    }

    /* TODO: Needs to be taken care of as soon as we enable shortest paths for ClusterProvider
    bool forShortestPath = true;
    if (!forShortestPath) {
      // Fill everything we did not find with NULL
      for (auto const& v : vertexIds) {
        result.try_emplace(v, VPackSlice::nullSlice());
      }
    vertexIds.clear();
    }
    */
  }

  // Note: This disables the TRI_DEFER
  futures.clear();

  // put back all looseEnds we we're able to cache
  for (auto& lE : looseEnds) {
    if (!_opts.getCache()->isVertexCached(lE->getVertexIdentifier())) {
      // if we end up here, we we're not able to cache the requested vertex (e.g. it does not exist)
      _query->warnings().registerWarning(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, lE->getVertexIdentifier().toString());
      _opts.getCache()->cacheVertex(std::move(lE->getVertexIdentifier()), VPackSlice::nullSlice());
    }
    result.emplace_back(std::move(lE));
  }
}

void ClusterProvider::destroyEngines() {
  if (!ServerState::instance()->isCoordinator()) {
    return;
  }

  auto* pool = trx()->vocbase().server().getFeature<NetworkFeature>().pool();
  // We have to clean up the engines in Coordinator Case.
  if (pool == nullptr) {
    return;
  }
  // nullptr only happens on controlled server shutdown

  network::RequestOptions options;
  options.database = trx()->vocbase().name();
  options.timeout = network::Timeout(30.0);
  options.skipScheduler = true;  // hack to speed up future.get()

  auto const* engines = _opts.engines();
  for (auto const& engine : *engines) {
    _stats.addHttpRequests(1);
    auto res = network::sendRequest(pool, "server:" + engine.first, fuerte::RestVerb::Delete,
                                    "/_internal/traverser/" +
                                    arangodb::basics::StringUtils::itoa(engine.second),
                                    VPackBuffer<uint8_t>(), options)
        .get();

    if (res.error != fuerte::Error::NoError) {
      // Note If there was an error on server side we do not have
      // CL_COMM_SENT
      LOG_TOPIC("d31a5", ERR, arangodb::Logger::GRAPHS)
      << "Could not destroy all traversal engines: "
      << TRI_errno_string(network::fuerteToArangoErrorCode(res));
    }
  }
}

Result ClusterProvider::fetchEdgesFromEngines(VertexType const& vertex) {
  auto const* engines = _opts.engines();
  // TODO Assert that the vertex is not in _vertexConnections after no-loose-end handling todo is done.
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

  TRI_DEFER({
    for (Future<network::Response>& f : futures) {
      try {
        // TODO: As soon as we switch to the new future library, we need to replace the wait with proper *finally* method.
        f.wait();
      } catch (...) {
      }
    }
  });

  for (auto const& engine : *engines) {
    futures.emplace_back(
        network::sendRequestRetry(pool, "server:" + engine.first, fuerte::RestVerb::Put,
                                  ::edgeUrl + StringUtils::itoa(engine.second),
                                  leased->bufferRef(), reqOpts));
  }

  std::vector<std::pair<EdgeType, VertexType>> connectedEdges;
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
    _stats.addFiltered(Helper::getNumericValue<size_t>(resSlice, "filtered", 0));
    _stats.addScannedIndex(Helper::getNumericValue<size_t>(resSlice, "readIndex", 0));

    bool allCached = true;
    VPackSlice edges = resSlice.get("edges");
    for (VPackSlice e : VPackArrayIterator(edges)) {
      VPackSlice id = e.get(StaticStrings::IdString);
      if (!id.isString()) {
        // invalid id type
        LOG_TOPIC("eb7cd", ERR, Logger::GRAPHS)
            << "got invalid edge id type: " << id.typeName();
        continue;
      }

      auto [edge, needToCache] = _opts.getCache()->persistEdgeData(e);
      if (needToCache) {
        allCached = false;
      }

      arangodb::velocypack::HashedStringRef edgeIdRef(edge.get(StaticStrings::IdString));

      auto edgeToEmplace = std::make_pair(edgeIdRef, VertexType{getEdgeDestination(edge, vertex)});

      connectedEdges.emplace_back(edgeToEmplace);
    }

    if (!allCached) {
      _opts.getCache()->datalake().add(std::move(payload));
    }
  }
  // Note: This disables the TRI_DEFER
  futures.clear();

  _resourceMonitor->increaseMemoryUsage(costPerVertexOrEdgeType + (connectedEdges.size() * (costPerVertexOrEdgeType * 2)));
  _vertexConnectedEdges.emplace(vertex, std::move(connectedEdges));

  return TRI_ERROR_NO_ERROR;
}

auto ClusterProvider::fetch(std::vector<Step*> const& looseEnds)
    -> futures::Future<std::vector<Step*>> {
  LOG_TOPIC("03c1b", TRACE, Logger::GRAPHS) << "<ClusterProvider> Fetching...";
  std::vector<Step*> result{};

  if (looseEnds.size() > 0) {
    result.reserve(looseEnds.size());
    fetchVerticesFromEngines(looseEnds, result);
    _stats.addHttpRequests(_opts.engines()->size() * looseEnds.size());

    for (auto const& step : result) {
      if (_vertexConnectedEdges.find(step->getVertex().getID()) ==
          _vertexConnectedEdges.end()) {
        auto res = fetchEdgesFromEngines(step->getVertex().getID());
        // TODO: check stats (also take a look of vertex stats)
        // add http stats
        _stats.addHttpRequests(_opts.engines()->size());

        if (res.fail()) {
          THROW_ARANGO_EXCEPTION(res);
        }
      }
      // else: We already fetched this vertex.

      // mark a looseEnd as fetched as vertex fetch + edges fetch was a success
      step->setFetched();
    }
  }

  // Note: Discuss if we want to keep it that way in the future.
  return futures::makeFuture(std::move(result));
}

auto ClusterProvider::expand(Step const& step, size_t previous,
                             std::function<void(Step)> const& callback) -> void {
  TRI_ASSERT(!step.isLooseEnd());
  auto const& vertex = step.getVertex();

  TRI_ASSERT(_opts.getCache()->isVertexCached(vertex.getID()));
  TRI_ASSERT(_vertexConnectedEdges.find(vertex.getID()) != _vertexConnectedEdges.end());
  for (auto const& relation : _vertexConnectedEdges.at(vertex.getID())) {
    bool fetched = _vertexConnectedEdges.find(relation.second) != _vertexConnectedEdges.end();
    callback(Step{relation.second, relation.first, previous, fetched});
  }
}


void ClusterProvider::addVertexToBuilder(Step::Vertex const& vertex,
                                         arangodb::velocypack::Builder& builder) {
  TRI_ASSERT(_opts.getCache()->isVertexCached(vertex.getID()));
  builder.add(_opts.getCache()->getCachedVertex(vertex.getID()));
};

auto ClusterProvider::addEdgeToBuilder(Step::Edge const& edge,
                                       arangodb::velocypack::Builder& builder) -> void {
  builder.add(_opts.getCache()->getCachedEdge(edge.getID()));
}

arangodb::transaction::Methods* ClusterProvider::trx() { return _trx.get(); }

arangodb::aql::TraversalStats ClusterProvider::stealStats() {
  auto t = _stats;
  // Placement new of stats, do not reallocate space.
  _stats.~TraversalStats();
  new (&_stats) aql::TraversalStats{};
  return t;
}
