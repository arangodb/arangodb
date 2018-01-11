////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#include "MaintenanceRestHandler.h"

#include <velocypack/vpack.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/StringUtils.h"
#include "Basics/conversions.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

MaintenanceRestHandler::MaintenanceRestHandler(GeneralRequest* request,
                               GeneralResponse* response)
    : RestBaseHandler(request, response) {

}

bool MaintenanceRestHandler::isDirect() const { return true; }

RestStatus MaintenanceRestHandler::execute() {
  // extract the sub-request type
  auto const type = _request->requestType();

  switch(type) {

    // retrieve list of all actions
    case rest::RequestType::GET:
      break;

    // add an action to the list (or execute it directly)
    case rest::RequestType::PUT:
      putAction();
      break;

    // remove an action, stopping it if executing
    case rest::RequestType::DELETE_REQ:
      break;

    default:
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                    (int)rest::ResponseCode::METHOD_NOT_ALLOWED);
      break;
  } // switch

  return RestStatus::DONE;
}


void MaintenanceRestHandler::putAction() {
  bool good(true);
  VPackSlice parameters;
  std::map<std::string, std::string> param_map;

  try {
    parameters = _request->payload();
  } catch (VPackException const& ex) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  std::string("expecting a valid JSON object in the request. got: ") + ex.what());
    good=false;
  } // catch

  if (good && _request->payload().isEmptyObject()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_CORRUPTED_JSON);
    good=false;
  }

  // convert vpack into key/value map
  if (good) {
    VPackObjectIterator it(parameters, true);

    for ( ; it.valid() && good; ++it) {
      VPackSlice key, value;

      key = it.key();
      value = it.value();

      good = key.isString() && value.isString();

      // attempt insert into map ... but needs to be unique
      if (good) {
        good = param_map.insert(std::pair<std::string,std::string>(key.copyString(), value.copyString())).second;
      }
    } // for

    // bad json
    if (!good) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    std::string("unable to parse JSON object into key/value pairs. got: "));
    } // if
  } // if

  if (good) {
    Result result;

    // build the action
    auto maintenance = ApplicationServer::getFeature<MaintenanceFeature>("Maintenance");
    result = maintenance->addAction(param_map);

    if (!result.ok()) {
      // possible errors? TRI_ERROR_BAD_PARAMETER    TRI_ERROR_TASK_DUPLICATE_ID  TRI_ERROR_SHUTTING_DOWN

    } // if
  } // if

} // MaintenanceRestHandler::putAction


#if 0
void MaintenanceRestHandler::putJob() {
  std::vector<std::string> const& suffixes = _request->suffixes();
  std::string const& value = suffixes[0];
  uint64_t jobId = StringUtils::uint64(value);

  AsyncJobResult::Status status;
  GeneralResponse* response = _jobManager->getJobResult(jobId, status, true); //gets job and removes it form the manager

  if (status == AsyncJobResult::JOB_UNDEFINED) {
    // unknown or already fetched job
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
    return;
  }

  if (status == AsyncJobResult::JOB_PENDING) {
    // job is still pending
    resetResponse(rest::ResponseCode::NO_CONTENT);
    return;
  }

  TRI_ASSERT(status == AsyncJobResult::JOB_DONE);
  TRI_ASSERT(response != nullptr);

  // return the original response
  _response.reset(response);

  // plus a new header
  static std::string const xArango = "x-arango-async-id";
  _response->setHeaderNC(xArango, value);
}

void MaintenanceRestHandler::putJobMethod() {
  std::vector<std::string> const& suffixes = _request->suffixes();
  std::string const& value = suffixes[0];
  std::string const& method = suffixes[1];
  uint64_t jobId = StringUtils::uint64(value);

  if (method == "cancel") {
    Result status = _jobManager->cancelJob(jobId);

    // unknown or already fetched job
    if (status.fail()) {
      generateError(status);
    } else {
      VPackBuilder json;
      json.add(VPackValue(VPackValueType::Object));
      json.add("result", VPackValue(true));
      json.close();

      VPackSlice slice(json.start());
      generateResult(rest::ResponseCode::OK, slice);
    }
    return;
  } else {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
  }
}

void MaintenanceRestHandler::getJob() {
  std::vector<std::string> const& suffixes = _request->suffixes();

  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  std::string const type = suffixes[0];

  if (!type.empty() && type[0] >= '1' && type[0] <= '9') {
    getJobById(type);
  } else {
    getJobByType(type);
  }
}

void MaintenanceRestHandler::getJobById(std::string const& value) {
  uint64_t jobId = StringUtils::uint64(value);

  // numeric job id, just pull the job status and return it
  AsyncJobResult::Status status;
  TRI_ASSERT(_jobManager != nullptr);
  _jobManager->getJobResult(jobId, status, false); //just gets status

  if (status == AsyncJobResult::JOB_UNDEFINED) {
    // unknown or already fetched job
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
    return;
  }

  if (status == AsyncJobResult::JOB_PENDING) {
    // job is still pending
    resetResponse(rest::ResponseCode::NO_CONTENT);
    return;
  }

  resetResponse(rest::ResponseCode::OK);
}

void MaintenanceRestHandler::getJobByType(std::string const& type) {
  size_t count = 100;

  // extract "count" parameter
  bool found;
  std::string const& value = _request->value("count", found);

  if (found) {
    count = (size_t)StringUtils::uint64(value);
  }

  std::vector<AsyncJobResult::IdType> ids;
  if (type == "done") {
    ids = _jobManager->done(count);
  } else if (type == "pending") {
    ids = _jobManager->pending(count);
  } else {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  VPackBuilder result;
  result.openArray();
  size_t const n = ids.size();
  for (size_t i = 0; i < n; ++i) {
    result.add(VPackValue(std::to_string(ids[i])));
  }
  result.close();
  generateResult(rest::ResponseCode::OK, result.slice());
}

void MaintenanceRestHandler::deleteJob() {
  std::vector<std::string> const& suffixes = _request->suffixes();

  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  std::string const& value = suffixes[0];

  if (value == "all") {
    _jobManager->deleteJobs();
  } else if (value == "expired") {
    bool found;
    std::string const& value = _request->value("stamp", found);

    if (!found) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
      return;
    }

    double stamp = StringUtils::doubleDecimal(value);
    _jobManager->deleteExpiredJobResults(stamp);
  } else {
    uint64_t jobId = StringUtils::uint64(value);

    bool found = _jobManager->deleteJobResult(jobId);

    if (!found) {
      generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
      return;
    }
  }

  VPackBuilder json;
  json.add(VPackValue(VPackValueType::Object));
  json.add("result", VPackValue(true));
  json.close();
  VPackSlice slice(json.start());
  generateResult(rest::ResponseCode::OK, slice);
}
#endif // if 0
