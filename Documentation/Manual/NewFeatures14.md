New Features in ArangoDB 1.4 {#NewFeatures14}
=============================================

@NAVIGATE_NewFeatures14
@EMBEDTOC{NewFeatures14TOC}

Features and Improvements {#NewFeatures14Introduction}
======================================================

The following list shows in detail which features have been added or improved in
ArangoDB 1.4.  ArangoDB 1.4 also contains several bugfixes that are not listed
here - see the CHANGELOG for details.

Asynchronous Execution {#NewFeatures14Async}
--------------------------------------------

ArangoDB is a multi-threaded server, allowing the processing of multiple client 
requests at the same time. Communication handling and the actual work can be performed
by multiple worker threads in parallel.

Though multiple clients can connect and send their requests in parallel to ArangoDB,
clients may need to wait for their requests to be processed.

By default, the server will fully process an incoming request and then return the
result to the client. The client must wait for the server's response before it can
send additional requests over the connection. For clients that are single-threaded
or not event-driven, waiting for the full server response may be non-optimal.

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


Replication{#NewFeatures14Replication}
--------------------------------------

See @ref UserManualReplication for details.
