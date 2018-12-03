<!-- don't edit here, it's from https://@github.com/arangodb/arangojs.git / docs/Drivers/ -->
# EdgeCollection API

The _EdgeCollection API_ extends the
[_Collection API_](README.md) with the following methods.

## edgeCollection.document

`async edgeCollection.document(documentHandle, [opts]): Object`

Alias: `edgeCollection.edge`.

Retrieves the edge with the given _documentHandle_ from the collection.

**Arguments**

- **documentHandle**: `string`

  The handle of the edge to retrieve. This can be either the `_id` or the `_key`
  of an edge in the collection, or an edge (i.e. an object with an `_id` or
  `_key` property).

- **opts**: `Object` (optional)

  If _opts_ is set, it must be an object with any of the following properties:

  - **graceful**: `boolean` (Default: `false`)

    If set to `true`, the method will return `null` instead of throwing an
    error if the edge does not exist.

  - **allowDirtyRead**: `boolean` (Default: `false`)

    {% hint 'info' %}
    This option is only available when targeting ArangoDB 3.4 or later,
    see [Compatibility](../../GettingStarted/README.md#compatibility).
    {% endhint %}

    If set to `true`, the request will explicitly permit ArangoDB to return a
    potentially dirty or stale result and arangojs will load balance the
    request without distinguishing between leaders and followers.

If a boolean is passed instead of an options object, it will be interpreted as
the _graceful_ option.

**Examples**

```js
const db = new Database();
const collection = db.edgeCollection("edges");

const edge = await collection.document("some-key");
// the edge exists
assert.equal(edge._key, "some-key");
assert.equal(edge._id, "edges/some-key");

// -- or --

const edge = await collection.document("edges/some-key");
// the edge exists
assert.equal(edge._key, "some-key");
assert.equal(edge._id, "edges/some-key");

// -- or --

const edge = await collection.document("some-key", true);
if (edge === null) {
  // the edge does not exist
}
```

## edgeCollection.documentExists

`async edgeCollection.documentExists(documentHandle): boolean`

Checks whether the edge with the given _documentHandle_ exists.

**Arguments**

- **documentHandle**: `string`

  The handle of the edge to retrieve. This can be either the `_id` or the
  `_key` of a edge in the collection, or an edge (i.e. an object with an
  `_id` or `_key` property).

**Examples**

```js
const db = new Database();
const collection = db.edgeCollection("my-docs");

const exists = await collection.documentExists("some-key");
if (exists === false) {
  // the edge does not exist
}
```

## edgeCollection.save

`async edgeCollection.save(data, [fromId, toId], [opts]): Object`

Creates a new edge between the documents _fromId_ and _toId_ with the given
_data_ and returns an object containing the edge's metadata.

**Arguments**

- **data**: `Object`

  The data of the new edge. If _fromId_ and _toId_ are not specified, the _data_
  needs to contain the properties `_from` and `_to`.

- **fromId**: `string` (optional)

  The handle of the start vertex of this edge. This can be either the `_id` of a
  document in the database, the `_key` of an edge in the collection, or a
  document (i.e. an object with an `_id` or `_key` property).

- **toId**: `string` (optional)

  The handle of the end vertex of this edge. This can be either the `_id` of a
  document in the database, the `_key` of an edge in the collection, or a
  document (i.e. an object with an `_id` or `_key` property).

- **opts**: `Object` (optional)

  If _opts_ is set, it must be an object with any of the following properties:

  - **waitForSync**: `boolean` (Default: `false`)

    Wait until document has been synced to disk.

  - **returnNew**: `boolean` (Default: `false`)

    If set to `true`, return additionally the complete new documents under the
    attribute `new` in the result.

  - **returnOld**: `boolean` (Default: `false`)

    If set to `true`, return additionally the complete old documents under the
    attribute `old` in the result.

  - **silent**: `boolean` (Default: `false`)

    If set to true, an empty object will be returned as response. No meta-data
    will be returned for the created document. This option can be used to save
    some network traffic.

  - **overwrite**: `boolean` (Default: `false`)

    If set to true, the insert becomes a replace-insert. If a document with the
    same \_key already exists the new document is not rejected with unique
    constraint violated but will replace the old document.

If a boolean is passed instead of an options object, it will be interpreted as
the _returnNew_ option.

**Examples**

```js
const db = new Database();
const collection = db.edgeCollection("edges");
const data = { some: "data" };

const info = await collection.save(
  data,
  "vertices/start-vertex",
  "vertices/end-vertex"
);
assert.equal(info._id, "edges/" + info._key);
const edge = await collection.edge(edge);
assert.equal(edge._key, info._key);
assert.equal(edge._rev, info._rev);
assert.equal(edge.some, data.some);
assert.equal(edge._from, "vertices/start-vertex");
assert.equal(edge._to, "vertices/end-vertex");

// -- or --

const info = await collection.save({
  some: "data",
  _from: "verticies/start-vertex",
  _to: "vertices/end-vertex"
});
// ...
```

## edgeCollection.edges

`async edgeCollection.edges(documentHandle): Array<Object>`

Retrieves a list of all edges of the document with the given _documentHandle_.

**Arguments**

- **documentHandle**: `string`

  The handle of the document to retrieve the edges of. This can be either the
  `_id` of a document in the database, the `_key` of an edge in the collection,
  or a document (i.e. an object with an `_id` or `_key` property).

**Examples**

```js
const db = new Database();
const collection = db.edgeCollection("edges");
await collection.import([
  ["_key", "_from", "_to"],
  ["x", "vertices/a", "vertices/b"],
  ["y", "vertices/a", "vertices/c"],
  ["z", "vertices/d", "vertices/a"]
]);
const edges = await collection.edges("vertices/a");
assert.equal(edges.length, 3);
assert.deepEqual(edges.map(edge => edge._key), ["x", "y", "z"]);
```

## edgeCollection.inEdges

`async edgeCollection.inEdges(documentHandle): Array<Object>`

Retrieves a list of all incoming edges of the document with the given
_documentHandle_.

**Arguments**

- **documentHandle**: `string`

  The handle of the document to retrieve the edges of. This can be either the
  `_id` of a document in the database, the `_key` of an edge in the collection,
  or a document (i.e. an object with an `_id` or `_key` property).

**Examples**

```js
const db = new Database();
const collection = db.edgeCollection("edges");
await collection.import([
  ["_key", "_from", "_to"],
  ["x", "vertices/a", "vertices/b"],
  ["y", "vertices/a", "vertices/c"],
  ["z", "vertices/d", "vertices/a"]
]);
const edges = await collection.inEdges("vertices/a");
assert.equal(edges.length, 1);
assert.equal(edges[0]._key, "z");
```

## edgeCollection.outEdges

`async edgeCollection.outEdges(documentHandle): Array<Object>`

Retrieves a list of all outgoing edges of the document with the given
_documentHandle_.

**Arguments**

- **documentHandle**: `string`

  The handle of the document to retrieve the edges of. This can be either the
  `_id` of a document in the database, the `_key` of an edge in the collection,
  or a document (i.e. an object with an `_id` or `_key` property).

**Examples**

```js
const db = new Database();
const collection = db.edgeCollection("edges");
await collection.import([
  ["_key", "_from", "_to"],
  ["x", "vertices/a", "vertices/b"],
  ["y", "vertices/a", "vertices/c"],
  ["z", "vertices/d", "vertices/a"]
]);
const edges = await collection.outEdges("vertices/a");
assert.equal(edges.length, 2);
assert.deepEqual(edges.map(edge => edge._key), ["x", "y"]);
```

## edgeCollection.traversal

`async edgeCollection.traversal(startVertex, opts): Object`

Performs a traversal starting from the given _startVertex_ and following edges
contained in this edge collection.

**Arguments**

- **startVertex**: `string`

  The handle of the start vertex. This can be either the `_id` of a document in
  the database, the `_key` of an edge in the collection, or a document (i.e. an
  object with an `_id` or `_key` property).

- **opts**: `Object`

  See the
  [HTTP API documentation](../../../..//HTTP/Traversal/index.html)
  for details on the additional arguments.

  Please note that while _opts.filter_, _opts.visitor_, _opts.init_,
  _opts.expander_ and _opts.sort_ should be strings evaluating to well-formed
  JavaScript code, it's not possible to pass in JavaScript functions directly
  because the code needs to be evaluated on the server and will be transmitted
  in plain text.

**Examples**

```js
const db = new Database();
const collection = db.edgeCollection("edges");
await collection.import([
  ["_key", "_from", "_to"],
  ["x", "vertices/a", "vertices/b"],
  ["y", "vertices/b", "vertices/c"],
  ["z", "vertices/c", "vertices/d"]
]);
const result = await collection.traversal("vertices/a", {
  direction: "outbound",
  visitor: "result.vertices.push(vertex._key);",
  init: "result.vertices = [];"
});
assert.deepEqual(result.vertices, ["a", "b", "c", "d"]);
```
