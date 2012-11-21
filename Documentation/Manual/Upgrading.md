Upgrading to ArangoDB 1.1 {#Upgrading}
======================================

ArangoDB 1.1 introduces new features but may in some respect have slightly different behavior than 1.0. 

This is unavoidable as new features get introduced and others become superfluous.

The following list contains changes in ArangoDB 1.1 that are not 100% downwards-compatible to ArangoDB 1.0. 

Existing users of ArangoDB 1.0 should read the list carefully and make sure they have undertaken all 
necessary steps and precautions before upgrading from ArangoDB 1.0 to ArangoDB 1.1.

## Database directory version check and upgrade

Starting with ArangoDB 1.1, _arangod_ will perform a database version check at startup. 

It will look for a file named _VERSION_ in its database directory. If the file is not present 
(it will not be present in an ArangoDB 1.0 database), _arangod_ in version 1.1 will refuse to start 
and ask the user to run the script _arango-upgrade_ first.

If the _VERSION_ file is present but is from a non-matching version of ArangoDB, _arangod_ will also 
refuse to start and ask the user to run the upgrade script first.
This procedure shall ensure that users have full control over when they perform any updates/upgrades 
of their data, and do not risk running an incompatible tandem of server and database.

ArangoDB users are asked to run _arango-upgrade_ when upgrading from one version of ArangoDB to a 
higher version (e.g. from 1.0 to 1.1 in this case), but also after pulling the latest ArangoDB source 
code while staying in the same minor version (e.g. when updating from 1.1-beta1 to 1.1-beta2).

When installing ArangoDB from scratch, users should also run _arango-upgrade_ once to initialise their 
database directory with some system collections that ArangoDB requires. When not run, _arangod_ will refuse 
to start as mentioned before.

_arango-upgrade_ can be invoked from the command-line, and takes the database directory as its 
only argument:

    > bin/arango-upgrade --database.directory /tmp/voctest

    ...
    2012-11-21T18:00:38Z [2411] INFO Upgrade script ./js/server/arango-upgrade.js started
    2012-11-21T18:00:38Z [2411] INFO Server version: 1.1.beta2, database directory version: (not set)
    2012-11-21T18:00:38Z [2411] INFO Found 9 defined task(s), 9 task(s) to run
    2012-11-21T18:00:38Z [2411] INFO Executing task #1 (setupUsers): setup _users collection
    2012-11-21T18:00:38Z [2411] INFO Task successful
    ...
    2012-11-21T18:00:40Z [2411] INFO Upgrade script successfully finished
    2012-11-21T18:00:42Z [2411] INFO ArangoDB has been shut down

The _arango-upgrade_ will execute the defined tasks to run _arangod_ with all new features and 
data formats. It should normally run without problems and indicate success at script end. If it detects
a problem that it cannot fix, it will halt on the first error and warn the user.

Re-executing _arango-upgrade_ will execute only the previously failed and not yet executed tasks.

_arango-upgrade_ needs exclusive access to the database, so it cannot be executed while an
instance of _arangod_ is currently running. 


## Server startup options changes

### Port options and endpoints

The following startup options have been removed for _arangod_ in version 1.1:
- `--port`
- `--server.port`
- `--server.http-port`
- `--server.admin-port`

All these options are replaced by the new `--server.endpoint` option in ArangoDB 1.1.


The server must now be started with a defined endpoint. 
The new `--server.endpoint` option is required to specify the protocol, hostname and port the 
server should use when listening for incoming connections.

The `--server.endpoint` option must be specified on server start, either on the command line or via 
a configuration file, otherwise _arangod_ will refuse to start.

The server can be bound to one or multiple endpoints at once. The following endpoint 
specification sytnax is currently supported:
- `tcp://host:port (HTTP over IPv4)`
- `tcp://[host]:port (HTTP over IPv6)`
- `ssl://host:port (HTTP over SSL-encrypted IPv4)`
- `ssl://[host]:port (HTTP over SSL-encrypted IPv6)`
- `unix://path/to/socket (HTTP over UNIX socket)`

An example value for the option is `--server.endpoint tcp://127.0.0.1:8529`. 
This will make the server listen to requests coming in on IP address 127.0.0.1 on port 8529, 
and that use HTTP over TCP/IPv4.

### Authorization

Starting from 1.1, _arangod_ may be started with authentication turned on.
When authentication is turned on, all requests incoming to _arangod_ via the HTTP interface must 
carry an _HTTP authorization_ header with a valid username and password in order to be processed.
Clients sending requests without HTTP autorization headers or with invalid usernames/passwords 
will be rejected by arangod with an HTTP 401 error.

_arango-upgrade_ will create a default user _root_ with an empty password when run initially.

To turn authorization off, the server can be started with the following command line option:
    --server.disable-authentication true
Of course this option can also be stored in a configuration file.


### HTTP keep-alive

The following _arangod_ startup options have been removed in ArangoDB 1.1:
- `--server.require-keep-alive`
- `--server.secure-require-keep-alive`

In version 1.1, The server will now behave as follows automatically which should be more 
conforming to the HTTP standard:
- if a client sends a `Connection: close` HTTP header, the server will close the connection as 
  requested
- if a client sends a `Connection: keep-alive` HTTP header, the server will not close the 
  connection but keep it alive as requested
- if a client does not send any `Connection` HTTP header, the server will assume _keep-alive_ 
  if the request was an HTTP/1.1 request, and "close" if the request was an HTTP/1.0 request
- dangling keep-alive connections will be closed automatically by the server after a configurable 
  amount of seconds. To adjust the value, use the new server option `--server.keep-alive-timeout`


## Start / stop scripts

The user used in start and stop scripts has changed from _arango_ to _arangodb_. Furthermore,
the start script name itself has changed from _arangod_ to _arangodb_. Additionally, the default 
database directory name changed from _/var/arangodb_ to _/var/lib/arangodb_.
This was done to be more compliant with various Linux policies.


## Collection types

In ArangoDB 1.1, collection types have been introduced: 
- regular documents go into _document_-only collections, 
- and edges go into _edge_ collections. 

The prefixing (`db.xxx` and `edges.xxx`) that could be used to access a collection thus works 
slightly differently in 1.1: 

`edges.xxx` can still be used to access collections, however, it will not determine the type 
of existing collections anymore. In 1.0, you could write `edges.xxx.something` and `xxx` was 
automatically treated as an _edge_ collection.

As collections know and save their type in ArangoDB 1.1, this might work slightly differently.
The type of existing collections is immutable and not modifiable by changing the prefix (`db` or `edges`).

In 1.1, _edge_ collections can be created via the following ways:
- `edges._create()` as in 1.0
- addtionally there is a new method `db._createEdgeCollection()`

To create _document_ collections, the following methods are available: 
- `db._create()` as in 1.0, 
- additionally there is now `db._createDocumentCollection()`

Collections in 1.1 are now either _document_-only or _edge_ collections, but the two concepts 
cannot be mixed in the same collection. 

_arango-upgrade_ will determine the types of existing collections from 1.0 once on upgrade, 
based on the inspection of the first 50 documents in the collection. 

If one of the documents contains either a `_from` or a `_to` attribute, the collection is made an 
_edge_ collection. Otherwise, the collection is marked as a _document_ collection.

This distinction is important because edges can only be created in _edge_ collections starting 
with 1.1. Client code may need to be adjusted to work with ArangoDB 1.1 if it tries to insert
edges into _document_-only collections.


## arangoimp / arangosh

The parameters `--connect-timeout` and `--request-timeout` for _arangosh_ and _arangoimp_ have been 
renamed to `--server.connect-timeout` and `--server.request-timeout`.

The parameter `--server` has been removed for both _arangoimp_ and _arangosh_. 

To specify a server to connect to, the client tools now provide an option `--server.endpoint`. 
This option can be used to specify the protocol, hostname and port for the connection. 
The default endpoint that is used when none is specified is `tcp://127.0.0.1:8529`. 
For more information on the endpoint specification syntax, see above.

The options `--server.username` and `--server.password` have been added for _arangoimp_ and _arangosh_
in order to use authorization from these client tools, too.

These options can be used to specify the username and password when connecting via client tools 
to the _arangod_ server. If no password is given on the command line, _arangoimp_ and _arangosh_ 
will interactively prompt for a password.
If no username is specified on the command line, the default user _root_ will be used but there 
will still be a password prompt. 
