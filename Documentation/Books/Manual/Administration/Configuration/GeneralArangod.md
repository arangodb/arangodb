General Options
===============

### Database Upgrade

`--database.auto-upgrade`

Specifying this option will make the server perform a database upgrade at
start. A database upgrade will first compare the version number stored in the
file VERSION in the database directory with the current server version.

If the two version numbers match, the server will start normally.

If the version number found in the database directory is higher than the version
number the server is running, the server expects this is an unintentional
downgrade and will warn about this. It will however start normally. Using the
server in these conditions is however not recommended nor supported.

If the version number found in the database directory is lower than the version
number the server is running, the server will check whether there are any
upgrade tasks to perform. It will then execute all required upgrade tasks and
print their statuses. If one of the upgrade tasks fails, the server will exit
and refuse to start. Re-starting the server with the upgrade option will then
again trigger the upgrade check and execution until the problem is fixed. If all
tasks are finished, the server will start normally.

Whether or not this option is specified, the server will always perform a
version check on startup. Running the server with a non-matching version number
in the VERSION file will make the server refuse to start.

### Storage Engine
As of ArangoDB 3.2 two storage engines are supported. The "traditional"
engine is called `MMFiles`, which is also the default storage engine.

An alternative engine based on [RocksDB](http://rocksdb.org) is also provided and
can be turned on manually.

One storage engine type is supported per server per installation. 
Live switching of storage engines on already installed systems isn't supported.
Configuring the wrong engine (not matching the previously used one) will result
in the server refusing to start. You may however use `auto` to let ArangoDB choose 
the previously used one. 


`--server.storage-engine [auto|mmfiles|rocksdb]`

### Daemon

`--daemon`

Runs the server as a daemon (as a background process). This parameter can only
be set if the pid (process id) file is specified. That is, unless a value to the
parameter pid-file is given, then the server will report an error and exit.


### Default Language

`--default-language default-language`

The default language ist used for sorting and comparing strings.  The language
value is a two-letter language code (ISO-639) or it is composed by a two-letter
language code with and a two letter country code (ISO-3166). Valid languages are
"de", "en", "en_US" or "en_UK".

The default default-language is set to be the system locale on that platform.


### Supervisor

`--supervisor`

Executes the server in supervisor mode. In the event that the server
unexpectedly terminates due to an internal error, the supervisor will
automatically restart the server. Setting this flag automatically implies that
the server will run as a daemon. Note that, as with the daemon flag, this flag
requires that the pid-file parameter will set.

```js
unix> ./arangod --supervisor --pid-file /var/run/arangodb.pid /tmp/vocbase/
2012-06-27T15:58:28Z [10133] INFO starting up in supervisor mode
```

As can be seen (e.g. by executing the ps command), this will start a supervisor
process and the actual database process:

```js
unix> ps fax | grep arangod
10137 ?        Ssl    0:00 ./arangod --supervisor --pid-file /var/run/arangodb.pid /tmp/vocbase/
10142 ?        Sl     0:00  \_ ./arangod --supervisor --pid-file /var/run/arangodb.pid /tmp/vocbase/
```

When the database process terminates unexpectedly, the supervisor process will
start up a new database process:

```
> kill -SIGSEGV 10142

> ps fax | grep arangod
10137 ?        Ssl    0:00 ./arangod --supervisor --pid-file /var/run/arangodb.pid /tmp/vocbase/
10168 ?        Sl     0:00  \_ ./arangod --supervisor --pid-file /var/run/arangodb.pid /tmp/vocbase/
```


### User identity

`--uid uid`

The name (identity) of the user the server will run as. If this parameter is not
specified, the server will not attempt to change its UID, so that the UID used
by the server will be the same as the UID of the user who started the server. If
this parameter is specified, then the server will change its UID after opening
ports and reading configuration files, but before accepting connections or
opening other files (such as recovery files).  This is useful when the server
must be started with raised privileges (in certain environments) but security
considerations require that these privileges be dropped once the server has
started work.

Observe that this parameter cannot be used to bypass operating system
security. In general, this parameter (and its corresponding relative gid) can
lower privileges but not raise them.


### Group identity

`--gid gid`

The name (identity) of the group the server will run as. If this parameter is
not specified, then the server will not attempt to change its GID, so that the
GID the server runs as will be the primary group of the user who started the
server. If this parameter is specified, then the server will change its GID
after opening ports and reading configuration files, but before accepting
connections or opening other files (such as recovery files).

This parameter is related to the parameter uid.


### Process identity

`--pid-file filename`

The name of the process ID file to use when running the server as a
daemon. This parameter must be specified if either the flag *daemon* or
*supervisor* is set.

### Check max memory mappings

`--server.check-max-memory-mappings` can be used on Linux to make arangod 
check the number of memory mappings currently used by the process (as reported in
*/proc/<pid>/maps*) and compare it with the maximum number of allowed mappings as 
determined by */proc/sys/vm/max_map_count*. If the current number of memory
mappings gets near the maximum allowed value, arangod will log a warning
and disallow the creation of further V8 contexts temporarily until the current
number of mappings goes down again. 

If the option is set to false, no such checks will be performed. All non-Linux
operating systems do not provide this option and will ignore it.


### Console

`--console`

Runs the server in an exclusive emergency console mode. When
starting the server with this option, the server is started with
an interactive JavaScript emergency console, with all networking
and HTTP interfaces of the server disabled.

No requests can be made to the server in this mode, and the only
way to work with the server in this mode is by using the emergency
console.
Note that the server cannot be started in this mode if it is
already running in this or another mode.


### Random Generator

`--random.generator arg`

The argument is an integer (1,2,3 or 4) which sets the manner in which
random numbers are generated. The default method (3) is to use the a
non-blocking random (or pseudorandom) number generator supplied by the
operating system.

Specifying an argument of 2, uses a blocking random (or
pseudorandom) number generator. Specifying an argument 1 sets a
pseudorandom
number generator using an implication of the Mersenne Twister MT19937
algorithm. Algorithm 4 is a combination of the blocking random number
generator and the Mersenne Twister.


### Enable/disable authentication

@startDocuBlock server_authentication

### JWT Secret

`--server.jwt-secret secret`

ArangoDB will use JWTs to authenticate requests. Using this option lets
you specify a JWT.

In single server setups and when not specifying this secret ArangoDB will
generate a secret.

In cluster deployments which have authentication enabled a secret must
be set consistently across all cluster tasks so they can talk to each other.

### Enable/disable authentication for UNIX domain sockets

`--server.authentication-unix-sockets value`

Setting *value* to true will turn off authentication on the server side
for requests coming in via UNIX domain sockets. With this flag enabled,
clients located on the same host as the ArangoDB server can use UNIX domain
sockets to connect to the server without authentication.
Requests coming in by other means (e.g. TCP/IP) are not affected by this option.

The default value is *false*.

**Note**: this option is only available on platforms that support UNIX
domain sockets.


### Enable/disable authentication for system API requests only

@startDocuBlock serverAuthenticateSystemOnly


### Enable/disable replication applier

`--database.replication-applier flag`

If *false* the server will start with replication appliers turned off,
even if the replication appliers are configured with the *autoStart* option.
Using the command-line option will not change the value of the *autoStart*
option in the applier configuration, but will suppress auto-starting the
replication applier just once.

If the option is not used, ArangoDB will read the applier configuration
from the file *REPLICATION-APPLIER-CONFIG* on startup, and use the value of the
*autoStart* attribute from this file.

The default is *true*.

### Keep-alive timeout

`--http.keep-alive-timeout`

Allows to specify the timeout for HTTP keep-alive connections. The timeout
value must be specified in seconds.
Idle keep-alive connections will be closed by the server automatically
when the timeout is reached. A keep-alive-timeout value 0 will disable the keep
alive feature entirely.

### Hide Product header

`--http.hide-product-header`

If *true*, the server will exclude the HTTP header "Server: ArangoDB" in
HTTP responses. If set to *false*, the server will send the header in
responses.

The default is *false*.


### Allow method override

`--http.allow-method-override`

When this option is set to *true*, the HTTP request method will optionally
be fetched from one of the following HTTP request headers if present in
the request:

- *x-http-method*
- *x-http-method-override*
- *x-method-override*

If the option is set to *true* and any of these headers is set, the
request method will be overridden by the value of the header. For example,
this allows issuing an HTTP DELETE request which to the outside world will
look like an HTTP GET request. This allows bypassing proxies and tools that
will only let certain request types pass.

Setting this option to *true* may impose a security risk so it should only
be used in controlled environments.

The default value for this option is *false*.

### Server threads

`--server.threads number`

Specifies the *number* of threads that are spawned to handle requests.

### Toggling server statistics

`--server.statistics value`

If this option is *value* is *false*, then ArangoDB's statistics gathering
is turned off. Statistics gathering causes regular CPU activity so using this
option to turn it off might relieve heavy-loaded instances a bit.

### Session timeout

time to live for server sessions
`--server.session-timeout value`

The timeout for web interface sessions, using for authenticating requests
to the web interface (/_admin/aardvark) and related areas.

Sessions are only used when authentication is turned on.

### Foxx queues
@startDocuBlock foxxQueues

### Foxx queues poll interval
@startDocuBlock foxxQueuesPollInterval

### Directory

`--database.directory directory`

The directory containing the collections and datafiles. Defaults
to */var/lib/arango*. When specifying the database directory, please
make sure the directory is actually writable by the arangod process.

You should further not use a database directory which is provided by a
network filesystem such as NFS. The reason is that networked filesystems
might cause inconsistencies when there are multiple parallel readers or
writers or they lack features required by arangod (e.g. flock()).

`directory`

When using the command line version, you can simply supply the database
directory as argument.


**Examples**


```
> ./arangod --server.endpoint tcp://127.0.0.1:8529 --database.directory
/tmp/vocbase
```


### Journal size
@startDocuBlock databaseMaximalJournalSize


### Wait for sync
@startDocuBlock databaseWaitForSync


### Force syncing of properties
@startDocuBlock databaseForceSyncProperties


### Limiting memory for AQL queries

`--query.memory-limit value`

The default maximum amount of memory (in bytes) that a single AQL query can use.
When a single AQL query reaches the specified limit value, the query will be
aborted with a *resource limit exceeded* exception. In a cluster, the memory
accounting is done per shard, so the limit value is effectively a memory limit per
query per shard.

The global limit value can be overriden per query by setting the *memoryLimit* 
option value for individual queries when running an AQL query. 

The default value is *0*, meaning that there is no memory limit.


### Turning AQL warnings into errors

`--query.fail-on-warning value`

When set to *true*, AQL queries that produce warnings will instantly abort and
throw an exception. This option can be set to catch obvious issues with AQL 
queries early. When set to *false*, AQL queries that produce warnings will not
abort and return the warnings along with the query results.
The option can also be overridden for each individual AQL query.


### Enable/disable AQL query tracking

`--query.tracking flag`

If *true*, the server's AQL slow query tracking feature will be enabled by
default. Tracking of queries can be disabled by setting the option to *false*.

The default is *true*.


### Enable/disable tracking of bind variables in AQL queries 

`--query.tracking-with-bindvars flag`

If *true*, then the bind variables will be tracked for all running and slow
AQL queries. This option only has an effect if `--query.tracking` was set to
*true*. Tracking of bind variables can be disabled by setting the option to *false*.

The default is *true*.


### Threshold for slow AQL queries

`--query.slow-threshold value`

By setting *value* it can be controlled after what execution time an AQL query
is considered "slow". Any slow queries that exceed the execution time specified
in *value* will be logged when they are finished. The threshold value is
specified in seconds. Tracking of slow queries can be turned off entirely by
setting the option `--query.tracking` to *false*.

The default value is *10.0*.


### Throw collection not loaded error

`--database.throw-collection-not-loaded-error flag`

Accessing a not-yet loaded collection will automatically load a collection on
first access. This flag controls what happens in case an operation would need to
wait for another thread to finalize loading a collection. If set to *true*, then
the first operation that accesses an unloaded collection will load it. Further
threads that try to access the same collection while it is still loading will
get an error (1238, *collection not loaded*). When the initial operation has
completed loading the collection, all operations on the collection can be
carried out normally, and error 1238 will not be thrown.

If set to *false*, the first thread that accesses a not-yet loaded collection
will still load it. Other threads that try to access the collection while
loading will not fail with error 1238 but instead block until the collection is
fully loaded. This configuration might lead to all server threads being blocked
because they are all waiting for the same collection to complete
loading. Setting the option to *true* will prevent this from happening, but
requires clients to catch error 1238 and react on it (maybe by scheduling a
retry for later).

The default value is *false*.


### AQL Query caching mode

`--query.cache-mode`

Toggles the AQL query cache behavior. Possible values are:

* *off*: do not use query cache
* *on*: always use query cache, except for queries that have their *cache*
  attribute set to *false*
* *demand*: use query cache only for queries that have their *cache*
  attribute set to *true*


### AQL Query cache size

`--query.cache-entries`

Maximum number of query results that can be stored per database-specific query
cache. If a query is eligible for caching and the number of items in the
database's query cache is equal to this threshold value, another cached query
result will be removed from the cache.

This option only has an effect if the query cache mode is set to either *on* or
*demand*.


### JavaScript code execution

`--javascript.allow-admin-execute`

This option can be used to control whether user-defined JavaScript code
is allowed to be executed on server by sending via HTTP to the API endpoint
`/_admin/execute`  with an authenticated user account. 
The default value is *false*, which disables the execution of user-defined
code. This is also the recommended setting for production. In test environments,
it may be convenient to turn the option on in order to send arbitrary setup
or teardown commands for execution on the server.


### V8 contexts

`--javascript.v8-contexts number`

Specifies the maximum *number* of V8 contexts that are created for executing 
JavaScript code. More contexts allow executing more JavaScript actions in parallel, 
provided that there are also enough threads available. Please note that each V8 context
will use a substantial amount of memory and requires periodic CPU processing
time for garbage collection.

Note that this value configures the maximum number of V8 contexts that can be
used in parallel. Upon server start only as many V8 contexts will be created as are
configured in option `--javascript.v8-contexts-minimum`. The actual number of
available V8 contexts may float at runtime between `--javascript.v8-contexts-minimum`
and `--javascript.v8-contexts`. When there are unused V8 contexts that linger around, 
the server's garbage collector thread will automatically delete them.


`--javascript.v8-contexts-minimum number`

Specifies the minimum *number* of V8 contexts that will be present at any time
the server is running. The actual number of V8 contexts will never drop below this
value, but it may go up as high as specified via the option `--javascript.v8-contexts`.

When there are unused V8 contexts that linger around and the number of V8 contexts
is greater than `--javascript.v8-contexts-minimum` the server's garbage collector 
thread will automatically delete them.
 
  
`--javascript.v8-contexts-max-invocations`

Specifies the maximum number of invocations after which a used V8 context is 
disposed. The default value of `--javascript.v8-contexts-max-invocations` is 0, 
meaning that the maximum number of invocations per context is unlimited. 

`--javascript.v8-contexts-max-age`

Specifies the time duration (in seconds) after which time a V8 context is disposed 
automatically after its creation. If the time is elapsed, the context will be disposed.
The default value for `--javascript.v8-contexts-max-age` is 60 seconds.

If both `--javascript.v8-contexts-max-invocations` and `--javascript.v8-contexts-max-age`
are set, then the context will be destroyed when either of the specified threshold 
values is reached.


### Garbage collection frequency (time-based)

`--javascript.gc-frequency frequency`

Specifies the frequency (in seconds) for the automatic garbage collection of
JavaScript objects. This setting is useful to have the garbage collection still
work in periods with no or little numbers of requests.


### Garbage collection interval (request-based)

`--javascript.gc-interval interval`

Specifies the interval (approximately in number of requests) that the garbage
collection for JavaScript objects will be run in each thread.


### V8 options

`--javascript.v8-options options`

Optional arguments to pass to the V8 Javascript engine. The V8 engine will run
with default settings unless explicit options are specified using this
option. The options passed will be forwarded to the V8 engine which will parse
them on its own. Passing invalid options may result in an error being printed on
stderr and the option being ignored.

Options need to be passed in one string, with V8 option names being prefixed
with double dashes. Multiple options need to be separated by whitespace.  To get
a list of all available V8 options, you can use the value *"--help"* as follows:

```
--javascript.v8-options="--help"
```

Another example of specific V8 options being set at startup:

```
--javascript.v8-options="--log"
```

Names and features or usable options depend on the version of V8 being used, and
might change in the future if a different version of V8 is being used in
ArangoDB. Not all options offered by V8 might be sensible to use in the context
of ArangoDB. Use the specific options only if you are sure that they are not
harmful for the regular database operation.
