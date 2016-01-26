

@brief ensures that a non-unique hash index exists
`collection.ensureIndex({ type: "hash", fields: [ "field1", ..., "fieldn" ] })`

Creates a non-unique hash index on all documents using  *field1*, ... *fieldn*
as attribute paths. At least one attribute path has to be given.
The index will be non-sparse by default.

To create a sparse unique index, set the *sparse* attribute to `true`:

`collection.ensureIndex({ type: "hash", fields: [ "field1", ..., "fieldn" ], sparse: true })`

In case that the index was successfully created, an object with the index
details, including the index-identifier, is returned.

@EXAMPLE_ARANGOSH_OUTPUT{ensureHashIndex}
~db._create("test");
db.test.ensureIndex({ type: "hash", fields: [ "a" ] });
db.test.save({ a : 1 });
db.test.save({ a : 1 });
db.test.save({ a : null });
~db._drop("test");
@END_EXAMPLE_ARANGOSH_OUTPUT

