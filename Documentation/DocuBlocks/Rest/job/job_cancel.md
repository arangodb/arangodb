
@startDocuBlock job_cancel
@brief cancels an async job

@RESTHEADER{PUT /_api/job/{job-id}/cancel, Cancel async job, putJobMethod:cancel}

@RESTURLPARAMETERS

@RESTURLPARAM{job-id,string,required}
The async job id.

@RESTDESCRIPTION
Cancels the currently running job identified by job-id. Note that it still
might take some time to actually cancel the running async job.

@RESTRETURNCODES

@RESTRETURNCODE{200}
cancel has been initiated.

@RESTRETURNCODE{400}
is returned if no job-id was specified in the request. In this case,
no x-arango-async-id HTTP header will be returned.

@RESTRETURNCODE{404}
is returned if the job was not found or already deleted or fetched from
the job result list. In this case, no x-arango-async-id HTTP header will
be returned.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{job_cancel}
  var url = "/_api/cursor";
  var headers = {'x-arango-async' : 'store'};
  var postData = {"query":
     "FOR i IN 1..10 FOR j IN 1..10 LET x = sleep(1.0) FILTER i == 5 && j == 5 RETURN 42"}

  var response = logCurlRequest('POST', url, postData, headers);
  assert(response.code === 202);
  logRawResponse(response);

  var queryId = response.headers['x-arango-async-id'];
  url = '/_api/job/pending';
  var response = logCurlRequest('GET', url);
  assert(response.code === 200);
  logJsonResponse(response);

  url = '/_api/job/' + queryId + '/cancel'
  var response = logCurlRequest('PUT', url, "");
  assert(response.code === 200);
  logJsonResponse(response);

  url = '/_api/job/pending';
  var response = logCurlRequest('GET', url, "");
  assert(response.code === 200);
  logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
