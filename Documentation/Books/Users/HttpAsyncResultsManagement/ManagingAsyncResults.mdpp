!CHAPTER Managing Async Results via HTTP

@COMMENT{######################################################################}
@anchor HttpJobPut
@RESTHEADER{PUT /_api/job/`job-id`,Returns the result of an async job}

@RESTURLPARAMETERS

@RESTURLPARAM{job-id,string,required}
The async job id.

@RESTDESCRIPTION
Returns the result of an async job identified by `job-id`.  If the async job
result is present on the server, the result will be removed from the list of
result. That means this method can be called for each `job-id` once.

The method will return the original job result's headers and body, plus the
additional HTTP header `x-arango-async-job-id`. If this header is present, then
the job was found and the response contains the original job's result. If the
header is not present, the job was not found and the response contains status
information from the job manager.

@RESTRETURNCODES

Any HTTP status code might be returned by this method. To tell the original job
response from a job manager response apart, check for the HTTP header
`x-arango-async-id`. If it is present, the response contains the original job's
result. Otherwise the response is from the job manager.

@RESTRETURNCODE{204}
is returned if the job requested via `job-id` is still in the queue of pending
(or not yet finished) jobs. In this case, no `x-arango-async-id` HTTP header
will be returned.

@RESTRETURNCODE{400}
is returned if no `job-id` was specified in the request. In this case, no
`x-arango-async-id` HTTP header will be returned.

@RESTRETURNCODE{404}
is returned if the job was not found or already deleted or fetched from the job
result list. In this case, no `x-arango-async-id` HTTP header will be returned.

@EXAMPLES

Not providing a job-id:

@EXAMPLE_ARANGOSH_RUN{RestJobHandlerPutNone}
    var url = "/_api/job/";

    var response = logCurlRequest('PUT', url, "");

    assert(response.code === 400);
    logRawResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Providing a job-id for a non-existing job:

@EXAMPLE_ARANGOSH_RUN{RestJobHandlerPutInvalid}
    var url = "/_api/job/foobar";

    var response = logCurlRequest('PUT', url, "");

    assert(response.code === 404);
    logRawResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Fetching the result of an HTTP GET job:

@EXAMPLE_ARANGOSH_RUN{RestJobHandlerPutGet}
    var url = "/_api/version";

    var response = logCurlRequest('GET', url, "", { "x-arango-async": "store" });

    assert(response.code === 202);
    logRawResponse(response);

    var id = response.headers['x-arango-async-id'];
    require("internal").wait(1);

    response = logCurlRequest('PUT', "/_api/job/" + id, "");
    assert(response.code === 200);
    assert(response.headers['x-arango-async-id'] === id);
    logRawResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Fetching the result of an HTTP POST job that failed:

@EXAMPLE_ARANGOSH_RUN{RestJobHandlerPutFail}
    var url = "/_api/collection";

    var response = logCurlRequest('POST', url, '{"name":" this name is invalid "}', { "x-arango-async": "store" });

    assert(response.code === 202);
    logRawResponse(response);

    var id = response.headers['x-arango-async-id'];
    require("internal").wait(1);

    response = logCurlRequest('PUT', "/_api/job/" + id, "");
    assert(response.code === 400);
    assert(response.headers['x-arango-async-id'] === id);
    logRawResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

@RESTDONE

@COMMENT{######################################################################}
@CLEARPAGE
@anchor HttpJobPutCancel
@RESTHEADER{PUT /_api/job/`job-id`/cancel,Cancels an async job}

@RESTURLPARAMETERS

@RESTURLPARAM{job-id,string,required}
The async job id.

@RESTDESCRIPTION
Cancels the currently running job identified by `job-id`. Note that it still
might take some time to actually cancel the running async job.

@RESTRETURNCODES

Any HTTP status code might be returned by this method. To tell the original job
response from a job manager response apart, check for the HTTP header
`x-arango-async-id`. If it is present, the response contains the original job's
result. Otherwise the response is from the job manager.

@RESTRETURNCODE{200}
cancel has been initiated.

@RESTRETURNCODE{400}
is returned if no `job-id` was specified in the request. In this case, no
`x-arango-async-id` HTTP header will be returned.

@RESTRETURNCODE{404}
is returned if the job was not found or already deleted or fetched from the job
result list. In this case, no `x-arango-async-id` HTTP header will be returned.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestJobHandlerPutCancel}
    var url = "/_api/cursor";
    var body = '{"query": "FOR i IN 1..10 FOR j IN 1..10 LET x = sleep(1.0) FILTER i == 5 && j == 5 RETURN 42"}';

    var response = logCurlRequest('POST', url, body, { "x-arango-async": "store" });

    assert(response.code == 202);
    logRawResponse(response);

    var id = response.headers['x-arango-async-id'];
    require("internal").wait(1);

    response = logCurlRequest('GET', "/_api/job/pending", "", {});
    logRawResponse(response);

    response = logCurlRequest('PUT', "/_api/job/"+id+"/cancel", "", {});
    logRawResponse(response);

    response = logCurlRequest('GET', "/_api/job/pending", "", {});
    logRawResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

@RESTDONE

@COMMENT{######################################################################}
@CLEARPAGE
@anchor HttpJobDelete
@RESTHEADER{DELETE /_api/job/`type`,Deletes the result of async jobs}

@RESTURLPARAMETERS

@RESTURLPARAM{type,string,required}
The type of jobs to delete. `type` can be:
- `all`: deletes all jobs results. Currently executing or queued async jobs
  will not be stopped by this call.
- `expired`: deletes expired results. To determine the expiration status of
  a result, pass the `stamp` URL parameter. `stamp` needs to be a UNIX
  timestamp, and all async job results created at a lower timestamp will be
  deleted.
- an actual job-id: in this case, the call will remove the result of the
  specified async job. If the job is currently executing or queued, it will
  not be aborted.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{stamp,number,optional}
A UNIX timestamp specifying the expiration threshold when type is `expired`.

@RESTDESCRIPTION
Deletes either all job results, expired job results, or the result of a specific
job. Clients can use this method to perform an eventual garbage collection of
job results.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the deletion operation was carried out successfully. This code
will also be returned if no results were deleted.

@RESTRETURNCODE{400}
is returned if `type` is not specified or has an invalid value.

@RESTRETURNCODE{404}
is returned if `type` is a job-id but no async job with the specified id was
found.

@EXAMPLES

Deleting all jobs:

@EXAMPLE_ARANGOSH_RUN{RestJobHandlerDeleteAll}
    var url = "/_api/version";

    var response = logCurlRequest('GET', url, "", { "x-arango-async": "store" });

    assert(response.code === 202);
    logRawResponse(response);

    require("internal").wait(1);

    response = logCurlRequest('DELETE', "/_api/job/all");
    assert(response.code === 200);
    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Deleting expired jobs:

@EXAMPLE_ARANGOSH_RUN{RestJobHandlerDeleteExpired}
    var url = "/_api/version";

    var response = logCurlRequest('GET', url, "", { "x-arango-async": "store" });

    assert(response.code === 202);
    logRawResponse(response);

    require("internal").wait(1);
    var stamp = parseInt(require("internal").time() + 360, 10);

    response = logCurlRequest('DELETE', "/_api/job/expired?stamp=" + stamp);
    assert(response.code === 200);
    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Deleting the result of a specific job:

@EXAMPLE_ARANGOSH_RUN{RestJobHandlerDeleteId}
    var url = "/_api/version";

    var response = logCurlRequest('GET', url, "", { "x-arango-async": "store" });

    assert(response.code === 202);
    logRawResponse(response);

    var id = response.headers['x-arango-async-id'];
    require("internal").wait(1);

    response = logCurlRequest('DELETE', "/_api/job/" + id);
    assert(response.code === 200);
    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Deleting the result of a non-existing job:

@EXAMPLE_ARANGOSH_RUN{RestJobHandlerDeleteInvalid}
    response = logCurlRequest('DELETE', "/_api/job/foobar");
    assert(response.code === 404);
    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

@RESTDONE

@COMMENT{######################################################################}
@CLEARPAGE
@anchor HttpJobGetId
@RESTHEADER{GET /_api/job/`job-id`,Returns the status of the specified job}

@RESTURLPARAMETERS

@RESTURLPARAM{job-id,string,required}
The async job id.

@RESTDESCRIPTION
Returns the processing status of the specified job. The processing status can be
determined by peeking into the HTTP response code of the response.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the job requested via `job-id` has been executed successfully and
its result is ready to fetch.

@RESTRETURNCODE{204}
is returned if the job requested via `job-id` is still in the queue of pending
(or not yet finished) jobs.

@RESTRETURNCODE{400}
is returned if no `job-id` was specified in the request.

@RESTRETURNCODE{404}
is returned if the job was not found or already deleted or fetched from the job
result list.

@EXAMPLES

Querying the status of a done job:

@EXAMPLE_ARANGOSH_RUN{RestJobHandlerGetIdDone}
    var url = "/_api/version";

    var response = logCurlRequest('GET', url, "", { "x-arango-async": "store" });

    assert(response.code === 202);
    var id = response.headers['x-arango-async-id'];
    logRawResponse(response);

    require("internal").wait(1);

    response = logCurlRequest('GET', "/_api/job/" + id);
    assert(response.code === 200 || response.code === 204);
    logRawResponse(response);

    response = curlRequest('DELETE', "/_api/job/" + id);
    assert(response.code === 200);
@END_EXAMPLE_ARANGOSH_RUN

Querying the status of a pending job:

@EXAMPLE_ARANGOSH_RUN{RestJobHandlerGetIdPending}
    var url = "/_admin/sleep?duration=3";

    var response = logCurlRequest('GET', url, "", { "x-arango-async": "store" });

    assert(response.code === 202);
    var id = response.headers['x-arango-async-id'];
    logRawResponse(response);

    response = logCurlRequest('GET', "/_api/job/" + id);
    assert(response.code === 204);
    logRawResponse(response);

    response = curlRequest('DELETE', "/_api/job/" + id);
@END_EXAMPLE_ARANGOSH_RUN

@RESTDONE

@COMMENT{######################################################################}
@CLEARPAGE
@anchor HttpJobGetType

@RESTHEADER{GET /_api/job/`type`,Returns the list of job result ids with a specific status}

@RESTURLPARAMETERS

@RESTURLPARAM{type,string,required}
The type of jobs to return. The type can be either `done` or `pending`.  Setting
the type to `done` will make the method return the ids of already completed
async jobs for which results can be fetched.  Setting the type to `pending` will
return the ids of not yet finished async jobs.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{count,number,optional}
The maximum number of ids to return per call. If not specified, a server-defined
maximum value will be used.

@RESTDESCRIPTION
Returns the list of ids of async jobs with a specific status (either done or
pending). The list can be used by the client to get an overview of the job
system status and to retrieve completed job results later.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the list can be compiled successfully. Note: the list might be
empty.

@RESTRETURNCODE{400}
is returned if `type` is not specified or has an invalid value.

@EXAMPLES

Fetching the list of done jobs:

@EXAMPLE_ARANGOSH_RUN{RestJobHandlerGetDone}
    var url = "/_api/version";

    var response = logCurlRequest('GET', url, "", { "x-arango-async": "store" });

    assert(response.code === 202);
    logRawResponse(response);

    require("internal").wait(1);

    response = logCurlRequest('GET', "/_api/job/done");
    assert(response.code === 200);
    logJsonResponse(response);

    response = curlRequest('DELETE', "/_api/job/all");
    assert(response.code === 200);
@END_EXAMPLE_ARANGOSH_RUN

Fetching the list of pending jobs:

@EXAMPLE_ARANGOSH_RUN{RestJobHandlerGetPending}
    var url = "/_api/version";

    var response = logCurlRequest('GET', url, "", { "x-arango-async": "store" });

    assert(response.code === 202);
    logRawResponse(response);

    var id = response.headers['x-arango-async-id'];
    require("internal").wait(1);

    response = logCurlRequest('GET', "/_api/job/pending");
    assert(response.code === 200);
    logJsonResponse(response);

    response = curlRequest('DELETE', "/_api/job/all");
    assert(response.code === 200);
@END_EXAMPLE_ARANGOSH_RUN

@RESTDONE

@COMMENT{######################################################################}

