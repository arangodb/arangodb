

@brief ensures that a unique constraint exists
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

@EXAMPLE_ARANGOSH_OUTPUT{ensureUniqueConstraint}
~db._create("test");
db.test.ensureIndex({ type: "hash", fields: [ "a", "b.c" ], unique: true });
db.test.save({ a : 1, b : { c : 1 } });
db.test.save({ a : 1, b : { c : 1 } }); // xpError(ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED)
db.test.save({ a : 1, b : { c : null } });
db.test.save({ a : 1 });  // xpError(ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED)
~db._drop("test");
@END_EXAMPLE_ARANGOSH_OUTPUT

