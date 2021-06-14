
@startDocuBlock delete_api_shutdown
@brief initiates the shutdown sequence

@RESTHEADER{DELETE /_admin/shutdown, Initiate shutdown sequence, RestShutdownHandler}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{soft,boolean,optional}
If set to `true`, this initiates a soft shutdown. This is only available
on coordinators. When issued, the coordinator tracks a number of ongoing
operations, waits until all have finished, and then shuts itself down
normally. In the meantime, new operations of these types are no longer
accepted and a 503 response is sent instead. However, further requests
to finish the ongoing operations are allowed. This feature can be used
to make restart operations of coordinators less intrusive for clients.

The following types of operations are tracked:

 - AQL cursors (in particular streaming cursors)
 - Transactions (in particular streaming transactions)
 - Pregel runs (conducted by this coordinator)
 - Ongoing asynchronous requests (using the `x-arango-async: store` HTTP header
 - Finished asynchronous requests, whose result has not yet been
   collected
 - Queued low priority requests (most normal requests)
 - Ongoing low priority requests

Note that the latter two are tracked but not prevented from being
started, to allow for finishing operations.

This query parameter was introduced in 3.7.12, 3.8.1 and all versions
greater or equal than 3.9.

@RESTDESCRIPTION
This call initiates a clean shutdown sequence. Requires administrive privileges.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned in all cases, `OK` will be returned in the result buffer on success.

@endDocuBlock
