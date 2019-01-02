<!-- don't edit here, it's from https://@github.com/arangodb/arangodb-java-driver.git / docs/Drivers/ -->
# Manipulating the vertex collection

## ArangoGraph.vertexCollection

`ArangoGraph.vertexCollection(String name) : ArangoVertexCollection`

Returns a _ArangoVertexCollection_ instance for the given vertex collection name.

**Arguments**

- **name**: `String`

  Name of the vertex collection

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoGraph graph = db.graph("some-graph");

ArangoVertexCollection collection = graph.vertexCollection("some-vertex-collection");
```

## ArangoGraph.getVertexCollections

`ArangoGraph.getVertexCollections() : Collection<String>`

Fetches all vertex collections from the graph and returns a list of collection names.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoGraph graph = db.graph("some-graph");

Collection<String> collections = graph.getVertexCollections();
```

## ArangoGraph.addVertexCollection

`ArangoGraph.addVertexCollection(String name) : GraphEntity`

Adds a vertex collection to the set of collections of the graph.
If the collection does not exist, it will be created.

**Arguments**

- **name**: `String`

  Name of the vertex collection

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoGraph graph = db.graph("some-graph");

graph.addVertexCollection("some-other-collection");
```
