
@startDocuBlock foxxQueuesPollInterval
@brief poll interval for Foxx queues
`--foxx.queues-poll-interval value`

The poll interval for the Foxx queues manager. The value is specified in
seconds. Lower values will mean more immediate and more frequent Foxx
queue job execution, but will make the queue thread wake up and query the
queues more often. When set to a low value, the queue thread might cause
CPU load.

The default is *1* second. If Foxx queues are not used much, then this
value may be increased to make the queues thread wake up less.
@endDocuBlock

