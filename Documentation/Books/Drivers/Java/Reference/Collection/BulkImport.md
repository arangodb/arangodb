<!-- don't edit here, it's from https://@github.com/arangodb/arangodb-java-driver.git / docs/Drivers/ -->
# Bulk importing documents

This function implements the
[HTTP API for bulk imports](../../../..//HTTP/BulkImports/index.html).

## ArangoCollection.importDocuments

`ArangoCollection.importDocuments(Collection<?> values, DocumentImportOptions options) : DocumentImportEntity`

`ArangoCollection.importDocuments(String values, DocumentImportOptions options) : DocumentImportEntity`

Bulk imports the given values into the collection.

**Arguments**

- **values**: `Collection<?>` or `String`

  - `Collection<?>`: A list of Objects that will be stored as documents

  - `String`: JSON-encoded array of objects that will be stored as documents

- **options**: `DocumentImportOptions`

  - **fromPrefix**: `String`

    An optional prefix for the values in \_from attributes. If specified,
    the value is automatically prepended to each \_from input value.
    This allows specifying just the keys for \_from.

  - **toPrefix**: `String`

    An optional prefix for the values in \_to attributes. If specified,
    the value is automatically prepended to each \_to input value.
    This allows specifying just the keys for \_to.

  - **overwrite**: `Boolean`

    If this parameter has a value of true, then all data in the collection
    will be removed prior to the import. Note that any existing index definitions
    will be preserved.

  - **waitForSync**: `Boolean`

    Wait until documents have been synced to disk before returning.

  - **onDuplicate**: `OnDuplicate`

    Controls what action is carried out in case of a unique key constraint violation.
    Possible values are:

    - **error**: this will not import the current document because of the
      unique key constraint violation. This is the default setting.

    - **update**: this will update an existing document in the database with
      the data specified in the request. Attributes of the existing document
      that are not present in the request will be preserved.

    - **replace**: this will replace an existing document in the database with
      the data specified in the request.

    - **ignore**: this will not update an existing document and simply ignore
      the error caused by the unique key constraint violation. Note that update,
      replace and ignore will only work when the import document in the request
      contains the \_key attribute. update and replace may also fail because of
      secondary unique key constraint violations.

  - **complete**: `Boolean`

    If set to true, it will make the whole import fail if any error occurs.
    Otherwise the import will continue even if some documents cannot be imported.

  - **details**: `Boolean`

    If set to true, the result will include an attribute details with details
    about documents that could not be imported.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoCollection collection = db.collection("some-collection");

BaseDocument doc1 = new BaseDocument();
BaseDocument doc2 = new BaseDocument();
BaseDocument doc3 = new BaseDocument();
collection.importDocuments(
  Arrays.asList(doc1, doc2, doc3),
  new DocumentImportOptions()
);
```
