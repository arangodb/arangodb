<!-- don't edit here, it's from https://@github.com/arangodb/spring-data.git / docs/Drivers/ -->
# Manipulating multiple documents

## ArangoOperations.find

```
ArangoOperations.find(Iterable<String> ids, Class<T> entityClass) : Iterable<T>
```

Retrieves multiple documents with the given _ids_ from a collection.

**Arguments**

- **ids**: `Iterable<String>`

  The ids or keys of the documents

- **entityClass**: `Class<T>`

  The entity type of the documents

**Examples**

```Java
@Autowired ArangoOperations template;

Iterable<MyObject> docs = template.find(Arrays.asList("some-id", "some-other-id"), MyObject.class);
```

## ArangoOperations.findAll

```
ArangoOperations.findAll(Class<T> entityClass) : Iterable<T>
```

Retrieves all documents from a collection.

**Arguments**

- **entityClass**: `Class<T>`

  The entity class which represents the collection

**Examples**

```Java
@Autowired ArangoOperations template;

Iterable<MyObject> docs = template.find(MyObject.class);
```

## ArangoOperations.insert

```
ArangoOperations.insert(Iterable<T> values, Class<T> entityClass, DocumentCreateOptions options) : MultiDocumentEntity
```

Creates new documents from the given documents, unless there is already a document with the \_key given. If no \_key is given, a new unique \_key is generated automatically.

**Arguments**

- **values**: `Iterable<T>`

  A List of documents

- **entityClass**: `Class<T>`

  The entity class which represents the collection

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

MyObject obj1 = ...
MyObject obj2 = ...
MyObject obj3 = ...
template.insert(Arrays.asList(obj1, obj2, obj3));
```

## ArangoOperations.replace

```
ArangoOperations.replace(Iterable<T> values, Class<T> entityClass, DocumentReplaceOptions options) : MultiDocumentEntity
```

Replaces multiple documents in the specified collection with the ones in the values, the replaced documents are specified by the \_key attributes in the documents in values.

**Arguments**

- **values**: `Iterable<T>`

  A List of documents

- **entityClass**: `Class<T>`

  The entity class which represents the collection

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

MyObject obj1 = ...
MyObject obj2 = ...
MyObject obj3 = ...
template.replace(Arrays.asList(obj1, obj2, obj3), new DocumentReplaceOptions());
```

## ArangoOperations.update

```
ArangoOperations.update(Iterable<T> values, Class<T> entityClass, DocumentUpdateOptions options) : MultiDocumentEntity
```

Partially updates documents, the documents to update are specified by the \_key attributes in the objects on values. Vales must contain a list of document updates with the attributes to patch (the patch documents). All attributes from the patch documents will be added to the existing documents if they do not yet exist, and overwritten in the existing documents if they do exist there.

**Arguments**

- **values**: `Iterable<T>`

  A List of documents

- **entityClass**: `Class<T>`

  The entity class which represents the collection

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

MyObject obj1 = ...
MyObject obj2 = ...
MyObject obj3 = ...
template.update(Arrays.asList(obj1, obj2, obj3), new DocumentUpdateOptions());
```

## ArangoOperations.delete

```
ArangoOperations.delete(Iterable<Object> values, Class<?> entityClass, DocumentDeleteOptions options) : MultiDocumentEntity
```

Deletes multiple documents from a collection.

**Arguments**

- **values**: `Iterable<Object>`

  The keys of the documents or the documents themselves

- **entityClass**: `Class<?>`

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

collection.delete(Arrays.asList("some-id", "some-other-id"), MyObject.class, new DocumentDeleteOptions());
```
