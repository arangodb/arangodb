
@startDocuBlock job_getByType
@brief Returns the ids of job results with a specific status

@RESTHEADER{GET /_api/job/{type}#by-type, Returns list of async jobs, getJob}

@RESTURLPARAMETERS

@RESTURLPARAM{type,string,required}
The type of jobs to return. The type can be either done or pending. Setting
the type to done will make the method return the ids of already completed
async
jobs for which results can be fetched. Setting the type to pending will
return
the ids of not yet finished async jobs.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{count,number,optional}

The maximum number of ids to return per call. If not specified, a
server-defined maximum value will be used.

@RESTDESCRIPTION
Returns the list of ids of async jobs with a specific status (either done or
pending).
The list can be used by the client to get an overview of the job system
status and
to retrieve completed job results later.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the list can be compiled successfully. Note: the list might
be empty.

@RESTRETURNCODE{400}
is returned if type is not specified or has an invalid value.

@EXAMPLES

Fetching the list of done jobs:

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

Fetching the list of pending jobs:

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

Querying the status of a pending job:
(we create a sleep job therefore...)

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
