<!-- don't edit here, it's from https://@github.com/arangodb/arangojs.git / docs/Drivers/ -->
# Manipulating indexes

These functions implement the
[HTTP API for manipulating indexes](../../../..//HTTP/Indexes/index.html).

## collection.createIndex

`async collection.createIndex(details): Object`

Creates an arbitrary index on the collection.

**Arguments**

- **details**: `Object`

  For information on the possible properties of the _details_ object, see the
  [HTTP API for manipulating indexes](../../../..//HTTP/Indexes/WorkingWith.html).

**Examples**

```js
const db = new Database();
const collection = db.collection("some-collection");
const index = await collection.createIndex({
  type: "hash",
  fields: ["a", "a.b"]
});
// the index has been created with the handle `index.id`
```

## collection.createHashIndex

`async collection.createHashIndex(fields, [opts]): Object`

Creates a hash index on the collection.

**Arguments**

- **fields**: `Array<string>`

  An array of names of document fields on which to create the index. If the
  value is a string, it will be wrapped in an array automatically.

- **opts**: `Object` (optional)

  Additional options for this index. If the value is a boolean, it will be
  interpreted as _opts.unique_.

For more information on hash indexes, see the
[HTTP API for hash indexes](../../../..//HTTP/Indexes/Hash.html).

**Examples**

```js
const db = new Database();
const collection = db.collection("some-collection");

const index = await collection.createHashIndex("favorite-color");
// the index has been created with the handle `index.id`
assert.deepEqual(index.fields, ["favorite-color"]);

// -- or --

const index = await collection.createHashIndex(["favorite-color"]);
// the index has been created with the handle `index.id`
assert.deepEqual(index.fields, ["favorite-color"]);
```

## collection.createSkipList

`async collection.createSkipList(fields, [opts]): Object`

Creates a skiplist index on the collection.

**Arguments**

- **fields**: `Array<string>`

  An array of names of document fields on which to create the index. If the
  value is a string, it will be wrapped in an array automatically.

- **opts**: `Object` (optional)

  Additional options for this index. If the value is a boolean, it will be
  interpreted as _opts.unique_.

For more information on skiplist indexes, see the
[HTTP API for skiplist indexes](../../../..//HTTP/Indexes/Skiplist.html).

**Examples**

```js
const db = new Database();
const collection = db.collection("some-collection");

const index = await collection.createSkipList("favorite-color");
// the index has been created with the handle `index.id`
assert.deepEqual(index.fields, ["favorite-color"]);

// -- or --

const index = await collection.createSkipList(["favorite-color"]);
// the index has been created with the handle `index.id`
assert.deepEqual(index.fields, ["favorite-color"]);
```

## collection.createGeoIndex

`async collection.createGeoIndex(fields, [opts]): Object`

Creates a geo-spatial index on the collection.

**Arguments**

- **fields**: `Array<string>`

  An array of names of document fields on which to create the index. Currently,
  geo indexes must cover exactly one field. If the value is a string, it will be
  wrapped in an array automatically.

- **opts**: `Object` (optional)

  An object containing additional properties of the index.

For more information on the properties of the _opts_ object see the
[HTTP API for manipulating geo indexes](../../../..//HTTP/Indexes/Geo.html).

**Examples**

```js
const db = new Database();
const collection = db.collection("some-collection");

const index = await collection.createGeoIndex(["latitude", "longitude"]);
// the index has been created with the handle `index.id`
assert.deepEqual(index.fields, ["longitude", "latitude"]);

// -- or --

const index = await collection.createGeoIndex("location", { geoJson: true });
// the index has been created with the handle `index.id`
assert.deepEqual(index.fields, ["location"]);
```

## collection.createFulltextIndex

`async collection.createFulltextIndex(fields, [minLength]): Object`

Creates a fulltext index on the collection.

**Arguments**

- **fields**: `Array<string>`

  An array of names of document fields on which to create the index. Currently,
  fulltext indexes must cover exactly one field. If the value is a string, it
  will be wrapped in an array automatically.

- **minLength** (optional):

  Minimum character length of words to index. Uses a server-specific default
  value if not specified.

For more information on fulltext indexes, see
[the HTTP API for fulltext indexes](../../../..//HTTP/Indexes/Fulltext.html).

**Examples**

```js
const db = new Database();
const collection = db.collection("some-collection");

const index = await collection.createFulltextIndex("description");
// the index has been created with the handle `index.id`
assert.deepEqual(index.fields, ["description"]);

// -- or --

const index = await collection.createFulltextIndex(["description"]);
// the index has been created with the handle `index.id`
assert.deepEqual(index.fields, ["description"]);
```

## collection.createPersistentIndex

`async collection.createPersistentIndex(fields, [opts]): Object`

Creates a Persistent index on the collection. Persistent indexes are similarly
in operation to skiplist indexes, only that these indexes are in disk as opposed
to in memory. This reduces memory usage and DB startup time, with the trade-off
being that it will always be orders of magnitude slower than in-memory indexes.

**Arguments**

- **fields**: `Array<string>`

  An array of names of document fields on which to create the index.

- **opts**: `Object` (optional)

  An object containing additional properties of the index.

For more information on the properties of the _opts_ object see
[the HTTP API for manipulating Persistent indexes](../../../..//HTTP/Indexes/Persistent.html).

**Examples**

```js
const db = new Database();
const collection = db.collection("some-collection");

const index = await collection.createPersistentIndex(["name", "email"]);
// the index has been created with the handle `index.id`
assert.deepEqual(index.fields, ["name", "email"]);
```

## collection.index

`async collection.index(indexHandle): Object`

Fetches information about the index with the given _indexHandle_ and returns it.

**Arguments**

- **indexHandle**: `string`

  The handle of the index to look up. This can either be a fully-qualified
  identifier or the collection-specific key of the index. If the value is an
  object, its _id_ property will be used instead. Alternatively, the index
  may be looked up by name.

**Examples**

```js
const db = new Database();
const collection = db.collection("some-collection");
const index = await collection.createFulltextIndex("description");
const result = await collection.index(index.id);
assert.equal(result.id, index.id);
// result contains the properties of the index

// -- or --

const result = await collection.index(index.id.split("/")[1]);
assert.equal(result.id, index.id);

// -- or --

const result = await collection.index(index.name);
assert.equal(result.id, index.id);
assert.equal(result.name, index.name);
// result contains the properties of the index
```

## collection.indexes

`async collection.indexes(): Array<Object>`

Fetches a list of all indexes on this collection.

**Examples**

```js
const db = new Database();
const collection = db.collection("some-collection");
await collection.createFulltextIndex("description");
const indexes = await collection.indexes();
assert.equal(indexes.length, 1);
// indexes contains information about the index
```

## collection.dropIndex

`async collection.dropIndex(indexHandle): Object`

Deletes the index with the given _indexHandle_ from the collection.

**Arguments**

- **indexHandle**: `string`

  The handle of the index to delete. This can either be a fully-qualified
  identifier or the collection-specific key of the index. If the value is an
  object, its _id_ property will be used instead.

**Examples**

```js
const db = new Database();
const collection = db.collection("some-collection");
const index = await collection.createFulltextIndex("description");
await collection.dropIndex(index.id);
// the index has been removed from the collection

// -- or --

await collection.dropIndex(index.id.split("/")[1]);
// the index has been removed from the collection
```

## collection.createCapConstraint

`async collection.createCapConstraint(size): Object`

Creates a cap constraint index on the collection.

{% hint 'warning' %}
This method is not available when targeting ArangoDB 3.0 or later,
see [Compatibility](../../GettingStarted/README.md#compatibility).
{% endhint %}

**Arguments**

- **size**: `Object`

  An object with any of the following properties:

  - **size**: `number` (optional)

    The maximum number of documents in the collection.

  - **byteSize**: `number` (optional)

    The maximum size of active document data in the collection (in bytes).

If _size_ is a number, it will be interpreted as _size.size_.

For more information on the properties of the _size_ object see the
[HTTP API for creating cap constraints](https://docs.arangodb.com/2.8/HttpIndexes/Cap.html).

**Examples**

```js
const db = new Database();
const collection = db.collection("some-collection");

const index = await collection.createCapConstraint(20);
// the index has been created with the handle `index.id`
assert.equal(index.size, 20);

// -- or --

const index = await collection.createCapConstraint({ size: 20 });
// the index has been created with the handle `index.id`
assert.equal(index.size, 20);
```
