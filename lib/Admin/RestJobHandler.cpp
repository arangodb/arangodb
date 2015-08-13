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

bool RestJobHandler::isDirect () const {
  return true;
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
/// @startDocuBlock JSF_job_fetch_result
/// @brief fetches a job result and removes it from the queue
/// 
/// @RESTHEADER{PUT /_api/job/job-id, Return result of an async job}
/// 
/// @RESTURLPARAMS
/// 
/// @RESTURLPARAM{job-id,string,required}
/// The async job id.
/// 
/// @RESTDESCRIPTION
/// Returns the result of an async job identified by job-id. If the async job 
/// result is present on the server, the result will be removed from the list of
/// result. That means this method can be called for each job-id once.
/// The method will return the original job result's headers and body, plus the 
/// additional HTTP header x-arango-async-job-id. If this header is present, then 
/// the job was found and the response contains the original job's result. If 
/// the header is not present, the job was not found and the response contains 
/// status information from the job manager.
/// 
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{204}
/// is returned if the job requested via job-id is still in the queue of pending 
/// (or not yet finished) jobs. In this case, no x-arango-async-id HTTP header 
/// will be returned.
/// 
/// @RESTRETURNCODE{400}
/// is returned if no job-id was specified in the request. In this case, 
/// no x-arango-async-id HTTP header will be returned.
/// 
/// @RESTRETURNCODE{404}
/// is returned if the job was not found or already deleted or fetched from 
/// the job result list. In this case, no x-arango-async-id HTTP header will 
/// be returned.
/// 
/// @EXAMPLES
/// Not providing a job-id:
/// 
/// ```js
/// unix> curl -X PUT --dump - http://localhost:8529/_api/job/
/// 
/// HTTP/1.1 400 Bad Request
/// content-type: application/json; charset=utf-8
/// 
/// {"error":true,"errorMessage":"bad parameter","code":400,"errorNum":400}
/// ```
/// 
/// Providing a job-id for a non-existing job:
/// 
/// ```js
/// unix> curl -X PUT --dump - http://localhost:8529/_api/job/foobar
/// 
/// HTTP/1.1 404 Not Found
/// content-type: application/json; charset=utf-8
/// 
/// {"error":true,"errorMessage":"not found","code":404,"errorNum":404}
/// ```
/// 
/// Fetching the result of an HTTP GET job:
/// 
/// ```js
/// unix> curl --header 'x-arango-async: store' --dump - http://localhost:8529/_api/version
/// 
/// HTTP/1.1 202 Accepted
/// content-type: text/plain; charset=utf-8
/// x-arango-async-id: 265413601
/// 
/// unix> curl -X PUT --dump - http://localhost:8529/_api/job/265413601
/// 
/// HTTP/1.1 200 OK
/// content-type: application/json; charset=utf-8
/// x-arango-async-id: 265413601
/// 
/// {"server":"arango","version":"2.1.0"}
/// ```
/// 
/// Fetching the result of an HTTP POST job that failed:
/// 
/// ```js
/// unix> curl -X POST --header 'x-arango-async: store' --data-binary @- --dump - http://localhost:8529/_api/collection
/// {"name":" this name is invalid "}
/// 
/// HTTP/1.1 202 Accepted
/// content-type: text/plain; charset=utf-8
/// x-arango-async-id: 265479137
/// 
/// unix> curl -X PUT --dump - http://localhost:8529/_api/job/265479137
/// 
/// HTTP/1.1 400 Bad Request
/// content-type: application/json; charset=utf-8
/// x-arango-async-id: 265479137
/// 
/// {"error":true,"code":400,"errorNum":1208,"errorMessage":"cannot create collection: illegal name"}
/// ```
/// @endDocuBlock
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
/// @startDocuBlock JSF_job_cancel
/// @brief cancels an async job
/// 
/// @RESTHEADER{PUT /_api/job/job-id/cancel, Cancel async job}
/// 
/// @RESTURLPARAMS
/// 
/// @RESTURLPARAM{job-id,string,required}
/// The async job id.
/// 
/// @RESTDESCRIPTION
/// Cancels the currently running job identified by job-id. Note that it still 
/// might take some time to actually cancel the running async job.
/// 
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// cancel has been initiated.
/// 
/// @RESTRETURNCODE{400}
/// is returned if no job-id was specified in the request. In this case, 
/// no x-arango-async-id HTTP header will be returned.
/// 
/// @RESTRETURNCODE{404}
/// is returned if the job was not found or already deleted or fetched from 
/// the job result list. In this case, no x-arango-async-id HTTP header will 
/// be returned.
/// 
/// @EXAMPLES
/// 
/// ```js
/// unix> curl -X POST --header 'x-arango-async: store' --data-binary @- --dump - http://localhost:8529/_api/cursor
/// {"query": "FOR i IN 1..10 FOR j IN 1..10 LET x = sleep(1.0) FILTER i == 5 && j == 5 RETURN 42"}
/// 
/// HTTP/1.1 202 Accepted
/// content-type: text/plain; charset=utf-8
/// x-arango-async-id: 268952545
/// 
/// unix> curl --dump - http://localhost:8529/_api/job/pending
/// 
/// HTTP/1.1 200 OK
/// content-type: application/json; charset=utf-8
/// 
/// ["268952545"]
/// 
/// unix> curl -X PUT --dump - http://localhost:8529/_api/job/268952545/cancel
/// 
/// HTTP/1.1 200 OK
/// content-type: application/json; charset=utf-8
/// 
/// {"result":true}
/// 
/// unix> curl --dump - http://localhost:8529/_api/job/pending
/// 
/// HTTP/1.1 200 OK
/// content-type: application/json; charset=utf-8
/// 
/// ["268952545"]
/// ```
/// @endDocuBlock
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
      TRI_json_t* json = TRI_CreateObjectJson(TRI_CORE_MEM_ZONE);

      TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, json, "result", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, true));
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
/// @startDocuBlock JSF_job_getStatusById
/// @brief Returns the status of a specific job
/// @RESTHEADER{GET /_api/job/job-id, Returns async job}
/// 
/// @RESTURLPARAMS
/// 
/// @RESTURLPARAM{job-id,string,required}
/// The async job id.
/// 
/// @RESTDESCRIPTION
/// Returns the processing status of the specified job. The processing status can be 
/// determined by peeking into the HTTP response code of the response.
/// 
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// is returned if the job requested via job-id has been executed 
/// and its result is ready to fetch.
/// 
/// @RESTRETURNCODE{204}
/// is returned if the job requested via job-id is still in the queue of pending 
/// (or not yet finished) jobs.
/// 
/// @RESTRETURNCODE{404}
/// is returned if the job was not found or already deleted or fetched from the job result list.
/// 
/// @EXAMPLES
/// 
/// Querying the status of a done job:
/// 
/// ```js
/// unix> curl --header 'x-arango-async: store' --dump - http://localhost:8529/_api/version
/// 
/// HTTP/1.1 202 Accepted
/// content-type: text/plain; charset=utf-8
/// x-arango-async-id: 270328801
/// 
/// unix> curl --dump - http://localhost:8529/_api/job/270328801
/// 
/// HTTP/1.1 200 OK
/// content-type: text/plain; charset=utf-8
/// 
/// Querying the status of a pending job:
/// 
/// unix> curl --header 'x-arango-async: store' --dump - http://localhost:8529/_admin/sleep?duration=3
/// 
/// HTTP/1.1 202 Accepted
/// content-type: text/plain; charset=utf-8
/// x-arango-async-id: 270394337
/// 
/// unix> curl --dump - http://localhost:8529/_api/job/270394337
/// 
/// HTTP/1.1 204 No Content
/// content-type: text/plain; charset=utf-8
/// ```
/// @endDocuBlock
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
/// @startDocuBlock JSF_job_getByType
/// @brief Returns the ids of job results with a specific status
/// 
/// @RESTHEADER{GET /_api/job/type, Returns list of async job}
/// 
/// @RESTURLPARAMS
/// 
/// @RESTURLPARAM{type,string,required}
/// The type of jobs to return. The type can be either done or pending. Setting 
/// the type to done will make the method return the ids of already completed async 
/// jobs for which results can be fetched. Setting the type to pending will return 
/// the ids of not yet finished async jobs.
/// 
/// @RESTQUERYPARAMS
/// 
/// @RESTPARAM{count, number, optional}
/// 
/// The maximum number of ids to return per call. If not specified, a 
/// server-defined maximum value will be used.
/// 
/// @RESTDESCRIPTION
/// Returns the list of ids of async jobs with a specific status (either done or pending). 
/// The list can be used by the client to get an overview of the job system status and 
/// to retrieve completed job results later.
/// 
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// is returned if the list can be compiled successfully. Note: the list might be empty.
/// 
/// @RESTRETURNCODE{400}
/// is returned if type is not specified or has an invalid value.
/// 
/// @EXAMPLES
/// 
/// Fetching the list of done jobs:
/// 
/// ```js
/// unix> curl --header 'x-arango-async: store' --dump - http://localhost:8529/_api/version
/// 
/// HTTP/1.1 202 Accepted
/// content-type: text/plain; charset=utf-8
/// x-arango-async-id: 270459873
/// 
/// unix> curl --dump - http://localhost:8529/_api/job/done
/// 
/// HTTP/1.1 200 OK
/// content-type: application/json; charset=utf-8
/// 
/// [ 
///   "270459873" 
/// ]
/// ```
/// 
/// Fetching the list of pending jobs:
/// 
/// ```js
/// unix> curl --header 'x-arango-async: store' --dump - http://localhost:8529/_api/version
/// 
/// HTTP/1.1 202 Accepted
/// content-type: text/plain; charset=utf-8
/// x-arango-async-id: 270525409
/// 
/// unix> curl --dump - http://localhost:8529/_api/job/pending
/// 
/// HTTP/1.1 200 OK
/// content-type: application/json; charset=utf-8
/// 
/// [ ]
/// ```
/// /// @endDocuBlock
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

  TRI_json_t* json = TRI_CreateArrayJson(TRI_CORE_MEM_ZONE, ids.size());

  if (json != nullptr) {
    size_t const n = ids.size();
    for (size_t i = 0; i < n; ++i) {
      char* idString = TRI_StringUInt64(ids[i]);

      if (idString != nullptr) {
        TRI_PushBack3ArrayJson(TRI_CORE_MEM_ZONE, json, TRI_CreateStringJson(TRI_CORE_MEM_ZONE, idString, strlen(idString)));
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
/// @startDocuBlock JSF_job_delete
/// @brief deletes an async job result
/// 
/// @RESTHEADER{DELETE /_api/job/type, Deletes async job}
/// 
/// @RESTURLPARAMS
/// 
/// @RESTURLPARAM{type,string,required}
/// The type of jobs to delete. type can be:
/// * *all*: Deletes all jobs results. Currently executing or queued async 
///   jobs will not be stopped by this call.
/// * *expired*: Deletes expired results. To determine the expiration status of a 
///   result, pass the stamp URL parameter. stamp needs to be a UNIX timestamp, 
///   and all async job results created at a lower timestamp will be deleted.
/// * *an actual job-id*: In this case, the call will remove the result of the 
///   specified async job. If the job is currently executing or queued, it will not be aborted.
/// 
/// @RESTQUERYPARAMS
/// 
/// @RESTPARAM{stamp, number, optional}
/// 
/// A UNIX timestamp specifying the expiration threshold when type is expired.
/// 
/// @RESTDESCRIPTION
/// Deletes either all job results, expired job results, or the result of a specific job. 
/// Clients can use this method to perform an eventual garbage collection of job results.
/// 
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// is returned if the deletion operation was carried out successfully. 
/// This code will also be returned if no results were deleted.
/// 
/// @RESTRETURNCODE{400}
/// is returned if type is not specified or has an invalid value.
/// 
/// @RESTRETURNCODE{404}
/// is returned if type is a job-id but no async job with the specified id was found.
/// 
/// @EXAMPLES
/// 
/// Deleting all jobs:
/// 
/// ```js
/// unix> curl --header 'x-arango-async: store' --dump - http://localhost:8529/_api/version
/// 
/// HTTP/1.1 202 Accepted
/// content-type: text/plain; charset=utf-8
/// x-arango-async-id: 270132193
/// 
/// unix> curl -X DELETE --dump - http://localhost:8529/_api/job/all
/// 
/// HTTP/1.1 200 OK
/// content-type: application/json; charset=utf-8
/// 
/// { 
///   "result" : true 
/// }
/// ```
/// 
/// Deleting expired jobs:
/// 
/// ```js
/// unix> curl --header 'x-arango-async: store' --dump - http://localhost:8529/_api/version
/// 
/// HTTP/1.1 202 Accepted
/// content-type: text/plain; charset=utf-8
/// x-arango-async-id: 270197729
/// 
/// unix> curl -X DELETE --dump - http://localhost:8529/_api/job/expired?stamp=1401376184
/// 
/// HTTP/1.1 200 OK
/// content-type: application/json; charset=utf-8
/// 
/// { 
///   "result" : true 
/// }
/// ```
/// 
/// Deleting the result of a specific job:
/// 
/// ```js
/// unix> curl --header 'x-arango-async: store' --dump - http://localhost:8529/_api/version
/// 
/// HTTP/1.1 202 Accepted
/// content-type: text/plain; charset=utf-8
/// x-arango-async-id: 270263265
/// 
/// unix> curl -X DELETE --dump - http://localhost:8529/_api/job/270263265
/// 
/// HTTP/1.1 200 OK
/// content-type: application/json; charset=utf-8
/// 
/// { 
///   "result" : true 
/// }
/// ```
/// 
/// Deleting the result of a non-existing job:
/// 
/// ```js
/// unix> curl -X DELETE --dump - http://localhost:8529/_api/job/foobar
/// 
/// HTTP/1.1 404 Not Found
/// content-type: application/json; charset=utf-8
/// 
/// { 
///   "error" : true, 
///   "errorMessage" : "not found", 
///   "code" : 404, 
///   "errorNum" : 404 
/// }
/// ```
/// @endDocuBlock
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

  TRI_json_t* json = TRI_CreateObjectJson(TRI_CORE_MEM_ZONE);

  if (json != nullptr) {
    TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, json, "result", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, true));

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
