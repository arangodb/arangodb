<!-- don't edit here, its from https://@github.com/arangodb/arangodbjs.git / docs/Drivers/ -->
# Accessing graphs

These functions implement the
[HTTP API for accessing general graphs](../../../..//HTTP/Gharial/index.html).

## database.graph

`database.graph(graphName): Graph`

Returns a _Graph_ instance representing the graph with the given graph name.

## database.listGraphs

`async database.listGraphs(): Array<Object>`

Fetches all graphs from the database and returns an array of graph descriptions.

**Examples**

```js
const db = new Database();
const graphs = await db.listGraphs();
// graphs is an array of graph descriptions
```

## database.graphs

`async database.graphs(): Array<Graph>`

Fetches all graphs from the database and returns an array of _Graph_ instances
for the graphs.

**Examples**

```js
const db = new Database();
const graphs = await db.graphs();
// graphs is an array of Graph instances
```
