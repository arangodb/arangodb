<!-- don't edit here, its from https://@github.com/arangodb/arangodbjs.git / docs/Drivers/ -->
# Collection API

These functions implement the
[HTTP API for manipulating collections](../../../..//HTTP/Collection/index.html).

The _Collection API_ is implemented by all _Collection_ instances, regardless of
their specific type. I.e. it represents a shared subset between instances of
[_DocumentCollection_](DocumentCollection.md),
[_EdgeCollection_](EdgeCollection.md),
[_GraphVertexCollection_](../Graph/VertexCollection.md) and
[_GraphEdgeCollection_](../Graph/EdgeCollection.md).

## Getting information about the collection

See
[the HTTP API documentation](../../../..//HTTP/Collection/Getting.html)
for details.

## collection.exists

`async collection.exists(): boolean`

Checks whether the collection exists.

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');
const result = await collection.exists();
// result indicates whether the collection exists
```

### collection.get

`async collection.get(): Object`

Retrieves general information about the collection.

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');
const data = await collection.get();
// data contains general information about the collection
```

### collection.properties

`async collection.properties(): Object`

Retrieves the collection's properties.

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');
const data = await collection.properties();
// data contains the collection's properties
```

### collection.count

`async collection.count(): Object`

Retrieves information about the number of documents in a collection.

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');
const data = await collection.count();
// data contains the collection's count
```

### collection.figures

`async collection.figures(): Object`

Retrieves statistics for a collection.

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');
const data = await collection.figures();
// data contains the collection's figures
```

### collection.revision

`async collection.revision(): Object`

Retrieves the collection revision ID.

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');
const data = await collection.revision();
// data contains the collection's revision
```

### collection.checksum

`async collection.checksum([opts]): Object`

Retrieves the collection checksum.

**Arguments**

- **opts**: `Object` (optional)

  For information on the possible options see
  [the HTTP API for getting collection information](../../../..//HTTP/Collection/Getting.html).

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');
const data = await collection.checksum();
// data contains the collection's checksum
```
