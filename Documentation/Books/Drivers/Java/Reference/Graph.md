<!-- don't edit here, its from https://@github.com/arangodb/arangodb-java-driver.git / docs/Drivers/ -->
# Graphs

The driver supports the [graph api](https://docs.arangodb.com/HTTP/Gharial/index.html).

Some of the basic graph operations are described in the following:

## add graph

A graph consists of vertices and edges (stored in collections). Which collections are used within a graph is defined via edge definitions. A graph can contain more than one edge definition, at least one is needed.

```Java
  Collection<EdgeDefinition> edgeDefinitions = new ArrayList<>();
  EdgeDefinition edgeDefinition = new EdgeDefinition();
  // define the edgeCollection to store the edges
  edgeDefinition.collection("myEdgeCollection");
  // define a set of collections where an edge is going out...
  edgeDefinition.from("myCollection1", "myCollection2");

  // repeat this for the collections where an edge is going into
  edgeDefinition.to("myCollection1", "myCollection3");

  edgeDefinitions.add(edgeDefinition);

  // A graph can contain additional vertex collections, defined in the set of orphan collections
  GraphCreateOptions options = new GraphCreateOptions();
  options.orphanCollections("myCollection4", "myCollection5");

  // now it's possible to create a graph
  arangoDB.db("myDatabase").createGraph("myGraph", edgeDefinitions, options);
```

## delete graph

A graph can be deleted by its name

```Java
  arangoDB.db("myDatabase").graph("myGraph").drop();
```

## add vertex

Vertices are stored in the vertex collections defined above.

```Java
  MyObject myObject1 = new MyObject("Homer", 38);
  MyObject myObject2 = new MyObject("Marge", 36);
  arangoDB.db("myDatabase").graph("myGraph").vertexCollection("collection1").insertVertex(myObject1, null);
  arangoDB.db("myDatabase").graph("myGraph").vertexCollection("collection3").insertVertex(myObject2, null);
```

## add edge

Now an edge can be created to set a relation between vertices

```Java
  arangoDB.db("myDatabase").graph("myGraph").edgeCollection("myEdgeCollection").insertEdge(myEdgeObject, null);
```
