
@startDocuBlock job_getStatusById
@brief Returns the status of a specific job

@RESTHEADER{GET /_api/job/{job-id}, Returns async job, getJobById}

@RESTURLPARAMETERS

@RESTURLPARAM{job-id,string,required}
The async job id.

@RESTDESCRIPTION
Returns the processing status of the specified job. The processing status
can be
determined by peeking into the HTTP response code of the response.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the job requested via job-id has been executed
and its result is ready to fetch.

@RESTRETURNCODE{204}
is returned if the job requested via job-id is still in the queue of pending
(or not yet finished) jobs.

@RESTRETURNCODE{404}
is returned if the job was not found or already deleted or fetched from the
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
  var response = logCurlRequest('PUT', url, "");
  assert(response.code === 200);
  logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Querying the status of a pending job:
(therefore we create a long runnging job...)

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
@endDocuBlock
