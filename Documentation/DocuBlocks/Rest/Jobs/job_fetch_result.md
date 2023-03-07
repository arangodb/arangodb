
@startDocuBlock job_fetch_result
@brief fetches a job result and removes it from the queue

@RESTHEADER{PUT /_api/job/{job-id}, Return result of an async job, getJobResult}

@RESTURLPARAMETERS

@RESTURLPARAM{job-id,string,required}
The async job id.

@RESTDESCRIPTION
Returns the result of an async job identified by job-id. If the async job
result is present on the server, the result will be removed from the list of
result. That means this method can be called for each job-id once.
The method will return the original job result's headers and body, plus the
additional HTTP header x-arango-async-job-id. If this header is present,
then
the job was found and the response contains the original job's result. If
the header is not present, the job was not found and the response contains
status information from the job manager.

@RESTRETURNCODES

@RESTRETURNCODE{204}
is returned if the job requested via job-id is still in the queue of pending
(or not yet finished) jobs. In this case, no x-arango-async-id HTTP header
will be returned.

@RESTRETURNCODE{400}
is returned if no job-id was specified in the request. In this case,
no x-arango-async-id HTTP header will be returned.

@RESTRETURNCODE{404}
is returned if the job was not found or already deleted or fetched from
the job result list. In this case, no x-arango-async-id HTTP header will
be returned.

@EXAMPLES
Not providing a job-id:

@EXAMPLE_ARANGOSH_RUN{job_fetch_result_01}
  var url = "/_api/job";
  var response = logCurlRequest('PUT', url, "");

  assert(response.code === 400);

  logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Providing a job-id for a non-existing job:

@EXAMPLE_ARANGOSH_RUN{job_fetch_result_02}
  var url = "/_api/job/notthere";
  var response = logCurlRequest('PUT', url, "");

  assert(response.code === 404);

  logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Fetching the result of an HTTP GET job:

@EXAMPLE_ARANGOSH_RUN{job_fetch_result_03}
  var url = "/_api/version";
  var headers = {'x-arango-async' : 'store'};
  var response = logCurlRequest('PUT', url, "", headers);

  assert(response.code === 202);
  logRawResponse(response);

  var queryId = response.headers['x-arango-async-id'];
  url = '/_api/job/' + queryId
  var response = logCurlRequest('PUT', url, "");
  assert(response.code === 200);
  logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Fetching the result of an HTTP POST job that failed:

@EXAMPLE_ARANGOSH_RUN{job_fetch_result_04}
  var url = "/_api/collection";
  var headers = {'x-arango-async' : 'store'};
  var response = logCurlRequest('PUT', url, {"name":" this name is invalid "}, headers);

  assert(response.code === 202);
  logRawResponse(response);

  var queryId = response.headers['x-arango-async-id'];
  url = '/_api/job/' + queryId
  var response = logCurlRequest('PUT', url, "");
  assert(response.code === 400);
  logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
