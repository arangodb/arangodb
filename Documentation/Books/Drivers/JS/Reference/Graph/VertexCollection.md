<!-- don't edit here, its from https://@github.com/arangodb/arangodbjs.git / docs/Drivers/ -->
# GraphVertexCollection API

The _GraphVertexCollection API_ extends the
[_Collection API_](../Collection/README.md) with the following methods.

## graphVertexCollection.remove

`async graphVertexCollection.remove(documentHandle): Object`

Deletes the vertex with the given _documentHandle_ from the collection.

**Arguments**

* **documentHandle**: `string`

  The handle of the vertex to retrieve. This can be either the `_id` or the
  `_key` of a vertex in the collection, or a vertex (i.e. an object with an
  `_id` or `_key` property).

**Examples**

```js
const graph = db.graph('some-graph');
const collection = graph.vertexCollection('vertices');

await collection.remove('some-key')
// document 'vertices/some-key' no longer exists

// -- or --

await collection.remove('vertices/some-key')
// document 'vertices/some-key' no longer exists
```

## graphVertexCollection.vertex

`async graphVertexCollection.vertex(documentHandle): Object`

Retrieves the vertex with the given _documentHandle_ from the collection.

**Arguments**

* **documentHandle**: `string`

  The handle of the vertex to retrieve. This can be either the `_id` or the
  `_key` of a vertex in the collection, or a vertex (i.e. an object with an
  `_id` or `_key` property).

**Examples**

```js
const graph = db.graph('some-graph');
const collection = graph.vertexCollection('vertices');

const doc = await collection.vertex('some-key');
// the vertex exists
assert.equal(doc._key, 'some-key');
assert.equal(doc._id, 'vertices/some-key');

// -- or --

const doc = await collection.vertex('vertices/some-key');
// the vertex exists
assert.equal(doc._key, 'some-key');
assert.equal(doc._id, 'vertices/some-key');
```

## graphVertexCollection.save

`async graphVertexCollection.save(data): Object`

Creates a new vertex with the given _data_.

**Arguments**

* **data**: `Object`

  The data of the vertex.

**Examples**

```js
const db = new Database();
const graph = db.graph('some-graph');
const collection = graph.vertexCollection('vertices');
const doc = await collection.save({some: 'data'});
assert.equal(doc._id, 'vertices/' + doc._key);
assert.equal(doc.some, 'data');
```
