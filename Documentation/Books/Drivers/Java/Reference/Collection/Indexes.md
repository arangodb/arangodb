<!-- don't edit here, it's from https://@github.com/arangodb/arangodb-java-driver.git / docs/Drivers/ -->
# Manipulating indexes

These functions implement the
[HTTP API for manipulating indexes](../../../..//HTTP/Indexes/index.html).

## ArangoCollection.ensureHashIndex

`ArangoCollection.ensureHashIndex(Iterable<String> fields, HashIndexOptions options) : IndexEntity`

Creates a hash index for the collection if it does not already exist.

**Arguments**

- **fields**: `Iterable<String>`

  A list of attribute paths

- **options**: `HashIndexOptions`

  - **unique**: `Boolean`

    If true, then create a unique index

  - **sparse**: `Boolean`

    If true, then create a sparse index

  - **deduplicate**: `Boolean`

    If false, the de-duplication of array values is turned off.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoCollection collection = db.collection("some-collection");

IndexEntity index = collection.ensureHashIndex(Arrays.asList("a", "b.c"));
// the index has been created with the handle `index.getId()`
```

## ArangoCollection.ensureSkipListIndex

`ArangoCollection.ensureSkipListIndex(Iterable<String> fields, SkipListIndexOptions options) : IndexEntity`

Creates a skip-list index for the collection if it does not already exist.

**Arguments**

- **fields**: `Iterable<String>`

  A list of attribute paths

- **options**: `SkipListIndexOptions`

  - **unique**: `Boolean`

    If true, then create a unique index

  - **sparse**: `Boolean`

    If true, then create a sparse index

  - **deduplicate**: `Boolean`

    If false, the de-duplication of array values is turned off.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoCollection collection = db.collection("some-collection");

IndexEntity index = collection.ensureSkipListIndex(
  Arrays.asList("a", "b.c")
);
// the index has been created with the handle `index.getId()`
```

## ArangoCollection.ensureGeoIndex

`ArangoCollection.ensureGeoIndex(Iterable<String> fields, GeoIndexOptions options) : IndexEntity`

Creates a geo index for the collection if it does not already exist.

**Arguments**

- **fields**: `Iterable<String>`

  A list of attribute paths

- **options**: `GeoIndexOptions`

  - **geoJson**: `Boolean`

    If a geo-spatial index on a location is constructed and geoJson is true,
    then the order within the array is longitude followed by latitude.
    This corresponds to the format described in.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoCollection collection = db.collection("some-collection");

IndexEntity index = collection.ensureGeoIndex(
  Arrays.asList("latitude", "longitude")
);
// the index has been created with the handle `index.getId()`
```

## ArangoCollection.ensureFulltextIndex

`ArangoCollection.ensureFulltextIndex(Iterable<String> fields, FulltextIndexOptions options) : IndexEntity`

Creates a fulltext index for the collection if it does not already exist.

**Arguments**

- **fields**: `Iterable<String>`

  A list of attribute paths

- **options**: `FulltextIndexOptions`

  - **minLength**: `Integer`

    Minimum character length of words to index. Will default to a server-defined
    value if unspecified. It is thus recommended to set this value explicitly
    when creating the index.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoCollection collection = db.collection("some-collection");

IndexEntity index = collection.ensureFulltextIndex(
  Arrays.asList("description")
);
// the index has been created with the handle `index.getId()`
```

## ArangoCollection.ensurePersistentIndex

`ArangoCollection.ensurePersistentIndex(Iterable<String> fields, PersistentIndexOptions options) : IndexEntity`

Creates a persistent index for the collection if it does not already exist.

**Arguments**

- **fields**: `Iterable<String>`

  A list of attribute paths

- **options**: `PersistentIndexOptions`

  - **unique**: `Boolean`

    If true, then create a unique index

  - **sparse**: `Boolean`

    If true, then create a sparse index

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoCollection collection = db.collection("some-collection");

IndexEntity index = collection.ensurePersistentIndex(Arrays.asList("a", "b.c"));
// the index has been created with the handle `index.getId()`
```

## ArangoCollection.getIndex

`ArangoCollection.getIndex(String id) : IndexEntity`

Fetches information about the index with the given _id_ and returns it.

**Arguments**

- **id**: `String`

  The index-handle

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoCollection collection = db.collection("some-collection");

IndexEntity index = collection.getIndex("some-index");
```

## ArangoCollection.getIndexes

`ArangoCollection.getIndexes() : Collection<IndexEntity>`

Fetches a list of all indexes on this collection.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoCollection collection = db.collection("some-collection");

Collection<IndexEntity> indexes = collection.getIndexs();
```

## ArangoCollection.deleteIndex

`ArangoCollection.deleteIndex(String id) : String`

Deletes the index with the given _id_ from the collection.

**Arguments**

- **id**: `String`

  The index-handle

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoCollection collection = db.collection("some-collection");

collection.deleteIndex("some-index");
```
