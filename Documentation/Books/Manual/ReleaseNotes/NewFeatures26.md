Features and Improvements in ArangoDB 2.6
=========================================

The following list shows in detail which features have been added or improved in
ArangoDB 2.6. ArangoDB 2.6 also contains several bugfixes that are not listed
here. For a list of bugfixes, please consult the
[CHANGELOG](https://github.com/arangodb/arangodb/blob/devel/CHANGELOG).

APIs added
----------

### Batch document removal and lookup commands

The following commands have been added for `collection` objects:

* `collection.lookupByKeys(keys)`
* `collection.removeByKeys(keys)`

These commands can be used to perform multi-document lookup and removal operations efficiently
from the ArangoShell. The argument to these operations is an array of document keys.

These commands can also be used via the HTTP REST API. Their endpoints are:

* `PUT /_api/simple/lookup-by-keys`
* `PUT /_api/simple/remove-by-keys`

### Collection export HTTP REST API

ArangoDB now provides a dedicated collection export API, which can take snapshots of entire
collections more efficiently than the general-purpose cursor API. The export API is useful
to transfer the contents of an entire collection to a client application. It provides optional
filtering on specific attributes.

The export API is available at endpoint `POST /_api/export?collection=...`. The API has the
same return value structure as the already established cursor API (`POST /_api/cursor`). 

An introduction to the export API is given in this blog post:
http://jsteemann.github.io/blog/2015/04/04/more-efficient-data-exports/

AQL improvements
----------------

### EDGES AQL Function

The AQL function EDGES got a new fifth optional parameter, which must be an 
object if specified. Right now only one option is available for it: 

* `includeVertices` this is a boolean parameter that allows to modify the result of 
  `EDGES()`. The default value for `includeVertices` is `false`, which does not have 
  any effect. Setting it to `true` will modify the result, such that also the connected
  vertices are returned along with the edges:

      { vertex: <vertexDocument>, edge: <edgeDocument> } 

### Subquery optimizations for AQL queries

This optimization avoids copying intermediate results into subqueries that are not required
by the subquery.

A brief description can be found here:
http://jsteemann.github.io/blog/2015/05/04/subquery-optimizations/

### Return value optimization for AQL queries

This optimization avoids copying the final query result inside the query's main `ReturnNode`.

A brief description can be found here:
http://jsteemann.github.io/blog/2015/05/04/return-value-optimization-for-aql/

### Speed up AQL queries containing big `IN` lists for index lookups

`IN` lists used for index lookups had performance issues in previous versions of ArangoDB.
These issues have been addressed in 2.6 so using bigger `IN` lists for filtering is much
faster.

A brief description can be found here:
http://jsteemann.github.io/blog/2015/05/07/in-list-improvements/

### Added alternative implementation for AQL COLLECT

The alternative method uses a hash table for grouping and does not require its input elements
to be sorted. It will be taken into account by the optimizer for `COLLECT` statements that do
not use an `INTO` clause.

In case a `COLLECT` statement can use the hash table variant, the optimizer will create an extra
plan for it at the beginning of the planning phase. In this plan, no extra `SORT` node will be
added in front of the `COLLECT` because the hash table variant of `COLLECT` does not require
sorted input. Instead, a `SORT` node will be added after it to sort its output. This `SORT` node
may be optimized away again in later stages. If the sort order of the result is irrelevant to
the user, adding an extra `SORT null` after a hash `COLLECT` operation will allow the optimizer to
remove the sorts altogether.

In addition to the hash table variant of `COLLECT`, the optimizer will modify the original plan
to use the regular `COLLECT` implementation. As this implementation requires sorted input, the
optimizer will insert a `SORT` node in front of the `COLLECT`. This `SORT` node may be optimized
away in later stages.

The created plans will then be shipped through the regular optimization pipeline. In the end,
the optimizer will pick the plan with the lowest estimated total cost as usual. The hash table
variant does not require an up-front sort of the input, and will thus be preferred over the
regular `COLLECT` if the optimizer estimates many input elements for the `COLLECT` node and 
cannot use an index to sort them.

The optimizer can be explicitly told to use the regular *sorted* variant of `COLLECT` by
suffixing a `COLLECT` statement with `OPTIONS { "method" : "sorted" }`. This will override the
optimizer guesswork and only produce the *sorted* variant of `COLLECT`.

A blog post on the new `COLLECT` implementation can be found here:
http://jsteemann.github.io/blog/2015/04/22/collecting-with-a-hash-table/

### Simplified return value syntax for data-modification AQL queries

ArangoDB 2.4 since version allows to return results from data-modification AQL queries. The
syntax for this was quite limited and verbose:

```
FOR i IN 1..10
  INSERT { value: i } IN test
  LET inserted = NEW
  RETURN inserted
```
The `LET inserted = NEW RETURN inserted` was required literally to return the inserted
documents. No calculations could be made using the inserted documents.

This is now more flexible. After a data-modification clause (e.g. `INSERT`, `UPDATE`, `REPLACE`,
`REMOVE`, `UPSERT`) there can follow any number of `LET` calculations. These calculations can
refer to the pseudo-values `OLD` and `NEW` that are created by the data-modification statements.

This allows returning projections of inserted or updated documents, e.g.:

```
FOR i IN 1..10
  INSERT { value: i } IN test
  RETURN { _key: NEW._key, value: i }
```

Still not every construct is allowed after a data-modification clause. For example, no functions
can be called that may access documents.

More information can be found here:
http://jsteemann.github.io/blog/2015/03/27/improvements-for-data-modification-queries/

### Added AQL `UPSERT` statement

This adds an `UPSERT` statement to AQL that is a combination of both `INSERT` and `UPDATE` /
`REPLACE`. The `UPSERT` will search for a matching document using a user-provided example.
If no document matches the example, the *insert* part of the `UPSERT` statement will be
executed. If there is a match, the *update* / *replace* part will be carried out:

```
UPSERT { page: 'index.html' }               /* search example */
INSERT { page: 'index.html', pageViews: 1 } /* insert part */
UPDATE { pageViews: OLD.pageViews + 1 }     /* update part */
IN pageViews
```

`UPSERT` can be used with an `UPDATE` or `REPLACE` clause. The `UPDATE` clause will perform
a partial update of the found document, whereas the `REPLACE` clause will replace the found
document entirely. The `UPDATE` or `REPLACE` parts can refer to the pseudo-value `OLD`, which
contains all attributes of the found document.

`UPSERT` statements can optionally return values. In the following query, the return
attribute `found` will return the found document before the `UPDATE` was applied. If no
document was found, `found` will contain a value of `null`. The `updated` result attribute will
contain the inserted / updated document:

```
UPSERT { page: 'index.html' }               /* search example */
INSERT { page: 'index.html', pageViews: 1 } /* insert part */
UPDATE { pageViews: OLD.pageViews + 1 }     /* update part */
IN pageViews
RETURN { found: OLD, updated: NEW }
```

A more detailed description of `UPSERT` can be found here:
http://jsteemann.github.io/blog/2015/03/27/preview-of-the-upsert-command/

### Miscellaneous AQL changes

When errors occur inside AQL user functions, the error message will now contain a stacktrace,
indicating the line of code in which the error occurred. This should make debugging AQL user functions
easier.

Web Admin Interface
-------------------

ArangoDB's built-in web interface now uses sessions. Session information is stored in cookies, so clients 
using the web interface must accept cookies in order to use it.

The new startup option `--server.session-timeout` can be used for adjusting the session lifetime.

The AQL editor in the web interface now provides an *explain* functionality, which can be used for inspecting and performance-tuning AQL queries.
The query execution time is now also displayed in the AQL editor.

Foxx apps that require configuration or are missing dependencies are now indicated in the app overview and details.

Foxx improvements
-----------------

### Configuration and Dependencies

Foxx app manifests can now define configuration options, as well as dependencies on other Foxx apps.

An introduction to Foxx configurations can be found in the blog:
https://www.arangodb.com/2015/05/reusable-foxx-apps-with-configurations/

And the blog post on Foxx dependencies can be found here:
https://www.arangodb.com/2015/05/foxx-dependencies-for-more-composable-foxx-apps/

### Mocha Tests

You can now write tests for your Foxx apps using the Mocha testing framework:
https://www.arangodb.com/2015/04/testing-foxx-mocha/

A recipe for writing tests for your Foxx apps can be found in the cookbook:
https://docs.arangodb.com/2.8/Cookbook/FoxxTesting.html

### API Documentation

The API documentation has been updated to Swagger 2. You can now also mount API
documentation in your own Foxx apps.

Also see the blog post introducing this feature:
https://www.arangodb.com/2015/05/document-your-foxx-apps-with-swagger-2/

### Custom Scripts and Foxx Queue

In addition to the existing *setup* and *teardown* scripts you can now define custom scripts
in your Foxx manifest and invoke these using the web admin interface or the Foxx manager CLI.
These scripts can now also take positional arguments and export return values.

Job types for the Foxx Queue can now be defined as a script name and app mount path allowing
the use of Foxx scripts as job types. The pre-2.6 job types are known to cause issues when
restarting the server and are error-prone; we strongly recommended converting any existing
job types to the new format.

Client tools
------------

The default configuration value for the option `--server.request-timeout` was increased from 
300 to 1200 seconds for all client tools (arangosh, arangoimp, arangodump, arangorestore).

The default configuration value for the option `--server.connect-timeout` was increased from 
3 to 5 seconds for client tools (arangosh, arangoimp, arangodump, arangorestore).

### Arangorestore

The option `--create-database` was added for arangorestore. 

Setting this option to `true` will now create the target database if it does not exist. When creating
the target database, the username and passwords passed to arangorestore will be used to create an 
initial user for the new database.

The default value for this option is `false`.

### Arangoimp

Arangoimp can now optionally update or replace existing documents, provided the import data contains 
documents with `_key` attributes.

Previously, the import could be used for inserting new documents only, and re-inserting a document with 
an existing key would have failed with a *unique key constraint violated* error.
  
The behavior of arangoimp (insert, update, replace on duplicate key) can now be controlled with the 
option `--on-duplicate`. The option can have one of the following values:
  
* `error`: when a unique key constraint error occurs, do not import or update the document but
  report an error. This is the default.
    
* `update`: when a unique key constraint error occurs, try to (partially) update the existing
  document with the data specified in the import. This may still fail if the document would
  violate secondary unique indexes. Only the attributes present in the import data will be
  updated and other attributes already present will be preserved. The number of updated documents
  will be reported in the `updated` attribute of the HTTP API result.

* `replace`: when a unique key constraint error occurs, try to fully replace the existing
  document with the data specified in the import. This may still fail if the document would
  violate secondary unique indexes. The number of replaced documents will be reported in the 
  `updated` attribute of the HTTP API result.

* `ignore`: when a unique key constraint error occurs, ignore this error. There will be no
  insert, update or replace for the particular document. Ignored documents will be reported
  separately in the `ignored` attribute of the HTTP API result.

The default value is `error`.

A few examples for using arangoimp with the `--on-duplicate` option can be found here:
http://jsteemann.github.io/blog/2015/04/14/updating-documents-with-arangoimp/

Miscellaneous changes
---------------------

* Some Linux-based ArangoDB packages are now using tcmalloc for memory allocator.

* Upgraded ICU library to version 54. This increases performance in many places.

* Allow to split an edge index into buckets which are resized individually. The default value is `1`,
  resembling the pre-2.6 behavior. Using multiple buckets will lead to the index entries being 
  distributed to the individual buckets, with each bucket being responsible only for a fraction of
  the total index entries. Using multiple buckets may lead to more frequent but much faster index
  bucket resizes, and is recommended for bigger edge collections.


* Default configuration value for option `--server.backlog-size` was changed from 10 to 64.

* Default configuration value for option `--database.ignore-datafile-errors` was changed from `true` to `false`

* Document keys can now contain `@` and `.` characters

* Fulltext index can now index text values contained in direct sub-objects of the indexed attribute.

  Previous versions of ArangoDB only indexed the attribute value if it was a string. Sub-attributes
  of the index attribute were ignored when fulltext indexing.

  Now, if the index attribute value is an object, the object's values will each be included in the
  fulltext index if they are strings. If the index attribute value is an array, the array's values
  will each be included in the fulltext index if they are strings.

  For example, with a fulltext index present on the `translations` attribute, the following text
  values will now be indexed:

      var c = db._create("example");
      c.ensureFulltextIndex("translations");
      c.insert({ translations: { en: "fox", de: "Fuchs", fr: "renard", ru: "лиса" } });
      c.insert({ translations: "Fox is the English translation of the German word Fuchs" });
      c.insert({ translations: [ "ArangoDB", "document", "database", "Foxx" ] });

      c.fulltext("translations", "лиса").toArray();       // returns only first document
      c.fulltext("translations", "Fox").toArray();        // returns first and second documents
      c.fulltext("translations", "prefix:Fox").toArray(); // returns all three documents

* Added configuration option `--server.foxx-queues-poll-interval`

  This startup option controls the frequency with which the Foxx queues manager is checking
  the queue (or queues) for jobs to be executed.

  The default value is `1` second. Lowering this value will result in the queue manager waking
  up and checking the queues more frequently, which may increase CPU usage of the server. 
  When not using Foxx queues, this value can be raised to save some CPU time.

* Added configuration option `--server.foxx-queues`

  This startup option controls whether the Foxx queue manager will check queue and job entries
  in the `_system` database only. Restricting the Foxx queue manager to the `_system` database 
  will lead to the queue manager having to check only the queues collection of a single database,
  whereas making it check the queues of all databases might result in more work to be done and 
  more CPU time to be used by the queue manager.
