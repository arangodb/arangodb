<!-- don't edit here, it's from https://@github.com/arangodb/arangojs.git / docs/Drivers/ -->
# Manipulating vertices

## graph.vertexCollection

`graph.vertexCollection(collectionName): GraphVertexCollection`

Returns a new [_GraphVertexCollection_ instance](VertexCollection.md)
with the given name for this graph.

**Arguments**

* **collectionName**: `string`

  Name of the vertex collection.

**Examples**

```js
const db = new Database();
const graph = db.graph("some-graph");
const collection = graph.vertexCollection("vertices");
assert.equal(collection.name, "vertices");
// collection is a GraphVertexCollection
```

## graph.listVertexCollections

`async graph.listVertexCollections([excludeOrphans]): Array<Object>`

Fetches all vertex collections from the graph and returns an array of collection descriptions.

**Arguments**

* **excludeOrphans**: `boolean` (Default: `false`)

  Whether orphan collections should be excluded.

**Examples**

```js
const graph = db.graph('some-graph');

const collections = await graph.listVertexCollections();
// collections is an array of collection descriptions
// including orphan collections

// -- or --

const collections = await graph.listVertexCollections(true);
// collections is an array of collection descriptions
// not including orphan collections
```

## graph.vertexCollections

`async graph.vertexCollections([excludeOrphans]): Array<Collection>`

Fetches all vertex collections from the database and returns an array of _GraphVertexCollection_ instances for the collections.

**Arguments**

* **excludeOrphans**: `boolean` (Default: `false`)

  Whether orphan collections should be excluded.

**Examples**

```js
const graph = db.graph('some-graph');

const collections = await graph.vertexCollections()
// collections is an array of GraphVertexCollection
// instances including orphan collections

// -- or --

const collections = await graph.vertexCollections(true)
// collections is an array of GraphVertexCollection
// instances not including orphan collections
```

## graph.addVertexCollection

`async graph.addVertexCollection(collectionName): Object`

Adds the collection with the given _collectionName_ to the graph's vertex
collections.

**Arguments**

* **collectionName**: `string`

  Name of the vertex collection to add to the graph.

**Examples**

```js
const db = new Database();
const graph = db.graph('some-graph');
await graph.addVertexCollection('vertices');
// the collection "vertices" has been added to the graph
```

## graph.removeVertexCollection

`async graph.removeVertexCollection(collectionName, [dropCollection]): Object`

Removes the vertex collection with the given _collectionName_ from the graph.

**Arguments**

* **collectionName**: `string`

  Name of the vertex collection to remove from the graph.

* **dropCollection**: `boolean` (optional)

  If set to `true`, the collection will also be deleted from the database.

**Examples**

```js
const db = new Database();
const graph = db.graph('some-graph');
await graph.removeVertexCollection('vertices')
// collection "vertices" has been removed from the graph

// -- or --

await graph.removeVertexCollection('vertices', true)
// collection "vertices" has been removed from the graph
// the collection has also been dropped from the database
// this may have been a bad idea
```
