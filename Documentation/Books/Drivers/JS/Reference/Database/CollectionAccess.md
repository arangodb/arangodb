<!-- don't edit here, its from https://@github.com/arangodb/arangodbjs.git / docs/Drivers/ -->
# Accessing collections

These functions implement the
[HTTP API for accessing collections](../../../..//HTTP/Collection/Getting.html).

## database.collection

`database.collection(collectionName): DocumentCollection`

Returns a _DocumentCollection_ instance for the given collection name.

**Arguments**

- **collectionName**: `string`

  Name of the edge collection.

**Examples**

```js
const db = new Database();
const collection = db.collection("potatoes");
```

## database.edgeCollection

`database.edgeCollection(collectionName): EdgeCollection`

Returns an _EdgeCollection_ instance for the given collection name.

**Arguments**

- **collectionName**: `string`

  Name of the edge collection.

**Examples**

```js
const db = new Database();
const collection = db.edgeCollection("potatoes");
```

## database.listCollections

`async database.listCollections([excludeSystem]): Array<Object>`

Fetches all collections from the database and returns an array of collection
descriptions.

**Arguments**

- **excludeSystem**: `boolean` (Default: `true`)

  Whether system collections should be excluded.

**Examples**

```js
const db = new Database();

const collections = await db.listCollections();
// collections is an array of collection descriptions
// not including system collections

// -- or --

const collections = await db.listCollections(false);
// collections is an array of collection descriptions
// including system collections
```

## database.collections

`async database.collections([excludeSystem]): Array<Collection>`

Fetches all collections from the database and returns an array of
_DocumentCollection_ and _EdgeCollection_ instances for the collections.

**Arguments**

- **excludeSystem**: `boolean` (Default: `true`)

  Whether system collections should be excluded.

**Examples**

```js
const db = new Database();

const collections = await db.collections()
// collections is an array of DocumentCollection
// and EdgeCollection instances
// not including system collections

// -- or --

const collections = await db.collections(false)
// collections is an array of DocumentCollection
// and EdgeCollection instances
// including system collections
```
