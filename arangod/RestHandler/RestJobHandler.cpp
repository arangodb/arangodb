////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include "RestJobHandler.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/StringUtils.h"
#include "Basics/conversions.h"
#include "GeneralServer/AsyncJobManager.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestJobHandler::RestJobHandler(GeneralRequest* request,
                               GeneralResponse* response,
                               AsyncJobManager* jobManager)
    : RestBaseHandler(request, response), _jobManager(jobManager) {
  TRI_ASSERT(jobManager != nullptr);
}

bool RestJobHandler::isDirect() const { return true; }

RestStatus RestJobHandler::execute() {
  // extract the sub-request type
  auto const type = _request->requestType();

  if (type == rest::RequestType::GET) {
    getJob();
  } else if (type == rest::RequestType::PUT) {
    std::vector<std::string> const& suffixes = _request->suffixes();

    if (suffixes.size() == 1) {
      putJob();
    } else if (suffixes.size() == 2) {
      putJobMethod();
    } else {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    }
  } else if (type == rest::RequestType::DELETE_REQ) {
    deleteJob();
  } else {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  (int)rest::ResponseCode::METHOD_NOT_ALLOWED);
  }

  return RestStatus::DONE;
}

void RestJobHandler::putJob() {
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

void RestJobHandler::putJobMethod() {
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

void RestJobHandler::getJob() {
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

void RestJobHandler::getJobById(std::string const& value) {
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

void RestJobHandler::getJobByType(std::string const& type) {
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

void RestJobHandler::deleteJob() {
  std::vector<std::string> const& suffixes = _request->suffixes();

  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  std::string const& value = suffixes[0];

  if (value == "all") {
    _jobManager->deleteJobResults();
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
