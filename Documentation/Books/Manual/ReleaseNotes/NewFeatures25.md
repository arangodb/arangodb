Features and Improvements in ArangoDB 2.5
=========================================

The following list shows in detail which features have been added or improved in
ArangoDB 2.5. ArangoDB 2.5 also contains several bugfixes that are not listed
here. For a list of bugfixes, please consult the [CHANGELOG](https://github.com/arangodb/arangodb/blob/devel/CHANGELOG).


V8 version upgrade
------------------

The built-in version of V8 has been upgraded from 3.29.54 to 3.31.74.1.
This allows activating additional ES6 (also dubbed *Harmony* or *ES.next*) features 
in ArangoDB, both in the ArangoShell and the ArangoDB server. They can be
used for scripting and in server-side actions such as Foxx routes, traversals
etc.

The following additional ES6 features become available in ArangoDB 2.5 by default:

* iterators and generators
* template strings
* enhanced object literals
* enhanced numeric literals
* block scoping with `let` and constant variables using `const` (note: constant 
  variables require using strict mode, too)
* additional string methods (such as `startsWith`, `repeat` etc.) 


Index improvements
------------------

### Sparse hash and skiplist indexes

Hash and skiplist indexes can optionally be made sparse. Sparse indexes exclude documents
in which at least one of the index attributes is either not set or has a value of `null`.
 
As such documents are excluded from sparse indexes, they may contain fewer documents than
their non-sparse counterparts. This enables faster indexing and can lead to reduced memory
usage in case the indexed attribute does occur only in some, but not all documents of the 
collection. Sparse indexes will also reduce the number of collisions in non-unique hash
indexes in case non-existing or optional attributes are indexed.

In order to create a sparse index, an object with the attribute `sparse` can be added to
the index creation commands:

```js
db.collection.ensureHashIndex(attributeName, { sparse: true }); 
db.collection.ensureHashIndex(attributeName1, attributeName2, { sparse: true }); 
db.collection.ensureUniqueConstraint(attributeName, { sparse: true }); 
db.collection.ensureUniqueConstraint(attributeName1, attributeName2, { sparse: true }); 

db.collection.ensureSkiplist(attributeName, { sparse: true }); 
db.collection.ensureSkiplist(attributeName1, attributeName2, { sparse: true }); 
db.collection.ensureUniqueSkiplist(attributeName, { sparse: true }); 
db.collection.ensureUniqueSkiplist(attributeName1, attributeName2, { sparse: true }); 
```

Note that in place of the above specialized index creation commands, it is recommended to use
the more general index creation command `ensureIndex`:

```js
db.collection.ensureIndex({ type: "hash", sparse: true, unique: true, fields: [ attributeName ] });
db.collection.ensureIndex({ type: "skiplist", sparse: false, unique: false, fields: [ "a", "b" ] });
```

When not explicitly set, the `sparse` attribute defaults to `false` for new hash or 
skiplist indexes.
  
This causes a change in behavior when creating a unique hash index without specifying the 
sparse flag: in 2.4, unique hash indexes were implicitly sparse, always excluding `null` values. 
There was no option to control this behavior, and sparsity was neither supported for non-unique
hash indexes nor skiplists in 2.4. This implicit sparsity of unique hash indexes was considered
an inconsistency, and therefore the behavior was cleaned up in 2.5. As of 2.5, indexes will
only be created sparse if sparsity is explicitly requested. Existing unique hash indexes from 2.4 
or before will automatically be migrated so they are still sparse after the upgrade to 2.5.

Geo indexes are implicitly sparse, meaning documents without the indexed location attribute or
containing invalid location coordinate values will be excluded from the index automatically. This
is also a change when compared to pre-2.5 behavior, when documents with missing or invalid
coordinate values may have caused errors on insertion when the geo index' `unique` flag was set
and its `ignoreNull` flag was not. This was confusing and has been rectified in 2.5. The method
`ensureGeoConstraint()` now does the same as `ensureGeoIndex()`. Furthermore, the attributes
`constraint`, `unique`, `ignoreNull` and `sparse` flags are now completely ignored when creating
geo indexes.

The same is true for fulltext indexes. There is no need to specify non-uniqueness or sparsity for 
geo or fulltext indexes.

As sparse indexes may exclude some documents, they cannot be used for every type of query.
Sparse hash indexes cannot be used to find documents for which at least one of the indexed 
attributes has a value of `null`. For example, the following AQL query cannot use a sparse 
index, even if one was created on attribute `attr`:

    FOR doc In collection 
      FILTER doc.attr == null 
      RETURN doc

If the lookup value is non-constant, a sparse index may or may not be used, depending on
the other types of conditions in the query. If the optimizer can safely determine that
the lookup value cannot be `null`, a sparse index may be used. When uncertain, the optimizer
will not make use of a sparse index in a query in order to produce correct results.

For example, the following queries cannot use a sparse index on `attr` because the optimizer
will not know beforehand whether the comparison values for `doc.attr` will include `null`:

    FOR doc In collection 
      FILTER doc.attr == SOME_FUNCTION(...) 
      RETURN doc

    FOR other IN otherCollection 
      FOR doc In collection 
        FILTER doc.attr == other.attr 
        RETURN doc

Sparse skiplist indexes can be used for sorting if the optimizer can safely detect that the 
index range does not include `null` for any of the index attributes. 


### Selectivity estimates

Indexes of type `primary`, `edge` and `hash` now provide selectivity estimates. These 
will be used by the AQL query optimizer when deciding about index usage. Using selectivity
estimates can lead to faster query execution when more selective indexes are used.

The selectivity estimates are also returned by the `GET /_api/index` REST API method
in a sub-attribute `selectivityEstimate` for each index that supports it. This
attribute will be omitted for indexes that do not provide selectivity estimates.
If provided, the selectivity estimate will be a numeric value between 0 and 1.

Selectivity estimates will also be reported in the result of `collection.getIndexes()`
for all indexes that support this. If no selectivity estimate can be determined for 
an index, the attribute `selectivityEstimate` will be omitted here, too.

The web interface also shows selectivity estimates for each index that supports this.

Currently the following index types can provide selectivity estimates:
- primary index
- edge index
- hash index (unique and non-unique)

No selectivity estimates will be provided for indexes when running in cluster mode.


AQL Optimizer improvements
--------------------------

### Sort removal

The AQL optimizer rule "use-index-for-sort" will now remove sorts also in case a non-sorted
index (e.g. a hash index) is used for only equality lookups and all sort attributes are covered 
by the equality lookup conditions.

For example, in the following query the extra sort on `doc.value` will be optimized away
provided there is an index on `doc.value`):

    FOR doc IN collection 
      FILTER doc.value == 1 
      SORT doc.value 
      RETURN doc

The AQL optimizer rule "use-index-for-sort" now also removes sort in case the sort criteria
excludes the left-most index attributes, but the left-most index attributes are used
by the index for equality-only lookups.

For example, in the following query with a skiplist index on `value1`, `value2`, the sort can
be optimized away:

    FOR doc IN collection 
      FILTER doc.value1 == 1 
      SORT doc.value2 
      RETURN doc


### Constant attribute propagation

The new AQL optimizer rule `propagate-constant-attributes` will look for attributes that are
equality-compared to a constant value, and will propagate the comparison value into other
equality lookups. This rule will only look inside `FILTER` conditions, and insert constant
values found in `FILTER`s, too.

For example, the rule will insert `42` instead of `i.value` in the second `FILTER` of the 
following query:

    FOR i IN c1 
      FOR j IN c2 
        FILTER i.value == 42 
        FILTER j.value == i.value 
        RETURN 1


### Interleaved processing

The optimizer will now inspect AQL data-modification queries and detect if the query's 
data-modification part can run in lockstep with the data retrieval part of the query, 
or if the data retrieval part must be executed and completed first before the data-modification 
can start.

Executing both data retrieval and data-modification in lockstep allows using much smaller 
buffers for intermediate results, reducing the memory usage of queries. Not all queries are
eligible for this optimization, and the optimizer will only apply the optimization when it can
safely detect that the data-modification part of the query will not modify data to be found
by the retrieval part.


### Query execution statistics

The `filtered` attribute was added to AQL query execution statistics. The value of this
attribute indicates how many documents were filtered by `FilterNode`s in the AQL query.
Note that `IndexRangeNode`s can also filter documents by selecting only the required ranges
from the index. The `filtered` value will not include the work done by `IndexRangeNode`s, 
but only the work performed by `FilterNode`s.


Language improvements
---------------------

### Dynamic attribute names in AQL object literals
  
This change allows using arbitrary expressions to construct attribute names in object
literals specified in AQL queries. To disambiguate expressions and other unquoted 
attribute names, dynamic attribute names need to be enclosed in brackets (`[` and `]`).

Example:

    FOR i IN 1..100
      RETURN { [ CONCAT('value-of-', i) ] : i }

### AQL functions

The following AQL functions were added in 2.5:

* `MD5(value)`: generates an MD5 hash of `value`
* `SHA1(value)`: generates an SHA1 hash of `value`
* `RANDOM_TOKEN(length)`: generates a random string value of the specified length

Simplify Foxx usage
-------------------

Thanks to our user feedback we learned that Foxx is a powerful, yet rather complicated concept.
With 2.5 we made it less complicated while keeping all its strength.
That includes a rewrite of the documentation as well as some code changes as follows:

### Moved Foxx applications to a different folder.

Until 2.4 Foxx apps were stored in the following folder structure:
`<app-path>/databases/<dbname>/<appname>:<appversion>`.
This caused some trouble as apps where cached based on name and version and updates did not apply.
Also the path on filesystem and the app's access URL had no relation to one another.
Now the path on filesystem is identical to the URL (except the appended APP):
`<app-path>/_db/<dbname>/<mointpoint>/APP`

### Rewrite of Foxx routing

The routing of Foxx has been exposed to major internal changes we adjusted because of user feedback.
This allows us to set the development mode per mount point without having to change paths and hold
apps at separate locations.

### Foxx Development mode

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

### Foxx install process

Installing Foxx apps has been a two step process: import them into ArangoDB and mount them at a
specific mount point. These operations have been joined together. You can install an app at one
mount point, that's it. No fetch, mount, unmount, purge cycle anymore. The commands have been 
simplified to just:

* install: get your Foxx app up and running
* uninstall: shut it down and erase it from disk

### Foxx error output

Until 2.4 the errors produced by Foxx were not optimal. Often, the error message was just
`unable to parse manifest` and contained only an internal stack trace.
In 2.5 we made major improvements there, including a much more fine grained error output that
helps you debug your Foxx apps. The error message printed is now much closer to its source and 
should help you track it down.

Also we added the default handlers for unhandled errors in Foxx apps:

* You will get a nice internal error page whenever your Foxx app is called but was not installed
  due to any error
* You will get a proper error message when having an uncaught error appears in any app route

In production mode the messages above will NOT contain any information about your Foxx internals
and are safe to be exposed to third party users.
In development mode the messages above will contain the stacktrace (if available), making it easier for
your in-house devs to track down errors in the application.

### Foxx console

We added a `console` object to Foxx apps. All Foxx apps now have a console object implementing
the familiar Console API in their global scope, which can be used to log diagnostic
messages to the database. This console also allows to read the error output of one specific Foxx.

### Foxx requests
We added `org/arangodb/request` module, which provides a simple API for making HTTP requests
to external services. This is enables Foxx to be directly part of a micro service architecture.

