////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

#include "RestTelemetricsHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "RestServer/arangod.h"
#include "Utils/ExecContext.h"
#include "Utils/SupportInfoBuilder.h"

#include <velocypack/Builder.h>

#include <chrono>
#include <memory>
#include <mutex>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

namespace {

struct RequestTracker {
 public:
  RequestTracker(uint64_t maxRequestsPerBucket)
      : _maxRequestsPerBucket(maxRequestsPerBucket) {}

  void reset() {
    std::unique_lock lk(_mtx);
    _lastRequestBucket = 0;
    _requestsInBucket = 0;
  }

  bool track() {
    // get current timestamp as a number
    uint64_t bucket = std::chrono::duration_cast<std::chrono::seconds>(
                          std::chrono::steady_clock::now().time_since_epoch())
                          .count();
    // round stamp down to full hours (note: we are using integer division here)
    bucket /= kbucketWidth;

    std::unique_lock lk(_mtx);
    if (_lastRequestBucket != bucket) {
      // first request for the current interval.
      // we can safely replace the bucket value, because the bucket is
      // determined only by us, and by a steady_clock, which will only ever
      // count forward.
      _lastRequestBucket = bucket;
      _requestsInBucket = 0;
    }

    // count the request.
    return ++_requestsInBucket <= _maxRequestsPerBucket;
  }

 private:
  // width of bucket (in seconds) in which we track telemetrics requests.
  static constexpr uint64_t kbucketWidth = 7200;
  // with these default value it means that we only let up to 3 requests
  // from the arangosh through to the telemetrics API every 2 hours.
  // any additional requests will be responded to by the API with HTTP
  // 420. we do this to not over-report telemetry data, and also to
  // protect servers from being overwhelmed by too many telemetrics API
  // requests from batch programs running in arangosh.
  // note that the counters here are only stored in RAM on single servers
  // and coordinators. they are not persisted, so after a server restart
  // the counters are back at 0. additionally, we do not keep track of
  // the value across different coordinators. in a load-balanced environment,
  // the arangosh can actually get up to (number of coordinators *
  // maxRequestsPerBucket) requests through in every interval.
  // we don't think this is a real problem that would justify a much more
  // complicated and less efficient request tracking (and probably some
  // coordination between servers).
  uint64_t const _maxRequestsPerBucket;

  std::mutex _mtx;
  uint64_t _lastRequestBucket = 0;
  uint64_t _requestsInBucket = 0;
};

std::mutex requestTrackerMutex;
std::unique_ptr<RequestTracker> requestTracker;

RequestTracker& ensureRequestTracker(GeneralServerFeature& gf) {
  {
    // check if requestTracker was already created
    std::unique_lock lk(requestTrackerMutex);
    if (requestTracker == nullptr) {
      requestTracker = std::make_unique<RequestTracker>(
          gf.telemetricsMaxRequestsPerInterval());
    }
  }
  TRI_ASSERT(requestTracker != nullptr);
  return *requestTracker;
}

}  // namespace

RestTelemetricsHandler::RestTelemetricsHandler(ArangodServer& server,
                                               GeneralRequest* request,
                                               GeneralResponse* response)
    : RestBaseHandler(server, request, response) {}

RestStatus RestTelemetricsHandler::execute() {
  GeneralServerFeature& gs = server().getFeature<GeneralServerFeature>();
  if (!gs.isTelemetricsEnabled()) {
    generateError(
        rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
        "telemetrics API is disabled. Must enable with startup parameter "
        "`--server.telemetrics-api`.");
    return RestStatus::DONE;
  }

  auto const& apiPolicy = gs.supportInfoApiPolicy();
  TRI_ASSERT(apiPolicy != "disabled");

  if (apiPolicy == "jwt") {
    if (!ExecContext::current().isSuperuser()) {
      generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                    "insufficient permissions");
      return RestStatus::DONE;
    }
  }

  if (apiPolicy == "admin" && !ExecContext::current().isAdminUser()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                  "insufficient permissions");
    return RestStatus::DONE;
  }

  if (request()->requestType() == rest::RequestType::DELETE_REQ) {
    // reset telemetrics access counter. this is an informal API that we
    // use only for testing the telemetrics behavior.
    resetTelemetricsRequestsCounter();
    generateOk(rest::ResponseCode::OK, velocypack::Slice::emptyObjectSlice());
    return RestStatus::DONE;
  }

  // only let GET and DELETE requests pass
  if (request()->requestType() != rest::RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }

  if (ServerState::instance()->isSingleServerOrCoordinator()) {
    std::string const& userAgent = _request->header(StaticStrings::UserAgent);
    if (userAgent.starts_with("arangosh/") &&
        !trackTelemetricsRequestsCounter()) {
      // telemetrics request sent from arangosh. now check when it last
      // sents its request. we don't want to let all arangosh requests come
      // through to avoid overwhelming the server with telemetrics requests
      // from rogue arangosh batch jobs.
      // respond with HTTP 420
      generateError(rest::ResponseCode::ENHANCE_YOUR_CALM,
                    TRI_ERROR_HTTP_ENHANCE_YOUR_CALM,
                    "too many recent requests to telemetrics API.");
      return RestStatus::DONE;
    }
  }

  velocypack::Builder result;

  bool isLocal = _request->parsedValue("local", false);
  SupportInfoBuilder::buildInfoMessage(result, _request->databaseName(),
                                       _server, isLocal, true);
  generateResult(rest::ResponseCode::OK, result.slice());
  // allow sending compressed responses out
  response()->setAllowCompression(true);

  return RestStatus::DONE;
}

void RestTelemetricsHandler::resetTelemetricsRequestsCounter() {
  GeneralServerFeature& gs = server().getFeature<GeneralServerFeature>();
  ::ensureRequestTracker(gs).reset();
}

bool RestTelemetricsHandler::trackTelemetricsRequestsCounter() {
  GeneralServerFeature& gs = server().getFeature<GeneralServerFeature>();
  return ::ensureRequestTracker(gs).track();
}
