////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Andreas Streichardt
////////////////////////////////////////////////////////////////////////////////

#include "RestAgencyCallbacksHandler.h"

#include "Cluster/AgencyCallbackRegistry.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"

using namespace arangodb;
using namespace arangodb::rest;

RestAgencyCallbacksHandler::RestAgencyCallbacksHandler(GeneralRequest* request,
                                                       GeneralResponse* response,
                                                       arangodb::AgencyCallbackRegistry* agencyCallbackRegistry)
    : RestVocbaseBaseHandler(request, response),
      _agencyCallbackRegistry(agencyCallbackRegistry) {}

RestStatus RestAgencyCallbacksHandler::execute() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid callback");
    return RestStatus::DONE;
  }

  // extract the sub-request type
  auto const type = _request->requestType();
  if (type != rest::RequestType::POST) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }

  bool parseSuccess = true;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess || body.isNone()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid JSON");
    return RestStatus::DONE;
  }

  uint32_t index = basics::StringUtils::uint32(suffixes.at(0));
  auto cb = _agencyCallbackRegistry->getCallback(index);
  if (cb.get() == nullptr) {
    // no entry by this id!
    resetResponse(arangodb::rest::ResponseCode::NOT_FOUND);
  } else {
    LOG_TOPIC(DEBUG, Logger::CLUSTER)
        << "Agency callback has been triggered. refetching!";

    // SchedulerFeature::SCHEDULER->queue(RequestPriority::MED, [cb] {
    try {
      cb->refetchAndUpdate(true, false);
    } catch (arangodb::basics::Exception const& e) {
      LOG_TOPIC(WARN, Logger::AGENCYCOMM) << "Error executing callback: " << e.message();
    }
    //});
    resetResponse(arangodb::rest::ResponseCode::ACCEPTED);
  }

  return RestStatus::DONE;
}
