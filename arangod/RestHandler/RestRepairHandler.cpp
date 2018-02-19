////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include <valarray>
#include <arangod/GeneralServer/AsyncJobManager.h>
#include "RestRepairHandler.h"

#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;


RestRepairHandler::RestRepairHandler(
  GeneralRequest *request, GeneralResponse *response
)
  : RestBaseHandler(request, response) {
}


RestStatus RestRepairHandler::execute() {
  LOG_TOPIC(ERR, arangodb::Logger::FIXME)
  << "[tg] RestRepairHandler::execute()";

  if (SchedulerFeature::SCHEDULER->isStopping()) {
    generateError(rest::ResponseCode::SERVICE_UNAVAILABLE, TRI_ERROR_SHUTTING_DOWN);
    return RestStatus::FAIL;
  }

  if (_request->requestType() != rest::RequestType::POST) {
    generateError(
      rest::ResponseCode::METHOD_NOT_ALLOWED,
      (int) rest::ResponseCode::METHOD_NOT_ALLOWED
    );

    return RestStatus::FAIL;
  }

  std::vector<std::string> const &suffixes = _request->suffixes();

  if (suffixes.size() != 1) {
    generateError(
      rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
      "Bad parameter: expected 'distributeShardsLike', got none"
    );

    return RestStatus::FAIL;
  }

  if (suffixes[0] != "distributeShardsLike") {
    generateError(
      rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
      "Bad parameter: expected 'distributeShardsLike', got '" + suffixes[0] + "'"
    );

    return RestStatus::FAIL;
  }

  //scheduler->
// TODO Instantiate job?

  repairDistributeShardsLike();

  resetResponse(rest::ResponseCode::OK);

  return RestStatus::DONE;
}


void RestRepairHandler::repairDistributeShardsLike() {
  LOG_TOPIC(ERR, arangodb::Logger::FIXME)
  << "[tg] RestRepairHandler::repairDistributeShardsLike()";

}
