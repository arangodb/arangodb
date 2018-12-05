<!-- don't edit here, it's from https://@github.com/arangodb/arangodb-java-driver.git / docs/Drivers/ -->
# Graph API

These functions implement the
[HTTP API for manipulating graphs](../../../..//HTTP/Gharial/index.html).

## ArangoDatabase.createGraph

`ArangoDatabase.createGraph(String name, Collection<EdgeDefinition> edgeDefinitions, GraphCreateOptions options) : GraphEntity`

Create a new graph in the graph module. The creation of a graph requires the
name of the graph and a definition of its edges.

**Arguments**

- **name**: `String`

  Name of the graph

- **edgeDefinitions**: `Collection<EdgeDefinition>`

  An array of definitions for the edge

- **options**: `GraphCreateOptions`

  - **orphanCollections**: `String...`

    Additional vertex collections

  - **isSmart**: `Boolean`

    Define if the created graph should be smart.
    This only has effect in Enterprise Edition.

  - **replicationFactor**: `Integer`

    (The default is 1): in a cluster, this attribute determines how many copies
    of each shard are kept on different DBServers. The value 1 means that only
    one copy (no synchronous replication) is kept. A value of k means that k-1
    replicas are kept. Any two copies reside on different DBServers.
    Replication between them is synchronous, that is, every write operation to
    the "leader" copy will be replicated to all "follower" replicas, before the
    write operation is reported successful. If a server fails, this is detected
    automatically and one of the servers holding copies take over, usually
    without an error being reported.

  - **numberOfShards**: `Integer`

    The number of shards that is used for every collection within this graph.
    Cannot be modified later.

  - **smartGraphAttribute**: `String`

    The attribute name that is used to smartly shard the vertices of a graph.
    Every vertex in this Graph has to have this attribute. Cannot be modified later.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");

EdgeDefinition edgeDefinition = new EdgeDefinition()
                                  .collection("edges")
                                  .from("start-vertices")
                                  .to("end-vertices");
GraphEntity graph = db.createGraph(
  "some-graph", Arrays.asList(edgeDefinition), new GraphCreateOptions()
);
// graph now exists
```

## ArangoGraph.create

`ArangoGraph.create(Collection<EdgeDefinition> edgeDefinitions, GraphCreateOptions options) : GraphEntity`

Create a new graph in the graph module. The creation of a graph requires the
name of the graph and a definition of its edges.

Alternative for [ArangoDatabase.createGraph](#arangodatabasecreategraph).

**Arguments**

- **edgeDefinitions**: `Collection<EdgeDefinition>`

  An array of definitions for the edge

- **options**: `GraphCreateOptions`

  - **orphanCollections**: `String...`

    Additional vertex collections

  - **isSmart**: `Boolean`

    Define if the created graph should be smart.
    This only has effect in Enterprise Edition.

  - **replicationFactor**: `Integer`

    (The default is 1): in a cluster, this attribute determines how many copies
    of each shard are kept on different DBServers. The value 1 means that only
    one copy (no synchronous replication) is kept. A value of k means that k-1
    replicas are kept. Any two copies reside on different DBServers.
    Replication between them is synchronous, that is, every write operation to
    the "leader" copy will be replicated to all "follower" replicas, before the
    write operation is reported successful. If a server fails, this is detected
    automatically and one of the servers holding copies take over, usually
    without an error being reported.

  - **numberOfShards**: `Integer`

    The number of shards that is used for every collection within this graph.
    Cannot be modified later.

  - **smartGraphAttribute**: `String`

    The attribute name that is used to smartly shard the vertices of a graph.
    Every vertex in this Graph has to have this attribute. Cannot be modified later.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");

ArangoGraph graph = db.graph("some-graph");
EdgeDefinition edgeDefinition = new EdgeDefinition()
                                  .collection("edges")
                                  .from("start-vertices")
                                  .to("end-vertices");
graph.create(Arrays.asList(edgeDefinition), new GraphCreateOptions());
// graph now exists
```

## ArangoGraph.exists

`ArangoGraph.exists() : boolean`

Checks whether the graph exists

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");

ArangoGraph graph = db.graph("some-graph");
boolean exists = graph.exists();
```

## ArangoGraph.getInfo

`ArangoGraph.getInfo() : GraphEntity`

Retrieves general information about the graph.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");

ArangoGraph graph = db.graph("some-graph");
GraphEntity info = graph.getInfo();
```

## ArangoGraph.drop

`ArangoGraph.drop(boolean dropCollections) : void`

Deletes the graph from the database.

**Arguments**

- **dropCollections**: `boolean`

  Drop collections of this graph as well. Collections will only be dropped if
  they are not used in other graphs.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");

ArangoGraph graph = db.graph("some-graph");
graph.drop();
// the graph "some-graph" no longer exists
```
