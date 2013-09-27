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
  else if (type == HttpRequest::HTTP_REQUEST_PUT) {
    putJob();
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
/// @brief fetches a job result and removes it from the queue
///
/// @RESTHEADER{PUT /_api/job/`job-id`,Returns the result of an async job}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{job-id,string,required}
/// The async job id.
/// 
/// @RESTDESCRIPTION
/// Returns the result of an async job identified by `job-id`.
/// If the async job result is present on the server, the result will be removed
/// from the list of result. That means this method can be called for each
/// `job-id` once.
///
/// The method will return the original job result's headers and body, plus
/// the additional HTTP header `x-arango-async-job-id`. If this header is 
/// present, then the job was found and the response contains the original job's
/// result. If the header is not present, the job was not found and the response
/// contains status information from the job amanger.
///
/// @RESTRETURNCODES
///
/// Any HTTP status code might be returned by this method. To tell the original
/// job response from a job manager response apart, check for the HTTP header
/// `x-arango-async-id`. If it is present, the response contains the original
/// job's result. Otherwise the response is from the job manager.
///
/// @RESTRETURNCODE{400}
/// is returned if no `job-id` was specified in the request. In this case, no
/// `x-arango-async-id` HTTP header will be returned.
///
/// @RESTRETURNCODE{404}
/// is returned if the job was not found or already deleted or fetched from the 
/// job result list. In this case, no `x-arango-async-id` HTTP header will be 
/// returned.
///
/// @EXAMPLES
///
/// Not providing a job-id:
///
/// @EXAMPLE_ARANGOSH_RUN{RestJobHandlerPutNone}
///     var url = "/_api/job/";
/// 
///     var response = logCurlRequest('PUT', url, "");
/// 
///     assert(response.code === 400);
///     logRawResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Providing a job-id for a non-existing job:
///
/// @EXAMPLE_ARANGOSH_RUN{RestJobHandlerPutInvalid}
///     var url = "/_api/job/foobar";
/// 
///     var response = logCurlRequest('PUT', url, "");
/// 
///     assert(response.code === 404);
///     logRawResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Fetching the result of an HTTP GET job:
///
/// @EXAMPLE_ARANGOSH_RUN{RestJobHandlerPutGet}
///     var url = "/_api/version";
/// 
///     var response = logCurlRequest('GET', url, "", { "x-arango-async": "store" });
/// 
///     assert(response.code === 202);
///     logRawResponse(response);
///
///     var id = response.headers['x-arango-async-id'];
///     require("internal").wait(1);
///
///     response = logCurlRequest('PUT', "/_api/job/" + id, "");
///     assert(response.code === 200);
///     assert(response.headers['x-arango-async-id'] === id);
///     logRawResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Fetching the result of an HTTP POST job that failed:
///
/// @EXAMPLE_ARANGOSH_RUN{RestJobHandlerPutFail}
///     var url = "/_api/collection";
/// 
///     var response = logCurlRequest('POST', url, '{"name":" this name is invalid "}', { "x-arango-async": "store" });
/// 
///     assert(response.code === 202);
///     logRawResponse(response);
///
///     var id = response.headers['x-arango-async-id'];
///     require("internal").wait(1);
///
///     response = logCurlRequest('PUT', "/_api/job/" + id, "");
///     assert(response.code === 400);
///     assert(response.headers['x-arango-async-id'] === id);
///     logRawResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///////////////////////////////////////////////////////////////////////////////

void RestJobHandler::putJob () {
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
  assert(response != 0);

  // delete our own response
  if (_response != 0) {
    delete _response;
  }

  // return the original response
  _response = response; 
  // plus a new header
  _response->setHeader("x-arango-async-id", value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the ids of jobs by status
///
/// @RESTHEADER{GET /_api/job/`type`,Returns the list of job ids by status}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{type,string,required}
/// The type of jobs to return. The type can be either `done` or `pending`. 
/// Setting the type to `done` will make the method return the ids of already
/// completed async jobs for which results can be fetched.
/// Setting the type to `pending` will return the ids of not yet finished
/// async jobs.
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{count,number,optional}
/// The maximum number of ids to return per call. If not specified, a server-defined
/// maximum value will be used.
/// 
/// @RESTDESCRIPTION
/// Returns the list of ids of async jobs with a specific status (either done
/// or pending). The list can be used by the client to get an overview of the
/// job system status and to retrieve completed job results later.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned if the list can be compiled successfully. Note: the list might
/// be empty.
///
/// @RESTRETURNCODE{400}
/// is returned if `type` is not specified or has an invalid value.
///
/// @EXAMPLES
///
/// Fetching the list of done jobs:
///
/// @EXAMPLE_ARANGOSH_RUN{RestJobHandlerGetDone}
///     var url = "/_api/version";
/// 
///     var response = logCurlRequest('GET', url, "", { "x-arango-async": "store" });
/// 
///     assert(response.code === 202);
///     logRawResponse(response);
///
///     require("internal").wait(1);
///
///     response = logCurlRequest('GET', "/_api/job/done");
///     assert(response.code === 200);
///     logJsonResponse(response);
///
///     response = curlRequest('DELETE', "/_api/job/all");
///     assert(response.code === 200);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Fetching the list of pending jobs:
///
/// @EXAMPLE_ARANGOSH_RUN{RestJobHandlerGetPending}
///     var url = "/_api/version";
/// 
///     var response = logCurlRequest('GET', url, "", { "x-arango-async": "store" });
/// 
///     assert(response.code === 202);
///     logRawResponse(response);
///
///     var id = response.headers['x-arango-async-id'];
///     require("internal").wait(1);
///
///     response = logCurlRequest('GET', "/_api/job/done");
///     assert(response.code === 200);
///     logJsonResponse(response);
///
///     response = curlRequest('DELETE', "/_api/job/all");
///     assert(response.code === 200);
/// @END_EXAMPLE_ARANGOSH_RUN
///////////////////////////////////////////////////////////////////////////////

void RestJobHandler::getJob () {
  const vector<string> suffix = _request->suffix();
    
  if (suffix.size() != 1) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  const string type = suffix[0];
  size_t count = 100;

  // extract "count" parameter
  bool found;
  char const* value = _request->value("count", found);

  if (found) {
    count = StringUtils::uint64(value);
  }

  vector<AsyncJobResult::IdType> ids;
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
/// @brief deletes an async job result
///
/// @RESTHEADER{DELETE /_api/job/`type`,Deletes the result of async jobs}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{type,string,required}
/// The type of jobs to delete. `type` can be:
///
/// - `all`: deletes all jobs results. Currently executing or queued async jobs 
///   will not be stopped by this call.
///
/// - `expired`: deletes expired results. To determine the expiration status of
///   a result, pass the `stamp` URL parameter. `stamp` needs to be a UNIX
///   timestamp, and all async job results created at a lower timestamp will be
///   deleted.
///
/// - an actual job-id: in this case, the call will remove the result of the
///   specified async job. If the job is currently executing or queued, it will
///   not be aborted.
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{stamp,number,optional}
/// A UNIX timestamp specifying the expiration threshold when type is `expired`.
/// 
/// @RESTDESCRIPTION
/// Deletes either all job results, expired job results, or the result of a 
/// specific job. Clients can use this method to perform an eventual garbage 
/// collection of job results.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned if the deletion operation was carried out successfully. This code
/// will also be returned if no results were deleted.
///
/// @RESTRETURNCODE{400}
/// is returned if `type` is not specified or has an invalid value.
///
/// @RESTRETURNCODE{404}
/// is returned if `type` is a job-id but no async job with the specified id was
/// found.
///
/// @EXAMPLES
///
/// Deleting all jobs:
///
/// @EXAMPLE_ARANGOSH_RUN{RestJobHandlerDeleteAll}
///     var url = "/_api/version";
/// 
///     var response = logCurlRequest('GET', url, "", { "x-arango-async": "store" });
/// 
///     assert(response.code === 202);
///     logRawResponse(response);
///
///     require("internal").wait(1);
///
///     response = logCurlRequest('DELETE', "/_api/job/all");
///     assert(response.code === 200);
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Deleting expired jobs:
///
/// @EXAMPLE_ARANGOSH_RUN{RestJobHandlerDeleteExpired}
///     var url = "/_api/version";
/// 
///     var response = logCurlRequest('GET', url, "", { "x-arango-async": "store" });
/// 
///     assert(response.code === 202);
///     logRawResponse(response);
///
///     require("internal").wait(1);
///     var stamp = require("internal").time() + 360;
///
///     response = logCurlRequest('DELETE', "/_api/job/expired?stamp=" + stamp);
///     assert(response.code === 200);
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
///
/// Fetching the list of pending jobs:
///
/// @EXAMPLE_ARANGOSH_RUN{RestJobHandlerGetPending}
///     var url = "/_api/version";
/// 
///     var response = logCurlRequest('GET', url, "", { "x-arango-async": "store" });
/// 
///     assert(response.code === 202);
///     logRawResponse(response);
///
///     var id = response.headers['x-arango-async-id'];
///     require("internal").wait(1);
///
///     response = logCurlRequest('DELETE', "/_api/job/" + id);
///     assert(response.code === 200);
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///////////////////////////////////////////////////////////////////////////////

void RestJobHandler::deleteJob () {
  const vector<string> suffix = _request->suffix();

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
