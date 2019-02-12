<!-- don't edit here, it's from https://@github.com/arangodb/arangodb-java-driver.git / docs/Drivers/ -->
# Manipulating documents

These functions implement the
[HTTP API for manipulating documents](../../../..//HTTP/Document/index.html).

## ArangoCollection.documentExists

`ArangoCollection.documentExists(String key) : Boolean`

Checks if the document exists by reading a single document head

**Arguments**

- **key**: `String`

  The key of the document

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoCollection collection = db.collection("some-collection");

Boolean exists = collection.documentExists("some-key");
```

## ArangoCollection.getDocument

`ArangoCollection.getDocument(String key, Class<T> type, DocumentReadOptions options) : T`

Retrieves the document with the given \_key from the collection.

**Arguments**

- **key**: `String`

  The key of the document

- **type**: `Class<T>`

  The type of the document (POJO class, `VPackSlice` or `String` for JSON)

- **options**: `DocumentReadOptions`

  - **ifNoneMatch**: `String`

    Document revision must not contain If-None-Match

  - **ifMatch**: `String`

    Document revision must contain If-Match

  - **catchException**: `Boolean`

    Whether or not catch possible thrown exceptions

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoCollection collection = db.collection("some-collection");

BaseDocument document = collection.getDocument("some-key", BaseDocument.class);
```

## ArangoCollection.getDocuments

`ArangoCollection.getDocuments(Collection<String> keys, Class<T> type) : MultiDocumentEntity<T>`

Retrieves multiple documents with the given \_key from the collection.

**Arguments**

- **keys**: `Collection<String>`

  The key of the document

- **type**: `Class<T>`

  The type of the document (POJO class, `VPackSlice` or `String` for JSON)

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoCollection collection = db.collection("some-collection");

Collection<String> keys = Arrays.asList("some-key", "some-other-key");
MultiDocumentEntity<BaseDocument> documents = collection.getDocuments(keys, BaseDocument.class);
```

## ArangoCollection.insertDocument

`ArangoCollection.insertDocument(T value, DocumentCreateOptions options) : DocumentCreateEntity<T>`

Creates a new document from the given document, unless there is already a
document with the \_key given. If no \_key is given, a new unique \_key is
generated automatically.

**Arguments**

- **value**: `T`

  A representation of a single document (POJO, `VPackSlice` or `String` for JSON)

- **options**: `DocumentCreateOptions`

  - **waitForSync**: `Boolean`

    Wait until document has been synced to disk.

  - **returnNew**: `Boolean`

    Return additionally the complete new document under the attribute new in the result.

  - **returnOld**: `Boolean`

    This options requires ArangoDB version 3.4.0 or higher. Additionally return
    the complete old document under the attribute old in the result.
    Only available if the _overwrite_ option is used.

  - **overwrite**: `Boolean`

    This options requires ArangoDB version 3.4.0 or higher. If set to true, the
    insert becomes a replace-insert. If a document with the same \_key already
    exists the new document is not rejected with unique constraint violated but
    will replace the old document.

  - **silent**: `Boolean`

    If set to true, an empty object will be returned as response. No meta-data
    will be returned for the created document. This option can be used to save
    some network traffic.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoCollection collection = db.collection("some-collection");

BaseDocument document = new BaseDocument();
document.addAttribute("some", "data");
collection.insertDocument(document, new DocumentCreateOptions());
```

## ArangoCollection.insertDocuments

`ArangoCollection.insertDocuments(Collection<T> values, DocumentCreateOptions options) : MultiDocumentEntity<DocumentCreateEntity<T>>`

Creates new documents from the given documents, unless there is already a
document with the \_key given. If no \_key is given, a new unique \_key is
generated automatically.

**Arguments**

- **values**: `Collection<T>`

  A List of documents (POJO, `VPackSlice` or `String` for JSON)

- **options**: `DocumentCreateOptions`

  - **waitForSync**: `Boolean`

    Wait until document has been synced to disk.

  - **returnNew**: `Boolean`

    Return additionally the complete new document under the attribute new in the result.

  - **returnOld**: `Boolean`

    This options requires ArangoDB version 3.4.0 or higher. Additionally return
    the complete old document under the attribute old in the result.
    Only available if the _overwrite_ option is used.

  - **overwrite**: `Boolean`

    This options requires ArangoDB version 3.4.0 or higher. If set to true, the
    insert becomes a replace-insert. If a document with the same \_key already
    exists the new document is not rejected with unique constraint violated but
    will replace the old document.

  - **silent**: `Boolean`

    If set to true, an empty object will be returned as response. No meta-data
    will be returned for the created document. This option can be used to save
    some network traffic.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoCollection collection = db.collection("some-collection");

BaseDocument doc1 = new BaseDocument();
BaseDocument doc2 = new BaseDocument();
BaseDocument doc3 = new BaseDocument();
collection.insertDocuments(
  Arrays.asList(doc1, doc2, doc3),
  new DocumentCreateOptions()
);
```

## ArangoCollection.replaceDocument

`ArangoCollection.replaceDocument(String key, T value, DocumentReplaceOptions options) : DocumentUpdateEntity<T>`

Replaces the document with _key_ with the one in the body, provided there is
such a document and no precondition is violated.

**Arguments**

- **key**: `String`

  The key of the document

- **value**: `T`

  A representation of a single document (POJO, `VPackSlice` or `String` for JSON)

- **options**: `DocumentReplaceOptions`

  - **waitForSync**: `Boolean`

    Wait until document has been synced to disk.

  - **ignoreRevs**: `Boolean`

    By default, or if this is set to true, the \_rev attributes in the given
    document is ignored. If this is set to false, then the \_rev attribute
    given in the body document is taken as a precondition. The document is
    only replaced if the current revision is the one specified.

  - **ifMatch**: `String`

    Replace a document based on target revision

  - **returnNew**: `Boolean`

    Return additionally the complete new document under the attribute new in the result.

  - **returnOld**: `Boolean`

    Additionally return the complete old document under the attribute old in the result. 

  - **silent**: `Boolean`

    If set to true, an empty object will be returned as response. No meta-data
    will be returned for the created document. This option can be used to save
    some network traffic.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoCollection collection = db.collection("some-collection");

BaseDocument document = new BaseDocument();
document.addAttribute("hello", "world");
DocumentCreateEntity<BaseDocument> info = collection.insertDocument(document);

document.addAttribute("hello", "world2");
collection.replaceDocument(info.getKey(), document, new DocumentReplaceOptions());

BaseDocument doc = collection.getDocument(info.getKey());
assertThat(doc.getAttribute("hello"), is("world2"));
```

## ArangoCollection.replaceDocuments

`ArangoCollection.replaceDocuments(Collection<T> values, DocumentReplaceOptions options) : MultiDocumentEntity<DocumentUpdateEntity<T>>`

Replaces multiple documents in the specified collection with the ones in the
values, the replaced documents are specified by the \_key attributes in the
documents in values.

**Arguments**

- **values**: `Collection<T>`

  A List of documents (POJO, `VPackSlice` or `String` for JSON)

- **options**: `DocumentReplaceOptions`

  - **waitForSync**: `Boolean`

    Wait until document has been synced to disk.

  - **ignoreRevs**: `Boolean`

    By default, or if this is set to true, the \_rev attributes in the given
    document is ignored. If this is set to false, then the \_rev attribute
    given in the body document is taken as a precondition. The document is
    only replaced if the current revision is the one specified.

  - **ifMatch**: `String`

    Replace a document based on target revision

  - **returnNew**: `Boolean`

    Return additionally the complete new document under the attribute new in the result.

  - **returnOld**: `Boolean`

    Additionally return the complete old document under the attribute old in the result.

  - **silent**: `Boolean`

    If set to true, an empty object will be returned as response. No meta-data
    will be returned for the created document. This option can be used to save
    some network traffic.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoCollection collection = db.collection("some-collection");

BaseDocument doc1 = new BaseDocument();
BaseDocument doc2 = new BaseDocument();
BaseDocument doc3 = new BaseDocument();
collection.insertDocuments(Arrays.asList(doc1, doc2, doc3));

// change values of doc1, doc2, doc3

collection.replaceDocuments(
  Arrays.asList(doc1, doc2, doc3),
  new DocumentReplaceOptions()
);
```

## ArangoCollection.updateDocument

`ArangoCollection.updateDocument(String key, T value, DocumentUpdateOptions options) : DocumentUpdateEntity<T>`

Updates the document with _key_ with the one in the body, provided there is
such a document and no precondition is violated.

**Arguments**

- **key**: `String`

  The key of the document

- **value**: `T`

  A representation of a single document (POJO, `VPackSlice` or `String` for JSON)

- **options**: `DocumentUpdateOptions`

  - **waitForSync**: `Boolean`

    Wait until document has been synced to disk.

  - **ignoreRevs**: `Boolean`

    By default, or if this is set to true, the \_rev attributes in the given
    document is ignored. If this is set to false, then the \_rev attribute
    given in the body document is taken as a precondition. The document is
    only replaced if the current revision is the one specified.

  - **ifMatch**: `String`

    Replace a document based on target revision

  - **returnNew**: `Boolean`

    Return additionally the complete new document under the attribute new in the result.

  - **returnOld**: `Boolean`

    Additionally return the complete old document under the attribute old in the result.

  - **silent**: `Boolean`

    If set to true, an empty object will be returned as response. No meta-data
    will be returned for the created document. This option can be used to save
    some network traffic.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoCollection collection = db.collection("some-collection");

BaseDocument document = new BaseDocument();
document.addAttribute("hello", "world");
DocumentCreateEntity<BaseDocument> info = collection.insertDocument(document);

document.addAttribute("hello", "world2");
collection.updateDocument(info.getKey(), document, new DocumentUpdateOptions());

BaseDocument doc = collection.getDocument(info.getKey());
assertThat(doc.getAttribute("hello"), is("world2"));
```

## ArangoCollection.updateDocuments

`ArangoCollection.updateDocuments(Collection<T> values, DocumentUpdateOptions options) : MultiDocumentEntity<DocumentUpdateEntity<T>>`

Updates multiple documents in the specified collection with the ones in the
values, the replaced documents are specified by the \_key attributes in the
documents in values.

**Arguments**

- **values**: `Collection<T>`

  A List of documents (POJO, `VPackSlice` or `String` for JSON)

- **options**: `DocumentUpdateOptions`

  - **waitForSync**: `Boolean`

    Wait until document has been synced to disk.

  - **ignoreRevs**: `Boolean`

    By default, or if this is set to true, the \_rev attributes in the given
    document is ignored. If this is set to false, then the \_rev attribute
    given in the body document is taken as a precondition. The document is
    only replaced if the current revision is the one specified.

  - **ifMatch**: `String`

    Replace a document based on target revision

  - **returnNew**: `Boolean`

    Return additionally the complete new document under the attribute new in the result.

  - **returnOld**: `Boolean`

    Additionally return the complete old document under the attribute old in the result.

  - **silent**: `Boolean`

    If set to true, an empty object will be returned as response. No meta-data
    will be returned for the created document. This option can be used to save
    some network traffic.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoCollection collection = db.collection("some-collection");

BaseDocument doc1 = new BaseDocument();
BaseDocument doc2 = new BaseDocument();
BaseDocument doc3 = new BaseDocument();
collection.insertDocuments(Arrays.asList(doc1, doc2, doc3));

// change values of doc1, doc2, doc3

collection.updateDocuments(
  Arrays.asList(doc1, doc2, doc3),
  new DocumentUpdateOptions()
);
```

## ArangoCollection.deleteDocument

`ArangoCollection.deleteDocument(String key) : DocumentDeleteEntity<Void>`

Deletes the document with the given _key_ from the collection.

**Arguments**:

- **key**: `String`

  The key of the document

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoCollection collection = db.collection("some-collection");

BaseDocument document = new BaseDocument("some-key");
collection.insertDocument(document);

collection.deleteDocument("some-key");
// document 'some-collection/some-key' no longer exists

Boolean exists = collection.documentExists("some-key");
assertThat(exists, is(false));
```

## ArangoCollection.deleteDocuments

`ArangoCollection.deleteDocuments(Collection<?> values) : MultiDocumentEntity<DocumentDeleteEntity<Void>>`

Deletes multiple documents from the collection.

**Arguments**:

- **values**: `Collection<?>`

  The keys of the documents or the documents themselves

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoCollection collection = db.collection("some-collection");

collection.deleteDocuments(Arrays.asList("some-key", "some-other-key");
// documents 'some-collection/some-key' and 'some-collection/some-other-key' no longer exists
```
