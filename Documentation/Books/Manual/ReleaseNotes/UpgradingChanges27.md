!CHAPTER Incompatible changes in ArangoDB 2.7

It is recommended to check the following list of incompatible changes **before** 
upgrading to ArangoDB 2.7, and adjust any client programs if necessary.


!SECTION AQL changes

`DISTINCT` is now a keyword in AQL. 

AQL queries that use `DISTINCT` (in lower, upper or mixed case) as an identifier (i.e. as a 
variable, a collection name or a function name) will stop working. To make such queries 
working again, each occurrence of `DISTINCT` in an AQL query should be enclosed in backticks.
This will turn `DISTINCT` from a keyword into an identifier again.

The AQL function `SKIPLIST()` has been removed in ArangoDB 2.7. This function was deprecated in
ArangoDB 2.6. It was a left-over from times when the query optimizer wasn't able to use skiplist 
indexes together with filters, skip and limit values. Since this issue been fixed since version 2.3, 
there is no AQL replacement function for `SKIPLIST`. Queries that use the `SKIPLIST` function 
can be fixed by using the usual combination of `FOR`, `FILTER` and `LIMIT`, e.g.

```
    FOR doc IN @@collection 
      FILTER doc.value >= @value 
      SORT doc.value DESC 
      LIMIT 1 
      RETURN doc
```

!SECTION Foxx changes

!SUBSECTION Bundling and compilation

The `assets` property is no longer supported in Foxx manifests and is scheduled to be removed in a future version of ArangoDB. The `files` property can still be used to serve static assets but it is recommended to use separate tooling to compile and bundle your assets.

!SUBSECTION Manifest scripts

The properties `setup` and `teardown` have been moved into the `scripts` property map:

**Before:**

```json
{
  ...
  "setup": "scripts/setup.js",
  "teardown": "scripts/teardown.js"
}
```

**After:**

```json
{
  ...
  "scripts": {
    "setup": "scripts/setup.js",
    "teardown": "scripts/teardown.js"
  }
}
```

!SUBSECTION Foxx Queues

Function-based Foxx Queue job types are no longer supported. To learn about how you can use the new script-based job types [follow the updated recipe in the cookbook](https://docs.arangodb.com/2.8/cookbook/FoxxQueues.html).

!SUBSECTION Foxx Sessions

The `jwt` and `type` options have been removed from the `activateSessions` API.

If you want to replicate the behavior of the `jwt` option you can use the JWT functions in the `crypto` module. A JWT-based session storage that doesn't write sessions to the database is available as the [sessions-jwt app](https://github.com/arangodb/foxx-sessions-jwt) in the Foxx app store.

The session type is now inferred from the presence of the `cookie` or `header` options (allowing you to enable support for both). If you want to use the default settings for `cookie` or `header` you can pass the value `true` instead.

The `sessionStorageApp` option has been removed in favour of the `sessionStorage` option.

**Before:**

```js
var Foxx = require('org/arangodb/foxx');
var ctrl = new Foxx.Controller(applicationContext);

ctrl.activateSessions({
  sessionStorageApp: 'some-sessions-app',
  type: 'cookie'
});
```

**After:**

```js
ctrl.activateSessions({
  sessionStorage: applicationContext.dependencies.sessions.sessionStorage,
  cookie: true
});
```

!SUBSECTION Request module

The module `org/arangodb/request` uses an internal library function for sending HTTP
requests. This library functionally unconditionally set an HTTP header `Accept-Encoding: gzip`
in all outgoing HTTP requests, without client code having to set this header explicitly.

This has been fixed in 2.7, so `Accept-Encoding: gzip` is not set automatically anymore.
Additionally the header `User-Agent: ArangoDB` is not set automatically either. If
client applications rely on these headers being sent, they are free to add it when
constructing requests using the request module.

The `internal.download()` function is also affected by this change. Again, the header
can be added here if required by passing it via a `headers` sub-attribute in the
third parameter (`options`) to this function. 


!SECTION arangodump / backups

The filenames in dumps created by arangodump now contain not only the name of the dumped 
collection, but also an additional 32-digit hash value. This is done to prevent overwriting 
dump files in case-insensitive file systems when there exist multiple collections with the 
same name (but with different cases). 

This change leads to changed filenames in dumps created by arangodump. If any client
scripts depend on the filenames in the dump output directory being equal to the collection
name plus one of the suffixes `.structure.json` and `.data.json`, they need to be adjusted.

Starting with ArangoDB 2.7, the file names will contain an underscore plus the 32-digit
MD5 value (represented in hexadecimal notation) of the collection name.

For example, when arangodump dumps data of two collections *test* and *Test*, the 
filenames in previous versions of ArangoDB were:
  
* `test.structure.json` (definitions for collection *test*)
* `test.data.json` (data for collection *test*)
* `Test.structure.json` (definitions for collection *Test*)
* `Test.data.json` (data for collection *Test*)

In 2.7, the filenames will be:

* `test_098f6bcd4621d373cade4e832627b4f6.structure.json` (definitions for collection *test*)
* `test_098f6bcd4621d373cade4e832627b4f6.data.json` (data for collection *test*)
* `Test_0cbc6611f5540bd0809a388dc95a615b.structure.json` (definitions for collection *Test*)
* `Test_0cbc6611f5540bd0809a388dc95a615b.data.json` (data for collection *Test*)


!SECTION Starting / stopping

When starting arangod, the server will now drop the process privileges to the 
specified values in options `--server.uid` and `--server.gid` instantly after 
parsing the startup options.

That means when either `--server.uid` or `--server.gid` are set, the privilege
change will happen earlier. This may prevent binding the server to an endpoint 
with a port number lower than 1024 if the arangodb user has no privileges 
for that. Previous versions of ArangoDB changed the privileges later, so some
startup actions were still carried out under the invoking user (i.e. likely 
*root* when started via init.d or system scripts) and especially binding to
low port numbers was still possible there.

The default privileges for user *arangodb* will not be sufficient for binding
to port numbers lower than 1024. To have an ArangoDB 2.7 bind to a port number 
lower than 1024, it needs to be started with either a different privileged user,
or the privileges of the *arangodb* user have to raised manually beforehand.

Additionally, Linux startup scripts and systemd configuration for arangod now 
will adjust the NOFILE (number of open files) limits for the process. The limit
value is set to 131072 (128k) when ArangoDB is started via start/stop commands.
The goal of this change is to prevent arangod from running out of available
file descriptors for socket connections and datafiles.


!SECTION Connection handling

arangod will now actually close lingering client connections when idle for at least 
the duration specified in the `--server.keep-alive-timeout` startup option.
 
In previous versions of ArangoDB, idle connections were not closed by the server 
when the timeout was reached and the client was still connected. Now the 
connection is properly closed by the server in case of timeout. Client
applications relying on the old behavior may now need to reconnect to the
server when their idle connections time out and get closed (note: connections 
being idle for a long time may be closed by the OS or firewalls anyway - 
client applications should be aware of that and try to reconnect).


!SECTION Option changes

!SUBSECTION Configure options removed

The following options for `configure` have been removed because they were unused
or exotic:

* `--enable-timings`
* `--enable-figures`

!SUBSECTION Startup options added

The following configuration options have been added in 2.7:

* `--database.query-cache-max-results`: sets the maximum number of results in AQL 
  query result cache per database 
* `--database.query-cache-mode`: sets the mode for the AQL query results cache. 
  Possible values are `on`, `off` and `demand`. The default value is `off`


!SECTION Miscellaneous changes

!SUBSECTION Simple queries

Many simple queries provide a `skip()` function that can be used to skip over a certain number
of documents in the result. This function allowed specifying negative offsets in previous versions
of ArangoDB. Specifying a negative offset led to the query result being iterated in reverse order,
so skipping was performed from the back of the result. As most simple queries do not provide a
guaranteed result order, skipping from the back of a result with unspecific order seems a rather
exotic use case and was removed to increase consistency with AQL, which also does not provide
negative skip values.

Negative skip values were deprecated in ArangoDB 2.6.

!SUBSECTION Tasks API

The undocumented function `addJob()` has been removed from the `org/arangodb/tasks` module in
ArangoDB 2.7. 

!SUBSECTION Runtime endpoints manipulation API

The following HTTP REST API methods for runtime manipulation of server endpoints have been
removed in ArangoDB 2.7:

* POST `/_api/endpoint`: to dynamically add an endpoint while the server was running
* DELETE `/_api/endpoint`: to dynamically remove an endpoint while the server was running

This change also affects the equivalent JavaScript endpoint manipulation methods available 
in Foxx. The following functions have been removed in ArangoDB 2.7:

* `db._configureEndpoint()`
* `db._removeEndpoint()` 

