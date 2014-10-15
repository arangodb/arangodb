////////////////////////////////////////////////////////////////////////////////
/// @brief job control request handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2010-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestJobHandler.h"

#include "Basics/StringUtils.h"
#include "Basics/conversions.h"
#include "Basics/tri-strings.h"
#include "Dispatcher/Dispatcher.h"
#include "HttpServer/AsyncJobManager.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"

using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::admin;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                                  public constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief name of the queue
////////////////////////////////////////////////////////////////////////////////

const std::string RestJobHandler::QUEUE_NAME = "STANDARD";

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestJobHandler::RestJobHandler (HttpRequest* request,
                                pair<Dispatcher*, AsyncJobManager*>* data)
  : RestBaseHandler(request),
    _dispatcher(data->first),
    _jobManager(data->second) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestJobHandler::isDirect () {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

std::string const& RestJobHandler::queue () const {
  return QUEUE_NAME;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_t RestJobHandler::execute () {

  // extract the sub-request type
  HttpRequest::HttpRequestType type = _request->requestType();

  if (type == HttpRequest::HTTP_REQUEST_GET) {
    getJob();
  }
  else if (type == HttpRequest::HTTP_REQUEST_PUT) {
    const vector<string>& suffix = _request->suffix();

    if (suffix.size() == 1) {
      putJob();
    }
    else if (suffix.size() == 2) {
      putJobMethod();
    }
    else {
      generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    }
  }
  else if (type == HttpRequest::HTTP_REQUEST_DELETE) {
    deleteJob();
  }
  else {
    generateError(HttpResponse::METHOD_NOT_ALLOWED, (int) HttpResponse::METHOD_NOT_ALLOWED);
  }

  return status_t(HANDLER_DONE);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief fetches a job result and removes it from the queue
////////////////////////////////////////////////////////////////////////////////

void RestJobHandler::putJob () {
  std::vector<string> const& suffix = _request->suffix();
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
    _response = createResponse(HttpResponse::NO_CONTENT);
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
  _response->setHeader("x-arango-async-id", value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cancels an async job
////////////////////////////////////////////////////////////////////////////////

void RestJobHandler::putJobMethod () {
  std::vector<std::string> const& suffix = _request->suffix();
  std::string const& value = suffix[0];
  std::string const& method = suffix[1];
  uint64_t jobId = StringUtils::uint64(value);

  if (method == "cancel") {
    bool status = _dispatcher->cancelJob(jobId);

    // unknown or already fetched job
    if (! status) {
      generateError(HttpResponse::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
    }
    else {
      TRI_json_t* json = TRI_CreateArrayJson(TRI_CORE_MEM_ZONE);

      TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "result", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, true));
      generateResult(json);
      TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
    }
    return;
  }
  else {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief trampoline function for HTTP GET requests
////////////////////////////////////////////////////////////////////////////////

void RestJobHandler::getJob () {
  std::vector<std::string> const suffix = _request->suffix();

  if (suffix.size() != 1) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  std::string const type = suffix[0];

  if (! type.empty() && type[0] >= '1' && type[0] <= '9') {
    getJobId(type);
  }
  else {
    getJobType(type);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Returns the status of a specific job
////////////////////////////////////////////////////////////////////////////////

void RestJobHandler::getJobId (std::string const& value) {
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
    _response = createResponse(HttpResponse::NO_CONTENT);
    return;
  }

  _response = createResponse(HttpResponse::OK);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Returns the ids of job results with a specific status
////////////////////////////////////////////////////////////////////////////////

void RestJobHandler::getJobType (std::string const& type) {
  size_t count = 100;

  // extract "count" parameter
  bool found;
  char const* value = _request->value("count", found);

  if (found) {
    count = (size_t) StringUtils::uint64(value);
  }

  std::vector<AsyncJobResult::IdType> ids;
  try {
    if (type == "done") {
      ids = _jobManager->done(count);
    }
    else if (type == "pending") {
      ids = _jobManager->pending(count);
    }
    else {
      generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
      return;
    }
  }
  catch (...) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_HTTP_SERVER_ERROR);
    return;
  }

  TRI_json_t* json = TRI_CreateList2Json(TRI_CORE_MEM_ZONE, ids.size());

  if (json != nullptr) {
    size_t const n = ids.size();
    for (size_t i = 0; i < n; ++i) {
      char* idString = TRI_StringUInt64(ids[i]);

      if (idString != nullptr) {
        TRI_PushBack3ListJson(TRI_CORE_MEM_ZONE, json, TRI_CreateStringJson(TRI_CORE_MEM_ZONE, idString));
      }
    }

    generateResult(json);
    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
  }
  else {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes an async job result
////////////////////////////////////////////////////////////////////////////////

void RestJobHandler::deleteJob () {
  std::vector<std::string> const suffix = _request->suffix();

  if (suffix.size() != 1) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  const string& value = suffix[0];

  if (value == "all") {
    _jobManager->deleteJobResults();
  }
  else if (value == "expired") {
    bool found;
    char const* value = _request->value("stamp", found);

    if (! found) {
      generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
      return;
    }

    double stamp = StringUtils::doubleDecimal(value);
    _jobManager->deleteExpiredJobResults(stamp);
  }
  else {
    uint64_t jobId = StringUtils::uint64(value);

    bool found = _jobManager->deleteJobResult(jobId);

    if (! found) {
      generateError(HttpResponse::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
      return;
    }
  }

  TRI_json_t* json = TRI_CreateArrayJson(TRI_CORE_MEM_ZONE);

  if (json != nullptr) {
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "result", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, true));

    generateResult(json);
    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
  }
  else {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
