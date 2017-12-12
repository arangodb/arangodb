# Fulldepth Graph-Traversal

## Problem
Lets assume you have a database and some edges and vertices. Now you need the node with the most connections in fulldepth.

## Solution
You need a custom traversal with the following properties:

* Store all vertices you have visited already
* If you visit an already visited vertex return the connections + 1 and do not touch the edges
* If you visit a fresh vertex visit all its children and sum up their connections. Store this sum and return it + 1
* Repeat for all vertices.

```js
var traversal = require("org/arangodb/graph/traversal");

var knownFilter = function(config, vertex, path) {
  if (config.known[vertex._key] !== undefined) {
    return "prune";
  }
  return "";
};

var sumVisitor = function(config, result, vertex, path) {
  if (config.known[vertex._key] !== undefined) {
    result.sum += config.known[vertex._key];
  } else {
    config.known[vertex._key] = result.sum;
  }
  result.sum += 1;
  return;
};

var config = {
  datasource: traversal.collectionDatasourceFactory(db.e), // e is my edge collection
  strategy: "depthfirst",
  order: "preorder",
  filter: knownFilter,
  expander: traversal.outboundExpander,
  visitor: sumVisitor,
  known: {}
};

var traverser = new traversal.Traverser(config);
var cursor = db.v.all(); // v is my vertex collection
while(cursor.hasNext()) {
  var node = cursor.next();
  traverser.traverse({sum: 0}, node);
}

config.known; // Returns the result of type name: counter. In arangosh this will print out complete result
```

To execute this script accordingly replace db.v and db.e with your collections (v is vertices, e is edges) and write it to a file: (e.g.) traverse.js
then execute it in arangosh:

```
cat traverse.js | arangosh
```

If you want to use it in production you should have a look at the Foxx framework which allows you to store and execute this script on server side and make it accessible via your own API:
[Foxx](https://docs.arangodb.com/2.8/Foxx/index.html)

## Comment
You only compute the connections of one vertex once and cache it then.
Complexity is almost equal to the amount of edges.
In the code below config.known contains the result of all vertices, you then can add the sorting on it.

**Author:** [Michael Hackstein](https://github.com/mchacki)

**Tags:** #graph
