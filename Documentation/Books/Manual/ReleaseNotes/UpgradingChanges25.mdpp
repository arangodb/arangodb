!CHAPTER Incompatible changes in ArangoDB 2.5

It is recommended to check the following list of incompatible changes **before** 
upgrading to ArangoDB 2.5, and adjust any client programs if necessary.


!SECTION Changed behavior

!SUBSECTION V8

The V8 version shipped with ArangoDB was upgraded from 3.29.59 to 3.31.74.1.
This leads to additional ECMAScript 6 (ES6 or "harmony") features being enabled by 
default in ArangoDB's scripting environment.

Apart from that, a change in the interpretation of command-line options by V8 may
affect users. ArangoDB passes the value of the command-line option `--javascript.v8-options`
to V8 and leaves interpretation of the contents to V8. For example, the ArangoDB option
`--javascript.v8-options="--harmony"` could be used to tell V8 to enable its harmony 
features.

In ArangoDB 2.4, the following harmony options were made available by V8:

* --harmony_scoping (enable harmony block scoping)
* --harmony_modules (enable harmony modules (implies block scoping))
* --harmony_proxies (enable harmony proxies)
* --harmony_generators (enable harmony generators)
* --harmony_numeric_literals (enable harmony numeric literals (0o77, 0b11))
* --harmony_strings (enable harmony string)
* --harmony_arrays (enable harmony arrays)
* --harmony_arrow_functions (enable harmony arrow functions)
* --harmony_classes (enable harmony classes)
* --harmony_object_literals (enable harmony object literal extensions)
* --harmony (enable all harmony features (except proxies))

There was the option `--harmony`, which turned on almost all harmony features.

In ArangoDB 2.5, V8 provides the following harmony-related options:
  
* --harmony (enable all completed harmony features)
* --harmony_shipping (enable all shipped harmony features)
* --harmony_modules (enable "harmony modules (implies block scoping)" (in progress))
* --harmony_arrays (enable "harmony array methods" (in progress))
* --harmony_array_includes (enable "harmony Array.prototype.includes" (in progress))
* --harmony_regexps (enable "harmony regular expression extensions" (in progress))
* --harmony_arrow_functions (enable "harmony arrow functions" (in progress))
* --harmony_proxies (enable "harmony proxies" (in progress))
* --harmony_sloppy (enable "harmony features in sloppy mode" (in progress))
* --harmony_unicode (enable "harmony unicode escapes" (in progress))
* --harmony_tostring (enable "harmony toString")
* --harmony_numeric_literals (enable "harmony numeric literals")
* --harmony_strings (enable "harmony string methods")
* --harmony_scoping (enable "harmony block scoping")
* --harmony_classes (enable "harmony classes (implies block scoping & object literal extension)")
* --harmony_object_literals (enable "harmony object literal extensions")
* --harmony_templates (enable "harmony template literals")

Note that there are extra options for better controlling the dedicated features,
and especially that the meaning of the `--harmony` option has changed from enabling
**all** harmony features to **all completed** harmony features!

Users should adjust the value of `--javascript.v8-options` accordingly.

Please note that incomplete harmony features are subject to change in future V8 releases.


!SUBSECTION Sparse indexes 

Hash indexes and skiplist indexes can now be created in a sparse variant. 
When not explicitly set, the `sparse` attribute defaults to `false` for new indexes.
  
This causes a change in behavior when creating a unique hash index without specifying the 
sparse flag. The unique hash index will be created in a non-sparse variant in ArangoDB 2.5. 

In 2.4 and before, unique hash indexes were implicitly sparse, always excluding `null` values 
from the index. There was no option to control this behavior, and sparsity was neither supported 
for non-unique hash indexes nor skiplists in 2.4. This implicit sparsity of just unique hash 
indexes was considered an inconsistency, and therefore the behavior was cleaned up in 2.5. 

As of 2.5, hash and skiplist indexes will only be created sparse if sparsity is explicitly requested. 
This may require a change in index-creating client code, but only if the client code creates 
unique hash indexes and if they are still intended to be sparse. In this case, the client code 
should explicitly set the `sparse` flag to `true` when creating a unique hash index.

Existing unique hash indexes from 2.4 or before will automatically be migrated so they are still 
sparse after the upgrade to 2.5. For these indexes, the `sparse` attribute will be populated
automatically with a value of `true`. 
  
Geo indexes are implicitly sparse, meaning documents without the indexed location attribute or
containing invalid location coordinate values will be excluded from the index automatically. This
is also a change when compared to pre-2.5 behavior, when documents with missing or invalid
coordinate values may have caused errors on insertion when the geo index' `unique` flag was set
and its `ignoreNull` flag was not. 

This was confusing and has been rectified in 2.5. The method `ensureGeoConstraint()` now does the 
same as `ensureGeoIndex()`. Furthermore, the attributes `constraint`, `unique`, `ignoreNull` and 
`sparse` flags are now completely ignored when creating geo indexes. Client index creation code
therefore does not need to set the `ignoreNull` or `constraint` attributes when creating a geo
index.

The same is true for fulltext indexes. There is no need to specify non-uniqueness or sparsity for 
geo or fulltext indexes. They will always be non-unique and sparse. 


!SUBSECTION Moved Foxx applications to a different folder.

Until 2.4 foxx apps were stored in the following folder structure:
`<app-path>/databases/<dbname>/<appname>:<appversion>`.
This caused some trouble as apps where cached based on name and version and updates did not apply.
Also the path on filesystem and the app's access URL had no relation to one another.
Now the path on filesystem is identical to the URL (except the appended APP):
`<app-path>/_db/<dbname>/<mointpoint>/APP`

!SUBSECTION Foxx Development mode

The development mode used until 2.4 is gone. It has been replaced by a much more mature version.
This includes the deprecation of the javascript.dev-app-path parameter, which is useless since 2.5.
Instead of having two separate app directories for production and development, apps now reside in 
one place, which is used for production as well as for development.
Apps can still be put into development mode, changing their behavior compared to production mode.
Development mode apps are still reread from disk at every request, and still they ship more debug 
output.

This change has also made the startup options `--javascript.frontend-development-mode` and 
`--javascript.dev-app-path` obsolete. The former option will not have any effect when set, and the
latter option is only read and used during the upgrade to 2.5 and does not have any effects later.

!SUBSECTION Foxx install process

Installing Foxx apps has been a two step process: import them into ArangoDB and mount them at a
specific mountpoint. These operations have been joined together. You can install an app at one
mountpoint, that's it. No fetch, mount, unmount, purge cycle anymore. The commands have been 
simplified to just:

* install: get your Foxx app up and running
* uninstall: shut it down and erase it from disk
!SECTION Deprecated features

* Foxx: method `Model#toJSONSchema(id)` is deprecated, it will raise a warning if you use it. Please use `Foxx.toJSONSchema(id, model)` instead.

!SECTION Removed features

* Startup switch `--javascript.frontend-development-mode`: Its major purpose was internal development
anyway. Now the web frontend can be set to development mode similar to any other foxx app.
* Startup switch `--javascript.dev-app-path`: Was used for the development mode of Foxx. This is
integrated with the normal app-path now and can be triggered on app level. The second app-path is
superfluous.
* Foxx: `controller.collection`: Please use `appContext.collection` instead.
* Foxx: `FoxxRepository.modelPrototype`: Please use `FoxxRepository.model` instead.
* Foxx: `Model.extend({}, {attributes: {}})`: Please use `Model.extend({schema: {}})` instead.
* Foxx: `requestContext.bodyParam(paramName, description, Model)`: Please use `requestContext.bodyParam(paramName, options)` instead.
* Foxx: `requestContext.queryParam({type: string})`: Please use `requestContext.queryParam({type: joi})` instead.
* Foxx: `requestContext.pathParam({type: string})`: Please use `requestContext.pathParam({type: joi})` instead.
* Graph: The modules `org/arangodb/graph` and `org/arangodb/graph-blueprint`: Please use module `org/arangodb/general-graph` instead. NOTE: This does not mean we do not support blueprints any more. General graph covers everything the graph--blueprint did, plus many more features.
* General-Graph: In the module `org/arangodb/general-graph` the functions `_undirectedRelation` and `_directedRelation` are no longer available. Both functions have been unified to `_relation`.

