
@startDocuBlock get_api_shutdown
@brief initiates the shutdown sequence

@RESTHEADER{GET /_admin/shutdown, Query progress of soft shutdown process, RestShutdownHandler}

@RESTDESCRIPTION
This call reports progress about a soft coordinator shutdown (see
documentation of `DELETE /_admin/shutdown?soft=true`). 
In this case, the following types of operations are tracked:

 - AQL cursors (in particular streaming cursors)
 - Transactions (in particular streaming transactions)
 - Pregel runs (conducted by this coordinator)
 - Ongoing asynchronous requests (using the `x-arango-async: store` HTTP header
 - Finished asynchronous requests, whose result has not yet been
   collected
 - Queued low priority requests (most normal requests)
 - Ongoing low priority requests

This call returns a JSON object of the following shape, indicating the
fact that a soft shutdown is ongoing and the number of active operations
of the various types:

```json
{
  "softShutdownOngoing" : true,
  "AQLcursors" : 0,
  "transactions" : 1,
  "pendingJobs" : 1,
  "doneJobs" : 0,
  "pregelConductors" : 0,
  "lowPrioOngoingRequests" : 1,
  "lowPrioQueuedRequests" : 0,
  "allClear" : false
}
```

Once all numbers have gone to 0, the flag `allClear` is set and the
coordinator shuts down automatically. This API is not available on
DBServers and Agents.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned in all cases, the above JSON object is in the body.

@endDocuBlock
