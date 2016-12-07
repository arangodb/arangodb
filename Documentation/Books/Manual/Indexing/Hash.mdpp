Hash Indexes
============

Introduction to Hash Indexes
----------------------------

It is possible to define a hash index on one or more attributes (or paths) of a
document. This hash index is then used in queries to locate documents in O(1)
operations. If the hash index is unique, then no two documents are allowed to have the
same set of attribute values.

Creating a new document or updating a document will fail if the uniqueness is violated. 
If the index is declared sparse, a document will be excluded from the index and no 
uniqueness checks will be performed if any index attribute value is not set or has a value 
of `null`. 

Accessing Hash Indexes from the Shell
-------------------------------------

### Unique Hash Indexes

<!-- js/server/modules/@arangodb/arango-collection.js-->

Ensures that a unique constraint exists:
`collection.ensureIndex({ type: "hash", fields: [ "field1", ..., "fieldn" ], unique: true })`

Creates a unique hash index on all documents using *field1*, ... *fieldn*
as attribute paths. At least one attribute path has to be given.
The index will be non-sparse by default.

All documents in the collection must differ in terms of the indexed 
attributes. Creating a new document or updating an existing document will
will fail if the attribute uniqueness is violated. 

To create a sparse unique index, set the *sparse* attribute to `true`:

`collection.ensureIndex({ type: "hash", fields: [ "field1", ..., "fieldn" ], unique: true, sparse: true })`

In case that the index was successfully created, the index identifier is returned.

Non-existing attributes will default to `null`.
In a sparse index all documents will be excluded from the index for which all
specified index attributes are `null`. Such documents will not be taken into account
for uniqueness checks.

In a non-sparse index, **all** documents regardless of `null` - attributes will be
indexed and will be taken into account for uniqueness checks.

In case that the index was successfully created, an object with the index
details, including the index-identifier, is returned.

    @startDocuBlockInline ensureUniqueConstraint
    @EXAMPLE_ARANGOSH_OUTPUT{ensureUniqueConstraint}
    ~db._create("test");
    db.test.ensureIndex({ type: "hash", fields: [ "a", "b.c" ], unique: true });
    db.test.save({ a : 1, b : { c : 1 } });
    db.test.save({ a : 1, b : { c : 1 } }); // xpError(ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED)
    db.test.save({ a : 1, b : { c : null } });
    db.test.save({ a : 1 });  // xpError(ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED)
    ~db._drop("test");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock ensureUniqueConstraint

### Non-unique Hash Indexes

<!-- js/server/modules/@arangodb/arango-collection.js-->

Ensures that a non-unique hash index exists:
`collection.ensureIndex({ type: "hash", fields: [ "field1", ..., "fieldn" ] })`

Creates a non-unique hash index on all documents using  *field1*, ... *fieldn*
as attribute paths. At least one attribute path has to be given.
The index will be non-sparse by default.

To create a sparse unique index, set the *sparse* attribute to `true`:

`collection.ensureIndex({ type: "hash", fields: [ "field1", ..., "fieldn" ], sparse: true })`

In case that the index was successfully created, an object with the index
details, including the index-identifier, is returned.

    @startDocuBlockInline ensureHashIndex
    @EXAMPLE_ARANGOSH_OUTPUT{ensureHashIndex}
    ~db._create("test");
    db.test.ensureIndex({ type: "hash", fields: [ "a" ] });
    db.test.save({ a : 1 });
    db.test.save({ a : 1 });
    db.test.save({ a : null });
    ~db._drop("test");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock ensureHashIndex

### Hash Array Indexes

Ensures that a hash array index exists (non-unique):
`collection.ensureIndex({ type: "hash", fields: [ "field1[*]", ..., "fieldn[*]" ] })`

Creates a non-unique hash array index for the individual elements of the array
attributes <i>field1[*]</i>, ... <i>fieldn[*]</i> found in the documents. At least
one attribute path has to be given. The index always treats the indexed arrays as
sparse.

It is possible to combine array indexing with standard indexing:
`collection.ensureIndex({ type: "hash", fields: [ "field1[*]", "field2" ] })`

In case that the index was successfully created, an object with the index
details, including the index-identifier, is returned.

    @startDocuBlockInline ensureHashIndexArray
    @EXAMPLE_ARANGOSH_OUTPUT{ensureHashIndexArray}
    ~db._create("test");
    db.test.ensureIndex({ type: "hash", fields: [ "a[*]" ] });
    db.test.save({ a : [ 1, 2 ] });
    db.test.save({ a : [ 1, 3 ] });
    db.test.save({ a : null });
    ~db._drop("test");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock ensureHashIndexArray


Ensure uniqueness of relations in edge collections
--------------------------------------------------

It is possible to create secondary indexes using the edge attributes `_from`
and `_to`, starting with ArangoDB 3.0. A combined index over both fields together
with the unique option enabled can be used to prevent duplicate relations from
being created.

For example, a document collection *verts* might contain vertices with the document
handles `verts/A`, `verts/B` and `verts/C`. Relations between these documents can
be stored in an edge collection *edges* for instance. Now, you may want to make sure
that the vertex `verts/A` is never linked to `verts/B` by an edge more than once.
This can be achieved by adding a unique, non-sparse hash index for the fields `_from`
and `_to`:

    db.edges.ensureIndex({ type: "hash", fields: [ "_from", "_to" ], unique: true });

Creating an edge `{ _from: "verts/A", _to: "verts/B" }` in *edges* will be accepted,
but only once. Another attempt to store an edge with the relation **A** → **B** will
be rejected by the server with a *unique constraint violated* error. This includes
updates to the `_from` and `_to` fields.

Note that adding a relation **B** → **A** is still possible, so is **A** → **A**
and **B** → **B**, because they are all different relations in a directed graph.
Each one can only occur once however.
