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
  if (type == HttpRequest::HTTP_REQUEST_DELETE) {
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
/// @brief returns a job result by id
////////////////////////////////////////////////////////////////////////////////

void RestJobHandler::getJob () {
  const vector<string> suffix = _request->suffix();

  if (suffix.size() != 1) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    return;
  }
  
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
  }

  generateResult(json);
  TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
