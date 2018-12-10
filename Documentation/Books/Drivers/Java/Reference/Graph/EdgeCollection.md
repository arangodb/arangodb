<!-- don't edit here, it's from https://@github.com/arangodb/arangodb-java-driver.git / docs/Drivers/ -->
# Manipulating the edge collection

## ArangoGraph.edgeCollection

`ArangoGraph.edgeCollection(String name) : ArangoEdgeCollection`

Returns a _ArangoEdgeCollection_ instance for the given edge collection name.

**Arguments**

- **name**: `String`

  Name of the edge collection

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoGraph graph = db.graph("some-graph");

ArangoEdgeCollection collection = graph.edgeCollection("some-edge-collection");
```

## ArangoGraph.getEdgeDefinitions

`ArangoGraph.getEdgeDefinitions() : Collection<String>`

Fetches all edge collections from the graph and returns a list of collection names.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoGraph graph = db.graph("some-graph");

Collection<String> collections = graph.getEdgeDefinitions();
```

## ArangoGraph.addEdgeDefinition

`ArangoGraph.addEdgeDefinition(EdgeDefinition definition) : GraphEntity`

Adds the given edge definition to the graph.

**Arguments**

- **definition**: `EdgeDefinition`

  The edge definition

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoGraph graph = db.graph("some-graph");

EdgeDefinition edgeDefinition = new EdgeDefinition()
                                  .collection("edges")
                                  .from("start-vertices")
                                  .to("end-vertices");
graph.addEdgeDefinition(edgeDefinition);
// the edge definition has been added to the graph
```

## ArangoGraph.replaceEdgeDefinition

`ArangoGraph.replaceEdgeDefinition(EdgeDefinition definition) : GraphEntity`

Change one specific edge definition. This will modify all occurrences of this
definition in all graphs known to your database.

**Arguments**

- **definition**: `EdgeDefinition`

  The edge definition

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoGraph graph = db.graph("some-graph");

EdgeDefinition edgeDefinition = new EdgeDefinition()
                                  .collection("edges")
                                  .from("start-vertices")
                                  .to("end-vertices");
graph.replaceEdgeDefinition(edgeDefinition);
// the edge definition has been modified
```

## ArangoGraph.removeEdgeDefinition

`ArangoGraph.removeEdgeDefinition(String definitionName) : GraphEntity`

Remove one edge definition from the graph. This will only remove the
edge collection, the vertex collections remain untouched and can still
be used in your queries.

**Arguments**

- **definitionName**: `String`

  The name of the edge collection used in the definition

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoGraph graph = db.graph("some-graph");

EdgeDefinition edgeDefinition = new EdgeDefinition()
                                  .collection("edges")
                                  .from("start-vertices")
                                  .to("end-vertices");
graph.addEdgeDefinition(edgeDefinition);
// the edge definition has been added to the graph

graph.removeEdgeDefinition("edges");
// the edge definition has been removed
```
