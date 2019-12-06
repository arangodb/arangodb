
@startDocuBlock job_delete
@brief deletes an async job result

@RESTHEADER{DELETE /_api/job/{type}#by-type, Deletes async job, deleteJob:byType}

@RESTURLPARAMETERS

@RESTURLPARAM{type,string,required}
The type of jobs to delete. type can be:
* *all*: Deletes all jobs results. Currently executing or queued async
  jobs will not be stopped by this call.
* *expired*: Deletes expired results. To determine the expiration status of a
  result, pass the stamp query parameter. stamp needs to be a UNIX timestamp,
  and all async job results created at a lower timestamp will be deleted.
* *an actual job-id*: In this case, the call will remove the result of the
  specified async job. If the job is currently executing or queued, it will
  not be aborted.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{stamp,number,optional}
A UNIX timestamp specifying the expiration threshold when type is expired.

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
is returned if type is not specified or has an invalid value.

@RESTRETURNCODE{404}
is returned if type is a job-id but no async job with the specified id was
found.

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
  now = JSON.parse(response.body).time;

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
