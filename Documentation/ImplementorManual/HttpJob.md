HTTP Interface for Async Results Management {#HttpJob}
======================================================

@NAVIGATE_HttpJob
@EMBEDTOC{HttpJobTOC}

Request Execution {#HttpJobIntro}
=================================

ArangoDB provides various methods of executing client requests. Clients can choose the appropriate method on a per-request level based on their throughput, control flow, and durability requirements.

Blocking execution {#HttpJobBlocking}
-------------------------------------

ArangoDB is a multi-threaded server, allowing the processing of multiple client 
requests at the same time. Communication handling and the actual work can be performed
by multiple worker threads in parallel.

Though multiple clients can connect and send their requests in parallel to ArangoDB,
clients may need to wait for their requests to be processed.

By default, the server will fully process an incoming request and then return the
result to the client. The client must wait for the server's response before it can
send additional requests over the connection. For clients that are single-threaded
or not event-driven, waiting for the full server response may be non-optimal.

Fire and Forget {#HttpJobFireForget}
------------------------------------

To mitigate client blocking issues, ArangoDB since version 1.4. offers a generic mechanism 
for non-blocking requests: if clients add the HTTP header `x-arango-async: true` to their
requests, ArangoDB will put the request into an in-memory task queue and return an HTTP 202
(accepted) response to the client instantly. The server will execute the tasks from
the queue asynchronously, decoupling the client requests and the actual work.

This allows for much higher throughput than if clients would wait for the server's
response. The downside is that the response that is sent to the client is always the
same (a generic HTTP 202) and clients cannot make a decision based on the actual
operation's result. In fact, the operation might have not even been executed at the
time the generic response has reached the client. Clients can thus not rely on their
requests having been processed successfully.

The asynchronous task queue on the server is not persisted, meaning not-yet processed
tasks from the queue might be lost in case of a crash.

Clients should thus not send the extra header when they have strict durability 
requirements or if they rely on result of the sent operation for further actions.

The maximum number of queued tasks is determined by the startup option 
`-scheduler.maximal-queue-size`. If more than this number of tasks are already queued,
the server will reject the request with an HTTP 500 error.

Async Execution and later Result Retrieval {#HttpJobAsync}
----------------------------------------------------------

By adding the HTTP header `x-arango-async: store` to a request, clients can instruct
the ArangoDB server to execute the operation asynchronously as @ref HttpJobFireForget
"above", but also store the operation result in memory for a later retrieval. The
queue and the results are kept in memory, so queued requests and results might be
lost in case of a crash.

For managing the results of requests marked with `x-arango-async: store`, ArangoDB 
provides an HTTP API at `/_api/job`, which is described in the following parts.

Clients can ask the ArangoDB server via the async jobs API which results are
ready for retrieval, and which are not. Clients can also use the async jobs API to
retrieve the original results of an already executed async job. The API will return
the job result as if the job was executed normally.

ArangoDB will keep all results of jobs initiated with the `x-arango-async: store` 
header. Results are removed from the server only if a client explicitly asks the
server for a specific result.

The async jobs API also provides methods for garbage collection that clients can
use to get rid of "old" not fetched results. Clients should call this method periodically
because ArangoDB does not artifically limit the number of not-yet-fetched results.

It is thus a client responsibility to store only as many results as needed and to fetch 
available results as soon as possible, or at least to clean up not fetched results
from time to time.

Managing Async Results via HTTP {#HttpJobHttp}
==============================================

@anchor HttpJobPut
@copydetails triagens::admin::RestJobHandler::putJob

@CLEARPAGE
@anchor HttpJobDelete
@copydetails triagens::admin::RestJobHandler::deleteJob

@CLEARPAGE
@anchor HttpJobGet
@copydetails triagens::admin::RestJobHandler::getJob

