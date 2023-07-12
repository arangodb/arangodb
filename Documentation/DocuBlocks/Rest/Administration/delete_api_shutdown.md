@startDocuBlock delete_api_shutdown

@RESTHEADER{DELETE /_admin/shutdown, Start the shutdown sequence, startShutdown}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{soft,boolean,optional}
<small>Introduced in: v3.7.12, v3.8.1, v3.9.0</small>

If set to `true`, this initiates a soft shutdown. This is only available
on Coordinators. When issued, the Coordinator tracks a number of ongoing
operations, waits until all have finished, and then shuts itself down
normally. It will still accept new operations.

This feature can be used to make restart operations of Coordinators less
intrusive for clients. It is designed for setups with a load balancer in front
of Coordinators. Remove the designated Coordinator from the load balancer before
issuing the soft-shutdown. The remaining Coordinators will internally forward
requests that need to be handled by the designated Coordinator. All other
requests will be handled by the remaining Coordinators, reducing the designated
Coordinator's load.

The following types of operations are tracked:

 - AQL cursors (in particular streaming cursors)
 - Transactions (in particular stream transactions)
 - Pregel runs (conducted by this Coordinator)
 - Ongoing asynchronous requests (using the `x-arango-async: store` HTTP header)
 - Finished asynchronous requests, whose result has not yet been
   collected
 - Queued low priority requests (most normal requests)
 - Ongoing low priority requests

@RESTDESCRIPTION
This call initiates a clean shutdown sequence. Requires administrative privileges.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned in all cases, `OK` will be returned in the result buffer on success.

@endDocuBlock
