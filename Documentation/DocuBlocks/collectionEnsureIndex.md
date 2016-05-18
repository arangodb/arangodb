

@brief ensures that an index exists
`collection.ensureIndex(index-description)`

Ensures that an index according to the *index-description* exists. A
new index will be created if none exists with the given description.

The *index-description* must contain at least a *type* attribute.
Other attributes may be necessary, depending on the index type.

**type** can be one of the following values:
- *hash*: hash index
- *skiplist*: skiplist index
- *fulltext*: fulltext index
- *geo1*: geo index, with one attribute
- *geo2*: geo index, with two attributes

**sparse** can be *true* or *false*.

For *hash*, and *skiplist* the sparsity can be controlled, *fulltext* and *geo*
are [sparse](WhichIndex.md) by definition.

**unique** can be *true* or *false* and is supported by *hash* or *skiplist*

Calling this method returns an index object. Whether or not the index
object existed before the call is indicated in the return attribute
*isNewlyCreated*.

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{collectionEnsureIndex}
~db._create("test");
db.test.ensureIndex({ type: "hash", fields: [ "a" ], sparse: true });
db.test.ensureIndex({ type: "hash", fields: [ "a", "b" ], unique: true });
~db._drop("test");
@END_EXAMPLE_ARANGOSH_OUTPUT


