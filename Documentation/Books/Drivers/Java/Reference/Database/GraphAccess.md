<!-- don't edit here, it's from https://@github.com/arangodb/arangodb-java-driver.git / docs/Drivers/ -->
# Accessing graphs

These functions implement the
[HTTP API for accessing general graphs](../../../..//HTTP/Gharial/index.html).

## ArangoDatabase.graph

`ArangoDatabase.graph(String name) : ArangoGraph`

Returns a _ArangoGraph_ instance for the given graph name.

**Arguments**

- **name**: `String`

  Name of the graph

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoGraph graph = db.graph("myGraph");
```
