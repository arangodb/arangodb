
@startDocuBlock foxxQueues
@brief enable or disable the Foxx queues feature
`--foxx.queues flag`

If *true*, the Foxx queues will be available and jobs in the queues will
be executed asynchronously.

The default is *true*.
When set to `false` the queue manager will be disabled and any jobs
are prevented from being processed, which may reduce CPU load a bit.
@endDocuBlock

