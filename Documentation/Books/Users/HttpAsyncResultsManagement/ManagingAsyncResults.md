<a name="managing_async_results_via_http"></a>
# Managing Async Results via HTTP

@COMMENT{######################################################################}
@anchor HttpJobPut
***
#### PUT /_api/job/`job-id`
***


UNKNOWN RESTURLPARAMETERS


UNKNOWN RESTURLPARAM

The async job id.

__Description__

Returns the result of an async job identified by `job-id`.  If the async job
result is present on the server, the result will be removed from the list of
result. That means this method can be called for each `job-id` once.

The method will return the original job result's headers and body, plus the
additional HTTP header `x-arango-async-job-id`. If this header is present, then
the job was found and the response contains the original job's result. If the
header is not present, the job was not found and the response contains status
information from the job manager.

__HTTP Return Codes__


Any HTTP status code might be returned by this method. To tell the original job
response from a job manager response apart, check for the HTTP header
`x-arango-async-id`. If it is present, the response contains the original job's
result. Otherwise the response is from the job manager.

__HTTP Code 204__

is returned if the job requested via `job-id` is still in the queue of pending
(or not yet finished) jobs. In this case, no `x-arango-async-id` HTTP header
will be returned.

__HTTP Code 400__

is returned if no `job-id` was specified in the request. In this case, no
`x-arango-async-id` HTTP header will be returned.

__HTTP Code 404__

is returned if the job was not found or already deleted or fetched from the job
result list. In this case, no `x-arango-async-id` HTTP header will be returned.

__Examples__


Not providing a job-id:

INCLUDE RestJobHandlerPutNone


Providing a job-id for a non-existing job:

INCLUDE RestJobHandlerPutInvalid


Fetching the result of an HTTP GET job:

INCLUDE RestJobHandlerPutGet


Fetching the result of an HTTP POST job that failed:

INCLUDE RestJobHandlerPutFail


UNKNOWN RESTDONE


@COMMENT{######################################################################}
@CLEARPAGE
@anchor HttpJobPutCancel
***
#### PUT /_api/job/`job-id`/cancel
***


UNKNOWN RESTURLPARAMETERS


UNKNOWN RESTURLPARAM

The async job id.

__Description__

Cancels the currently running job identified by `job-id`. Note that it still
might take some time to actually cancel the running async job.

__HTTP Return Codes__


Any HTTP status code might be returned by this method. To tell the original job
response from a job manager response apart, check for the HTTP header
`x-arango-async-id`. If it is present, the response contains the original job's
result. Otherwise the response is from the job manager.

__HTTP Code 200__

cancel has been initiated.

__HTTP Code 400__

is returned if no `job-id` was specified in the request. In this case, no
`x-arango-async-id` HTTP header will be returned.

__HTTP Code 404__

is returned if the job was not found or already deleted or fetched from the job
result list. In this case, no `x-arango-async-id` HTTP header will be returned.

__Examples__


INCLUDE RestJobHandlerPutCancel


UNKNOWN RESTDONE


@COMMENT{######################################################################}
@CLEARPAGE
@anchor HttpJobDelete
***
#### DELETE /_api/job/`type`
***


UNKNOWN RESTURLPARAMETERS


UNKNOWN RESTURLPARAM

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

UNKNOWN RESTQUERYPARAMETERS


UNKNOWN RESTQUERYPARAM

A UNIX timestamp specifying the expiration threshold when type is `expired`.

__Description__

Deletes either all job results, expired job results, or the result of a specific
job. Clients can use this method to perform an eventual garbage collection of
job results.

__HTTP Return Codes__


__HTTP Code 200__

is returned if the deletion operation was carried out successfully. This code
will also be returned if no results were deleted.

__HTTP Code 400__

is returned if `type` is not specified or has an invalid value.

__HTTP Code 404__

is returned if `type` is a job-id but no async job with the specified id was
found.

__Examples__


Deleting all jobs:

INCLUDE RestJobHandlerDeleteAll


Deleting expired jobs:

INCLUDE RestJobHandlerDeleteExpired


Deleting the result of a specific job:

INCLUDE RestJobHandlerDeleteId


Deleting the result of a non-existing job:

INCLUDE RestJobHandlerDeleteInvalid


UNKNOWN RESTDONE


@COMMENT{######################################################################}
@CLEARPAGE
@anchor HttpJobGetId
***
#### GET /_api/job/`job-id`
***


UNKNOWN RESTURLPARAMETERS


UNKNOWN RESTURLPARAM

The async job id.

__Description__

Returns the processing status of the specified job. The processing status can be
determined by peeking into the HTTP response code of the response.

__HTTP Return Codes__


__HTTP Code 200__

is returned if the job requested via `job-id` has been executed successfully and
its result is ready to fetch.

__HTTP Code 204__

is returned if the job requested via `job-id` is still in the queue of pending
(or not yet finished) jobs.

__HTTP Code 400__

is returned if no `job-id` was specified in the request.

__HTTP Code 404__

is returned if the job was not found or already deleted or fetched from the job
result list.

__Examples__


Querying the status of a done job:

INCLUDE RestJobHandlerGetIdDone


Querying the status of a pending job:

INCLUDE RestJobHandlerGetIdPending


UNKNOWN RESTDONE


@COMMENT{######################################################################}
@CLEARPAGE
@anchor HttpJobGetType

***
#### GET /_api/job/`type`
***


UNKNOWN RESTURLPARAMETERS


UNKNOWN RESTURLPARAM

The type of jobs to return. The type can be either `done` or `pending`.  Setting
the type to `done` will make the method return the ids of already completed
async jobs for which results can be fetched.  Setting the type to `pending` will
return the ids of not yet finished async jobs.

UNKNOWN RESTQUERYPARAMETERS


UNKNOWN RESTQUERYPARAM

The maximum number of ids to return per call. If not specified, a server-defined
maximum value will be used.

__Description__

Returns the list of ids of async jobs with a specific status (either done or
pending). The list can be used by the client to get an overview of the job
system status and to retrieve completed job results later.

__HTTP Return Codes__


__HTTP Code 200__

is returned if the list can be compiled successfully. Note: the list might be
empty.

__HTTP Code 400__

is returned if `type` is not specified or has an invalid value.

__Examples__


Fetching the list of done jobs:

INCLUDE RestJobHandlerGetDone


Fetching the list of pending jobs:

INCLUDE RestJobHandlerGetPending


UNKNOWN RESTDONE


@COMMENT{######################################################################}

