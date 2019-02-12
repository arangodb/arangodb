<!-- don't edit here, it's from https://@github.com/arangodb/arangodb-java-driver.git / docs/Drivers/ -->
# Manipulating vertices

## ArangoVertexCollection.getVertex

`ArangoVertexCollection.getVertex(String key, Class<T> type, DocumentReadOptions options) : T`

Retrieves the vertex document with the given `key` from the collection.

**Arguments**

- **key**: `String`

  The key of the vertex

- **type**: `Class<T>`

  The type of the vertex-document (POJO class, `VPackSlice` or `String` for JSON)

- **options**: `DocumentReadOptions`

  - **ifNoneMatch**: `String`

    Document revision must not contain If-None-Match

  - **ifMatch**: `String`

    Document revision must contain If-Match

  - **catchException**: `Boolean`

    Whether or not catch possible thrown exceptions

## ArangoVertexCollection.insertVertex

`ArangoVertexCollection.insertVertex(T value, VertexCreateOptions options) : VertexEntity`

Creates a new vertex in the collection.

**Arguments**

- **value**: `T`

  A representation of a single vertex (POJO, `VPackSlice` or `String` for JSON)

- **options**: `VertexCreateOptions`

  - **waitForSync**: `Boolean`

    Wait until document has been synced to disk.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoGraph graph = db.graph("some-graph");
ArangoVertexCollection collection = graph.vertexCollection("some-vertex-collection");

BaseDocument document = new BaseDocument();
document.addAttribute("some", "data");
collection.insertVertex(document, new VertexCreateOptions());
```

## ArangoVertexCollection.replaceVertex

`ArangoVertexCollection.replaceVertex(String key, T value, VertexReplaceOptions options) : VertexUpdateEntity`

Replaces the vertex with key with the one in the body, provided there is such
a vertex and no precondition is violated.

**Arguments**

- **key**: `String`

  The key of the vertex

- **value**: `T`

  A representation of a single vertex (POJO, `VPackSlice` or `String` for JSON)

- **options**: `VertexReplaceOptions`

  - **waitForSync**: `Boolean`

    Wait until document has been synced to disk.

  - **ifMatch**: `String`

    Replace a document based on target revision

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoGraph graph = db.graph("some-graph");
ArangoVertexCollection collection = graph.vertexCollection("some-vertex-collection");

BaseDocument document = new BaseDocument();
collection.replaceVertex("some-key", document, new VertexReplaceOptions());
```

## ArangoVertexCollection.updateVertex

`ArangoVertexCollection.updateVertex(String key, T value, VertexUpdateOptions options) : VertexUpdateEntity`

Updates the vertex with key with the one in the body, provided there is such
a vertex and no precondition is violated.

**Arguments**

- **key**: `String`

  The key of the vertex

- **value**: `T`

  A representation of a single vertex (POJO, `VPackSlice` or `String` for JSON)

- **options**: `VertexUpdateOptions`

  - **waitForSync**: `Boolean`

    Wait until document has been synced to disk.

  - **ifMatch**: `String`

    Update a document based on target revision

  - **keepNull**: `Boolean`

    If the intention is to delete existing attributes with the patch command,
    the URL query parameter keepNull can be used with a value of false.
    This will modify the behavior of the patch command to remove any attributes
    from the existing document that are contained in the patch document with
    an attribute value of null.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoGraph graph = db.graph("some-graph");
ArangoVertexCollection collection = graph.vertexCollection("some-vertex-collection");

BaseDocument document = new BaseDocument();
collection.updateVertex("some-key", document, new VertexUpdateOptions());
```

## ArangoVertexCollection.deleteVertex

`ArangoVertexCollection.deleteVertex(String key, VertexDeleteOptions options) : void`

Deletes the vertex with the given _key_ from the collection.

**Arguments**

- **key**: `String`

  The key of the vertex

- **options** : `VertexDeleteOptions`

  - **waitForSync**: `Boolean`

    Wait until document has been synced to disk.

  - **ifMatch**: `String`

    Remove a document based on target revision

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoGraph graph = db.graph("some-graph");
ArangoVertexCollection collection = graph.vertexCollection("some-vertex-collection");

collection.deleteVertex("some-key", new VertexDeleteOptions());
```
