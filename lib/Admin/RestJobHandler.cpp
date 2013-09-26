////////////////////////////////////////////////////////////////////////////////
/// @brief job control request handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestJobHandler.h"

#include "Basics/StringUtils.h"
#include "BasicsC/conversions.h"
#include "BasicsC/tri-strings.h"
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
  
const string RestJobHandler::QUEUE_NAME = "STANDARD";

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup RestServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestJobHandler::RestJobHandler (HttpRequest* request, void* data) 
  : RestBaseHandler(request) {

  _jobManager = static_cast<AsyncJobManager*>(data);    
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup RestServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestJobHandler::isDirect () {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

string const& RestJobHandler::queue () const {
  return QUEUE_NAME;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_e RestJobHandler::execute () {
  // extract the sub-request type
  HttpRequest::HttpRequestType type = _request->requestType();

  if (type == HttpRequest::HTTP_REQUEST_GET) {
    getJob();
  }
  else if (type == HttpRequest::HTTP_REQUEST_DELETE) {
    deleteJob();
  }
  else {
    generateError(HttpResponse::METHOD_NOT_ALLOWED, (int) HttpResponse::METHOD_NOT_ALLOWED);
  }

  return HANDLER_DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup RestServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief get handler
////////////////////////////////////////////////////////////////////////////////

void RestJobHandler::getJob () {
  const vector<string> suffix = _request->suffix();
    
  if (suffix.size() > 1) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  if (suffix.size() == 0) {
    return getJobList();
  }

  return getJobSingle();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the list of jobs (by type)
////////////////////////////////////////////////////////////////////////////////

void RestJobHandler::getJobList () {
  size_t maxCount = 100;

  // extract "count" parameter
  bool found;
  char const* value = _request->value("count", found);

  if (found) {
    maxCount = StringUtils::uint64(value);
  }

  // extract "type" parameter
  char const* type = _request->value("type", found);

  if (! found) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  vector<AsyncJobResult::IdType> ids;
  if (TRI_EqualString(type, "done")) {
    ids = _jobManager->done(maxCount);
  }
  else if (TRI_EqualString(type, "pending")) {
    ids = _jobManager->pending(maxCount);
  }
  else {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  TRI_json_t* json = TRI_CreateList2Json(TRI_CORE_MEM_ZONE, ids.size());

  if (json != 0) {
    for (size_t i = 0, n = ids.size(); i < n; ++i) {
      char* idString = TRI_StringUInt64(ids[i]);

      TRI_PushBack3ListJson(TRI_CORE_MEM_ZONE, json, TRI_CreateStringJson(TRI_CORE_MEM_ZONE, idString));
    }
  
    generateResult(json);
    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
  }
  else {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about a single job (by job id)
////////////////////////////////////////////////////////////////////////////////

void RestJobHandler::getJobSingle () {
  const vector<string> suffix = _request->suffix();
  const string& value = suffix[0];
  uint64_t jobId = StringUtils::uint64(value);

  AsyncJobResult::Status status;
  HttpResponse* response = _jobManager->getJobResult(jobId, status);

  if (status == AsyncJobResult::JOB_UNDEFINED) {
    generateError(HttpResponse::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
    return;
  }
  
  if (status == AsyncJobResult::JOB_PENDING) {
    // TODO: signal "job not ready"
    generateError(HttpResponse::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
    return;
  }

  assert(status == AsyncJobResult::JOB_DONE);

  // delete our own response
  if (_response != 0) {
    delete _response;
  }

  // return the original response
  _response = response; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a result by id
////////////////////////////////////////////////////////////////////////////////

void RestJobHandler::deleteJob () {
  const vector<string> suffix = _request->suffix();

  if (suffix.size() != 1) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  const string& value = suffix[0];
  uint64_t jobId = StringUtils::uint64(value);

  bool found = _jobManager->deleteJobResult(jobId);
  
  if (! found) {
    generateError(HttpResponse::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
    return;
  }

  TRI_json_t* json = TRI_CreateArrayJson(TRI_CORE_MEM_ZONE);

  if (json != 0) {
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "result", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, true));
  
    generateResult(json);
    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
  }
  else {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
