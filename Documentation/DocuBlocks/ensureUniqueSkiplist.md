

@brief ensures that a unique skiplist index exists
`collection.ensureIndex({ type: "skiplist", fields: [ "field1", ..., "fieldn" ], unique: true })`

Creates a unique skiplist index on all documents using *field1*, ... *fieldn*
as attribute paths. At least one attribute path has to be given. The index will
be non-sparse by default.

All documents in the collection must differ in terms of the indexed 
attributes. Creating a new document or updating an existing document will
will fail if the attribute uniqueness is violated. 

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

@EXAMPLE_ARANGOSH_OUTPUT{ensureUniqueSkiplist}
~db._create("ids");
db.ids.ensureIndex({ type: "skiplist", fields: [ "myId" ], unique: true });
db.ids.save({ "myId": 123 });
db.ids.save({ "myId": 456 });
db.ids.save({ "myId": 789 });
db.ids.save({ "myId": 123 });  // xpError(ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED)
~db._drop("ids");
@END_EXAMPLE_ARANGOSH_OUTPUT

@EXAMPLE_ARANGOSH_OUTPUT{ensureUniqueSkiplistMultiColmun}
~db._create("ids");
db.ids.ensureIndex({ type: "skiplist", fields: [ "name.first", "name.last" ], unique: true });
db.ids.save({ "name" : { "first" : "hans", "last": "hansen" }});
db.ids.save({ "name" : { "first" : "jens", "last": "jensen" }});
db.ids.save({ "name" : { "first" : "hans", "last": "jensen" }});
| db.ids.save({ "name" : { "first" : "hans", "last": "hansen" }}); 
~ // xpError(ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED)
~db._drop("ids");
@END_EXAMPLE_ARANGOSH_OUTPUT
