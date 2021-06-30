
@startDocuBlock get_api_shutdown
@brief query progress of soft shutdown process

@RESTHEADER{GET /_admin/shutdown, Query progress of soft shutdown process, RestGetShutdownHandler}

@RESTDESCRIPTION
<small>Introduced in: v3.7.12, v3.8.1, v3.9.0</small>

This call reports progress about a soft Coordinator shutdown (see
documentation of `DELETE /_admin/shutdown?soft=true`).
In this case, the following types of operations are tracked:

 - AQL cursors (in particular streaming cursors)
 - Transactions (in particular stream transactions)
 - Pregel runs (conducted by this Coordinator)
 - Ongoing asynchronous requests (using the `x-arango-async: store` HTTP header
 - Finished asynchronous requests, whose result has not yet been
   collected
 - Queued low priority requests (most normal requests)
 - Ongoing low priority requests

This API is only available on Coordinators.

@RESTRETURNCODES

@RESTRETURNCODE{200}
The response indicates the fact that a soft shutdown is ongoing and the
number of active operations of the various types. Once all numbers have gone
to 0, the flag `allClear` is set and the Coordinator shuts down automatically.

@RESTREPLYBODY{softShutdownOngoing,boolean,required,}
Whether a soft shutdown of the Coordinator is in progress.

@RESTREPLYBODY{AQLcursors,number,required,}
Number of AQL cursors that are still active.

@RESTREPLYBODY{transactions,number,required,}
Number of ongoing transactions.

@RESTREPLYBODY{pendingJobs,number,required,}
Number of ongoing asynchronous requests.

@RESTREPLYBODY{doneJobs,number,required,}
Number of finished asynchronous requests, whose result has not yet been collected.

@RESTREPLYBODY{pregelConductors,number,required,}
Number of ongoing Pregel jobs.

@RESTREPLYBODY{lowPrioOngoingRequests,number,required,}
Number of queued low priority requests.

@RESTREPLYBODY{lowPrioQueuedRequests,number,required,}
Number of ongoing low priority requests.

@RESTREPLYBODY{allClear,boolean,required,}
Whether all active operations finished.

@endDocuBlock
