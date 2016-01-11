

@brief ensures that a non-unique skiplist index exists
`collection.ensureIndex({ type: "skiplist", fields: [ "field1", ..., "fieldn" ] })`

Creates a non-unique skiplist index on all documents using *field1*, ...
*fieldn* as attribute paths. At least one attribute path has to be given.
The index will be non-sparse by default.

To create a sparse unique index, set the *sparse* attribute to `true`.

In case that the index was successfully created, an object with the index
details, including the index-identifier, is returned.

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

