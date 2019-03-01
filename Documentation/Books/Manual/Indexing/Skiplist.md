Skiplists
=========

Introduction to Skiplist Indexes
--------------------------------

This is an introduction to ArangoDB's skiplists.

It is possible to define a skiplist index on one or more attributes (or paths)
of documents. This skiplist is then used in queries to locate documents
within a given range. If the skiplist is declared unique, then no two documents are
allowed to have the same set of attribute values.

Creating a new document or updating a document will fail if the uniqueness is violated.
If the skiplist index is declared sparse, a document will be excluded from the index and no
uniqueness checks will be performed if any index attribute value is not set or has a value
of `null`.

Accessing Skiplist Indexes from the Shell
-----------------------------------------

### Unique Skiplist Index

<!-- js/server/modules/@arangodb/arango-collection.js-->

Ensures that a unique skiplist index exists:
`collection.ensureIndex({ type: "skiplist", fields: [ "field1", ..., "fieldn" ], unique: true })`

Creates a unique skiplist index on all documents using *field1*, ... *fieldn*
as attribute paths. At least one attribute path has to be given. The index will
be non-sparse by default.

All documents in the collection must differ in terms of the indexed
attributes. Creating a new document or updating an existing document will
fail if the attribute uniqueness is violated.

To create a sparse unique index, set the *sparse* attribute to `true`:

`collection.ensureIndex({ type: "skiplist", fields: [ "field1", ..., "fieldn" ], unique: true, sparse: true })`

In a sparse index all documents will be excluded from the index that do not
contain at least one of the specified index attributes or that have a value
of `null` in any of the specified index attributes. Such documents will
not be indexed, and not be taken into account for uniqueness checks.

In a non-sparse index, these documents will be indexed (for non-present
indexed attributes, a value of `null` will be used) and will be taken into
account for uniqueness checks.

In case that the index was successfully created, an object with the index
details, including the index-identifier, is returned.

    @startDocuBlockInline ensureUniqueSkiplistSingle
    @EXAMPLE_ARANGOSH_OUTPUT{ensureUniqueSkiplistSingle}
    ~db._create("ids");
    db.ids.ensureIndex({ type: "skiplist", fields: [ "myId" ], unique: true });
    db.ids.save({ "myId": 123 });
    db.ids.save({ "myId": 456 });
    db.ids.save({ "myId": 789 });
    db.ids.save({ "myId": 123 });  // xpError(ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED)
    ~db._drop("ids");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock ensureUniqueSkiplistSingle

    @startDocuBlockInline ensureUniqueSkiplistMultiColumn
    @EXAMPLE_ARANGOSH_OUTPUT{ensureUniqueSkiplistMultiColumn}
    ~db._create("ids");
    db.ids.ensureIndex({ type: "skiplist", fields: [ "name.first", "name.last" ], unique: true });
    db.ids.save({ "name" : { "first" : "hans", "last": "hansen" }});
    db.ids.save({ "name" : { "first" : "jens", "last": "jensen" }});
    db.ids.save({ "name" : { "first" : "hans", "last": "jensen" }});
    db.ids.save({ "name" : { "first" : "hans", "last": "hansen" }});  // xpError(ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED)
    ~db._drop("ids");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock ensureUniqueSkiplistMultiColumn


### Non-unique Skiplist Index

<!-- js/server/modules/@arangodb/arango-collection.js-->

Ensures that a non-unique skiplist index exists:
`collection.ensureIndex({ type: "skiplist", fields: [ "field1", ..., "fieldn" ] })`

Creates a non-unique skiplist index on all documents using *field1*, ...
*fieldn* as attribute paths. At least one attribute path has to be given.
The index will be non-sparse by default.

To create a sparse non-unique index, set the *sparse* attribute to `true`.

`collection.ensureIndex({ type: "skiplist", fields: [ "field1", ..., "fieldn" ], sparse: true })`

In case that the index was successfully created, an object with the index
details, including the index-identifier, is returned.

    @startDocuBlockInline ensureSkiplist
    @EXAMPLE_ARANGOSH_OUTPUT{ensureSkiplist}
    ~db._create("names");
    db.names.ensureIndex({ type: "skiplist", fields: [ "first" ] });
    db.names.save({ "first" : "Tim" });
    db.names.save({ "first" : "Tom" });
    db.names.save({ "first" : "John" });
    db.names.save({ "first" : "Tim" });
    db.names.save({ "first" : "Tom" });
    ~db._drop("names");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock ensureSkiplist

### Skiplist Array Index

Ensures that a skiplist array index exists (non-unique):
`collection.ensureIndex({ type: "skiplist", fields: [ "field1[*]", ..., "fieldn[*]" ] })`

Creates a non-unique skiplist array index for the individual elements of the array
attributes <i>field1[*]</i>, ... <i>fieldn[*]</i> found in the documents. At least
one attribute path has to be given. The index always treats the indexed arrays as
sparse.

It is possible to combine array indexing with standard indexing:
`collection.ensureIndex({ type: "skiplist", fields: [ "field1[*]", "field2" ] })`

In case that the index was successfully created, an object with the index
details, including the index-identifier, is returned.

    @startDocuBlockInline ensureSkiplistArray
    @EXAMPLE_ARANGOSH_OUTPUT{ensureSkiplistArray}
    ~db._create("test");
    db.test.ensureIndex({ type: "skiplist", fields: [ "a[*]" ] });
    db.test.save({ a : [ 1, 2 ] });
    db.test.save({ a : [ 1, 3 ] });
    db.test.save({ a : null });
    ~db._drop("test");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock ensureSkiplistArray

### Query by example using a skiplist index

Constructs a query-by-example using a skiplist index:
`collection.byExample(example)`

Selects all documents from the collection that match the specified example
and returns a cursor. A skiplist index will be used if present.

You can use *toArray*, *next*, or *hasNext* to access the
result. The result can be limited using the *skip* and *limit*
operator.

An attribute name of the form *a.b* is interpreted as attribute path,
not as attribute. If you use

```json
{ "a" : { "c" : 1 } }
```

as example, then you will find all documents, such that the attribute
*a* contains a document of the form *{c : 1 }*. For example the document

```json
{ "a" : { "c" : 1 }, "b" : 1 }
```

will match, but the document

```json
{ "a" : { "c" : 1, "b" : 1 } }
```

will not.

However, if you use

```json
{ "a.c" : 1 },
```

then you will find all documents, which contain a sub-document in *a*
that has an attribute *c* of value *1*. Both the following documents

```json
{ "a" : { "c" : 1 }, "b" : 1 }
```
and

```json
{ "a" : { "c" : 1, "b" : 1 } }
```
will match.


Creating Skiplist Index in Background
-------------------------------------

{% hint 'info' %}
This section only applies to the *rocksdb* storage engine
{% endhint %}

Creating new indexes is by default done under an exclusive collection lock. This means
that the collection (or the respective shards) are not available as long as the index
is created. This "foreground" index creation can be undesirable, if you have to perform it
on a live system without a dedicated maintenance window.

Indexes can also be created in "background", not using an exclusive lock during the creation. 
The collection remains available, other CRUD operations can run on the collection while the index is created.
This can be achieved by using the *inBackground* option.

To create a Skiplist index in the background in *arangosh* just specify `inBackground: true`:

```js
db.collection.ensureIndex({ type: "skiplist", fields: [ "value" ], inBackground: true });
```

For more information see "Creating Indexes in Background" in the [Index basics](IndexBasics.md#) page.

