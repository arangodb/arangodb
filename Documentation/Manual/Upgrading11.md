Upgrading to ArangoDB 1.1 {#Upgrading11}
========================================

@NAVIGATE_Upgrading11
@EMBEDTOC{Upgrading11TOC}

Upgrading {#Upgrading11Introduction}
====================================

ArangoDB 1.1 introduces new features but may in some respect have
slightly different behavior than in 1.0.

This is unavoidable as new features get introduced and others become
superfluous.

The following list contains changes in ArangoDB 1.1 that are not 100%
downwards-compatible to ArangoDB 1.0.

Existing users of ArangoDB 1.0 should read the list carefully and make
sure they have undertaken all necessary steps and precautions before
upgrading from ArangoDB 1.0 to ArangoDB 1.1. Also check
@ref Upgrading11Troubleshooting.

New Dependencies {#Upgrading11NewDependencies}
----------------------------------------------

As ArangoDB 1.1 supports SSL connections, ArangoDB can only be built
on servers with the OpenSSL library installed. The OpenSSL is not
bundled with ArangoDB and must be installed separately.

Database Directory Version Check and Upgrade {#Upgrading11VersionCheck}
-----------------------------------------------------------------------

Starting with ArangoDB 1.1, _arangod_ will perform a database version
check at startup.

It will look for a file named _VERSION_ in its database directory. If
the file is not present (it will not be present in an ArangoDB 1.0
database), _arangod_ in version 1.1 will perform an auto-upgrade.
This auto-upgrade will create the system collections necessary to run
ArangoDB 1.1, and it will also create the VERSION file.

If the _VERSION_ file is present but is from a non-matching version of
ArangoDB, _arangod_ will also refuse to start and ask the user to start
the server with the option `--upgrade`.  

This procedure shall ensure that users in the future will have full 
control over when they perform any updates/upgrades of their data, and 
do not risk running an incompatible tandem of server and database.

ArangoDB users are asked to start the server with the `--upgrade` option
when upgrading from one version of ArangoDB to a higher version (e.g. 
from 1.0 to 1.1 in this case), but also after pulling the latest ArangoDB 
source code while staying in the same minor version (e.g. when updating from
1.1-beta1 to 1.1-beta2).

The upgrade procedure is started when the server is started with the 
additional command line option `--upgrade` as follows:

    > bin/arangod --server.endpoint tcp://127.0.0.1:8529 --database.directory /tmp/voctest --upgrade

    ...
    2012-12-03T11:22:08Z [11573] INFO Starting upgrade from version 1.0 to 1.1.beta2
    2012-12-03T11:22:08Z [11573] INFO Found 9 defined task(s), 9 task(s) to run
    ...
    2012-12-03T11:22:08Z [11573] INFO Upgrade successfully finished

The upgrade procecure will execute the defined tasks to run _arangod_
with all new features and data formats. It should normally run without
problems and indicate success at script end. If it detects a problem
that it cannot fix, it will halt on the first error and warn the user.

Re-starting arangod with the `--upgrade` option will execute only the 
previously failed and not yet executed tasks.

Server Startup Options Changes {#Upgrading11ServerOptions}
----------------------------------------------------------

### Port options and endpoints

The following startup options have been removed for _arangod_ in
version 1.1:

- `--port`
- `--server.port`
- `--server.http-port`
- `--server.admin-port`

All these options are replaced by the new `--server.endpoint` option
in ArangoDB 1.1.

The server must now be started with a defined endpoint.  The new
`--server.endpoint` option is required to specify the protocol,
hostname and port the server should use when listening for incoming
connections.

The `--server.endpoint` option must be specified on server start,
either on the command line or via a configuration file, otherwise
_arangod_ will refuse to start.

The server can be bound to one or multiple endpoints at once. The
following endpoint specification sytnax is currently supported:

- `tcp://host:port` (HTTP over IPv4)
- `tcp://[host]:port` (HTTP over IPv6)
- `ssl://host:port` (HTTP over SSL-encrypted IPv4)
- `ssl://[host]:port` (HTTP over SSL-encrypted IPv6)
- `unix:///path/to/socket` (HTTP over UNIX socket)

An example value for the option is `--server.endpoint
tcp://127.0.0.1:8529`.  This will make the server listen to requests
coming in on IP address 127.0.0.1 on port 8529, and that use HTTP over
TCP/IPv4.

### Authorization

Starting from 1.1, _arangod_ may be started with authentication turned
on.  When authentication is turned on, all requests incoming to
_arangod_ via the HTTP interface must carry an _HTTP authorization_
header with a valid username and password in order to be processed.
Clients sending requests without HTTP autorization headers or with
invalid usernames/passwords will be rejected by arangod with an HTTP
401 error.

The upgrade procedure for ArangoDB 1.1 will create a default user _root_ 
with an empty password when run initially.

To turn authorization off, the server can be started with the
following command line option:

    --server.disable-authentication true

Of course this option can also be stored in a configuration file.

### HTTP keep-alive

The following _arangod_ startup options have been removed in ArangoDB
1.1:

- `--server.require-keep-alive`
- `--server.secure-require-keep-alive`

In version 1.1, the server will behave as follows automatically which
should be more conforming to the HTTP standard:

- if a client sends a `Connection: close` HTTP header, the server will
  close the connection as requested
- if a client sends a `Connection: keep-alive` HTTP header, the server
  will not close the connection but keep it alive as requested
- if a client does not send any `Connection` HTTP header, the server
  will assume _keep-alive_ if the request was an HTTP/1.1 request, and
  _close_ if the request was an HTTP/1.0 request
- dangling keep-alive connections will be closed automatically by the
  server after a configurable amount of seconds. To adjust the value,
  use the new server option `--server.keep-alive-timeout`.
- Keep-alive can be turned off in ArangoDB by setting
  `--server.keep-alive-timeout` to a value of `0`.

As ArangoDB 1.1 will use keep-alive by default for incoming HTTP/1.1
requests without a `Connection` header, using ArangoDB 1.1 from a
browser will likely result in the same connection being re-used. This
may be unintuitive because requests from a browser to ArangoDB will
effectively be serialised, not parallelised. To conduct parallel
requests from a browser, you should either set
`--server.keep-alive-timeout` to a value of `0`, or make your browser
send `Connection: close` HTTP headers with its requests.

Start / Stop Scripts {#Upgrading11StartScripts}
-----------------------------------------------

The user used in start and stop scripts has changed from _arango_ to
_arangodb_. Furthermore, the start script name itself has changed from
_arangod_ to _arangodb_. Additionally, the default database directory
name changed from _/var/arangodb_ to _/var/lib/arangodb_.  This was
necessary to be more compliant with various Linux policies.

Collection Types {#Upgrading11CollectionTypes}
----------------------------------------------

In ArangoDB 1.1, collection types have been introduced:

- regular documents go into _document_-only collections, 
- and edges go into _edge_ collections. 

The prefixing (`db.xxx` and `edges.xxx`) that could be used to access
a collection thus works slightly differently in 1.1:

`edges.xxx` can still be used to access collections, however, it will
not determine the type of existing collections anymore. In 1.0, you
could write `edges.xxx.something` and `xxx` was automatically treated
as an _edge_ collection.

As collections know and save their type in ArangoDB 1.1, this might
work slightly differently.  The type of existing collections is
immutable and not modifiable by changing the prefix (`db` or `edges`).

In 1.1, _edge_ collections can be created via the following ways:

- `edges._create()` as in 1.0
- addtionally there is a new method `db._createEdgeCollection()`

To create _document_ collections, the following methods are available: 

- `db._create()` as in 1.0, 
- additionally there is now `db._createDocumentCollection()`

Collections in 1.1 are now either _document_-only or _edge_
collections, but the two concepts cannot be mixed in the same
collection.

The upgrade procedure for ArangoDB 1.1 will determine the types of existing 
collections from 1.0 once, based on the inspection of the first 50 documents
in the collection.

If one of the documents contains either a `_from` or a `_to`
attribute, the collection is made an _edge_ collection. Otherwise, the
collection is marked as a _document_ collection.

This distinction is important because edges can only be created in
_edge_ collections starting with 1.1. User code may need to be
adjusted to work with ArangoDB 1.1 if it tries to insert edges into
_document_-only collections.

User code must also be adjusted if it uses the `ArangoEdges` or
`ArangoEdgesCollection` objects that were present in ArangoDB 1.0 on
the server. This only affects user code that was intended to be run on
the server, directly in ArangoDB. The `ArangoEdges` or
`ArangoEdgesCollection` objects were not exposed to _arangosh_ or any
other clients.

arangoimp / arangosh {#Upgrading11ShellImport}
----------------------------------------------

The parameters `--connect-timeout` and `--request-timeout` for
_arangosh_ and _arangoimp_ have been renamed to
`--server.connect-timeout` and `--server.request-timeout`.

The parameter `--server` has been removed for both _arangoimp_ and
_arangosh_.

To specify a server to connect to, the client tools now provide an
option `--server.endpoint`.  This option can be used to specify the
protocol, hostname and port for the connection.  The default endpoint
that is used when none is specified is `tcp://127.0.0.1:8529`.  For
more information on the endpoint specification syntax, see above.

The options `--server.username` and `--server.password` have been
added for _arangoimp_ and _arangosh_ in order to use authorization
from these client tools, too.

These options can be used to specify the username and password when
connecting via client tools to the _arangod_ server. If no password is
given on the command line, _arangoimp_ and _arangosh_ will
interactively prompt for a password.  If no username is specified on
the command line, the default user _root_ will be used but there will
still be a password prompt.

Change of Syslog Usage {#Upgrading11Syslog}
-------------------------------------------

In 1.0, arangod always logged its output to the syslog, regardless of
any other logging that was configured. In 1.1, this has changed. Log
messages will be sent to the syslog only if the server is started with
the `--log.syslog` option and a non-empty string (the log facility) is
given to it. This is in accordance with the 1.0 documentation.

Troubleshooting {#Upgrading11Troubleshooting}
=============================================

If you cannot find a solution here, please ask the Google-Group at 
http://groups.google.com/group/arangodb

Problem: ArangoDB does not start after upgrade
----------------------------------------------

- Check the logfile `/var/log/arangodb/arangod.log`

- Check the permissions of these directories:

  - `/var/lib/arangodb/`
  - `/var/run/arangodb/`
  - `/var/log/arangodb/`

  These directories and all files have to be readable and writable for the user
  "arangodb" and group "arangodb" (not for MacOSX). Double check that the user
  is "arangodb" not "arango".

  Change the permissions using:

      unix> chown -R arangodb:arangodb /var/lib/arangodb/ /var/run/arangodb/ /var/log/arangodb/

- Check the configuration file in:

      /etc/arangodb/arangod.conf

Problem: Packet manager finds no upgrade
----------------------------------------

- Check the name of the repository here:

    http://www.arangodb.org/download

Problem: Database is empty
--------------------------

Check that the database file 

    /var/lib/arangodb

contains your collections. If it is empty, check the old location
of the database at

    /var/arangodb

If necessary, stop the server, copy the files using

    cp /var/arangodb/* /var/lib/arangodb

and start the server again.

Removed Features {#Upgrading11RemovedFeatures}
==============================================

Removed Dependencies {#Upgrading11RemovedDependencies}
------------------------------------------------------

ArangoDB no longer requires BOOST, ZeroMQ, or ProtocolBuffers.

Removed Functionality {#Upgrading11RemovedFunctionality}
--------------------------------------------------------

### Configuration

In 1.0, there were unfinished REST APIs available at the
`/_admin/config` URL suffix.  These APIs were stubs only and have been
removed in ArangoDB 1.1.

### Front-End User and Session Management

In 1.0, there was an API to manage user and session for the GUI
administraion interface. In 1.1 the user management is part of the
database (not just the front-end). There the calls to
`_admin/user-manager` where removed.
