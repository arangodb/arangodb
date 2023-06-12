
@startDocuBlock delete_api_job_type

@RESTHEADER{DELETE /_api/job/{job-id}, Delete async job results, deleteJob}

@RESTURLPARAMETERS

@RESTURLPARAM{job-id,string,required}
The ID of the job to delete. The ID can be:
- `all`: Deletes all jobs results. Currently executing or queued async
  jobs are not stopped by this call.
- `expired`: Deletes expired results. To determine the expiration status of a
  result, pass the stamp query parameter. stamp needs to be a Unix timestamp,
  and all async job results created before this time are deleted.
- **A numeric job ID**: In this case, the call removes the result of the
  specified async job. If the job is currently executing or queued, it is
  not aborted.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{stamp,number,optional}
A Unix timestamp specifying the expiration threshold for when the `job-id` is
set to `expired`.

@RESTDESCRIPTION
Deletes either all job results, expired job results, or the result of a
specific job.
Clients can use this method to perform an eventual garbage collection of job
results.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the deletion operation was carried out successfully.
This code will also be returned if no results were deleted.

@RESTRETURNCODE{400}
is returned if `job-id` is not specified or has an invalid value.

@RESTRETURNCODE{404}
is returned if `job-id` is a syntactically valid job ID but no async job with
the specified ID is found.

@EXAMPLES

Deleting all jobs:

@EXAMPLE_ARANGOSH_RUN{job_delete_01}
  var url = "/_api/version";
  var headers = {'x-arango-async' : 'store'};
  var response = logCurlRequest('PUT', url, "", headers);

  assert(response.code === 202);
  logRawResponse(response);

  url = '/_api/job/all'
  var response = logCurlRequest('DELETE', url, "");
  assert(response.code === 200);
  logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Deleting expired jobs:

@EXAMPLE_ARANGOSH_RUN{job_delete_02}
  var url = "/_api/version";
  var headers = {'x-arango-async' : 'store'};
  var response = logCurlRequest('PUT', url, "", headers);

  assert(response.code === 202);
  logRawResponse(response);

  var response = logCurlRequest('GET', "/_admin/time");
  assert(response.code === 200);
  logJsonResponse(response);
  now = response.parsedBody.time;

  url = '/_api/job/expired?stamp=' + now
  var response = logCurlRequest('DELETE', url, "");
  assert(response.code === 200);
  logJsonResponse(response);

  url = '/_api/job/pending';
  var response = logCurlRequest('GET', url);
  assert(response.code === 200);
  logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Deleting the result of a specific job:

@EXAMPLE_ARANGOSH_RUN{job_delete_03}
  var url = "/_api/version";
  var headers = {'x-arango-async' : 'store'};
  var response = logCurlRequest('PUT', url, "", headers);

  assert(response.code === 202);
  logRawResponse(response);

  var queryId = response.headers['x-arango-async-id'];
  url = '/_api/job/' + queryId
  var response = logCurlRequest('DELETE', url, "");
  assert(response.code === 200);
  logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Deleting the result of a non-existing job:

@EXAMPLE_ARANGOSH_RUN{job_delete_04}
  url = '/_api/job/AreYouThere'
  var response = logCurlRequest('DELETE', url, "");
  assert(response.code === 404);
  logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
