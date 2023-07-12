
@startDocuBlock get_api_job_job

@RESTHEADER{GET /_api/job/{job-id}, List async jobs by status or get the status of specific job, getJob}

@RESTDESCRIPTION
This endpoint returns either of the following, depending on the specified value
for the `job-id` parameter:

- The IDs of async jobs with a specific status
- The processing status of a specific async job

@RESTURLPARAMETERS

@RESTURLPARAM{job-id,string,required}
If you provide a value of `pending` or `done`, then the endpoint returns an
array of strings with the job IDs of ongoing or completed async jobs.

If you provide a numeric job ID, then the endpoint returns the status of the
specific async job in the form of an HTTP reply without payload. Check the
HTTP status code of the response for the job status.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{count,number,optional}
The maximum number of job IDs to return per call. If not specified, a
server-defined maximum value is used. Only applicable if you specify `pending`
or `done` as `job-id` to list jobs.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the job with the specified `job-id` has finished and you can
fetch its results, or if your request for the list of `pending` or `done` jobs
is successful (the list might be empty).

@RESTRETURNCODE{204}
is returned if the job with the specified `job-id` is still in the queue of
pending (or not yet finished) jobs.

@RESTRETURNCODE{400}
is returned if you specified an invalid value for `job-id` or no value.

@RESTRETURNCODE{404}
is returned if the job cannot be found or is already fetched or deleted from the
job result list.

@EXAMPLES

Querying the status of a done job:

@EXAMPLE_ARANGOSH_RUN{job_getStatusById_01}
  var url = "/_api/version";
  var headers = {'x-arango-async' : 'store'};
  var response = logCurlRequest('PUT', url, "", headers);

  assert(response.code === 202);
  logRawResponse(response);

  var queryId = response.headers['x-arango-async-id'];
  url = '/_api/job/' + queryId
  var response = logCurlRequest('GET', url);
  assert(response.code === 200);
  logRawResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Querying the status of a pending job:
(therefore we create a long running job...)

@EXAMPLE_ARANGOSH_RUN{job_getStatusById_02}
  var url = "/_api/transaction";
  var body = {
    collections: {
      read : [ "_aqlfunctions" ]
    },
    action: "function () {require('internal').sleep(15.0);}"
  };
  var headers = {'x-arango-async' : 'store'};
  var response = logCurlRequest('POST', url, body, headers);

  assert(response.code === 202);
  logRawResponse(response);

  var queryId = response.headers['x-arango-async-id'];
  url = '/_api/job/' + queryId
  var response = logCurlRequest('GET', url);
  assert(response.code === 204);
  logRawResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Fetching the list of `done` jobs:

@EXAMPLE_ARANGOSH_RUN{job_getByType_01}
  var url = "/_api/version";
  var headers = {'x-arango-async' : 'store'};
  var response = logCurlRequest('PUT', url, "", headers);

  assert(response.code === 202);
  logRawResponse(response);

  url = '/_api/job/done'
  var response = logCurlRequest('GET', url, "");
  assert(response.code === 200);
  logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Fetching the list of `pending` jobs:

@EXAMPLE_ARANGOSH_RUN{job_getByType_02}
  var url = "/_api/version";
  var headers = {'x-arango-async' : 'store'};
  var response = logCurlRequest('PUT', url, "", headers);

  assert(response.code === 202);
  logRawResponse(response);

  url = '/_api/job/pending'
  var response = logCurlRequest('GET', url, "");
  assert(response.code === 200);
  logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Fetching the list of a `pending` jobs while a long-running job is executing
(and aborting it):

@EXAMPLE_ARANGOSH_RUN{job_getByType_03}
  var url = "/_api/transaction";
  var body = {
    collections: {
      read : [ "_frontend" ]
    },
    action: "function () {require('internal').sleep(15.0);}"
  };
  var headers = {'x-arango-async' : 'store'};
  var response = logCurlRequest('POST', url, body, headers);

  assert(response.code === 202);
  logRawResponse(response);

  var queryId = response.headers['x-arango-async-id'];
  url = '/_api/job/pending'
  var response = logCurlRequest('GET', url);
  assert(response.code === 200);
  logJsonResponse(response);

  url = '/_api/job/' + queryId
  var response = logCurlRequest('DELETE', url, "");
  assert(response.code === 200);
  logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
