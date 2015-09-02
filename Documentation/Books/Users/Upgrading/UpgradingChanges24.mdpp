!CHAPTER Incompatible changes in ArangoDB 2.4

It is recommended to check the following list of incompatible changes **before** 
upgrading to ArangoDB 2.4, and adjust any client programs if necessary.


!SECTION Changed behavior

!SUBSECTION V8 upgrade

The bundled V8 version has been upgraded from 3.16.14 to 3.29.59.

The new version provides better error checking, which can lead to subtle changes 
in the execution of JavaScript code. 

The following code, though nonsense, runs without error in 2.3 and 2.4 when 
strict mode is not enabled: 

    (function () { 
      a = true; 
      a.foo = 1; 
    })();

When enabling strict mode, the function will throw an error in 2.4 but not in 2.3:

    (function () { 
      "use strict"; 
      a = true; 
      a.foo = 1; 
    })();

    TypeError: Cannot assign to read only property 'foo' of true

Though this is a change in behavior it can be considered an improvement. The new version actually 
uncovers an error that went undetected in the old version.

Error messages have also changed slightly in the new version. Applications that
rely on the exact error messages of the JavaScript engine may need to be adjusted so they
look for the updated error messages.

!SUBSECTION Default endpoint

The default endpoint for arangod is now `127.0.0.1`.

This change will modify the IP address ArangoDB listens on to 127.0.0.1 by default.
This will make new ArangoDB installations unaccessible from clients other than
localhost unless the configuration is changed. This is a security feature. 

To make ArangoDB accessible from any client, change the server's configuration 
(`--server.endpoint`) to either `tcp://0.0.0.0:8529` or the server's publicly
visible IP address.

!SUBSECTION Replication

System collections are now included in the replication and all replication API return 
values by default. 

This will lead to user accounts and credentials data being replicated from master to 
slave servers. This may overwrite slave-specific database users.

This may be considered a feature or an anti-feature, so it is configurable.

If replication of system collections is undesired, they can be excluded from replication
by setting the `includeSystem` attribute to `false` in the following commands:

* initial synchronization: `replication.sync({ includeSystem: false })`
* continuous replication: `replication.applier.properties({ includeSystem: false })`

This will exclude all system collections (including `_aqlfunctions`, `_graphs` etc.)
from the initial synchronization and the continuous replication.

If this is also undesired, it is also possible to specify a list of collections to
exclude from the initial synchronization and the continuous replication using the
`restrictCollections` attribute, e.g.:

    require("org/arangodb/replication").applier.properties({ 
      includeSystem: true,
      restrictType: "exclude",
      restrictCollections: [ "_users", "_graphs", "foo" ] 
    });

The above example will in general include system collections, but will exclude the
specified three collections from continuous replication.

The HTTP REST API methods for fetching the replication inventory and for dumping 
collections also support the `includeSystem` control flag via a URL parameter of
the same name.

!SECTION Build process changes

Several options for the `configure` command have been removed in 2.4. The options

* `--enable-all-in-one-v8`
* `--enable-all-in-one-icu`
* `--enable-all-in-one-libev`
* `--with-libev=DIR`
* `--with-libev-lib=DIR`
* `--with-v8=DIR`
* `--with-v8-lib=DIR`
* `--with-icu-config=FILE`

are not available anymore because the build process will always use the bundled
versions of the libraries.

When building ArangoDB from source in a directory that already contained a pre-2.4 
version, it will be necessary to run a `make superclean` command once and a full
rebuild afterwards:

    git pull
    make superclean
    make setup
    ./configure <options go here>
    make

!SECTION Miscellaneous changes

As a consequence of global renaming in the codebase, the option `mergeArrays` has
been renamed to `mergeObjects`. This option controls whether JSON objects will be 
merged on an update operation or overwritten. The default has been, and still is, 
to merge. Not specifying the parameter will lead to a merge, as it has been the
behavior in ArangoDB ever since.

This affects the HTTP REST API method PATCH `/_api/document/collection/key`. Its
optional URL parameter `mergeArrays` for the option has been renamed to `mergeObjects`. 

The AQL `UPDATE` statement is also affected, as its option `mergeArrays` has also
been renamed to `mergeObjects`. The 2.3 query

    UPDATE doc IN collection WITH { ... } IN collection OPTIONS { mergeArrays: false }

should thus be rewritten to the following in 2.4:

    UPDATE doc IN collection WITH { ... } IN collection OPTIONS { mergeObjects: false }


!SECTION Deprecated features

For `FoxxController` objects, the method `collection()` is deprecated and will be
removed in future version of ArangoDB. Using this method will issue a warning. 
Please use `applicationContext.collection()` instead.

For `FoxxRepository` objects, the property `modelPrototype` is now deprecated.
Using it will issue a warning. Please use `FoxxRepository.model` instead.

In `FoxxController` / `RequestContext`, calling method `bodyParam()` with three 
arguments is deprecated. Please use `.bodyParam(paramName, options)` instead.

In `FoxxController` / `RequestContext` calling method `queryParam({type: string})` 
is deprecated. Please use `requestContext.queryParam({type: joi})` instead.

In `FoxxController` / `RequestContext` calling method `pathParam({type: string})` 
is deprecated. Please use `requestContext.pathParam({type: joi})` instead.

For `FoxxModel`, calling `Model.extend({}, {attributes: {}})` is deprecated. 
Please use `Model.extend({schema: {}})` instead.

In module `org/arangodb/general-graph`, the functions `_undirectedRelation()` 
and `_directedRelation()` are deprecated and will be removed in a future version
of ArangoDB. Both functions have been unified to `_relation()`.

The modules `org/arangodb/graph` and `org/arangodb/graph-blueprint` are deprecated. 
Please use module `org/arangodb/general-graph` instead.

The HTTP REST API `_api/graph` and all its methods are deprecated. Please use 
the general graph API `_api/gharial` instead.


!SECTION Removed features

The following replication-related JavaScript methods became obsolete in ArangoDB
2.2 and have been removed in ArangoDB 2.4:

* `require("org/arangodb/replication").logger.start()`
* `require("org/arangodb/replication").logger.stop()`
* `require("org/arangodb/replication").logger.properties()`

The REST API methods for these functions have also been removed in ArangoDB 2.4:  

* HTTP PUT `/_api/replication/logger-start` 
* HTTP PUT `/_api/replication/logger-stop` 
* HTTP GET `/_api/replication/logger-config` 
* HTTP PUT `/_api/replication/logger-config` 

Client applications that call one of these methods should be adjusted by removing
the calls to these methods. This shouldn't be problematic as these methods have
been no-ops since ArangoDB 2.2 anyway.

