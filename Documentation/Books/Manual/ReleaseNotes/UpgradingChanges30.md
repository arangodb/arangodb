!CHAPTER Incompatible changes in ArangoDB 3.0

It is recommended to check the following list of incompatible changes **before**
upgrading to ArangoDB 3.0, and adjust any client programs if necessary.

!SECTION Build system

Building ArangoDB 3.0 from source now requires CMake.

The pre-3.0 build system used a configure-based approach. The steps to build
ArangoDB 2.8 from source code were:

```
make setup
./configure <options>
make
```

These steps will not work anymore, as ArangoDB 3.0 does not come with a
configure script.

To build 3.0 on Linux, create a separate build directory first:

```
mkdir -p build
```

and then create the initial build scripts once using CMake:

```
(cd build && cmake <options> ..)
```

The above command will configure the build and check for the required
dependencies. If everything works well the actual build can be started with

```
(cd build && make)
```

The binaries for the ArangoDB server and all client tools will then be created
inside the `build` directory. To start ArangoDB locally from the `build` directory,
use

```
build/bin/arangod <options>
```

!SECTION Datafiles and datafile names

ArangoDB 3.0 uses a new VelocyPack-based format for storing data in WAL logfiles
and collection datafiles. The file format is not compatible with the files used
in prior versions of ArangoDB. That means datafiles written by ArangoDB 3.0 cannot be
used in earlier versions and vice versa.

The pattern for collection directory names was changed in 3.0 to include a random 
id component at the end. The new pattern is `collection-<id>-<random>`, where `<id>` 
is the collection id and `<random>` is a random number. Previous versions of ArangoDB 
used a pattern `collection-<id>` without the random number.

!SECTION User Management

Unlike ArangoDB 2.x, ArangoDB 3.0 users are now separated from databases, and you can 
grant one or more database permissions to a user.

If you want to mimic the behavior of ArangoDB, you should name your users like 
`username@dbname`.

Users that can access the *_system* database are allowed to manage users and
permissions for all databases.

!SECTION Edges and edges attributes

In ArangoDB prior to 3.0 the attributes `_from` and `_to` of edges were treated
specially when loading or storing edges. That special handling led to these attributes
being not as flexible as regular document attributes. For example, the `_from` and 
`_to` attribute values of an existing edge could not be updated once the edge was
created. Additionally, the `_from` and `_to` attributes could not be indexed in
user-defined indexes, e.g. to make each combination of `_from` and `_to` unique.
Finally, as `_from` and `_to` referenced the linked collections by collection id
and not by collection name, their meaning became unclear once a referenced collection 
was dropped. The collection id stored in edges then became unusable, and when
accessing such edge the collection name part of it was always translated to `_undefined`.

In ArangoDB 3.0, the `_from` and `_to` values of edges are saved as regular strings.
This allows using `_from` and `_to` in user-defined indexes. Additionally this allows
updating the `_from` and `_to` values of existing edges. Furthermore, collections
referenced by `_from` and `_to` values may be dropped and re-created later. Any
`_from` and `_to` values of edges pointing to such dropped collection are unaffected
by the drop operation now. Also note that renaming the collection referenced in
`_from` and `_to` in ArangoDB 2.8 also relinked the edges. In 3.0 the edges are NOT
automatically relinked to the new collection anymore.

!SECTION Documents

Documents (in contrast to edges) cannot contain the attributes `_from` or `_to` on the
main level in ArangoDB 3.0. These attributes will be automatically removed when saving
documents (i.e. non-edges). `_from` and `_to` can be still used in sub-objects inside
documents.

The `_from` and `_to` attributes will of course be preserved and are still required when 
saving edges.

!SECTION AQL

!SUBSECTION Edges handling

When updating or replacing edges via AQL, any modifications to the `_from` and `_to`
attributes of edges were ignored by previous versions of ArangoDB, without signaling
any errors. This was due to the `_from` and `_to` attributes being immutable in earlier
versions of ArangoDB.

From 3.0 on, the `_from` and `_to` attributes of edges are mutable, so any AQL queries that
modify the `_from` or `_to` attribute values of edges will attempt to actually change these
attributes. Clients should be aware of this change and should review their queries that
modify edges to rule out unintended side-effects.

Additionally, when completely replacing the data of existing edges via the AQL `REPLACE`
operation, it is now required to specify values for the `_from` and `_to` attributes,
as `REPLACE` requires the entire new document to be specified. If either `_from` or `_to`
are missing from the replacement document, an `REPLACE` operation will fail.

!SUBSUBSECTION Graph functions

In version 3.0 all former graph related functions have been removed from AQL to
be replaced by [native AQL constructs](../../AQL/Graphs/index.html).
These constructs allow for more fine-grained filtering on several graph levels.
Also this allows the AQL optimizer to automatically improve these queries by
enhancing them with appropriate indexes.
We have created recipes to upgrade from 2.8 to 3.0 when using these functions.

The functions:

* GRAPH_COMMON_NEIGHBORS
* GRAPH_COMMON_PROPERTIES
* GRAPH_DISTANCE_TO
* GRAPH_EDGES
* GRAPH_NEIGHBORS
* GRAPH_TRAVERSAL
* GRAPH_TRAVERSAL_TREE
* GRAPH_SHORTEST_PATH
* GRAPH_PATHS
* GRAPH_VERTICES

are covered in [Migrating GRAPH_* Functions from 2.8 or earlier to 3.0](https://docs.arangodb.com/cookbook/AQL/MigratingGraphFunctionsTo3.html)

* GRAPH_ABSOLUTE_BETWEENNESS
* GRAPH_ABSOLUTE_CLOSENESS
* GRAPH_ABSOLUTE_ECCENTRICITY
* GRAPH_BETWEENNESS
* GRAPH_CLOSENESS
* GRAPH_DIAMETER
* GRAPH_ECCENTRICITY
* GRAPH_RADIUS

are covered in [Migrating GRAPH_* Measurements from 2.8 or earlier to 3.0](https://docs.arangodb.com/cookbook/AQL/MigratingMeasurementsTo3.html)

* EDGES
* NEIGHBORS
* PATHS
* TRAVERSAL
* TRAVERSAL_TREE

are covered in [Migrating anonymous graph functions from 2.8 or earlier to 3.0](https://docs.arangodb.com/3/cookbook/AQL/MigratingEdgeFunctionsTo3.html)

!SUBSECTION Typecasting functions

The type casting applied by the `TO_NUMBER()` AQL function has changed as follows:
- string values that do not contain a valid numeric value are now converted to the number
  `0`. In previous versions of ArangoDB such string values were converted to the value
  `null`.
- array values with more than 1 member are now converted to the number `0`. In previous
  versions of ArangoDB such arrays were converted to the value `null`.
- objects / documents are now converted to the number `0`. In previous versions of ArangoDB
  objects / documents were converted to the value `null`.

Additionally, the `TO_STRING()` AQL function now converts `null` values into an empty string
(`""`) instead of the string `"null"`, which is more in line with `LENGTH(null)` returning
`0` and not `4` since v2.6.

The output of `TO_STRING()` has also changed for arrays and objects as follows:

- arrays are now converted into their JSON-stringify equivalents, e.g.

  - `[ ]` is now converted to `[]`
  - `[ 1, 2, 3 ]` is now converted to `[1,2,3]`
  - `[ "test", 1, 2 ] is now converted to `["test",1,2]`
   
  Previous versions of ArangoDB converted arrays with no members into the
  empty string, and non-empty arrays into a comma-separated list of member
  values, without the surrounding angular brackets. Additionally, string
  array members were not enclosed in quotes in the result string:

  - `[ ]` was converted to ``
  - `[ 1, 2, 3 ]` was converted to `1,2,3`
  - `[ "test", 1, 2 ] was converted to `test,1,2`
  
- objects are now converted to their JSON-stringify equivalents, e.g.

  - `{ }` is converted to `{}`
  - `{ a: 1, b: 2 }` is converted to `{"a":1,"b":2}`
  - `{ "test" : "foobar" }` is converted to `{"test":"foobar"}`
    
  Previous versions of ArangoDB always converted objects into the string
  `[object Object]`  

This change also affects other parts in AQL that used `TO_STRING()` to implicitly
cast operands to strings. It also affects the AQL functions `CONCAT()` and 
`CONCAT_SEPARATOR()` which treated array values differently. Previous versions
of ArangoDB automatically flattened array values in the first level of the array, 
e.g. `CONCAT([1, 2, 3, [ 4, 5, 6 ]])` produced `1,2,3,4,5,6`. Now this will produce
`[1,2,3,[4,5,6]]`. To flatten array members on the top level, you can now use
the more explicit `CONCAT(FLATTEN([1, 2, 3, [4, 5, 6]], 1))`.

!SUBSECTION Arithmetic operators

As the arithmetic operations in AQL implicitly convert their operands to numeric values using 
`TO_NUMBER()`, their casting behavior has also changed as described above.

Some examples of the changed behavior:

- `"foo" + 1` produces `1` now. In previous versions this produced `null`.
- `[ 1, 2 ] + 1` produces `1`. In previous versions this produced `null`.
- `1 + "foo" + 1´ produces `2` now. In previous version this produced `1`.

!SUBSECTION Attribute names and parameters

Previous versions of ArangoDB had some trouble with attribute names that contained the dot
symbol (`.`). Some code parts in AQL used the dot symbol to split an attribute name into
sub-components, so an attribute named `a.b` was not completely distinguishable from an
attribute `a` with a sub-attribute `b`. This inconsistent behavior sometimes allowed "hacks"
to work such as passing sub-attributes in a bind parameter as follows:

```
FOR doc IN collection
  FILTER doc.@name == 1
  RETURN doc
```

If the bind parameter `@name` contained the dot symbol (e.g. `@bind` = `a.b`, it was unclear 
whether this should trigger sub-attribute access (i.e. `doc.a.b`) or a access to an attribute 
with exactly the specified name (i.e. `doc["a.b"]`).

ArangoDB 3.0 now handles attribute names containing the dot symbol properly, and sending a
bind parameter `@name` = `a.b` will now always trigger an access to the attribute `doc["a.b"]`,
not the sub-attribute `b` of `a` in `doc`.

For users that used the "hack" of passing bind parameters containing dot symbol to access
sub-attributes, ArangoDB 3.0 allows specifying the attribute name parts as an array of strings,
e.g. `@name` = `[ "a", "b" ]`, which will be resolved to the sub-attribute access `doc.a.b` 
when the query is executed.

!SUBSECTION Keywords

`LIKE` is now a keyword in AQL. Using `LIKE` in either case as an attribute or collection 
name in AQL queries now requires quoting.


`SHORTEST_PATH` is now a keyword in AQL. Using `SHORTEST_PATH` in either case as an attribute or collection 
name in AQL queries now requires quoting.

!SUBSECTION Subqueries

Queries that contain subqueries that contain data-modification operations such as `INSERT`, 
`UPDATE`, `REPLACE`, `UPSERT` or `REMOVE` will now refuse to execute if the collection
affected by the subquery's data-modification operation is read-accessed in an outer scope 
of the query.

For example, the following query will refuse to execute as the collection `myCollection`
is modified in the subquery but also read-accessed in the outer scope:

```
FOR doc IN myCollection
  LET changes = (
    FOR what IN myCollection
      FILTER what.value == 1
      REMOVE what IN myCollection
  )
  RETURN doc
```

It is still possible to write to collections from which data is read in the same query,
e.g.

```
FOR doc IN myCollection
  FILTER doc.value == 1
  REMOVE doc IN myCollection
```

and to modify data in different collection via subqueries.

!SUBSECTION Other changes

The AQL optimizer rule "merge-traversal-filter" that already existed in 3.0 was renamed to 
"optimize-traversals". This should be of no relevance to client applications except if 
they programatically look for applied optimizer rules in the explain out of AQL queries.

The order of results created by the AQL functions `VALUES()` and `ATTRIBUTES()` was never 
guaranteed and it only had the "correct" ordering by accident when iterating over objects 
that were not loaded from the database. As some of the function internals have changed, the
"correct" ordering will not appear anymore, and still no result order is guaranteed by
these functions unless the `sort` parameter is specified (for the `ATTRIBUTES()` function).

!SECTION Upgraded V8 version

The V8 engine that is used inside ArangoDB to execute JavaScript code has been upgraded from
version 4.3.61 to 5.0.71.39. The new version should be mostly compatible to the old version,
but there may be subtle differences, including changes of error message texts thrown by the
engine.
Furthermore, some V8 startup parameters have changed their meaning or have been removed in
the new version. This is only relevant when ArangoDB or ArangoShell are started with a custom
value for the `--javascript.v8-options` startup option.

Among others, the following V8 options change in the new version of ArangoDB:

- `--es_staging`: in 2.8 it had the meaning `enable all completed harmony features`, in 3.0 
  the option means `enable test-worthy harmony features (for internal use only)`

- `--strong_this`: this option wasn't present in 2.8. In 3.0 it means `don't allow 'this' to
  escape from constructors` and defaults to true.

- `--harmony_regexps`: this options means `enable "harmony regular expression extensions"` 
  and changes its default value from false to true

- `--harmony_proxies`: this options means `enable "harmony proxies"` and changes its default 
   value from false to true

- `--harmony_reflect`: this options means `enable "harmony Reflect API"` and changes its 
  default value from false to true

- `--harmony_sloppy`: this options means `enable "harmony features in sloppy mode"` and 
  changes its default value from false to true

- `--harmony_tostring`: this options means `enable "harmony toString"` and changes its 
  default value from false to true

- `--harmony_unicode_regexps`: this options means `enable "harmony unicode regexps"` and 
  changes its default value from false to true

- `--harmony_arrays`, `--harmony_array_includes`, `--harmony_computed_property_names`, 
  `--harmony_arrow_functions`, `--harmony_rest_parameters`, `--harmony_classes`, 
  `--harmony_object_literals`, `--harmony_numeric_literals`, `--harmony_unicode`:
   these option have been removed in V8 5.

As a consequence of the upgrade to V8 version 5, the implementation of the
JavaScript `Buffer` object had to be changed. JavaScript `Buffer` objects in
ArangoDB now always store their data on the heap. There is no shared pool
for small Buffer values, and no pointing into existing Buffer data when
extracting slices. This change may increase the cost of creating Buffers with
short contents or when peeking into existing Buffers, but was required for
safer memory management and to prevent leaks.

!SECTION JavaScript API changes

The following incompatible changes have been made to the JavaScript API in ArangoDB 3.0:

!SUBSECTION Foxx

The Foxx framework has been completely rewritten for 3.0 with a new, simpler and more 
familiar API. To make Foxx services developed for 2.8 or earlier ArangoDB versions run in 3.0, the service's manifest file needs to be edited.

To enable the legacy mode for a Foxx service, add `"engines": {"arangodb": "^2.8.0"}` 
(or similar version ranges that exclude 3.0 and up) to the service manifest file
(named "manifest.json", located in the service's base directory).

!SUBSECTION Require

Modules shipped with ArangoDB can now be required using the pattern `@arangodb/<module>`
instead of `org/arangodb/<module>`, e.g.

```js
var cluster = require("@arangodb/cluster");
```

The old format can still be used for compatibility:

```js
var cluster = require("org/arangodb/cluster");
```

ArangoDB prior to version 3.0 allowed a transparent use of CoffeeScript
source files with the `require()` function. Files with a file name extension
of `coffee` were automatically sent through a CoffeeScript parser and
transpiled into JavaScript on-the-fly. This support is gone with ArangoDB
3.0. To run any CoffeeScript source files, they must be converted to JavaScript
by the client application.

!SUBSECTION Response object

The `@arangodb/request` response object now stores the parsed JSON response
body in a property `json` instead of `body` when the request was made using the
`json` option. The `body` instead contains the response body as a string.

!SUBSECTION Edges API

When completely replacing an edge via a collection's `replace()` function the replacing 
edge data now needs to contain the `_from` and `_to` attributes for the new edge. Previous
versions of ArangoDB did not require the edge data to contain `_from` and `_to` attributes
when replacing an edge, since `_from` and `_to` values were immutable for existing edges.

For example, the following call worked in ArangoDB 2.8 but will fail in 3.0:

```js
db.edgeCollection.replace("myKey", { value: "test" });
```

To make this work in ArangoDB 3.0, `_from` and `_to` need to be added to the replacement
data:

```js
db.edgeCollection.replace("myKey", { _from: "myVertexCollection/1", _to: "myVertexCollection/2", value: "test" });
```

Note that this only affects the `replace()` function but not `update()`, which will
only update the specified attributes of the edge and leave all others intact.

Additionally, the functions `edges()`, `outEdges()` and `inEdges()` with an array of edge 
ids will now make the edge ids unique before returning the connected edges. This is probably 
desired anyway, as results will be returned only once per distinct input edge id. However, 
it may break client applications that rely on the old behavior.

!SUBSECTION Databases API

The `_listDatabases()` function of the `db` object has been renamed to `_databases()`, making it 
consistent with the `_collections()` function. Also the `_listEndpoints()` function has been 
renamed to `_endpoints()`.

!SUBSECTION Collection API

!SUBSUBSECTION Example matching

The collection function `byExampleHash()` and `byExampleSkiplist()` have been removed in 3.0.
Their functionality is provided by collection's `byExample()` function, which will automatically
use a suitable index if present.

The collection function `byConditionSkiplist()` has been removed in 3.0. The same functionality
can be achieved by issuing an AQL query with the target condition, which will automatically use
a suitable index if present.

!SUBSUBSECTION Revision id handling

The `exists()` method of a collection now throws an exception when the specified document
exists but its revision id does not match the revision id specified. Previous versions of
ArangoDB simply returned `false` if either no document existed with the specified key or
when the revision id did not match. It was therefore impossible to distinguish these two
cases from the return value alone. 3.0 corrects this. Additionally, `exists()` in previous
versions always returned a boolean if only the document key was given. 3.0 now returns the
document's meta-data, which includes the document's current revision id.

Given there is a document with key `test` in collection `myCollection`, then the behavior
of 3.0 is as follows:

```js
/* test if document exists. this returned true in 2.8 */
db.myCollection.exists("test");
{
  "_key" : "test",
  "_id" : "myCollection/test",
  "_rev" : "9758059"
}

/* test if document exists. this returned true in 2.8 */
db.myCollection.exists({ _key: "test" });
{
  "_key" : "test",
  "_id" : "myCollection/test",
  "_rev" : "9758059"
}

/* test if document exists. this also returned false in 2.8 */
db.myCollection.exists("foo");
false

/* test if document with a given revision id exists. this returned true in 2.8 */
db.myCollection.exists({ _key: "test", _rev: "9758059" });
{
  "_key" : "test",
  "_id" : "myCollection/test",
  "_rev" : "9758059"
}

/* test if document with a given revision id exists. this returned false in 2.8 */
db.myCollection.exists({ _key: "test", _rev: "1234" });
JavaScript exception: ArangoError 1200: conflict
```

!SUBSUBSECTION Cap constraints

The cap constraints feature has been removed. This change has led to the removal of the
collection operations `first()` and `last()`, which were internally based on data from 
cap constraints.

As cap constraints have been removed in ArangoDB 3.0 it is not possible to create an
index of type "cap" with a collection's `ensureIndex()` function. The dedicated function
`ensureCapConstraint()` has also been removed from the collection API.

!SUBSUBSECTION Graph Blueprints JS Module

The deprecated module `graph-blueprints` has been deleted.
All it's features are covered by the `general-graph` module.

!SUBSUBSECTION General Graph Fluent AQL interface

The fluent interface has been removed from ArangoDB.
It's features were completely overlapping with ["aqb"](https://github.com/arangodb/aqbjs)
which comes pre installed as well.
Please switch to AQB instead.

!SUBSUBSECTION Undocumented APIs

The undocumented functions `BY_EXAMPLE_HASH()` and `BY_EXAMPLE_SKIPLIST()`,
`BY_CONDITION_SKIPLIST`, `CPP_NEIGHBORS` and `CPP_SHORTEST_PATH` have been removed.
These functions were always hidden and not intended to be part of
the public JavaScript API for collections.


!SECTION HTTP API changes

!SUBSECTION CRUD operations

The following incompatible changes have been made to the HTTP API in ArangoDB 3.0:

!SUBSUBSECTION General

The HTTP insert operations for single documents and edges (POST `/_api/document`) do 
not support the URL parameter "createCollection" anymore. In previous versions of
ArangoDB this parameter could be used to automatically create a collection upon
insertion of the first document. It is now required that the target collection already
exists when using this API, otherwise it will return an HTTP 404 error.
The same is true for the import API at POST `/_api/import`.

Collections can still be created easily via a separate call to POST `/_api/collection`
as before.

The "location" HTTP header returned by ArangoDB when inserting a new document or edge
now always contains the database name. This was also the default behavior in previous
versions of ArangoDB, but it could be overridden by clients sending the HTTP header
`x-arango-version: 1.4` in the request. Clients can continue to send this header to
ArangoDB 3.0, but the header will not influence the location response headers produced 
by ArangoDB 3.0 anymore.

Additionally the CRUD operations APIs do not return an attribute "error" in the
response body with an attribute value of "false" in case an operation succeeded.

!SUBSUBSECTION Revision id handling

The operations for updating, replacing and removing documents can optionally check the
revision number of the document to be updated, replaced or removed so the caller can
ensure the operation works on a specific version of the document and there are no
lost updates.

Previous versions of ArangoDB allowed passing the revision id of the previous document
either in the HTTP header `If-Match` or in the URL parameter `rev`. For example, 
removing a document with a specific revision id could be achieved as follows:

```
curl -X DELETE \
     "http://127.0.0.1:8529/_api/document/myCollection/myKey?rev=123"
```

ArangoDB 3.0 does not support passing the revision id via the "rev" URL parameter
anymore. Instead the previous revision id must be passed in the HTTP header `If-Match`,
e.g.

```
curl -X DELETE \
     --header "If-Match: '123'" \
     "http://127.0.0.1:8529/_api/document/myCollection/myKey"
```

The URL parameter "policy" was also usable in previous versions of ArangoDB to
control revision handling. Using it was redundant to specifying the expected revision
id via the "rev" parameter or "If-Match" HTTP header and therefore support for the "policy"
parameter was removed in 3.0.

In order to check for a previous revision id when updating, replacing or removing 
documents please use the `If-Match` HTTP header as described above. When no revision
check if required the HTTP header can be omitted, and the operations will work on the
current revision of the document, regardless of its revision id.

!SUBSECTION All documents API

The HTTP API for retrieving the ids, keys or URLs of all documents from a collection
was previously located at GET `/_api/document?collection=...`. This API was moved to
PUT `/_api/simple/all-keys` and is now executed as an AQL query.
The name of the collection must now be passed in the HTTP request body instead of in
the request URL. The same is true for the "type" parameter, which controls the type of
the result to be created.

Calls to the previous API can be translated as follows:

- old: GET `/_api/document?collection=<collection>&type=<type>` without HTTP request body
- 3.0: PUT `/_api/simple/all-keys` with HTTP request body `{"collection":"<collection>","type":"id"}`

The result format of this API has also changed slightly. In previous versions calls to
the API returned a JSON object with a `documents` attribute. As the functionality is
based on AQL internally in 3.0, the API now returns a JSON object with a `result` attribute.

!SUBSECTION Edges API

!SUBSUBSECTION CRUD operations

The API for documents and edges have been unified in ArangoDB 3.0. The CRUD operations 
for documents and edges are now handled by the same endpoint at `/_api/document`. For 
CRUD operations there is no distinction anymore between documents and edges API-wise.

That means CRUD operations concerning edges need to be sent to the HTTP endpoint 
`/_api/document` instead of `/_api/edge`. Sending requests to `/_api/edge` will
result in an HTTP 404 error in 3.0. The following methods are available at
`/_api/document` for documents and edge:

- HTTP POST: insert new document or edge
- HTTP GET: fetch an existing document or edge
- HTTP PUT: replace an existing document or edge
- HTTP PATCH: partially update an existing document or edge
- HTTP DELETE: remove an existing document or edge

When completely replacing an edge via HTTP PUT please note that the replacing edge
data now needs to contain the `_from` and `_to` attributes for the edge. Previous
versions of ArangoDB did not require sending `_from` and `_to` when replacing edges, 
as `_from` and `_to` values were immutable for existing edges.

The `_from` and `_to` attributes of edges now also need to be present inside the
edges objects sent to the server:

```
curl -X POST \
     --data '{"value":1,"_from":"myVertexCollection/1","_to":"myVertexCollection/2"}' \
     "http://127.0.0.1:8529/_api/document?collection=myEdgeCollection"
```

Previous versions of ArangoDB required the `_from` and `_to` attributes of edges be 
sent separately in URL parameter `from` and `to`:

```
curl -X POST \
     --data '{"value":1}' \
     "http://127.0.0.1:8529/_api/edge?collection=e&from=myVertexCollection/1&to=myVertexCollection/2" 
```

!SUBSUBSECTION Querying connected edges

The REST API for querying connected edges at GET `/_api/edges/<collection>` will now
make the edge ids unique before returning the connected edges. This is probably desired anyway
as results will now be returned only once per distinct input edge id. However, it may break 
client applications that rely on the old behavior.

!SUBSUBSECTION Graph API

Some data-modification operations in the named graphs API at `/_api/gharial` now return either
HTTP 202 (Accepted) or HTTP 201 (Created) if the operation succeeds. Which status code is returned
depends on the `waitForSync` attribute of the affected collection. In previous versions some
of these operations return HTTP 200 regardless of the `waitForSync` value.

The deprecated graph API `/_api/graph` has been removed.
All it's features can be replaced using `/_api/gharial` and AQL instead.

!SUBSECTION Simple queries API

The REST routes PUT `/_api/simple/first` and `/_api/simple/last` have been removed
entirely. These APIs were responsible for returning the first-inserted and
last-inserted documents in a collection. This feature was built on cap constraints
internally, which have been removed in 3.0.

Calling one of these endpoints in 3.0 will result in an HTTP 404 error.

!SUBSECTION Indexes API

It is not supported in 3.0 to create an index with type `cap` (cap constraint) in 
3.0 as the cap constraints feature has bee removed. Calling the index creation
endpoint HTTP API POST `/_api/index?collection=...` with an index type `cap` will 
therefore result in an HTTP 400 error.

!SUBSECTION Log entries API

The REST route HTTP GET `/_admin/log` is now accessible from within all databases. In
previous versions of ArangoDB, this route was accessible from within the `_system`
database only, and an HTTP 403 (Forbidden) was thrown by the server for any access
from within another database.

!SUBSECTION Figures API

The REST route HTTP GET `/_api/collection/<collection>/figures` will not return the 
following result attributes as they became meaningless in 3.0:

- shapefiles.count
- shapes.fileSize
- shapes.count
- shapes.size
- attributes.count
- attributes.size

!SUBSECTION Databases and Collections APIs

When creating a database via the API POST `/_api/database`, ArangoDB will now always
return the HTTP status code 202 (created) if the operation succeeds. Previous versions
of ArangoDB returned HTTP 202 as well, but this behavior was changable by sending an
HTTP header `x-arango-version: 1.4`. When sending this header, previous versions of
ArangoDB returned an HTTP status code 200 (ok). Clients can still send this header to
ArangoDB 3.0 but this will not influence the HTTP status code produced by ArangoDB.

The "location" header produced by ArangoDB 3.0 will now always contain the database 
name. This was also the default in previous versions of ArangoDB, but the behavior
could be overridden by sending the HTTP header `x-arango-version: 1.4`. Clients can
still send the header, but this will not make the database name in the "location"
response header disappear.

The result format for querying all collections via the API GET `/_api/collection` 
has been changed.

Previous versions of ArangoDB returned an object with an attribute named `collections` 
and an attribute named `names`. Both contained all available collections, but
`collections` contained the collections as an array, and `names` contained the
collections again, contained in an object in which the attribute names were the
collection names, e.g.

```
{
  "collections": [
    {"id":"5874437","name":"test","isSystem":false,"status":3,"type":2},
    {"id":"17343237","name":"something","isSystem":false,"status":3,"type":2},
    ...
  ],
  "names": {
    "test": {"id":"5874437","name":"test","isSystem":false,"status":3,"type":2},
    "something": {"id":"17343237","name":"something","isSystem":false,"status":3,"type":2},
    ...
  }
}
```
This result structure was redundant, and therefore has been simplified to just

```
{
  "result": [
    {"id":"5874437","name":"test","isSystem":false,"status":3,"type":2},
    {"id":"17343237","name":"something","isSystem":false,"status":3,"type":2},
    ...
  ]
}
```

in ArangoDB 3.0.

!SUBSECTION Replication APIs

The URL parameter "failOnUnknown" was removed from the REST API GET `/_api/replication/dump`.
This parameter controlled whether dumping or replicating edges should fail if one
of the vertex collections linked in the edge's `_from` or `_to` attributes was not
present anymore. In this case the `_from` and `_to` values could not be translated into
meaningful ids anymore.

There were two ways for handling this:
- setting `failOnUnknown` to `true` caused the HTTP request to fail, leaving error 
  handling to the user
- setting `failOnUnknown` to `false` caused the HTTP request to continue, translating
  the collection name part in the `_from` or `_to` value to `_unknown`.

In ArangoDB 3.0 this parameter is obsolete, as `_from` and `_to` are stored as self-contained
string values all the time, so they cannot get invalid when referenced collections are
dropped.

The result format of the API GET `/_api/replication/logger-follow` has changed slightly in
the following aspects:

- documents and edges are reported in the same way. The type for document insertions/updates
  and edge insertions/updates is now always `2300`. Previous versions of ArangoDB returned
  a `type` value of `2300` for documents and `2301` for edges.
- records about insertions, updates or removals of documents and edges do not have the
  `key` and `rev` attributes on the top-level anymore. Instead, `key` and `rev` can be 
  accessed by peeking into the `_key` and `_rev` attributes of the `data` sub-attributes
  of the change record.

The same is true for the collection-specific changes API GET `/_api/replication/dump`.

!SUBSECTION User management APIs

The REST API endpoint POST `/_api/user` for adding new users now requires the request to
contain a JSON object with an attribute named `user`, containing the name of the user to
be created. Previous versions of ArangoDB also checked this attribute, but additionally 
looked for an attribute `username` if the `user` attribute did not exist. 

!SUBSECTION Undocumented APIs

The following undocumented HTTP REST endpoints have been removed from ArangoDB's REST
API:

- `/_open/cerberus` and `/_system/cerberus`: these endpoints were intended for some 
  ArangoDB-internal applications only
- PUT `/_api/simple/by-example-hash`, PUT `/_api/simple/by-example-skiplist` and
  PUT `/_api/simple/by-condition-skiplist`: these methods were documented in early
  versions of ArangoDB but have been marked as not intended to be called by end
  users since ArangoDB version 2.3. These methods should not have been part of any
  ArangoDB manual since version 2.4.
- `/_api/structure`: an older unfinished and unpromoted API for data format and type 
  checks, superseded by Foxx applications.

!SUBSECTION Administration APIs

- `/_admin/shutdown` now needs to be called with the HTTP DELETE method

!SUBSECTION Handling of CORS requests

It can now be controlled in detail for which origin hosts CORS (Cross-origin resource 
sharing) requests with credentials will be allowed. ArangoDB 3.0 provides the startup
option `--http.trusted-origin` that can be used to specify one or many origins from
which CORS requests are treated as "trustworthy".

The option can be specified multiple times, once per trusted origin, e.g.

```
--http.trusted-origin http://127.0.0.1:8529 --http.trusted-origin https://127.0.0.1:8599
```

This will make the ArangoDB server respond to CORS requests from these origins with an
`Access-Control-Allow-Credentials` HTTP header with a value of `true`. Web browsers can
inspect this header and can allow passing ArangoDB web interface credentials (if stored 
in the browser) to the requesting site. ArangoDB will not forward or provide any credentials.

Setting this option is only required if applications on other hosts need to access the 
ArangoDB web interface or other HTTP REST APIs from a web browser with the same credentials 
that the user has entered when logging into the web interface. When a web browser finds 
the `Access-Control-Allow-Credentials` HTTP response header, it may forward the credentials
entered into the browser for the ArangoDB web interface login to the other site. 

This is a potential security issue, so there are no trusted origins by default. It may
be required to set some trusted origins if you're planning to issue AJAX requests to ArangoDB
from other sites from the browser, with the credentials entered during the ArangoDB interface 
login (i.e. single sign-on). If such functionality is not used, the option should not
be set.

To specify a trusted origin, specify the option once per trusted origin as shown above.
Note that the trusted origin values specified in this option will be compared bytewise
with the `Origin` HTTP header value sent by clients, and only exact matches will pass.

There is also the wildcard `all` for enabling CORS access from all origins in a 
test or development setup:

```
--http.trusted-origin all
```

Setting this option will lead to the ArangoDB server responding with an 
`Access-Control-Allow-Credentials: true` HTTP header to all incoming CORS requests.

!SECTION Command-line options

Quite a few startup options in ArangoDB 2 were double negations (like
`--server.disable-authentication false`). In ArangoDB 3 these are now expressed as 
positives (e. g. `--server.authentication`). Also the options between the ArangoDB
server and its client tools have being unified. For example, the logger options are 
now the same for the server and the client tools. Additionally many options have
been moved into more appropriate topic sections.

!SUBSECTION Renamed options

The following options have been available before 3.0 and have changed their name 
in 3.0:

- `--server.disable-authentication` was renamed to `--server.authentication`.
  Note that the meaning of the option `--server.authentication` is the opposite of 
  the previous `--server.disable-authentication`.
- `--server.disable-authentication-unix-sockets` was renamed to
  `--server.authentication-unix-sockets`. Note that the meaning of the option
  `--server.authentication-unix-sockets` is the opposite of the previous
  `--server.disable-authentication-unix-sockets`.
- `--server.authenticate-system-only` was renamed to `--server.authentication-system-only`.
  The meaning of the option in unchanged.
- `--server.disable-statistics` was renamed to `--server.statistics`. Note that the
  meaning of the option `--server.statistics` is the opposite of the previous
  `--server.disable-statistics`.
- `--server.cafile` was renamed to `--ssl.cafile`. The meaning of the option is
  unchanged.
- `--server.keyfile` was renamed to `--ssl.keyfile`. The meaning of the option is
  unchanged.
- `--server.ssl-cache` was renamed to `--ssl.session-cache`. The meaning of the option
  is unchanged.
- `--server.ssl-cipher-list` was renamed to `--ssl.cipher-list`. The meaning of the
  option is unchanged.
- `--server.ssl-options` was renamed to `--ssl.options`. The meaning of the option
  is unchanged.
- `--server.ssl-protocol` was renamed to `--ssl.protocol`. The meaning of the option
  is unchanged.
- `--server.backlog-size` was renamed to `--tcp.backlog-size`. The meaning of the
  option is unchanged.
- `--server.reuse-address` was renamed to `--tcp.reuse-address`. The meaning of the
  option is unchanged.
- `--server.disable-replication-applier` was renamed to `--database.replication-applier`.
  The meaning of the option `--database.replication-applier` is the opposite of the
  previous `--server.disable-replication-applier`.
- `--server.allow-method-override` was renamed to `--http.allow-method-override`. The
  meaning of the option is unchanged.
- `--server.hide-product-header` was renamed to `--http.hide-product-header`. The
  meaning of the option is unchanged.
- `--server.keep-alive-timeout` was renamed to `--http.keep-alive-timeout`. The
  meaning of the option is unchanged.
- `--server.foxx-queues` was renamed to `--foxx.queues`. The meaning of the option
  is unchanged.
- `--server.foxx-queues-poll-interval` was renamed to `--foxx.queues-poll-interval`.
  The meaning of the option is unchanged.
- `--no-server` was renamed to `--server.rest-server`. Note that the meaning of the
  option `--server.rest-server` is the opposite of the previous `--no-server`.
- `--database.query-cache-mode` was renamed to `--query.cache-mode`. The meaning of
  the option is unchanged.
- `--database.query-cache-max-results` was renamed to `--query.cache-entries`. The
  meaning of the option is unchanged.
- `--database.disable-query-tracking` was renamed to `--query.tracking`. The meaning 
  of the option `--query.tracking` is the opposite of the previous
  `--database.disable-query-tracking`.
- `--log.tty` was renamed to `--log.foreground-tty`. The meaning of the option is
  unchanged.
- `--upgrade` has been renamed to `--database.auto-upgrade`. In contrast to 2.8 this
  option now requires a boolean parameter. To actually perform an automatic database 
  upgrade at startup use `--database.auto-upgrade true`. To not perform it, use
  `--database.auto-upgrade false`.
- `--check-version` has been renamed to `--database.check-version`.
- `--temp-path` has been renamed to `--temp.path`.

!SUBSECTION Log verbosity, topics and output files

Logging now supports log topics. You can control these by specifying a log
topic in front of a log level or an output. For example

```
  --log.level startup=trace --log.level info
```

will log messages concerning startup at trace level, everything else at info
level. `--log.level` can be specified multiple times at startup, for as many
topics as needed.

Some relevant log topics available in 3.0 are:

- *collector*: information about the WAL collector's state
- *compactor*: information about the collection datafile compactor
- *datafiles*: datafile-related operations
- *mmap*: information about memory-mapping operations
- *performance*: some performance-related information
- *queries*: executed AQL queries
- *replication*: replication-related info
- *requests*: HTTP requests
- *startup*: information about server startup and shutdown
- *threads*: information about threads

The new log option `--log.output <definition>` allows directing the global
or per-topic log output to different outputs. The output definition "<definition>" 
can be one of

- "-" for stdin
- "+" for stderr
- "syslog://<syslog-facility>"
- "syslog://<syslog-facility>/<application-name>"
- "file://<relative-path>"

The option can be specified multiple times in order to configure the output
for different log topics. To set up a per-topic output configuration, use
`--log.output <topic>=<definition>`, e.g.

  queries=file://queries.txt

logs all queries to the file "queries.txt".

The old option `--log.file` is still available in 3.0 for convenience reasons. In
3.0 it is a shortcut for the more general option `--log.output file://filename`.

The old option `--log.requests-file` is still available in 3.0. It is now a shortcut
for the more general option `--log.output requests=file://...`.

The old option `--log.performance` is still available in 3.0. It is now a shortcut
for the more general option `--log.level performance=trace`.

!SUBSECTION Removed options for logging

The options `--log.content-filter` and `--log.source-filter` have been removed. They
have most been used during ArangoDB's internal development.

The syslog-related options `--log.application` and `--log.facility` have been removed.
They are superseded by the more general `--log.output` option which can also handle 
syslog targets.

!SUBSECTION Removed other options

The option `--server.default-api-compatibility` was present in earlier version of
ArangoDB to control various aspects of the server behavior, e.g. HTTP return codes
or the format of HTTP "location" headers. Client applications could send an HTTP
header "x-arango-version" with a version number to request the server behavior of
a certain ArangoDB version.

This option was only honored in a handful of cases (described above) and was removed
in 3.0 because the changes in server behavior controlled by this option were changed
even before ArangoDB 2.0. This should have left enough time for client applications
to adapt to the new behavior, making the option superfluous in 3.0.

!SUBSECTION Thread options

The options `--server.threads` and `--scheduler.threads` now have a default value of 
`0`. When `--server.threads` is set to `0` on startup, the suitable number of
threads will be determined by ArangoDB by asking the OS for the number of available
CPUs and using that as a baseline. If the number of CPUs is lower than 4, ArangoDB
will still start 4 dispatcher threads. When `--scheduler.threads` is set to `0`,
then ArangoDB will automatically determine the number of scheduler threads to start.
This will normally create 2 scheduler threads. 

If the exact number of threads needs to be set by the admin, then it is still possible
to set `--server.threads` and `--scheduler.threads` to non-zero values. ArangoDB will
use these values and start that many threads (note that some threads may be created
lazily so they may not be present directly after startup). 

The number of V8 JavaScript contexts to be created (`--javascript.v8-contexts`) now 
has a default value of `0` too, meaning that ArangoDB will create as many V8 contexts
as there will be dispatcher threads (controlled by the `--server.threads` option).
Setting this option to a non-zero value will create exactly as many V8 contexts as
specified.

Setting these options explicitly to non-zero values may be beneficial in environments
that have few resources (processing time, maximum thread count, available memory). 

!SECTION Authentication

The default value for `--server.authentication` is now `true` in the configuration
files shipped with ArangoDB. This means the server will be started with authentication 
enabled by default, requiring all client connections to provide authentication data 
when connecting to ArangoDB APIs. Previous ArangoDB versions used the setting
`--server.disable-authentication true`, effectively disabling authentication by default.

The default value for `--server.authentication-system-only` is now `true` in ArangoDB.
That means that Foxx applications running in ArangoDB will be public accessible (at
least they will not use ArangoDB's builtin authentication mechanism). Only requests to
ArangoDB APIs at URL path prefixes `/_api/` and `/_admin` will require authentication.
To change that, and use the builtin authentication mechanism for Foxx applications too,
set `--server.authentication-system-only` to `false`, and make sure to have the option
`--server.authentication` set to `true` as well.

Though enabling the authentication is recommended for production setups, it may be
overkill in a development environment. To turn off authentication, the option 
`--server.authentication` can be set to `false` in ArangoDB's configuration file or
on the command-line.

!SECTION Web Admin Interface

The JavaScript shell has been removed from ArangoDB's web interface. The functionality
the shell provided is still fully available in the ArangoShell (arangosh) binary shipped
with ArangoDB.

!SECTION ArangoShell and client tools

The ArangoShell (arangosh) and the other client tools bundled with ArangoDB can only
connect to an ArangoDB server of version 3.0 or higher. They will not connect to an
ArangoDB 2.8. This is because the server HTTP APIs have changed between 2.8 and 3.0,
and all client tools uses these APIs.

In order to connect to earlier versions of ArangoDB with the client tools, an older
version of the client tools needs to be kept installed.

The preferred name for the template string generator function `aqlQuery` is now
`aql` and is automatically available in arangosh. Elsewhere, it can be loaded
like `const aql = require('@arangodb').aql`.

!SUBSECTION Command-line options added

All client tools in 3.0 provide an option `--server.max-packet-size` for controlling
the maximum size of HTTP packets to be handled by the client tools. The default value
is 128 MB, as in previous versions of ArangoDB. In contrast to previous versions in
which the value was hard-coded, the option is now configurable. It can be increased to
make the client tools handle very large HTTP result messages sent by the server. 

!SUBSECTION Command-line options changed

For all client tools, the option `--server.disable-authentication` was renamed to
`--server.authentication`. Note that the meaning of the option `--server.authentication` 
is the opposite of the previous `--server.disable-authentication`.

The option `--server.ssl-protocol` was renamed to `--ssl.protocol`. The meaning of 
the option is unchanged.

The command-line option `--quiet` was removed from all client tools except arangosh 
because it had no effect in them.

!SUBSECTION Arangobench

In order to make its purpose more apparent the former `arangob` client tool has 
been renamed to `arangobench` in 3.0.

!SECTION Miscellaneous changes

The checksum calculation algorithm for the `collection.checksum()` method and its
corresponding REST API GET `/_api/collection/<collection</checksum` has changed in 3.0. 
Checksums calculated in 3.0 will differ from checksums calculated with 2.8 or before.

The ArangoDB server in 3.0 does not read a file `ENDPOINTS` containing a list of 
additional endpoints on startup. In 2.8 this file was automatically read if present
in the database directory.

The names of the sub-threads started by ArangoDB have changed in 3.0. This is relevant
on Linux only, where threads can be named and thread names may be visible to system
tools such as *top* or monitoring solutions.
