

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

**Note**: *fulltext*, *geo1*, and *geo2* are not yet supported by the RocksDB storage engine and will be rejected as invalid input.

**sparse** can be *true* or *false*.

For *hash*, and *skiplist* the sparsity can be controlled, *fulltext* and *geo*
are [sparse](WhichIndex.md) by definition.

**unique** can be *true* or *false* and is supported by *hash* or *skiplist*

**deduplicate** can be *true* or *false* and is supported by array indexes of
type *hash* or *skiplist*. It controls whether inserting duplicate index values 
from the same document into a unique array index will lead to a unique constraint
error or not. The default value is *true*, so only a single instance of each
non-unique index value will be inserted into the index per document. Trying to
insert a value into the index that already exists in the index will always fail,
regardless of the value of this attribute.

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
