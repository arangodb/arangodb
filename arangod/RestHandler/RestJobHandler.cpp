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
#include "Basics/conversions.h"
#include "Basics/StringUtils.h"
#include "Dispatcher/Dispatcher.h"
#include "HttpServer/AsyncJobManager.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestJobHandler::RestJobHandler(HttpRequest* request,
                               std::pair<Dispatcher*, AsyncJobManager*>* data)
    : RestBaseHandler(request),
      _dispatcher(data->first),
      _jobManager(data->second) {}

bool RestJobHandler::isDirect() const { return true; }

HttpHandler::status_t RestJobHandler::execute() {
  // extract the sub-request type
  HttpRequest::HttpRequestType type = _request->requestType();

  if (type == HttpRequest::HTTP_REQUEST_GET) {
    getJob();
  } else if (type == HttpRequest::HTTP_REQUEST_PUT) {
    std::vector<std::string> const& suffix = _request->suffix();

    if (suffix.size() == 1) {
      putJob();
    } else if (suffix.size() == 2) {
      putJobMethod();
    } else {
      generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    }
  } else if (type == HttpRequest::HTTP_REQUEST_DELETE) {
    deleteJob();
  } else {
    generateError(HttpResponse::METHOD_NOT_ALLOWED,
                  (int)HttpResponse::METHOD_NOT_ALLOWED);
  }

  return status_t(HANDLER_DONE);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_job_fetch_result
////////////////////////////////////////////////////////////////////////////////

void RestJobHandler::putJob() {
  std::vector<std::string> const& suffix = _request->suffix();
  std::string const& value = suffix[0];
  uint64_t jobId = StringUtils::uint64(value);

  AsyncJobResult::Status status;
  HttpResponse* response = _jobManager->getJobResult(jobId, status, true);

  if (status == AsyncJobResult::JOB_UNDEFINED) {
    // unknown or already fetched job
    generateError(HttpResponse::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
    return;
  }

  if (status == AsyncJobResult::JOB_PENDING) {
    // job is still pending
    createResponse(HttpResponse::NO_CONTENT);
    return;
  }

  TRI_ASSERT(status == AsyncJobResult::JOB_DONE);
  TRI_ASSERT(response != nullptr);

  // delete our own response
  if (_response != nullptr) {
    delete _response;
  }

  // return the original response
  _response = response;

  // plus a new header
  _response->setHeader(TRI_CHAR_LENGTH_PAIR("x-arango-async-id"), value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_job_cancel
////////////////////////////////////////////////////////////////////////////////

void RestJobHandler::putJobMethod() {
  std::vector<std::string> const& suffix = _request->suffix();
  std::string const& value = suffix[0];
  std::string const& method = suffix[1];
  uint64_t jobId = StringUtils::uint64(value);

  if (method == "cancel") {
    bool status = _dispatcher->cancelJob(jobId);

    // unknown or already fetched job
    if (!status) {
      generateError(HttpResponse::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
    } else {
      try {
        VPackBuilder json;
        json.add(VPackValue(VPackValueType::Object));
        json.add("result", VPackValue(true));
        json.close();

        VPackSlice slice(json.start());
        generateResult(slice);
      } catch (...) {
        // Ignore the error
      }
    }
    return;
  } else {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief trampoline function for HTTP GET requests
////////////////////////////////////////////////////////////////////////////////

void RestJobHandler::getJob() {
  std::vector<std::string> const suffix = _request->suffix();

  if (suffix.size() != 1) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  std::string const type = suffix[0];

  if (!type.empty() && type[0] >= '1' && type[0] <= '9') {
    getJobById(type);
  } else {
    getJobByType(type);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_job_getStatusById
////////////////////////////////////////////////////////////////////////////////

void RestJobHandler::getJobById(std::string const& value) {
  uint64_t jobId = StringUtils::uint64(value);

  // numeric job id, just pull the job status and return it
  AsyncJobResult::Status status;
  _jobManager->getJobResult(jobId, status, false);

  if (status == AsyncJobResult::JOB_UNDEFINED) {
    // unknown or already fetched job
    generateError(HttpResponse::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
    return;
  }

  if (status == AsyncJobResult::JOB_PENDING) {
    // job is still pending
    createResponse(HttpResponse::NO_CONTENT);
    return;
  }

  createResponse(HttpResponse::OK);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_job_getByType
////////////////////////////////////////////////////////////////////////////////

void RestJobHandler::getJobByType(std::string const& type) {
  size_t count = 100;

  // extract "count" parameter
  bool found;
  char const* value = _request->value("count", found);

  if (found) {
    count = (size_t)StringUtils::uint64(value);
  }

  std::vector<AsyncJobResult::IdType> ids;
  try {
    if (type == "done") {
      ids = _jobManager->done(count);
    } else if (type == "pending") {
      ids = _jobManager->pending(count);
    } else {
      generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
      return;
    }
  } catch (...) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_HTTP_SERVER_ERROR);
    return;
  }

  try {
    VPackBuilder json;
    json.add(VPackValue(VPackValueType::Array));
    size_t const n = ids.size();
    for (size_t i = 0; i < n; ++i) {
      char* idString = TRI_StringUInt64(ids[i]);
      if (idString != nullptr) {
        json.add(VPackValue(idString));
      }
    }
    json.close();
    VPackSlice slice(json.start());
    generateResult(slice);
  } catch (...) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_job_delete
////////////////////////////////////////////////////////////////////////////////

void RestJobHandler::deleteJob() {
  std::vector<std::string> const suffix = _request->suffix();

  if (suffix.size() != 1) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  std::string const& value = suffix[0];

  if (value == "all") {
    _jobManager->deleteJobResults();
  } else if (value == "expired") {
    bool found;
    char const* value = _request->value("stamp", found);

    if (!found) {
      generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
      return;
    }

    double stamp = StringUtils::doubleDecimal(value);
    _jobManager->deleteExpiredJobResults(stamp);
  } else {
    uint64_t jobId = StringUtils::uint64(value);

    bool found = _jobManager->deleteJobResult(jobId);

    if (!found) {
      generateError(HttpResponse::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
      return;
    }
  }

  try {
    VPackBuilder json;
    json.add(VPackValue(VPackValueType::Object));
    json.add("result", VPackValue(true));
    json.close();
    VPackSlice slice(json.start());
    generateResult(slice);
  } catch (...) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
  }
}
