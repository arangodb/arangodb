#include "ClusterNeighbourCursor.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionPlan.h"
#include "Basics/VelocyPackHelper.h"
#include "Graph/Steps/ClusterProviderStep.h"
#include "Graph/Types/VertexRef.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"
#include "Transaction/Methods.h"
#include "VocBase/vocbase.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Logger/LogMacros.h"
#include "Basics/ThreadLocalLeaser.h"
#include "RestHandler/RestVocbaseBaseHandler.h"
#include "Basics/StringUtils.h"

using namespace arangodb::graph;
using Helper = arangodb::basics::VelocyPackHelper;

namespace {
VertexType getEdgeDestination(arangodb::velocypack::Slice edge,
                              VertexType const& origin) {
  if (edge.isString()) {
    return VertexType{edge};
  }

  TRI_ASSERT(edge.isObject());
  auto from = edge.get(arangodb::StaticStrings::FromString);
  TRI_ASSERT(from.isString());
  if (from.stringView() == origin.stringView()) {
    auto to = edge.get(arangodb::StaticStrings::ToString);
    TRI_ASSERT(to.isString());
    return VertexType{to};
  }
  return VertexType{from};
}
ClusterProviderStep::FetchedType getFetchedType(bool vertexFetched) {
  if (vertexFetched) {
    return ClusterProviderStep::FetchedType::VERTEX_FETCHED;
  } else {
    return ClusterProviderStep::FetchedType::UNFETCHED;
  }
}
}  // namespace

template<typename Step>
ClusterNeighbourCursor<Step>::ClusterNeighbourCursor(
    Step step, size_t position, arangodb::transaction::Methods* trx,
    ClusterBaseProviderOptions& options, aql::TraversalStats& stats)
    : _step{step},
      _position{position},
      _trx{trx},
      _opts{options},
      _stats{stats} {
  auto const* engines = _opts.engines();
  auto leased = ThreadLocalBuilderLeaser::lease();
  leased->openObject(true);
  leased->add("backward", VPackValue(_opts.isBackward()));
  leased->add("depth", VPackValue(step.getDepth()));
  if (_opts.expressionContext() != nullptr) {
    leased->add(VPackValue("variables"));
    leased->openArray();
    _opts.expressionContext()->serializeAllVariables(trx->vpackOptions(),
                                                     *(leased.get()));
    leased->close();
  }
  leased->add("keys", VPackValue(step.getVertex().getID().toString()));
  leased->add("batchSize", VPackValue(aql::ExecutionBlock::DefaultBatchSize));
  leased->close();

  auto* pool =
      trx->vocbase().server().template getFeature<NetworkFeature>().pool();

  network::RequestOptions reqOpts;
  reqOpts.database = trx->vocbase().name();
  reqOpts.skipScheduler = true;  // hack to avoid scheduler queue

  _requests.reserve(engines->size());

  for (auto const& [server, engineId] : *engines) {
    _requests.emplace_back(EngineRequest{
        server, network::sendRequestRetry(
                    pool, "server:" + server, fuerte::RestVerb::Put,
                    RestVocbaseBaseHandler::TRAVERSER_PATH_EDGE +
                        basics::StringUtils::itoa(engineId),
                    leased->bufferRef(), reqOpts)});
    _stats.incrHttpRequests(1);
  }
}

template<typename Step>
auto ClusterNeighbourCursor<Step>::hasMore() -> bool {
  return not _requests.empty();
}

template<typename Step>
auto ClusterNeighbourCursor<Step>::next() -> std::vector<Step> {
  auto* pool =
      _trx->vocbase().server().template getFeature<NetworkFeature>().pool();

  network::RequestOptions reqOpts;
  reqOpts.database = _trx->vocbase().name();
  reqOpts.skipScheduler = true;  // hack to avoid scheduler queue

  auto const* engines = _opts.engines();

  auto newSteps = std::vector<Step>{};
  std::vector<EngineRequest> continuation_requests;
  while (not _requests.empty()) {
    // 1. wait on next response

    auto& [server_ref, f_ref] = _requests.back();
    auto server = server_ref;
    auto f = std::move(f_ref);
    _requests.pop_back();

    // NOTE: If you remove this waitAndGet() in favour of an asynchronous
    // operation, remember to remove the `skipScheduler = true` option of the
    // corresponding requests.
    network::Response const& r = f.waitAndGet();

    // 2. process response

    if (r.fail()) {
      THROW_ARANGO_EXCEPTION(network::fuerteToArangoErrorCode(r));
    }

    auto payload = r.response().stealPayload();
    VPackSlice resSlice(payload->data());
    if (!resSlice.isObject()) {
      // Response has invalid format
      THROW_ARANGO_EXCEPTION(TRI_ERROR_HTTP_CORRUPTED_JSON);
    }
    Result res = network::resultFromBody(resSlice, TRI_ERROR_NO_ERROR);
    if (res.fail()) {
      THROW_ARANGO_EXCEPTION(res);
    }
    _stats.incrScannedIndex(
        Helper::getNumericValue<size_t>(resSlice, "readIndex", 0));
    _stats.incrFiltered(
        Helper::getNumericValue<size_t>(resSlice, "filtered", 0));
    _stats.incrCursorsCreated(
        Helper::getNumericValue<size_t>(resSlice, "cursorsCreated", 0));
    _stats.incrCursorsRearmed(
        Helper::getNumericValue<size_t>(resSlice, "cursorsRearmed", 0));
    _stats.incrCacheHits(
        Helper::getNumericValue<size_t>(resSlice, "cacheHits", 0));
    _stats.incrCacheMisses(
        Helper::getNumericValue<size_t>(resSlice, "cacheMisses", 0));

    bool allCached = true;
    VPackSlice edges = resSlice.get("edges");
    for (VPackSlice e : VPackArrayIterator(edges)) {
      VPackSlice id = e.get(StaticStrings::IdString);
      if (!id.isString()) {
        // invalid id type
        LOG_TOPIC("eb7b2", ERR, Logger::GRAPHS)
            << "got invalid edge id type: " << id.typeName();
        continue;
      }
      LOG_TOPIC("fdb3b", TRACE, Logger::GRAPHS)
          << "<ClusterProvider> Neighbor of " << _step.getVertex().getID()
          << " -> " << id.toJson();

      auto [edge, needToCache] = _opts.getCache()->persistEdgeData(e);
      if (needToCache) {
        allCached = false;
      }

      arangodb::velocypack::HashedStringRef edgeIdRef(
          edge.get(StaticStrings::IdString));
      auto vid =
          VertexType{getEdgeDestination(edge, _step.getVertex().getID())};

      bool vertexCached = _opts.getCache()->isVertexCached(vid);
      // currently we don't use edge cache (see _vertexConnectedEdges in
      // ClusterProvider)
      typename Step::FetchedType fetchedType = getFetchedType(vertexCached);

      newSteps.emplace_back(Step{VertexRef{vid}, std::move(edgeIdRef),
                                 _position, fetchedType, _step.getDepth() + 1,
                                 _opts.weightEdge(_step.getWeight(), edge)});
    }

    if (!allCached) {
      _opts.getCache()->datalake().add(std::move(payload));
    }

    // 3. if iteration is not yet finished, send a continuation request

    auto maybeDone = resSlice.get("done");
    if (not maybeDone.isNone() && maybeDone.isBool()) {
      auto done = maybeDone.getBool();
      if (not done) {
        auto maybeCursorId = resSlice.get("cursorId");
        if (not maybeCursorId.isNone() && maybeCursorId.isInteger()) {
          auto maybeBatchId = resSlice.get("batchId");
          if (not maybeBatchId.isNone() && maybeBatchId.isInteger()) {
            auto leasedContinue = ThreadLocalBuilderLeaser::lease();
            leasedContinue->openObject(true);
            leasedContinue->add("cursorId", maybeCursorId);
            leasedContinue->add(
                "batchId",
                VPackValue(maybeBatchId.getNumericValue<size_t>() + 1));
            leasedContinue->close();

            auto engineIdLookup = engines->find(server);
            if (engineIdLookup == engines->end()) {
              THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
            }
            auto& [_server, engineId] = *engineIdLookup;

            continuation_requests.emplace_back(EngineRequest{
                server, network::sendRequestRetry(
                            pool, "server:" + server, fuerte::RestVerb::Put,
                            RestVocbaseBaseHandler::TRAVERSER_PATH_EDGE +
                                basics::StringUtils::itoa(engineId),
                            leasedContinue->bufferRef(), reqOpts)});
            _stats.incrHttpRequests(1);
          }
        }
      }
    }
  }

  if (not continuation_requests.empty()) {
    _requests = std::move(continuation_requests);
  }

  // TODO check if we need to add connectedEdges to cache
  // (_vertexConnectedEdges) as well (is done in non-batched version, see
  // fetchEdgesFromEngines fn)

  return newSteps;
}

template struct arangodb::graph::ClusterNeighbourCursor<ClusterProviderStep>;
