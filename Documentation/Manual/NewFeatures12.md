New Features in ArangoDB 1.2 {#NewFeatures12}
=============================================

@NAVIGATE_NewFeatures12
@EMBEDTOC{NewFeatures12TOC}

Features and Improvements {#NewFeatures12Introduction}
======================================================

The following list shows in detail which features have been added or
improved in ArangoDB 1.2.  ArangoDB 1.2 also contains several bugfixes
that are not listed here.

User-defined document keys {#NewFeatures12UserDefinedKeys}
----------------------------------------------------------

ArangoDB 1.2 allows users to determine their own document keys to uniquely
identify documents.
In previous versions of ArangoDB, each document automatically got an `_id` 
value determined by the server. The document id part of this `_id` value 
was an ever-increasing integer number that the user could not change or preselect.
Additionally, big integers are hard to memoize.

ArangoDB 1.2 allows specifying own values for the document id part of the `_id` 
values. This can be done by providing a `_key` attribute when creating documents.

The value specified for `_key` will automatically become part of the `_id` value
of a document, and can be used to retrieve a document later:

    arangosh> db._create("mycollection");
    [ArangoCollection 316959596850, "mycollection" (type document, status loaded)]

    arangosh> db.mycollection.save({ _key: "mytest" });
    { "error" : false, "_id" : "mycollection/mytest", "_rev" : "316961300786", "_key" : "mytest" }

    arangosh> db.mycollection.toArray();
    [ { "_id" : "mycollection/mytest", "_rev" : "316961300786", "_key" : "mytest" } ]

    arangosh> db.mycollection.document("mytest");
    { "_key" : "mytest", "_id" : "mycollection/mytest", "_rev" : "316961300786" }

This should be much more intuitive and easier than dealing with the server-generated
integer values of previous versions.

Specifiying a value for the `_key` attribute is optional. If the attribute is not
specified, ArangoDB will create its own value as it did in previous versions.
When upgrading from ArangoDB 1.1 to ArangoDB 1.2, the document ids will automatically
be converted into equivalent `_key` values, meaning the same identifiers can be
used after an upgrade.

There are a few restrictions for the values that can be used inside `_key` (please
refer to @ref DocumentKeys for details). Document key values are immutable, i.e. they
cannot be changed after a document has been created. The only way to change a `_key` 
value of a document is to remove the document and re-insert it with a different key.

Document keys can also be used with the REST API at all places where a document id
was required before.

Usage of collection names instead of ids {#NewFeatures12CollectionNames}
------------------------------------------------------------------------

ArangoDB 1.1 and earlier versions required clients to the use server-generated collection 
ids instead of the collections' actual names in many of the REST APIs.
To make using the REST APIs easier, this has been changed so collection names can be
specified instead of collection ids.

Collection names are also returned as part of documents' `_id` values to make comparing
results easier in case just the collection name was known.

Here is an example AQL query from ArangoDB 1.1 that works fine, because both the 
collection id and the document id are known:

    arangosh> db._create("test");
    [ArangoCollection 402238582, "test" (type document, status loaded)]

    arangosh> db.test.save({ value : 1});
    { "_id" : "402238582/403745910", "_rev" : 403745910 }

    arangosh> stmt = db._createStatement({ query: 'FOR t IN test FILTER t._id == "402238582/403745910" RETURN t' });
    arangosh> cursor = stmt.execute();                                                                            
    arangosh> while (cursor.hasNext()) { print(cursor.next()); }
    { "_id" : "402238582/403745910", "_rev" : 403745910, "value" : 1 }   

However, if either the collection id or the document id are not precisely known
at query time, then the filter condition will fail.
    
    arangosh> stmt = db._createStatement({ query: 'FOR t IN test FILTER t._id == "403745910" RETURN t' });
    arangosh> cursor = stmt.execute();                                                                            
    arangosh> while (cursor.hasNext()) { print(cursor.next()); }

Effectively this meant that in 1.1 you had to know both collection names and collection ids 
in several cases, making ArangoDB 1.1 harder to use than necessary.

In ArangoDB 1.2, the above query can be rewritten to the much more intuitive form:
    
    arangosh> stmt = db._createStatement({ query: 'FOR t IN test FILTER t._id == "test/mykey" RETURN t' });
    ...

or even:

    arangosh> stmt = db._createStatement({ query: 'FOR t IN test FILTER t._key == "mykey" RETURN t' });
    ...

Collection names can also be used to reference other documents when creating edges,
which makes setting up and querying edges much easier in ArangoDB 1.2:

    arangosh> db._create("myvertexcollection");
    [ArangoCollection 316959891364, "myvertexcollection" (type document, status loaded)]

    arangosh> db._createEdgeCollection("myedgecollection");
    [ArangoCollection 316961464228, "myedgecollection" (type edge, status loaded)]

    arangosh> db.myvertexcollection.save({ _key: "vertex1", value: 1234 });
    { "error" : false, "_id" : "myvertexcollection/vertex1", "_rev" : "316963299236", "_key" : "vertex1" }

    arangosh> db.myvertexcollection.save({ _key: "vertex2", value: 6789 });
    { "error" : false, "_id" : "myvertexcollection/vertex2", "_rev" : "316963364772", "_key" : "vertex2" }

    arangosh> db.myedgecollection.save("myvertexcollection/vertex1", "myvertexcollection/vertex2", { type: "some value" });
    { "error" : false, "_id" : "myedgecollection/316963561380", "_rev" : "316963561380", "_key" : "316963561380" } 

    arangosh> db.myedgecollection.outEdges("myvertexcollection/vertex1");
    [ 
      { "_id" : "myedgecollection/316963561380", "_rev" : "316963561380", "_key" : "316963561380", "_from" : "myvertexcollection/vertex1", "_to" : "myvertexcollection/vertex2", "type" : "some value" } 
    ]  

Additional graph functionality {#NewFeatures12GraphFunctionality}
-----------------------------------------------------------------

- @BOOK_REF{JSModuleGraph}

- @BOOK_REF{HttpGraph}

AQL Improvements {#NewFeatures12AqlImprovements}
------------------------------------------------

### Additional functions

The following functions have been added in the ArangoDB Query Language (AQL) 
in ArangoDB 1.2:

* `TRAVERSAL()`: returns the result of a graph traversal. 

  The starting point and direction of the traversal have to be specified. 
  It is also possible to configure the traversal by setting the visitation 
  order (e.g. depth-first vs. breadth-first, pre-order vs. post-order visitation), 
  limiting the traversal depth, specifying uniqueness constraints, and limiting 
  the traversal to specific types of edges.
  
* `TRAVERSAL_TREE()`: the same as `TRAVERSAL()`, but will return the result in 
  a hierarchical format

* `EDGES()`: access edges connected to a vertex from via an AQL function

* `ATTRIBUTES()`: returns the names of all attributes of a document as a list

* `KEEP()`: keeps only the specified attributes of a document, and removes all others

* `UNSET()`: removes only the specified attributes from a document, and preserves all others

* `MATCHES()`: to check if a document matches one of multiple example documents

* `LIKE()`: pattern-based text comparison

* `FULLTEXT()`: to execute queries on a fulltext index

The documentation for the individual functions is available at @ref AqlFunctions

### Syntactic extensions

AQL now also allows the following syntactic constructs, which were not available
in previous versions of ArangoDB:

* `FUNC(...)[....]`
* `FUNC(...).attribute`

The above syntax constructs allow accessing an indexed or named attribute of
a function return value directly, without having to store the return value in
an intermediate variable.

An example query that is made possible by this change (returns names of all 
available collections):

    RETURN COLLECTIONS()[*].name

This query did not work in ArangoDB 1.1, and the workaround was to either use
a for loop and process the individual function return values separately, or 
to store them in a temporary variable as follows:

    FOR c IN COLLECTIONS() RETURN c.name
    LET c = COLLECTIONS() RETURN c[*].name

Note: ArangoDB 1.2 still supports these queries, but the above syntax is much
simpler and intuitive.

ArangoDB 1.2 also supports negative list indexes to return list elements from
the end of the list using the `[ ]` operator.

To return the last element from a list, the following query can be used in
ArangoDB 1.2:
    
    LET l = [ 1, 2, 3, 4 ] RETURN l[-1]

whereas in ArangoDB 1.1, the length of the list had to be determined, and the
element offset had to be calculated based on that:

    LET l = [ 1, 2, 3, 4 ] RETURN l[LENGTH(l) - 1]

Note: ArangoDB 1.2 still supports this, but the above form is much simpler.

### Index usage and optimisations

AQL queries in ArangoDB 1.2 will use indexes in more cases than in previous
versions of ArangoDB:

- querying with an equality predicate on the `_key` attribute will try to
  use the primary index
- querying on `_from` and `_to` attributes of documents in an edge collection 
  will try to use the edges index
- if the result of the `PATHS()` function is restricted to specific `source` or
  `destination` vertexes (identified by dedicated `_id` values), the query will
  try to use the edges index.
- some specific LIMIT and SORT operations will be pushed upwards in the execution 
  pipeline to make queries faster.

Note: `try` in this context means that the query optimiser will try to apply the
mentioned optimisations. Specific optimisations might not be applied if other parts
of the query prohibit the optimisations (e.g. querying with logical OR on different
attributes will likely disable several optimisations).

Fulltext indexes {#NewFeatures12Fulltext}
-----------------------------------------

* added fulltext index type. This index type allows indexing words and prefixes of
  words from a specific document attribute. The index can be queries using a
  SimpleQueryFull object, the HTTP REST API at /_api/simple/fulltext, or via AQL

CORS support {#NewFeatures12CORS}
---------------------------------

ArangoDB 1.2 offers support for Cross-Origin Resource Sharing (CORS), allowing it
to be used as AJAX back end even in a cross-domain request setup.

Browsers will normally not allow cross-domain requests for AJAX calls due to security
reasons. Modern browsers allow defined exceptions from this same-origin policy rule,
provided the client code sets some specific properties in the XMLHttpRequest object.
Browsers will then patch requests with some additional headers, and will allow 
cross-domain requests if the server returns that request with a specifically patched
response (see 
@EXTREF_S{http://en.wikipedia.org/wiki/Cross-origin_resource_sharing, CORS on Wikipedia}
for more details.

To support CORS, ArangoDB 1.2 will answer requests that include a HTTP `Origin` header
with an `Access-Control-Allow-Origin` header with the originally request value.
ArangoDB 1.2 will also answer CORS preflight requests that are issued from browsers
via the HTTP OPTIONS method.
More details on how ArangoDB handles CORS requests can be found in @ref CommunicationCors.

In-memory collections {#NewFeatures12MemoryCollections}
-------------------------------------------------------

ArangoDB collections are persistent and collection data is synchronised to disk 
frequently.

For some specific use cases persistence and synchronisation are overkill. This is
true for "unimportant" data that can be reproduced from other sources easily.
An example for such data are temporary calculations or caches.

To support managing data that does not require any sort of persistence, ArangoDB 1.2
introduces in-memory collections as an *experimental feature*.

Experimental means that the feature might not be available on all platforms, and
that it might change drastically or even be removed in a future version of ArangoDB.

To create an in-memory collection without persistence, the optional attribute
`isVolatile` can be set to `true`:

    arangosh> db._create("mycache", { isVolatile: true});
    [ArangoCollection 316961790811, "mycache" (type document, status loaded)]

The collection can be used normally, and its definition (name, id, type) will remain
even after a server restart. However, all data written into in-memory collections
is lost when the collection is unloaded or the server is shut down:

    arangosh> db.mycache.save({ value : 1 });
    { "error" : false, "_id" : "mycache/316963298139", "_rev" : "316963298139", "_key" : "316963298139" } 

    arangosh> db.mycache.count();
    1

    arangosh> db.mycache.unload();
    
    arangosh> db.mycache.count();
    0

So please be careful when employing this type of collection. It should never be used
for any critical data that requires persistence and durability.

The benefits of this type of collection are that there is no disk overhead for
document operations in it. The actual performance gains over regular collections are
highly platform-dependent.

Other Improvements {#NewFeatures12Other}
----------------------------------------

### Remove by Example

ArangoDB 1.2 provides a "Remove by Example" method that can be used to delete all documents
from a collection that match the specified example. This can be used to delete multiple
documents with a single operation.

The `removeByExample` method returns the number of documents removed:

    arangosh> db._create("mycollection");
    arangosh> db.mycollection.save({ value: 1 });
    arangosh> db.mycollection.save({ value: 2 });
    arangosh> db.mycollection.save({ value: 3 });
    arangosh> db.mycollection.save({ value: 1 });

    arangosh> db.mycollection.removeByExample({ value: 1 });
    2

The number of documents to remove can optionally be limited.

The method is also available via the REST API (@ref HttpSimple).

### Replace / Update by Example

ArangoDB 1.2 also provides "Replace by Example" and "Update by Example" methods that 
can be used to fully or partially update documents from a collection that match the 
specified example. This can be used to replace document values easily with a single 
operation.

The `replaceByExample` and `updateByExaple` methods return the number of documents 
modified. Both operations can be limited to a specific number of documents if required.

Both methods are also available via the REST API (@ref HttpSimple).

### Collection revision id

ArangoDB 1.2 collections have a revision property. The current revision of a collection
can be queried to determine whether data in a collection has changed.

The information can be used by clients to invalidate caches etc. Here's an example:

    arangosh> db._create("mycollection");
    [ArangoCollection 316964608859, "mycollection" (type document, status loaded)]

    arangosh> db.mycollection.save({ value : 1 });
    { "error" : false, "_id" : "mycollection/316966312795", "_rev" : "316966312795", "_key" : "316966312795" }

    arangosh> db.mycollection.revision();
    316966312795

    arangosh> db.mycollection.save({ value : 2 });
    { "error" : false, "_id" : "mycollection/316966378331", "_rev" : "316966378331", "_key" : "316966378331" }

    arangosh> db.mycollection.revision();
    316966378331

    arangosh> db.mycollection.remove("316966378331");
    true

    arangosh> db.mycollection.revision();
    316966443867

The revision value returned should be treated as an opaque string. Clients should only
use the value to check whether it is different from a previous revision value.

The revision value is also available via the REST API (@ref HttpCollection).

### Number of shapes

In some cases it is useful to determine how many shapes are present in a specific collection
("shape" is ArangoDB lingo for a document format, consisting of attribute names and data types).

Though shapes are implicitly managed by ArangoDB and users can not create or delete shapes,
the number of shapes can give a useful hint on how much variety the documents in a collection
have or had.

The number of shapes is returned by a collection's `figures()` method, and is also provided 
in the return value of the figures REST API call (@ref HttpCollection).

### Edges index

Insertion into an edges index is faster than in ArangoDB 1.1. This speeds up inserting
documents into an edges collection, or loading an edges collection. The benefits will
only be noticeable if the collection is big.

### Allow importing of JSON lists

`arangoimp` and the REST bulk import API (@ref HttpImport) of ArangoDB allow mass importing
documents from a file in these formats:

- individual JSON documents: each line in the input file is a standalone JSON document:
      { _key: "key1", value: 1 }
      { _key: "key2", value: 2 }
      ...

- CSV: the input file must have a header line which defines the attribute names. All 
  following lines are considered to be the attribute values

- TSV: like CSV, but not using a comma as the separator but tabs

ArangoDB 1.2 also allows to import an input file that contains a JSON list of JSON
documents as follows:

    [
      { _key: "key1", value: 1 },
      { _key: "key2", value: 2 },
      ...
    ]

Note that there is a size restriction for this format. The whole input file needs to 
fit into a single buffer, or `arangoimp` will refuse to import the data.

The buffer size can be adjusted when invoking `arangoimp` by setting the `--max-upload-size`
option to an appropriate value. To set the buffer to 16 MB:

    > arangoimp --file "input.json" --type json --max-upload-size 16777216 --collection myimport --create-collection true

### Importing into edge collections

ArangoDB 1.2 also allows to import documents into edge collections. Previous versions
only supported importing documents into document collections.

When importing data into edge collections, it is mandatory that all imported documents
contain the `_from` and `_to` attributes containing valid references.

### Logging of import failures

When documents are imported via the REST bulk import API (@ref HttpImport), ArangoDB 
returns an aggregate response containing the number of documents inserted plus the
number of errors that occurred during the import.

ArangoDB 1.2 supplies additional information about the errors that occurred in the
server log files. This information can be used to find documents that could not be
imported (for example, because of a unique constraint violation, malformed data etc.).

### Pretty printing in arangosh

`arangosh` now uses colorised output by default, which makes all of its output
more easy to grasp.

Note that colorised output may not be supported on all platforms.

### Deactivatable statistics gathering

ArangoDB normally includes functionality to automatically gather request and
connection-related statistics. Gathering these statistics happens automatically
in a background thread. Collecting statistics can be quite CPU-intensive, especially
when multiple ArangoDB instances are run on the same physical host.

Statistics features can be turned off at compile time using the `--enable-figures=no`
configure option. However, this option affects compilation and thus was not available 
when using any of th precompiled packages. 

For those, ArangoDB 1.2 now provides a startup option `--server.disable-statistics` that
allows turning off the statistics gathering for performance reasons even with an
already compiled binary.

Usage is:
    
    > arangod --server.disable-statistics true ...
