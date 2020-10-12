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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "RestTestHandler.h"

#include "Basics/ResultT.h"
#include "GeneralServer/GeneralServer.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "GeneralServer/RestHandlerFactory.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Scheduler/SchedulerFeature.h"
#include "Utils/ExecContext.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestTestHandler::RestTestHandler(application_features::ApplicationServer& server,
                                 GeneralRequest* request, GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

RestTestHandler::~RestTestHandler() = default;

namespace {
#define LANE_ENTRY(s) {#s, RequestLane::s},
const std::map<std::string, RequestLane> lanes = {
    LANE_ENTRY(CLIENT_FAST) LANE_ENTRY(CLIENT_AQL) LANE_ENTRY(CLIENT_V8)
        LANE_ENTRY(CLIENT_SLOW) LANE_ENTRY(AGENCY_INTERNAL) LANE_ENTRY(AGENCY_CLUSTER)
            LANE_ENTRY(CLUSTER_INTERNAL) LANE_ENTRY(CLUSTER_V8) LANE_ENTRY(CLUSTER_ADMIN)
                LANE_ENTRY(SERVER_REPLICATION) LANE_ENTRY(TASK_V8)};
}  // namespace

ResultT<RequestLane> RestTestHandler::requestLaneFromString(const std::string& str) {
  auto entry = lanes.find(str);

  if (entry != lanes.end()) {
    return entry->second;
  }

  return Result(TRI_ERROR_HTTP_BAD_PARAMETER,
                "Expected request-lane, found `" + str + "`");
}

RestStatus RestTestHandler::execute() {
  // extract the request type
  auto const type = _request->requestType();

  if (type != rest::RequestType::POST) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }

  // Get the execution class
  // then optionally get the duration to work
  // then check if we should be a waiting rest handler

  auto const& suffixes = _request->suffixes();

  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                  "expecting GET /_api/test/<request-lane>");
    return RestStatus::DONE;
  }

  auto res = requestLaneFromString(suffixes[0]);

  if (res.fail()) {
    generateError(std::move(res).result());
    return RestStatus::DONE;
  }

  bool parsingSuccess = false;
  VPackSlice const body = this->parseVPackBody(parsingSuccess);
  if (!parsingSuccess) {
    return RestStatus::DONE;
  }

  LOG_TOPIC("8c671", TRACE, Logger::FIXME) << "Generating work on lane " << suffixes[0];

  clock::duration duration = std::chrono::milliseconds(100);

  if (!body.isNone()) {
    if (!body.isObject()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_TYPE_ERROR,
                    "expecting JSON object body");
      return RestStatus::DONE;
    }

    if (body.hasKey("workload")) {
      auto workload = body.get("workload");

      if (workload.isNumber()) {
        duration = std::chrono::milliseconds(workload.getInt());
      } else {
        generateError(rest::ResponseCode::BAD, TRI_ERROR_TYPE_ERROR,
                      "expecting int for `workload`");
        return RestStatus::DONE;
      }
    }
  }

  auto self(shared_from_this());
  bool ok = SchedulerFeature::SCHEDULER->queue(res.get(), [this, self, duration]() {
    auto stop = clock::now() + duration;

    uint64_t count = 0;

    // Please think of a better method to generate work.
    //  Do we actually need work or is a sleep ok?
    while (clock::now() < stop) {
      for (int i = 0; i < 10000; i++) {
        count += i * i;
      }
    }

    VPackBuffer<uint8_t> buffer;
    VPackBuilder builder(buffer);
    builder.openObject();
    builder.add("count", VPackValue(count));
    builder.close();

    resetResponse(rest::ResponseCode::OK);
    _response->setPayload(std::move(buffer));
    wakeupHandler();
  });

  if (ok) {
    return RestStatus::WAITING;
  }

  generateError(rest::ResponseCode::SERVICE_UNAVAILABLE, TRI_ERROR_QUEUE_FULL);
  return RestStatus::DONE;
}
