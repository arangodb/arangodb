<!-- don't edit here, its from https://@github.com/arangodb/arangodbjs.git / docs/Drivers/ -->
# Graph API

These functions implement the
[HTTP API for manipulating graphs](https://docs.arangodb.com/latest/HTTP/Gharial/index.html).

## graph.get

`async graph.get(): Object`

Retrieves general information about the graph.

**Examples**

```js
const db = new Database();
const graph = db.graph('some-graph');
const data = await graph.get();
// data contains general information about the graph
```

## graph.create

`async graph.create(properties): Object`

Creates a graph with the given _properties_ for this graph's name, then returns
the server response.

**Arguments**

* **properties**: `Object`

  For more information on the _properties_ object, see
  [the HTTP API documentation for creating graphs](https://docs.arangodb.com/latest/HTTP/Gharial/Management.html).

**Examples**

```js
const db = new Database();
const graph = db.graph('some-graph');
const info = await graph.create({
  edgeDefinitions: [{
    collection: 'edges',
    from: ['start-vertices'],
    to: ['end-vertices']
  }]
});
// graph now exists
```

## graph.drop

`async graph.drop([dropCollections]): Object`

Deletes the graph from the database.

**Arguments**

* **dropCollections**: `boolean` (optional)

  If set to `true`, the collections associated with the graph will also be
  deleted.

**Examples**

```js
const db = new Database();
const graph = db.graph('some-graph');
await graph.drop();
// the graph "some-graph" no longer exists
```
