<!-- don't edit here, it's from https://@github.com/arangodb/arangodb-java-driver.git / docs/Drivers/ -->
# Collection API

These functions implement the
[HTTP API for collections](../../../..//HTTP/Collection/index.html).

The _ArangoCollection_ API is used for all collections, regardless of
their specific type (document/edge collection).

## Getting information about the collection

See
[the HTTP API documentation](../../../..//HTTP/Collection/Getting.html)
for details.

## ArangoCollection.exists

`ArangoCollection.exists() : boolean`

Checks whether the collection exists

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoCollection collection = db.collection("potatoes");

boolean exists = collection.exists();
```

## ArangoCollection.getInfo

`ArangoCollection.getInfo() : CollectionEntity`

Returns information about the collection.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoCollection collection = db.collection("potatoes");

CollectionEntity info = collection.getInfo();
```

## ArangoCollection.getProperties

`ArangoCollection.getProperties() : CollectionPropertiesEntity`

Reads the properties of the specified collection.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoCollection collection = db.collection("potatoes");

CollectionPropertiesEntity properties = collection.getProperties();
```

## ArangoCollection.getRevision

`ArangoCollection.getRevision() : CollectionRevisionEntity`

Retrieve the collections revision.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoCollection collection = db.collection("potatoes");

CollectionRevisionEntity revision = collection.getRevision();
```

## ArangoCollection.getIndexes

`ArangoCollection.getIndexes() : Collection<IndexEntity>`

Fetches a list of all indexes on this collection.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoCollection collection = db.collection("potatoes");

Collection<IndexEntity> indexes = collection.getIndexes();
```
