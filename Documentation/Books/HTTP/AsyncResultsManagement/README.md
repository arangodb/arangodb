HTTP Interface for Async Results Management
===========================================
### Request Execution

ArangoDB provides various methods of executing client requests. Clients can choose the appropriate method on a per-request level based on their throughput, control flow, and durability requirements.

#### Blocking execution

ArangoDB is a multi-threaded server, allowing the processing of multiple client 
requests at the same time. Communication handling and the actual work can be performed
by multiple worker threads in parallel.

Though multiple clients can connect and send their requests in parallel to ArangoDB,
clients may need to wait for their requests to be processed.

By default, the server will fully process an incoming request and then return the
result to the client. The client must wait for the server's response before it can
send additional requests over the connection. For clients that are single-threaded
or not event-driven, waiting for the full server response may be non-optimal.

Furthermore, please note that even if the client closes the HTTP
connection, the request running on the server will still continue until
it is complete and only then notice that the client no longer listens.
Thus closing the connection does not help to abort a long running query!
See below under [Async Execution and later Result Retrieval](#async-execution-and-later-result-retrieval)
and [HttpJobPutCancel](#managing-async-results-via-http) for details.

#### Fire and Forget

To mitigate client blocking issues, ArangoDB since version 1.4. offers a generic mechanism 
for non-blocking requests: if clients add the HTTP header *x-arango-async: true* to their
requests, ArangoDB will put the request into an in-memory task queue and return an HTTP 202
(accepted) response to the client instantly. The server will execute the tasks from
the queue asynchronously, decoupling the client requests and the actual work.

This allows for much higher throughput than if clients would wait for the server's
response. The downside is that the response that is sent to the client is always the
same (a generic HTTP 202) and clients cannot make a decision based on the actual
operation's result at this point. In fact, the operation might have not even been executed at the
time the generic response has reached the client. Clients can thus not rely on their
requests having been processed successfully.

The asynchronous task queue on the server is not persisted, meaning not-yet processed
tasks from the queue will be lost in case of a crash. However, the client will
not know whether they were processed or not.

Clients should thus not send the extra header when they have strict durability 
requirements or if they rely on result of the sent operation for further actions.

The maximum number of queued tasks is determined by the startup option 
*-scheduler.maximal-queue-size*. If more than this number of tasks are already queued,
the server will reject the request with an HTTP 500 error.

Finally, please note that it is not possible to cancel such a
fire and forget job, since you won't get any handle to identify it later on.
If you need to cancel requests,
use [Async Execution and later Result Retrieval](#async-execution-and-later-result-retrieval) 
and [HttpJobPutCancel](#managing-async-results-via-http) below.

#### Async Execution and later Result Retrieval

By adding the HTTP header *x-arango-async: store* to a request, clients can instruct
the ArangoDB server to execute the operation asynchronously as [above](#fire-and-forget),
but also store the operation result in memory for a later retrieval. The
server will return a job id in the HTTP response header *x-arango-async-id*. The client
can use this id in conjunction with the HTTP API at */_api/job*, which is described in
detail in this manual.

Clients can ask the ArangoDB server via the async jobs API which results are
ready for retrieval, and which are not. Clients can also use the async jobs API to
retrieve the original results of an already executed async job by passing it the
originally returned job id. The server will then return the job result as if the job was 
executed normally. Furthermore, clients can cancel running async jobs by
their job id, see [HttpJobPutCancel](#managing-async-results-via-http).

ArangoDB will keep all results of jobs initiated with the *x-arango-async: store* 
header. Results are removed from the server only if a client explicitly asks the
server for a specific result.

The async jobs API also provides methods for garbage collection that clients can
use to get rid of "old" not fetched results. Clients should call this method periodically
because ArangoDB does not artificially limit the number of not-yet-fetched results.

It is thus a client responsibility to store only as many results as needed and to fetch 
available results as soon as possible, or at least to clean up not fetched results
from time to time.

The job queue and the results are kept in memory only on the server, so they will be
lost in case of a crash.

#### Canceling asynchronous jobs

As mentioned above it is possible to cancel an asynchronously running
job using its job ID. This is done with a PUT request as described in
[HttpJobPutCancel](#managing-async-results-via-http). 

However, a few words of explanation about what happens behind the
scenes are in order. Firstly, a running async query can internally be
executed by C++ code or by JavaScript code. For example CRUD operations
are executed directly in C++, whereas AQL queries and transactions
are executed by JavaScript code. The job cancelation only works for
JavaScript code, since the mechanism used is simply to trigger an
uncatchable exception in the JavaScript thread, which will be caught
on the C++ level, which in turn leads to the cancelation of the job.
No result can be retrieved later, since all data about the request is
discarded.

If you cancel a job running on a coordinator of a cluster (Sharding),
then only the code running on the coordinator is stopped, there may
remain tasks within the cluster which have already been distributed to
the DBservers and it is currently not possible to cancel them as well.

#### Async Execution and Authentication

If a request requires authentication, the authentication procedure is run before 
queueing. The request will only be queued if it valid credentials and the authentication 
succeeds. If the request does not contain valid credentials, it will not be queued but
rejected instantly in the same way as a "regular", non-queued request.

Managing Async Results via HTTP
===============================

@startDocuBlock JSF_job_fetch_result
@startDocuBlock JSF_job_cancel
@startDocuBlock JSF_job_delete
@startDocuBlock JSF_job_getStatusById
@startDocuBlock JSF_job_getByType

