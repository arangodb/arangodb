<!-- don't edit here, it's from https://@github.com/arangodb/spring-data.git / docs/Drivers/ -->
# Manipulating documents

## ArangoOperations.exists

```
ArangoOperations.exists(String id, Class<?> entityClass) : boolean
```

Checks whether the document exists by reading a single document head

**Arguments**

- **id**: `String`

  The id or key of the document

- **entityClass**: `Class<T>`

  The entity class which represents the collection

**Examples**

```Java
@Autowired ArangoOperations template;

boolean exists = template.exists("some-id", MyObject.class);
```

## ArangoOperations.find

```
ArangoOperations.find(String id, Class<T> entityClass, DocumentReadOptions options) : Optional<T>
```

Retrieves the document with the given _id_ from a collection.

**Arguments**

- **id**: `String`

  The id or key of the document

- **entityClass**: `Class<T>`

  The entity class which represents the collection

- **options**: `DocumentReadOptions`

  - **ifNoneMatch**: `String`

    Document revision must not contain If-None-Match

  - **ifMatch**: `String`

    Document revision must contain If-Match

  - **catchException**: `Boolean`

    Whether or not catch possible thrown exceptions

**Examples**

```Java
@Autowired ArangoOperations template;

Optional<MyObject> doc = template.find("some-id", MyObject.class, new DocumentReadOptions());
```

## ArangoOperations.repsert

```
ArangoOperations.repsert(T value) : void
```

Creates a new document from the given document, unless there is already a document with the id given. In that case it replaces the document.

**Arguments**

- **value**: `T`

  A representation of a single document

**Examples**

```Java
@Autowired ArangoOperations template;

MyObject myObj = ...
template.repsert(myObj);
```

## ArangoOperations.insert

```
ArangoOperations.insert(T value, DocumentCreateOptions options) : DocumentEntity
```

Creates a new document from the given document, unless there is already a document with the \_key given. If no \_key is given, a new unique \_key is generated automatically.

**Arguments**

- **value**: `T`

  A representation of a single document

- **options**: `DocumentCreateOptions`

  - **waitForSync**: `Boolean`

    Wait until document has been synced to disk.

  - **returnNew**: `Boolean`

    Return additionally the complete new document under the attribute new in the result.

  - **returnOld**: `Boolean`

    Additionally return the complete old document under the attribute old in the result. Only available if the _overwrite_ option is used.

  - **overwrite**: `Boolean`

    If set to true, the insert becomes a replace-insert. If a document with the same \_key already exists the new document is not rejected with unique constraint violated but will replace the old document.

  - **silent**: `Boolean`

    If set to true, an empty object will be returned as response. No meta-data will be returned for the created document. This option can be used to save some network traffic.

**Examples**

```Java
@Autowired ArangoOperations template;

MyObject myObj = ...
DocumentEntity info = template.insert(myObj, new DocumentCreateOptions());
```

## ArangoOperations.replace

```
ArangoOperations.replace(String id, T value, DocumentReplaceOptions options) : DocumentEntity
```

Replaces the document with _id_ with the one in the body, provided there is such a document and no precondition is violated.

**Arguments**

- **id**: `String`

  The id or key of the document

- **value**: `T`

  A representation of a single document

- **options**: `DocumentReplaceOptions`

  - **waitForSync**: `Boolean`

    Wait until document has been synced to disk.

  - **ignoreRevs**: `Boolean`

    By default, or if this is set to true, the \_rev attributes in the given document is ignored. If this is set to false, then the \_rev attribute given in the body document is taken as a precondition. The document is only replaced if the current revision is the one specified.

  - **ifMatch**: `String`

    Replace a document based on target revision

  - **returnNew**: `Boolean`

    Return additionally the complete new document under the attribute new in the result.

  - **returnOld**: `Boolean`

    Additionally return the complete old document under the attribute old in the result. Only available if the _overwrite_ option is used.

  - **silent**: `Boolean`

    If set to true, an empty object will be returned as response. No meta-data will be returned for the created document. This option can be used to save some network traffic.

**Examples**

```Java
@Autowired ArangoOperations template;

MyObject myObj = ...
DocumentEntity info = template.replace("some-id", myObj, new DocumentReplaceOptions());
```

## ArangoOperations.update

```
ArangoOperations.update(String id, T value, DocumentUpdateOptions options) : DocumentEntity
```

Partially updates the document identified by document id or key. The value must contain a document with the attributes to patch (the patch document). All attributes from the patch document will be added to the existing document if they do not yet exist, and overwritten in the existing document if they do exist there.

**Arguments**

- **id**: `String`

  The id or key of the document

- **value**: `T`

  A representation of a single document

- **options**: `DocumentUpdateOptions`

  - **waitForSync**: `Boolean`

    Wait until document has been synced to disk.

  - **ignoreRevs**: `Boolean`

    By default, or if this is set to true, the \_rev attributes in the given document is ignored. If this is set to false, then the \_rev attribute given in the body document is taken as a precondition. The document is only replaced if the current revision is the one specified.

  - **ifMatch**: `String`

    Replace a document based on target revision

  - **returnNew**: `Boolean`

    Return additionally the complete new document under the attribute new in the result.

  - **returnOld**: `Boolean`

    Additionally return the complete old document under the attribute old in the result. Only available if the _overwrite_ option is used.

  - **silent**: `Boolean`

    If set to true, an empty object will be returned as response. No meta-data will be returned for the created document. This option can be used to save some network traffic.

**Examples**

```Java
@Autowired ArangoOperations template;

MyObject myObj = ...
DocumentEntity info = template.update("some-id", myObj, new DocumentReplaceOptions());
```

## ArangoOperations.delete

```
ArangoOperations.delete(String id, Class<?> entityClass, DocumentDeleteOptions options) : DocumentEntity
```

Deletes the document with the given _id_ from a collection.

**Arguments**

- **id**: `String`

  The id or key of the document

- **entityClass**: `Class<T>`

  The entity class which represents the collection

- **options**: `DocumentDeleteOptions`

  - **waitForSync**: `Boolean`

    Wait until document has been synced to disk.

  - **ifMatch**: `String`

    Replace a document based on target revision

  - **returnOld**: `Boolean`

    Additionally return the complete old document under the attribute old in the result. Only available if the _overwrite_ option is used.

  - **silent**: `Boolean`

    If set to true, an empty object will be returned as response. No meta-data will be returned for the created document. This option can be used to save some network traffic.

**Examples**

```Java
@Autowired ArangoOperations template;

template.delete("some-id", MyObject.class, new DocumentDeleteOptions());
```
