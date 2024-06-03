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
#include "Cluster/AgencyCache.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Futures/Utilities.h"
#include "Logger/LogMacros.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "RestServer/VocbaseContext.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Methods.h"
#include "Transaction/OperationOrigin.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/Events.h"
#include "Utils/ExecContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Indexes.h"

#include <absl/strings/str_cat.h>

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::futures;
using namespace arangodb::rest;

RestIndexHandler::RestIndexHandler(ArangodServer& server,
                                   GeneralRequest* request,
                                   GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

RestIndexHandler::~RestIndexHandler() {
  std::unique_lock<std::mutex> locker(_mutex);
  shutdownBackgroundThread();
}

RestStatus RestIndexHandler::execute() {
  // extract the request type
  rest::RequestType const type = _request->requestType();
  if (type == rest::RequestType::GET) {
    if (_request->suffixes().size() == 1 &&
        _request->suffixes()[0] == "selectivity") {
      return waitForFuture(getSelectivityEstimates());
    }
    return getIndexes();
  } else if (type == rest::RequestType::POST) {
    if (_request->suffixes().size() == 1 &&
        _request->suffixes()[0] == "sync-caches") {
      // this is an unofficial API to sync the in-memory
      // index caches with the data queued in the index
      // refill background thread. it is not supposed to
      // be used publicly.
      return syncCaches();
    }
    return createIndex();
  } else if (type == rest::RequestType::DELETE_REQ) {
    return dropIndex();
  } else {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }
}

RestStatus RestIndexHandler::continueExecute() {
  TRI_ASSERT(_request->requestType() == rest::RequestType::POST);

  std::unique_lock<std::mutex> locker(_mutex);

  shutdownBackgroundThread();

  if (_createInBackgroundData.result.ok()) {
    TRI_ASSERT(_createInBackgroundData.response.slice().isObject());

    VPackSlice created =
        _createInBackgroundData.response.slice().get("isNewlyCreated");
    auto r = created.isBool() && created.getBool() ? rest::ResponseCode::CREATED
                                                   : rest::ResponseCode::OK;

    generateResult(r, _createInBackgroundData.response.slice());
  } else {
    if (_createInBackgroundData.result.is(TRI_ERROR_FORBIDDEN) ||
        _createInBackgroundData.result.is(TRI_ERROR_ARANGO_INDEX_NOT_FOUND)) {
      generateError(_createInBackgroundData.result);
    } else {  // http_server compatibility
      generateError(rest::ResponseCode::BAD,
                    _createInBackgroundData.result.errorNumber(),
                    _createInBackgroundData.result.errorMessage());
    }
  }

  return RestStatus::DONE;
}

void RestIndexHandler::shutdownExecute(bool isFinalized) noexcept {
  auto sg = arangodb::scopeGuard(
      [&]() noexcept { RestVocbaseBaseHandler::shutdownExecute(isFinalized); });
  if (isFinalized && _request->requestType() == rest::RequestType::POST) {
    std::unique_lock<std::mutex> locker(_mutex);
    shutdownBackgroundThread();
  }
}

void RestIndexHandler::shutdownBackgroundThread() {
  if (_createInBackgroundData.thread != nullptr) {
    _createInBackgroundData.thread->join();
    _createInBackgroundData.thread.reset();
  }
}

std::shared_ptr<LogicalCollection> RestIndexHandler::collection(
    std::string const& cName) {
  if (!cName.empty()) {
    if (ServerState::instance()->isCoordinator()) {
      return server()
          .getFeature<ClusterFeature>()
          .clusterInfo()
          .getCollectionNT(_vocbase.name(), cName);
    } else {
      return _vocbase.lookupCollection(cName);
    }
  }

  return nullptr;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief get index infos
// //////////////////////////////////////////////////////////////////////////////

RestStatus RestIndexHandler::getIndexes() {
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
      return RestStatus::DONE;
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
          methods::Indexes::getAll(*coll, flags, withHidden, indexes).get();
      if (!res.ok()) {
        generateError(rest::ResponseCode::BAD, res.errorNumber(),
                      res.errorMessage());
        return RestStatus::DONE;
      }

      TRI_ASSERT(indexes.slice().isArray());

      tmp.add("indexes", indexes.slice());
      tmp.add("identifiers", VPackValue(VPackValueType::Object));
      for (auto index : VPackArrayIterator(indexes.slice())) {
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
      auto& ac = _vocbase.server().getFeature<ClusterFeature>().agencyCache();
      // we need to wait for the latest commit index here, because otherwise
      // we may not see all indexes that were declared ready by the
      // supervision.
      ac.waitForLatestCommitIndex().get();

      auto [plannedIndexes, idx] = ac.get(ap);

      // now fetch list of ready indexes
      VPackBuilder indexes;
      Result res =
          methods::Indexes::getAll(*coll, flags, withHidden, indexes).get();
      if (!res.ok()) {
        generateError(rest::ResponseCode::BAD, res.errorNumber(),
                      res.errorMessage());
        return RestStatus::DONE;
      }

      TRI_ASSERT(indexes.slice().isArray());

      // all indexes we already reported
      containers::FlatHashSet<std::string> covered;

      tmp.add(VPackValue("indexes"));

      {
        VPackArrayBuilder guard(&tmp);
        // first return all ready indexes
        for (auto pi : VPackArrayIterator(indexes.slice())) {
          std::string_view iid = pi.get("id").stringView();
          tmp.add(pi);

          // note this index as already covered
          covered.emplace(iid);
        }
        // now return all in-progress indexes, if any
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
            tmp.add(source.key.stringView(), source.value);
          }

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
            network::Response const& r = f.get();

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
        }
      }

      // also report all indexes in the "identifiers" attribute.
      // TODO: this is redundant and unncessarily complicates the API return
      // value. this attribute should be deprecated and removed
      tmp.add("identifiers", VPackValue(VPackValueType::Object));
      for (auto pi : VPackArrayIterator(indexes.slice())) {
        std::string_view iid = pi.get("id").stringView();
        tmp.add(iid, pi);
      }
      for (auto pi : VPackArrayIterator(plannedIndexes->slice())) {
        std::string_view iid = pi.get("id").stringView();
        // avoid reporting an index twice
        if (covered.contains(iid) ||
            !pi.get(StaticStrings::IndexIsBuilding).isTrue()) {
          continue;
        }
        tmp.add(iid, pi);
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
      return RestStatus::DONE;
    }

    std::string const& iid = suffixes[1];
    VPackBuilder tmp;
    tmp.add(VPackValue(cName + TRI_INDEX_HANDLE_SEPARATOR_CHR + iid));

    VPackBuilder output;
    Result res = methods::Indexes::getIndex(*coll, tmp.slice(), output).get();
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
  return RestStatus::DONE;
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

  Result res = trx->begin();
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
  for (std::shared_ptr<Index> idx : idxs) {
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
  co_return;
}

RestStatus RestIndexHandler::createIndex() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();
  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    return RestStatus::DONE;
  }
  if (!suffixes.empty()) {
    events::CreateIndexEnd(_vocbase.name(), "(unknown)", body,
                           TRI_ERROR_BAD_PARAMETER);
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  absl::StrCat("expecting POST ", _request->requestPath(),
                               "?collection=<collection-name>"));
    return RestStatus::DONE;
  }

  bool found = false;
  std::string cName = _request->value("collection", found);
  if (!found) {
    events::CreateIndexEnd(_vocbase.name(), "(unknown)", body,
                           TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    generateError(rest::ResponseCode::NOT_FOUND,
                  TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    return RestStatus::DONE;
  }

  auto coll = collection(cName);
  if (coll == nullptr) {
    events::CreateIndexEnd(_vocbase.name(), cName, body,
                           TRI_ERROR_ARANGO_INDEX_NOT_FOUND);
    generateError(rest::ResponseCode::NOT_FOUND,
                  TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    return RestStatus::DONE;
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

  auto execContext = VocbaseContext::create(*_request, _vocbase);
  // this is necessary, because the execContext will release the vocbase in its
  // dtor
  if (!_vocbase.use()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  std::unique_lock<std::mutex> locker(_mutex);

  // the following callback is executed in a background thread
  auto cb = [this, self = shared_from_this(),
             execContext = std::move(execContext), collection = std::move(coll),
             body = std::move(indexInfo)] {
    ExecContextScope scope(std::move(execContext));
    {
      std::unique_lock<std::mutex> locker(_mutex);

      try {
        _createInBackgroundData.result =
            methods::Indexes::ensureIndex(*collection, body.slice(), true,
                                          _createInBackgroundData.response)
                .get();

        if (_createInBackgroundData.result.ok()) {
          VPackSlice created =
              _createInBackgroundData.response.slice().get("isNewlyCreated");
          auto r = created.isBool() && created.getBool()
                       ? rest::ResponseCode::CREATED
                       : rest::ResponseCode::OK;

          VPackBuilder b;
          b.openObject();
          b.add(StaticStrings::Error, VPackValue(false));
          b.add(StaticStrings::Code, VPackValue(static_cast<int>(r)));
          b.close();
          _createInBackgroundData.response = VPackCollection::merge(
              _createInBackgroundData.response.slice(), b.slice(), false);
        }
      } catch (basics::Exception const& ex) {
        _createInBackgroundData.result = Result(ex.code(), ex.message());
      } catch (std::exception const& ex) {
        _createInBackgroundData.result = Result(TRI_ERROR_INTERNAL, ex.what());
      }
    }

    // notify REST handler
    SchedulerFeature::SCHEDULER->queue(RequestLane::INTERNAL_LOW,
                                       [self]() { self->wakeupHandler(); });
  };

  // start background thread
  _createInBackgroundData.thread = std::make_unique<std::thread>(std::move(cb));

  return RestStatus::WAITING;
}

RestStatus RestIndexHandler::dropIndex() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();
  if (suffixes.size() != 2) {
    events::DropIndex(_vocbase.name(), "(unknown)", "(unknown)",
                      TRI_ERROR_HTTP_BAD_PARAMETER);
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting DELETE /<collection-name>/<index-identifier>");
    return RestStatus::DONE;
  }

  std::string const& cName = suffixes[0];
  auto coll = collection(cName);
  if (coll == nullptr) {
    events::DropIndex(_vocbase.name(), cName, "(unknown)",
                      TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    generateError(rest::ResponseCode::NOT_FOUND,
                  TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    return RestStatus::DONE;
  }

  std::string const& iid = suffixes[1];
  VPackBuilder idBuilder;
  if (iid.starts_with(cName + TRI_INDEX_HANDLE_SEPARATOR_CHR)) {
    idBuilder.add(VPackValue(iid));
  } else {
    idBuilder.add(
        VPackValue(absl::StrCat(cName, TRI_INDEX_HANDLE_SEPARATOR_STR, iid)));
  }

  Result res = methods::Indexes::drop(*coll, idBuilder.slice()).get();
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
  return RestStatus::DONE;
}

RestStatus RestIndexHandler::syncCaches() {
  StorageEngine& engine = _vocbase.engine();
  engine.syncIndexCaches();

  generateResult(rest::ResponseCode::OK, VPackSlice::emptyObjectSlice());
  return RestStatus::DONE;
}
