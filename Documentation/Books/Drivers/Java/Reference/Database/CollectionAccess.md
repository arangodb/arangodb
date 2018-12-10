<!-- don't edit here, it's from https://@github.com/arangodb/arangodb-java-driver.git / docs/Drivers/ -->
# Accessing collections

These functions implement the
[HTTP API for accessing collections](../../../..//HTTP/Collection/Getting.html).

## ArangoDatabase.collection

`ArangoDatabase.collection(String name) : ArangoCollection`

Returns a _ArangoCollection_ instance for the given collection name.

**Arguments**

- **name**: `String`

  Name of the collection

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoCollection collection = db.collection("myCollection");
```

## ArangoDatabase.getCollections

`ArangoDatabase.getCollections() : Collection<CollectionEntity>`

Fetches all collections from the database and returns an list of collection descriptions.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
Collection<CollectionEntity> infos = db.getCollections();
```
