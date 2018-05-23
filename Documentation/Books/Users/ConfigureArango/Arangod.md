Command-Line Options for arangod
================================

### Endpoint
@startDocuBlock serverEndpoint


### Reuse address
@startDocuBlock serverReuseAddress


### Disable authentication
@startDocuBlock server_authentication


### Disable authentication-unix-sockets
@startDocuBlock serverAuthenticationDisable


### Authenticate system only
@startDocuBlock serverAuthenticateSystemOnly


### Disable replication-applier
@startDocuBlock serverDisableReplicationApplier


### Keep-alive timeout
@startDocuBlock keep_alive_timeout


### Default API compatibility
@startDocuBlock serverDefaultApi


### Hide Product header
@startDocuBlock serverHideProductHeader


### Allow method override
@startDocuBlock serverAllowMethod


### Server threads
@startDocuBlock serverThreads


### Keyfile
@startDocuBlock serverKeyfile


### Cafile
@startDocuBlock serverCafile


### SSL protocol
@startDocuBlock serverSSLProtocol


### SSL cache
@startDocuBlock serverSSLCache


### SSL options
@startDocuBlock serverSSLOptions


### SSL cipher
@startDocuBlock serverSSLCipher


### Backlog size
@startDocuBlock serverBacklog


### Disable server statistics

`--server.disable-statistics value`

If this option is *value* is *true*, then ArangoDB's statistics gathering
is turned off. Statistics gathering causes regular CPU activity so using this
option to turn it off might relieve heavy-loaded instances.
Note: this option is only available when ArangoDB has not been compiled with
the option *--disable-figures*.


### Session timeout
@startDocuBlock SessionTimeout


### Foxx queues
@startDocuBlock foxxQueues


### Foxx queues poll interval
@startDocuBlock foxxQueuesPollInterval


### Directory
@startDocuBlock DatabaseDirectory


### Journal size
@startDocuBlock databaseMaximalJournalSize


### Wait for sync
@startDocuBlock databaseWaitForSync


### Force syncing of properties
@startDocuBlock databaseForceSyncProperties


### Disable AQL query tracking
@startDocuBlock databaseDisableQueryTracking


### Throw collection not loaded error
@startDocuBlock databaseThrowCollectionNotLoadedError


### AQL Query caching mode
@startDocuBlock queryCacheMode


### AQL Query cache size
@startDocuBlock queryCacheMaxResults


### Index threads
@startDocuBlock indexThreads


### V8 contexts
@startDocuBlock v8Contexts


### Garbage collection frequency (time-based)
@startDocuBlock jsGcFrequency


### Garbage collection interval (request-based)
@startDocuBlock jsStartupGcInterval


### V8 options
@startDocuBlock jsV8Options
