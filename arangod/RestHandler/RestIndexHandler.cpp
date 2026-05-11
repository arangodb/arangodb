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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "RestIndexHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Async/async.h"
#include "Basics/StaticStrings.h"
#include "Cluster/AgencyCache.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/CollectionInfoCurrent.h"
#include "Cluster/Utils/VectorIndexShardStates.h"
#include "Cluster/ServerState.h"
#include "RocksDBEngine/RocksDBBuilderIndex.h"
#include "RocksDBEngine/RocksDBVectorIndex.h"
#include "Logger/LogMacros.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "RestServer/VocbaseContext.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Methods.h"
#include "Transaction/OperationOrigin.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/Events.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VectorIndex/VectorIndexFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Indexes.h"

#include <absl/strings/str_cat.h>

#include <algorithm>
#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::futures;
using namespace arangodb::rest;

namespace {

// Returns the least-progressed training state across all shards.
// The progression order is: unusable < training < ingesting < ready.
// If states is empty, returns "unusable".
template<typename StatesMap>
std::string_view aggregateTrainingState(StatesMap const& states) {
  if (states.empty()) {
    return StaticStrings::IndexTrainingStateUnusable;
  }
  auto stateOrder = [](std::string_view s) -> int {
    if (s == StaticStrings::IndexTrainingStateReady) return 3;
    if (s == StaticStrings::IndexTrainingStateIngesting) return 2;
    if (s == StaticStrings::IndexTrainingStateTraining) return 1;
    return 0;  // unusable or unknown
  };
  int minOrder = 3;
  for (auto const& [_, shardState] : states) {
    minOrder = std::min(minOrder, stateOrder(shardState.trainingState));
  }
  switch (minOrder) {
    case 3:
      return StaticStrings::IndexTrainingStateReady;
    case 2:
      return StaticStrings::IndexTrainingStateIngesting;
    case 1:
      return StaticStrings::IndexTrainingStateTraining;
    default:
      return StaticStrings::IndexTrainingStateUnusable;
  }
}

containers::FlatHashMap<ShardID, VectorIndexShardState>
getShardStatesIfAvailable(
    std::shared_ptr<CollectionInfoCurrent> const& collCurrent,
    std::shared_ptr<ShardMap const> const& shardIds, std::string_view bareId) {
  if (!collCurrent || !shardIds) {
    return {};
  }
  return getVectorIndexShardStates(*collCurrent, *shardIds, bareId);
}

// Enrich vector indexes with a top-level trainingState and per-shard details.
template<typename ShardStateProvider>
VPackBuilder enrichVectorIndexes(VPackSlice indexes,
                                 ShardStateProvider const& getShardStates,
                                 bool withShardDetails) {
  VPackBuilder result;
  {
    VPackArrayBuilder guard(&result);
    for (auto indexSlice : VPackArrayIterator(indexes)) {
      auto typeSlice = indexSlice.get("type");
      if (!typeSlice.isString() || typeSlice.stringView() != "vector") {
        result.add(indexSlice);
        continue;
      }

      std::string_view iid = indexSlice.get("id").stringView();
      std::string_view bareId = iid;
      if (auto pos = bareId.find('/'); pos != std::string::npos) {
        bareId = bareId.substr(pos + 1);
      }

      auto states = getShardStates(bareId);
      if (states.empty()) {
        result.add(indexSlice);
        continue;
      }

      std::string_view aggregateState = aggregateTrainingState(states);

      VPackObjectBuilder o(&result);
      for (auto it : VPackObjectIterator(indexSlice)) {
        auto key = it.key.stringView();
        if (key == StaticStrings::IndexResolvedNLists ||
            key == StaticStrings::IndexTrainingState ||
            key == StaticStrings::ErrorMessage) {
          continue;
        }
        result.add(key, it.value);
      }
      result.add(StaticStrings::IndexTrainingState, VPackValue(aggregateState));

      if (aggregateState == StaticStrings::IndexTrainingStateUnusable) {
        std::string_view shardError;
        for (auto const& [_, shardState] : states) {
          if (!shardState.error.empty()) {
            shardError = shardState.error;
            break;
          }
        }
        result.add(StaticStrings::ErrorMessage,
                   VPackValue(shardError.empty()
                                  ? "not enough training data for vector index"
                                  : shardError));
      }

      if (withShardDetails) {
        result.add(VPackValue("shards"));
        VPackObjectBuilder shardsObj(&result);
        for (auto const& [shardId, shardState] : states) {
          result.add(VPackValue(shardId));
          VPackObjectBuilder shardEntry(&result);
          result.add(StaticStrings::IndexTrainingState,
                     VPackValue(shardState.trainingState));
          result.add("error", VPackValue(shardState.error));
          TRI_ASSERT(shardState.trainingState !=
                         StaticStrings::IndexTrainingStateReady ||
                     shardState.resolvedNLists > 0);
          result.add(StaticStrings::IndexResolvedNLists,
                     VPackValue(shardState.resolvedNLists));
        }
      }
    }
  }
  return result;
}

}  // namespace

RestIndexHandler::RestIndexHandler(
    application_features::ApplicationServer& server, GeneralRequest* request,
    GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response),
      _clusterFeature(server.getFeature<ClusterFeature>()) {}

futures::Future<futures::Unit> RestIndexHandler::executeAsync() {
  // extract the request type
  rest::RequestType const type = _request->requestType();
  if (type == rest::RequestType::GET) {
    if (_request->suffixes().size() == 1 &&
        _request->suffixes()[0] == "selectivity") {
      co_await getSelectivityEstimates();
      co_return;
    }
    co_await getIndexes();
    co_return;
  }
  if (type == rest::RequestType::POST) {
    if (_request->suffixes().size() == 1 &&
        _request->suffixes()[0] == "sync-caches") {
      // this is an unofficial API to sync the in-memory
      // index caches with the data queued in the index
      // refill background thread. it is not supposed to
      // be used publicly.
      co_return syncCaches();
    }
    co_await createIndex();
    co_return;
  }
  if (type == rest::RequestType::DELETE_REQ) {
    co_await dropIndex();
    co_return;
  }
  generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
}

std::shared_ptr<LogicalCollection> RestIndexHandler::collection(
    std::string const& cName) {
  if (!cName.empty()) {
    if (ServerState::instance()->isCoordinator()) {
      return _clusterFeature.clusterInfo().getCollectionNT(_vocbase.name(),
                                                           cName);
    }
    return _vocbase.lookupCollection(cName);
  }
  return nullptr;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief get index infos
// //////////////////////////////////////////////////////////////////////////////

async<void> RestIndexHandler::getIndexes() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();
  if (suffixes.empty()) {
    // .............................................................................
    // /_api/index?collection=<collection-name>
    // .............................................................................

    bool found = false;
    std::string cName = _request->value("collection", found);
    auto coll = collection(cName);
    if (coll == nullptr) {
      generateError(rest::ResponseCode::NOT_FOUND,
                    TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
      co_return;
    }

    auto flags = Index::makeFlags(Index::Serialize::Estimates);
    if (_request->parsedValue("withStats", false)) {
      flags = Index::makeFlags(Index::Serialize::Estimates,
                               Index::Serialize::Figures);
    }
    bool withHidden = _request->parsedValue("withHidden", false);

    // result container
    VPackBuilder tmp;
    tmp.openObject();
    tmp.add(StaticStrings::Error, VPackValue(false));
    tmp.add(StaticStrings::Code,
            VPackValue(static_cast<int>(ResponseCode::OK)));

    if (!ServerState::instance()->isCoordinator() || !withHidden) {
      // simple case: no in-progress indexes to return
      VPackBuilder indexes;
      Result res =
          co_await methods::Indexes::getAll(*coll, flags, withHidden, indexes);
      if (!res.ok()) {
        generateError(rest::ResponseCode::BAD, res.errorNumber(),
                      res.errorMessage());
        co_return;
      }

      TRI_ASSERT(indexes.slice().isArray());

      VPackBuilder enriched;
      if (ServerState::instance()->isCoordinator()) {
        auto& ci = _vocbase.server().getFeature<ClusterFeature>().clusterInfo();
        std::shared_ptr<CollectionInfoCurrent> collCurrent;
        try {
          collCurrent = ci.getCollectionCurrent(
              _vocbase.name(), std::to_string(coll->planId().id()));
        } catch (...) {
        }
        auto const shardIds = coll->shardIds();

        enriched = enrichVectorIndexes(
            indexes.slice(),
            [&](std::string_view bareId) {
              return getShardStatesIfAvailable(collCurrent, shardIds, bareId);
            },
            withHidden);
      } else {
        // This lambda fakes shard states for a non-cluster setup, where a
        // single shard is just a collection
        enriched = enrichVectorIndexes(
            indexes.slice(),
            [&](std::string_view bareId) {
              std::unordered_map<std::string, VectorIndexShardState> states;
              auto idx = coll->lookupIndex(
                  IndexId{basics::StringUtils::uint64(bareId)});
              if (idx != nullptr &&
                  idx->type() == Index::TRI_IDX_TYPE_VECTOR_INDEX) {
                // During ingestion the real index is swapped for a
                // RocksDBBuilderIndex; unwrap to reach vector-specific state.
                Index const* raw = idx.get();
                if (auto const* builder =
                        dynamic_cast<RocksDBBuilderIndex const*>(raw)) {
                  raw = &builder->wrapped();
                }
                auto const* vecIdx =
                    dynamic_cast<RocksDBVectorIndex const*>(raw);
                TRI_ASSERT(vecIdx != nullptr);
                if (vecIdx == nullptr) [[unlikely]] {
                  return states;
                }
                auto const ts = vecIdx->trainingState();
                VectorIndexShardState state;
                state.trainingState = std::string(trainingStateToString(ts));
                if (ts == VectorIndexTrainingState::kUnusable) {
                  auto err = vecIdx->trainingError();
                  state.error =
                      err.empty() ? "not enough training data for vector index"
                                  : std::move(err);
                }
                state.resolvedNLists = vecIdx->resolvedNLists().value_or(0);
                states.emplace(std::string(coll->name()), std::move(state));
              }
              return states;
            },
            withHidden);
      }

      tmp.add("indexes", enriched.slice());
      tmp.add("identifiers", VPackValue(VPackValueType::Object));
      for (auto index : VPackArrayIterator(enriched.slice())) {
        VPackSlice id = index.get("id");
        tmp.add(id.stringView(), index);
      }
    } else {
      // more complicated case: we need to also return all indexes that
      // are still in the making
      TRI_ASSERT(ServerState::instance()->isCoordinator());

      // first fetch list of planned indexes. this includes all indexes,
      // even the in-progress indexes
      std::string ap = absl::StrCat("Plan/Collections/", _vocbase.name(), "/",
                                    coll->planId().id(), "/indexes");
      auto& ac = _clusterFeature.agencyCache();
      // we need to wait for the latest commit index here, because otherwise
      // we may not see all indexes that were declared ready by the
      // supervision.
      co_await ac.waitForLatestCommitIndex();

      auto [plannedIndexes, idx] = ac.get(ap);

      // Let's wait until the ClusterInfo has processed at least this
      // Raft index. This means that if an index is no longer `isBuilding`
      // in the agency Plan, then ClusterInfo should know it.
      co_await _clusterFeature.clusterInfo().waitForPlan(idx);

      // now fetch list of ready indexes
      VPackBuilder indexes;
      Result res =
          co_await methods::Indexes::getAll(*coll, flags, withHidden, indexes);
      if (!res.ok()) {
        generateError(rest::ResponseCode::BAD, res.errorNumber(),
                      res.errorMessage());
        co_return;
      }

      TRI_ASSERT(indexes.slice().isArray());

      // ATTENTION: In the agency, the ID of the index is stored as a string
      // without a prefix for the collection name. However, in the velocypack
      // which is reported from `getAll` above, the ID is a string with the
      // collection name and a slash as a prefix, like it is reported in the
      // external API. Since we now must compare IDs between the two sources,
      // we must be careful!

      // Our task is now the following: We first take the indexes reported by
      // `getAll`. However, this misses indexes which are still being built.
      // Therefore, we then add those indexes from the agency plan, which have
      // the `isBuilding` attribute still set to `true` (unless they are already
      // actually present locally, which can happen, if our agency snapshot is
      // a bit older, note that above we **first** get the indexes from the
      // agency cache, then we wait until `ClusterInfo` has processed the
      // raft index, and then we get the indexes
      // from the local `LogicalCollection`!).

      // all indexes we already reported:
      containers::FlatHashSet<std::string> covered;

      // Fetch per-shard index data from Current for vector index
      // state enrichment.
      auto& ci = _vocbase.server().getFeature<ClusterFeature>().clusterInfo();
      std::shared_ptr<CollectionInfoCurrent> collCurrent;
      try {
        collCurrent = ci.getCollectionCurrent(
            _vocbase.name(), std::to_string(coll->planId().id()));
      } catch (...) {
        // Best effort — if Current is not available, we skip shard
        // state enrichment.
      }

      auto const shardIds = coll->shardIds();
      auto enriched = enrichVectorIndexes(
          indexes.slice(),
          [&](std::string_view bareId) {
            return getShardStatesIfAvailable(collCurrent, shardIds, bareId);
          },
          withHidden);

      tmp.add(VPackValue("indexes"));

      {
        VPackArrayBuilder guard(&tmp);
        // first return all ready indexes from the `LogicalCollection` object.
        for (auto pi : VPackArrayIterator(enriched.slice())) {
          std::string_view iid = pi.get("id").stringView();

          // Strip collection prefix to get bare index id.
          std::string_view bareId = iid;
          if (auto pos = bareId.find('/'); pos != std::string::npos) {
            bareId = bareId.substr(pos + 1);
          }

          tmp.add(pi);
          covered.emplace(bareId);
        }
        // now return all indexes which are currently being built:
        for (auto pi : VPackArrayIterator(plannedIndexes->slice())) {
          std::string_view iid = pi.get("id").stringView();
          // avoid reporting an index twice
          if (covered.contains(iid) ||
              !pi.get(StaticStrings::IndexIsBuilding).isTrue()) {
            continue;
          }

          VPackObjectBuilder o(&tmp);
          for (auto source :
               VPackObjectIterator(pi, /* useSequentialIterator */ true)) {
            if (source.key.stringView() == StaticStrings::IndexId) {
              tmp.add(StaticStrings::IndexId,
                      VPackValue(
                          absl::StrCat(cName, "/", source.value.stringView())));
            } else {
              tmp.add(source.key.stringView(), source.value);
            }
          }

          // In this case we have to ask the shards about how far they are:
          double progress = 0;
          auto const shards = coll->shardIds();
          auto const body = VPackBuffer<uint8_t>();
          auto* pool =
              coll->vocbase().server().getFeature<NetworkFeature>().pool();
          std::vector<Future<network::Response>> futures;
          futures.reserve(shards->size());
          std::string const prefix = "/_api/index/";
          network::RequestOptions reqOpts;
          reqOpts.param("withHidden", withHidden ? "true" : "false");
          reqOpts.database = _vocbase.name();
          // best effort. only displaying progress
          reqOpts.timeout = network::Timeout(10.0);
          for (auto const& shard : *shards) {
            std::string url =
                absl::StrCat(prefix, "s", shard.first.id(), "/", iid);
            futures.emplace_back(network::sendRequestRetry(
                pool, "shard:" + shard.first, fuerte::RestVerb::Get, url, body,
                reqOpts));
          }
          for (Future<network::Response>& f : futures) {
            network::Response const& r = f.waitAndGet();

            // Only best effort accounting. If something breaks here, we
            // just ignore the output. Account for what we can and move
            // on.
            if (r.fail()) {
              LOG_TOPIC("afde4", INFO, Logger::CLUSTER)
                  << "Communication error while fetching index data "
                     "for collection "
                  << coll->name() << " from " << r.destination;
              continue;
            }
            VPackSlice resSlice = r.slice();
            if (!resSlice.isObject() ||
                !resSlice.get(StaticStrings::Error).isBoolean()) {
              LOG_TOPIC("aabe4", INFO, Logger::CLUSTER)
                  << "Result of collecting index data for collection "
                  << coll->name() << " from " << r.destination << " is invalid";
              continue;
            }
            if (resSlice.get(StaticStrings::Error).getBoolean()) {
              // this can happen when the DB-Servers have not yet
              // started the creation of the index on a shard, for
              // example if the number of maintenance threads is low.
              auto errorNum = TRI_ERROR_NO_ERROR;
              if (VPackSlice errorNumSlice =
                      resSlice.get(StaticStrings::ErrorNum);
                  errorNumSlice.isNumber()) {
                errorNum = ::ErrorCode{errorNumSlice.getNumber<int>()};
              }
              // do not log an expected error such as "index not found",
              if (errorNum != TRI_ERROR_ARANGO_INDEX_NOT_FOUND) {
                LOG_TOPIC("a4bea", INFO, Logger::CLUSTER)
                    << "Failed to collect index data for collection "
                    << coll->name() << " from " << r.destination << ": "
                    << resSlice.toJson();
              }
              continue;
            }
            if (resSlice.get("progress").isNumber()) {
              progress += resSlice.get("progress").getNumber<double>();
            } else {
              // Obviously, the index is already ready there.
              progress += 100.0;
              LOG_TOPIC("aeab4", DEBUG, Logger::CLUSTER)
                  << "No progress entry on index " << iid << " from "
                  << r.destination << ": " << resSlice.toJson()
                  << " index already finished.";
            }
          }
          if (progress != 0 && shards->size() != 0) {
            // Don't show progress 0, this is in particular relevant
            // when isBackground is false, in which case no progress
            // is reported by design.
            tmp.add("progress", VPackValue(progress / shards->size()));
          }

          // For in-progress vector indexes, also add per-shard states.
          if (auto typeSlice = pi.get("type");
              typeSlice.isString() && typeSlice.stringView() == "vector" &&
              collCurrent && shardIds) {
            auto states =
                getVectorIndexShardStates(*collCurrent, *shardIds, iid);
            std::string_view aggregateState = aggregateTrainingState(states);
            tmp.add(StaticStrings::IndexTrainingState,
                    VPackValue(aggregateState));

            if (aggregateState == StaticStrings::IndexTrainingStateUnusable) {
              std::string_view shardError;
              for (auto const& [_, shardState] : states) {
                if (!shardState.error.empty()) {
                  shardError = shardState.error;
                  break;
                }
              }
              tmp.add(
                  StaticStrings::ErrorMessage,
                  VPackValue(shardError.empty()
                                 ? "not enough training data for vector index"
                                 : shardError));
            }

            tmp.add(VPackValue("shards"));
            VPackObjectBuilder shardsObj(&tmp);
            for (auto const& [shardId, shardState] : states) {
              tmp.add(VPackValue(static_cast<std::string>(shardId)));
              VPackObjectBuilder shardEntry(&tmp);
              tmp.add(StaticStrings::IndexTrainingState,
                      VPackValue(shardState.trainingState));
              tmp.add("error", VPackValue(shardState.error));
              tmp.add(StaticStrings::IndexResolvedNLists,
                      VPackValue(shardState.resolvedNLists));
            }
          }
        }
      }

      // also report all indexes in the "identifiers" attribute.
      // TODO: this is redundant and unncessarily complicates the API return
      // value. this attribute should be deprecated and removed
      tmp.add("identifiers", VPackValue(VPackValueType::Object));
      for (auto pi : VPackArrayIterator(indexes.slice())) {
        tmp.add(pi.get(StaticStrings::IndexId).stringView(), pi);
      }
      for (auto pi : VPackArrayIterator(plannedIndexes->slice())) {
        std::string_view iid = pi.get("id").stringView();
        // avoid reporting an index twice
        if (covered.contains(iid) ||
            !pi.get(StaticStrings::IndexIsBuilding).isTrue()) {
          continue;
        }
        std::string id_str = absl::StrCat(cName, "/", iid);
        tmp.add(VPackValue(id_str));
        VPackObjectBuilder o(&tmp);
        for (auto source :
             VPackObjectIterator(pi, /* useSequentialIterator */ true)) {
          if (source.key.stringView() == StaticStrings::IndexId) {
            tmp.add(StaticStrings::IndexId, VPackValue(id_str));
          } else {
            tmp.add(source.key.stringView(), source.value);
          }
        }
      }
    }

    tmp.close();
    tmp.close();
    generateResult(rest::ResponseCode::OK, tmp.slice());
  } else if (suffixes.size() == 2) {
    // .............................................................................
    // /_api/index/<collection-name>/<index-identifier>
    // .............................................................................

    std::string const& cName = suffixes[0];
    auto coll = collection(cName);
    if (coll == nullptr) {
      generateError(rest::ResponseCode::NOT_FOUND,
                    TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
      co_return;
    }

    std::string const& iid = suffixes[1];
    VPackBuilder tmp;
    tmp.add(VPackValue(cName + TRI_INDEX_HANDLE_SEPARATOR_CHR + iid));

    VPackBuilder output;
    Result res =
        co_await methods::Indexes::getIndex(*coll, tmp.slice(), output);
    if (res.ok()) {
      VPackBuilder b;
      b.openObject();
      b.add(StaticStrings::Error, VPackValue(false));
      b.add(StaticStrings::Code,
            VPackValue(static_cast<int>(ResponseCode::OK)));
      b.close();
      output = VPackCollection::merge(output.slice(), b.slice(), false);
      generateResult(rest::ResponseCode::OK, output.slice());
    } else {
      generateError(res);
    }
  } else {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
  }
}

futures::Future<futures::Unit> RestIndexHandler::getSelectivityEstimates() {
  // .............................................................................
  // /_api/index/selectivity?collection=<collection-name>
  // .............................................................................

  bool found = false;
  std::string const& cName = _request->value("collection", found);
  if (cName.empty()) {
    generateError(rest::ResponseCode::NOT_FOUND,
                  TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    co_return;
  }

  auto origin =
      transaction::OperationOriginREST{"fetching selectivity estimates"};

  // transaction protects access onto selectivity estimates
  std::unique_ptr<transaction::Methods> trx;

  try {
    trx = co_await createTransaction(cName, AccessMode::Type::READ,
                                     OperationOptions(), origin);
  } catch (basics::Exception const& ex) {
    if (ex.code() == TRI_ERROR_TRANSACTION_NOT_FOUND) {
      // this will happen if the tid of a managed transaction is passed in,
      // but the transaction hasn't yet started on the DB server. in
      // this case, we create an ad-hoc transaction on the underlying
      // collection
      trx = std::make_unique<SingleCollectionTransaction>(
          transaction::StandaloneContext::create(_vocbase, origin), cName,
          AccessMode::Type::READ);
    } else {
      throw;
    }
  }

  TRI_ASSERT(trx != nullptr);

  Result res = co_await trx->beginAsync();
  if (res.fail()) {
    generateError(res);
    co_return;
  }

  LogicalCollection* coll = trx->documentCollection(cName);
  auto idxs = coll->getPhysical()->getReadyIndexes();

  VPackBuffer<uint8_t> buffer;
  VPackBuilder builder(buffer);
  builder.openObject();
  builder.add(StaticStrings::Error, VPackValue(false));
  builder.add(StaticStrings::Code,
              VPackValue(static_cast<int>(rest::ResponseCode::OK)));
  builder.add("indexes", VPackValue(VPackValueType::Object));
  for (auto const& idx : idxs) {
    if (idx->inProgress() || idx->isHidden()) {
      continue;
    }
    if (idx->hasSelectivityEstimate() || idx->unique()) {
      builder.add(absl::StrCat(coll->name(), TRI_INDEX_HANDLE_SEPARATOR_STR,
                               idx->id().id()),
                  VPackValue(idx->selectivityEstimate()));
    }
  }
  builder.close();
  builder.close();

  generateResult(rest::ResponseCode::OK, std::move(buffer));
}

futures::Future<Result> RestIndexHandler::waitForVectorIndexReady(
    std::shared_ptr<LogicalCollection> const& coll, IndexId indexId) {
  // Polling interval when waiting for vector index readiness on Coordinator.
  static constexpr auto kCoordinatorPollInterval = std::chrono::seconds(3);
  // Maximum time to wait for vector index readiness on Coordinator.
  static constexpr auto kCoordinatorWaitTimeout = std::chrono::seconds(300);

  // DBServer / SingleServer: delegate to build manager.
  if (ServerState::instance()->isDBServer() ||
      ServerState::instance()->isSingleServer()) {
    auto& vectorIndexFeature = server().getFeature<VectorIndexFeature>();
    co_return co_await vectorIndexFeature.waitForIndexReady(indexId);
  }

  // Coordinator: poll shard training states until all report "ready".
  TRI_ASSERT(ServerState::instance()->isCoordinator());

  auto bareIndexId = std::to_string(indexId.id());
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  auto planIdStr = std::to_string(coll->planId().id());

  auto const deadline =
      std::chrono::steady_clock::now() + kCoordinatorWaitTimeout;

  while (true) {
    if (server().isStopping()) {
      co_return Result(TRI_ERROR_SHUTTING_DOWN);
    }

    std::shared_ptr<CollectionInfoCurrent> collCurrent =
        ci.getCollectionCurrent(_vocbase.name(), planIdStr);

    if (collCurrent) {
      auto const shardIds = coll->shardIds();
      if (shardIds) {
        auto const states =
            getVectorIndexShardStates(*collCurrent, *shardIds, bareIndexId);
        auto const aggregateState = aggregateTrainingState(states);

        if (aggregateState == StaticStrings::IndexTrainingStateReady) {
          co_return Result();
        }

        // "unusable" with an explicit shard error is a permanent failure.
        if (aggregateState == StaticStrings::IndexTrainingStateUnusable) {
          for (auto const& [shardId, shardState] : states) {
            if (!shardState.error.empty()) {
              co_return Result(
                  TRI_ERROR_QUERY_VECTOR_INDEX_NOT_READY,
                  absl::StrCat("vector index training failed on shard ",
                               static_cast<std::string>(shardId), ": ",
                               shardState.error));
            }
          }
        }
      }
    }

    if (std::chrono::steady_clock::now() >= deadline) {
      co_return Result(TRI_ERROR_LOCK_TIMEOUT,
                       "timed out waiting for vector index to become ready");
    }

    co_await SchedulerFeature::SCHEDULER->delay("wait-vec-idx-ready",
                                                kCoordinatorPollInterval);
  }
}

futures::Future<Result> RestIndexHandler::awaitAndRefreshVectorIndex(
    std::shared_ptr<LogicalCollection> const& coll, VPackSlice body,
    velocypack::Builder& response) {
  bool const inBackground = body.get("inBackground").isTrue();
  if (inBackground || !response.slice().get("type").isString() ||
      response.slice().get("type").stringView() !=
          StaticStrings::IndexNameVector) {
    co_return Result{};
  }

  auto& vectorIndexFeature = server().getFeature<VectorIndexFeature>();
  if (!vectorIndexFeature.isVectorIndexEnabled()) {
    co_return Result{};
  }

  auto idSlice = response.slice().get("id");
  if (!idSlice.isString()) {
    co_return Result{};
  }

  auto const indexId = IndexId::fromString(idSlice.stringView());
  if (indexId.fail()) {
    co_return indexId.result();
  }

  auto res = co_await waitForVectorIndexReady(coll, indexId.get());
  if (res.fail()) {
    co_return res;
  }

  // Update the trainingState field to reflect that the index is now ready
  // and clear any errorMessage that was set while the index was still building.
  VPackBuilder patch;
  {
    VPackObjectBuilder guard(&patch);
    patch.add(StaticStrings::IndexTrainingState,
              VPackValue(StaticStrings::IndexTrainingStateReady));
    patch.add(StaticStrings::ErrorMessage, VPackValue(""));
  }
  response = VPackCollection::merge(response.slice(), patch.slice(), false);

  co_return Result{};
}

async<void> RestIndexHandler::createIndex() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();
  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    co_return;
  }
  if (!suffixes.empty()) {
    events::CreateIndexEnd(_vocbase.name(), "(unknown)", body,
                           TRI_ERROR_BAD_PARAMETER);
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  absl::StrCat("expecting POST ", _request->requestPath(),
                               "?collection=<collection-name>"));
    co_return;
  }

  bool found = false;
  std::string cName = _request->value("collection", found);
  if (!found) {
    events::CreateIndexEnd(_vocbase.name(), "(unknown)", body,
                           TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    generateError(rest::ResponseCode::NOT_FOUND,
                  TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    co_return;
  }

  auto coll = collection(cName);
  if (coll == nullptr) {
    events::CreateIndexEnd(_vocbase.name(), cName, body,
                           TRI_ERROR_ARANGO_INDEX_NOT_FOUND);
    generateError(rest::ResponseCode::NOT_FOUND,
                  TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    co_return;
  }

  VPackBuilder copy;
  if (body.get("collection").isNone()) {
    copy.openObject();
    copy.add("collection", VPackValue(cName));
    copy.close();
    copy = VPackCollection::merge(body, copy.slice(), false);
    body = copy.slice();
  }

  VPackBuilder indexInfo;
  indexInfo.add(body);

  Result result;
  try {
    arangodb::velocypack::Builder response;
    result = co_await methods::Indexes::ensureIndex(*coll, indexInfo.slice(),
                                                    true, response);

    if (result.ok()) {
      TRI_ASSERT(response.slice().isObject());
      auto vecRes = co_await awaitAndRefreshVectorIndex(coll, body, response);
      if (vecRes.fail()) {
        generateError(vecRes);
        co_return;
      }

      VPackSlice const created = response.slice().get("isNewlyCreated");
      auto const resCode = created.isBool() && created.getBool()
                               ? rest::ResponseCode::CREATED
                               : rest::ResponseCode::OK;

      VPackBuilder b;
      b.openObject();
      b.add(StaticStrings::Error, VPackValue(false));
      b.add(StaticStrings::Code, VPackValue(static_cast<int>(resCode)));
      b.close();
      response = VPackCollection::merge(response.slice(), b.slice(), false);
      generateResult(resCode, response.slice());
      co_return;
    }
  } catch (basics::Exception const& ex) {
    result = Result(ex.code(), ex.message());
  } catch (std::exception const& ex) {
    result = Result(TRI_ERROR_INTERNAL, ex.what());
  }

  if (result.is(TRI_ERROR_FORBIDDEN) ||
      result.is(TRI_ERROR_ARANGO_INDEX_NOT_FOUND)) {
    generateError(result);
  } else {
    // http_server compatibility
    generateError(rest::ResponseCode::BAD, result.errorNumber(),
                  result.errorMessage());
  }
}

async<void> RestIndexHandler::dropIndex() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();
  if (suffixes.size() != 2) {
    events::DropIndex(_vocbase.name(), "(unknown)", "(unknown)",
                      TRI_ERROR_HTTP_BAD_PARAMETER);
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting DELETE /<collection-name>/<index-identifier>");
    co_return;
  }

  std::string const& cName = suffixes[0];
  auto coll = collection(cName);
  if (coll == nullptr) {
    events::DropIndex(_vocbase.name(), cName, "(unknown)",
                      TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    generateError(rest::ResponseCode::NOT_FOUND,
                  TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    co_return;
  }

  std::string const& iid = suffixes[1];
  VPackBuilder idBuilder;
  if (iid.starts_with(cName + TRI_INDEX_HANDLE_SEPARATOR_CHR)) {
    idBuilder.add(VPackValue(iid));
  } else {
    idBuilder.add(
        VPackValue(absl::StrCat(cName, TRI_INDEX_HANDLE_SEPARATOR_STR, iid)));
  }

  Result res = co_await methods::Indexes::drop(*coll, idBuilder.slice());
  if (res.ok()) {
    VPackBuilder b;
    b.openObject();
    b.add("id", idBuilder.slice());
    b.add(StaticStrings::Error, VPackValue(false));
    b.add(StaticStrings::Code, VPackValue(static_cast<int>(ResponseCode::OK)));
    b.close();
    generateResult(rest::ResponseCode::OK, b.slice());
  } else {
    generateError(res);
  }
}

void RestIndexHandler::syncCaches() {
  StorageEngine& engine = _vocbase.engine();
  engine.syncIndexCaches();

  generateResult(rest::ResponseCode::OK, VPackSlice::emptyObjectSlice());
}
