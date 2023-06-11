////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RestDumpHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterTypes.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/RequestLane.h"
#include "RocksDBEngine/RocksDBDumpManager.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Utils/ExecContext.h"
#include "VocBase/ComputedValues.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <cstdint>
#include <vector>

using namespace arangodb;
using namespace arangodb::rest;

namespace {
constexpr uint64_t defaultBatchSize = 16 * 1024;
constexpr uint64_t defaultPrefetchCount = 2;
constexpr uint64_t defaultParallelism = 2;
constexpr double defaultTtl = 600.0;
}  // namespace

RestDumpHandler::RestDumpHandler(ArangodServer& server, GeneralRequest* request,
                                 GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

struct DumpExpressionContext : ComputedValuesExpressionContext {
  DumpExpressionContext(transaction::Methods& trx,
                        LogicalCollection& collection, TRI_vocbase_t& vocbase)
      : ComputedValuesExpressionContext(trx, collection),
        _resolver(std::make_unique<CollectionNameResolver>(vocbase)) {}

  CollectionNameResolver const* resolver() override { return _resolver.get(); }

  std::unique_ptr<CollectionNameResolver> _resolver;
};

struct ComputedValueProjection : DumpProjection {
  explicit ComputedValueProjection(ComputedValues::ComputedValue& cv,
                                   transaction::Methods* mthds,
                                   LogicalCollection* col,
                                   TRI_vocbase_t& vocbase)
      : ctx(*mthds, *col, vocbase), cv(cv) {
    ctx.failOnWarning(cv.failOnWarning());
    // update "name" value for each computation (for errors/warnings)
    ctx.setName(cv.name());
  }
  bool operator()(VPackSlice input, VPackBuilder& output,
                  velocypack::Options const* vopts) override {
    // inject document into temporary variable (@doc)
    ctx.setVariable(cv.tempVariable(), input);
    // if "computeAttribute" throws, then the operation is
    // intentionally aborted here. caller has to catch the
    // exception
    cv.computeValue(ctx, output, vopts);
    // discard documents that produce null values
    return !output.slice().isNull();
  }

  DumpExpressionContext ctx;
  ComputedValues::ComputedValue& cv;
};

struct ComputedValueDumpProjectionFactory : DumpProjectionFactory {
  ComputedValueDumpProjectionFactory(TRI_vocbase_t& voc,
                                     std::string_view expression)
      : voc(voc),
        cv(voc, "projection", expression, arangodb::ComputeValuesOn::kNever,
           false, false, true) {}

  std::unique_ptr<DumpProjection> createProjection() override {
    return std::make_unique<ComputedValueProjection>(cv, nullptr, nullptr, voc);
  }

  TRI_vocbase_t& voc;
  ComputedValues::ComputedValue cv;
};

// main function that dispatches the different routes and commands
RestStatus RestDumpHandler::execute() {
  // we should not get here on coordinators, simply because of the
  // request forwarding.
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  auto type = _request->requestType();
  auto const& suffixes = _request->suffixes();
  size_t const len = suffixes.size();

  if (type == rest::RequestType::DELETE_REQ) {
    if (len == 1) {
      // end a dump
      handleCommandDumpFinished();
    } else {
      generateError(
          Result(TRI_ERROR_BAD_PARAMETER, "expecting DELETE /_api/dump/<id>"));
    }
  } else if (type == rest::RequestType::POST) {
    if (len == 1 && suffixes[0] == "start") {
      // start a dump
      handleCommandDumpStart();
    } else if (len == 2 && suffixes[0] == "next") {
      // fetch next data from a dump
      handleCommandDumpNext();
    } else {
      generateError(
          Result(TRI_ERROR_BAD_PARAMETER,
                 "expecting POST /_api/dump/start or /_api/dump/next/<id>"));
    }
  } else {
    // invalid HTTP method
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  }

  return RestStatus::DONE;
}

RequestLane RestDumpHandler::lane() const {
  auto type = _request->requestType();

  if (type == rest::RequestType::DELETE_REQ) {
    // delete should be prioritized, because it frees
    // up resources
    return RequestLane::CLUSTER_INTERNAL;
  }
  return RequestLane::SERVER_REPLICATION;
}

/// @brief returns the short id of the server which should handle this request
ResultT<std::pair<std::string, bool>> RestDumpHandler::forwardingTarget() {
  auto base = RestVocbaseBaseHandler::forwardingTarget();
  if (base.ok() && !std::get<0>(base.get()).empty()) {
    return base;
  }

  auto type = _request->requestType();
  auto const& suffixes = _request->suffixes();
  size_t const len = suffixes.size();
  if (type == rest::RequestType::DELETE_REQ) {
    if (len != 1) {
      return ResultT<std::pair<std::string, bool>>::error(
          TRI_ERROR_BAD_PARAMETER, "expecting DELETE /_api/dump/<id>");
    }
  } else if (type == rest::RequestType::POST) {
    if ((len == 1 && suffixes[0] != "start") ||
        (len == 2 && suffixes[0] != "next") || len < 1 || len > 2) {
      return ResultT<std::pair<std::string, bool>>::error(
          TRI_ERROR_BAD_PARAMETER,
          "expecting POST /_api/dump/start or /_api/dump/next/<id>");
    }
  } else {
    // invalid HTTP method
    return ResultT<std::pair<std::string, bool>>::error(
        TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  }

  if (ServerState::instance()->isCoordinator()) {
    ServerID const& DBserver = _request->value("dbserver");
    if (!DBserver.empty()) {
      // if DBserver property present, remove auth header
      return std::make_pair(DBserver, true);
    }

    return ResultT<std::pair<std::string, bool>>::error(
        TRI_ERROR_BAD_PARAMETER, "need a 'dbserver' parameter");
  }

  return {std::make_pair(StaticStrings::Empty, false)};
}

void RestDumpHandler::handleCommandDumpStart() {
  std::string database = _request->databaseName();
  std::string user = _request->user();

  // TODO: check permissions

  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {  // error message generated in parseVPackBody
    return;
  }

  if (!body.isObject()) {
    generateError(Result(TRI_ERROR_BAD_PARAMETER, "invalid request body"));
    return;
  }

  RocksDBDumpContextOptions opts;
  opts.batchSize = [&body]() {
    if (auto s = body.get("batchSize"); s.isNumber()) {
      return s.getNumber<uint64_t>();
    }
    return ::defaultBatchSize;
  }();
  opts.prefetchCount = [&body]() {
    if (auto s = body.get("prefetchCount"); s.isNumber()) {
      return s.getNumber<uint64_t>();
    }
    return ::defaultPrefetchCount;
  }();
  opts.parallelism = [&body]() {
    if (auto s = body.get("parallelism"); s.isNumber()) {
      return s.getNumber<uint64_t>();
    }
    return ::defaultParallelism;
  }();
  opts.ttl = [&body]() {
    if (auto s = body.get("ttl"); s.isNumber()) {
      return s.getNumber<double>();
    }
    return ::defaultTtl;
  }();

  if (auto s = body.get("shards"); !s.isArray()) {
    generateError(
        Result(TRI_ERROR_BAD_PARAMETER, "invalid 'shards' value in request"));
    return;
  } else {
    for (auto it : VPackArrayIterator(s)) {
      opts.shards.emplace_back(it.copyString());
    }
  }

  opts.projection = [&]() -> std::unique_ptr<DumpProjectionFactory> {
    if (auto s = body.get("projection"); s.isString()) {
      return std::make_unique<ComputedValueDumpProjectionFactory>(
          _vocbase, s.stringView());
    } else if (!s.isNone()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "projection should be a string");
    } else {
      return nullptr;
    }
  }();

  auto& engine =
      server().getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
  auto* manager = engine.dumpManager();
  auto guard = manager->createContext(std::move(opts), user, database);

  resetResponse(rest::ResponseCode::CREATED);
  _response->setHeaderNC("x-arango-dump-id", guard->id());
}

void RestDumpHandler::handleCommandDumpNext() {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  auto const& suffixes = _request->suffixes();
  // checked before
  TRI_ASSERT(suffixes.size() == 2);
  auto const& id = _request->suffixes()[1];

  std::string database = _request->databaseName();
  std::string user = _request->user();

  auto batchId = _request->parsedValue<uint64_t>("batchId");
  if (!batchId.has_value()) {
    generateError(Result(TRI_ERROR_BAD_PARAMETER, "expecting 'batchId'"));
    return;
  }

  auto lastBatch = _request->parsedValue<uint64_t>("lastBatch");

  auto& engine =
      server().getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
  auto* manager = engine.dumpManager();
  auto guard = manager->find(id, database, user);

  auto batch = guard->next(*batchId, lastBatch);
  auto counts = guard->getBlockCounts();

  if (batch == nullptr) {
    // all batches have been received
    return generateOk(rest::ResponseCode::NO_CONTENT, VPackSlice::noneSlice());
  }

  // output the batch value
  _response->setHeaderNC("x-arango-dump-shard-id", std::string{batch->shard});
  _response->setHeaderNC("x-arango-dump-block-counts", std::to_string(counts));
  _response->setContentType(rest::ContentType::DUMP);
  _response->addRawPayload(batch->content);
  _response->setGenerateBody(true);
  _response->setResponseCode(rest::ResponseCode::OK);
}

void RestDumpHandler::handleCommandDumpFinished() {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  auto const& suffixes = _request->suffixes();
  // checked before
  TRI_ASSERT(suffixes.size() == 1);
  auto const& id = _request->suffixes()[0];

  std::string database = _request->databaseName();
  std::string user = _request->user();

  auto& engine =
      server().getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
  auto* manager = engine.dumpManager();
  // will throw if dump context is not found or cannot be accessed
  manager->remove(id, database, user);

  generateOk(rest::ResponseCode::OK, VPackSlice::noneSlice());
}
