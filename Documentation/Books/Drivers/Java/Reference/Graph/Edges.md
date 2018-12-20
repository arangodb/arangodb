<!-- don't edit here, it's from https://@github.com/arangodb/arangodb-java-driver.git / docs/Drivers/ -->
# Manipulating edges

## ArangoEdgeCollection.getEdge

`ArangoEdgeCollection.getEdge(String key, Class<T> type, DocumentReadOptions options) : T`

Retrieves the edge document with the given `key` from the collection.

**Arguments**

- **key**: `String`

  The key of the edge

- **type**: `Class<T>`

  The type of the edge-document (POJO class, `VPackSlice` or `String` for JSON)

- **options**: `DocumentReadOptions`

  - **ifNoneMatch**: `String`

    Document revision must not contain If-None-Match

  - **ifMatch**: `String`

    Document revision must contain If-Match

  - **catchException**: `Boolean`

    Whether or not catch possible thrown exceptions

## ArangoEdgeCollection.insertEdge

`ArangoEdgeCollection.insertEdge(T value, EdgeCreateOptions options) : EdgeEntity`

Creates a new edge in the collection.

**Arguments**

- **value**: `T`

  A representation of a single edge (POJO, `VPackSlice` or `String` for JSON)

- **options**: `EdgeCreateOptions`

  - **waitForSync**: `Boolean`

    Wait until document has been synced to disk.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoGraph graph = db.graph("some-graph");
ArangoEdgeCollection collection = graph.edgeCollection("some-edge-collection");

BaseEdgeDocument document = new BaseEdgeDocument("some-from-key", "some-to-key");
document.addAttribute("some", "data");
collection.insertEdge(document, new EdgeCreateOptions());
```

## ArangoEdgeCollection.replaceEdge

`ArangoEdgeCollection.replaceEdge(String key, T value, EdgeReplaceOptions options) : EdgeUpdateEntity`

Replaces the edge with key with the one in the body, provided there is such
a edge and no precondition is violated.

**Arguments**

- **key**: `String`

  The key of the edge

- **value**: `T`

  A representation of a single edge (POJO, `VPackSlice` or `String` for JSON)

- **options**: `EdgeReplaceOptions`

  - **waitForSync**: `Boolean`

    Wait until document has been synced to disk.

  - **ifMatch**: `String`

    Replace a document based on target revision

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoGraph graph = db.graph("some-graph");
ArangoEdgeCollection collection = graph.edgeCollection("some-edge-collection");

BaseEdgeDocument document = new BaseEdgeDocument("some-from-key", "some-to-key");
collection.replaceEdge("some-key", document, new EdgeReplaceOptions());
```

## ArangoEdgeCollection.updateEdge

`ArangoEdgeCollection.updateEdge(String key, T value, EdgeUpdateOptions options) : EdgeUpdateEntity`

Updates the edge with key with the one in the body, provided there is such a
edge and no precondition is violated.

**Arguments**

- **key**: `String`

  The key of the edge

- **value**: `T`

  A representation of a single edge (POJO, `VPackSlice` or `String` for JSON)

- **options**: `EdgeUpdateOptions`

  - **waitForSync**: `Boolean`

    Wait until document has been synced to disk.

  - **ifMatch**: `String`

    Update a document based on target revision

  - **keepNull**: `Boolean`

    If the intention is to delete existing attributes with the patch command,
    the URL query parameter keepNull can be used with a value of false.
    This will modify the behavior of the patch command to remove any attributes
    from the existing document that are contained in the patch document with an
    attribute value of null.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoGraph graph = db.graph("some-graph");
ArangoEdgeCollection collection = graph.edgeCollection("some-edge-collection");

BaseEdgeDocument document = new BaseEdgeDocument("some-from-key", "some-to-key");
collection.updateEdge("some-key", document, new EdgeUpdateOptions());
```

## ArangoEdgeCollection.deleteEdge

`ArangoEdgeCollection.deleteEdge(String key, EdgeDeleteOptions options) : void`

Deletes the edge with the given _key_ from the collection.

**Arguments**

- **key**: `String`

  The key of the edge

- **options** : `EdgeDeleteOptions`

  - **waitForSync**: `Boolean`

    Wait until document has been synced to disk.

  - **ifMatch**: `String`

    Remove a document based on target revision

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoGraph graph = db.graph("some-graph");
ArangoEdgeCollection collection = graph.edgeCollection("some-edge-collection");

collection.deleteEdge("some-key", new EdgeDeleteOptions());
```
