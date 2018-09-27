<!-- don't edit here, it's from https://@github.com/arangodb/arangodbjs.git / docs/Drivers/ -->
# Manipulating documents

These functions implement the
[HTTP API for manipulating documents](../../../..//HTTP/Document/index.html).

## collection.replace

`async collection.replace(documentHandle, newValue, [opts]): Object`

Replaces the content of the document with the given _documentHandle_ with the
given _newValue_ and returns an object containing the document's metadata.

**Arguments**

- **documentHandle**: `string`

  The handle of the document to replace. This can either be the `_id` or the
  `_key` of a document in the collection, or a document (i.e. an object with an
  `_id` or `_key` property).

- **newValue**: `Object`

  The new data of the document.

- **opts**: `Object` (optional)

  If _opts_ is set, it must be an object with any of the following properties:

  - **waitForSync**: `boolean` (Default: `false`)

    Wait until the document has been synced to disk. Default: `false`.

  - **rev**: `string` (optional)

    Only replace the document if it matches this revision.

  - **policy**: `string` (optional)

    {% hint 'warning' %}
    This option has no effect in ArangoDB 3.0 and later.
    {% endhint %}

    Determines the behavior when the revision is not matched:

    - if _policy_ is set to `"last"`, the document will be replaced regardless
      of the revision.
    - if _policy_ is set to `"error"` or not set, the replacement will fail with
      an error.

If a string is passed instead of an options object, it will be interpreted as
the _rev_ option.

For more information on the _opts_ object, see the
[HTTP API documentation for working with documents](../../../..//HTTP/Document/WorkingWithDocuments.html).

**Examples**

```js
const db = new Database();
const collection = db.collection("some-collection");
const data = { number: 1, hello: "world" };
const info1 = await collection.save(data);
const info2 = await collection.replace(info1, { number: 2 });
assert.equal(info2._id, info1._id);
assert.notEqual(info2._rev, info1._rev);
const doc = await collection.document(info1);
assert.equal(doc._id, info1._id);
assert.equal(doc._rev, info2._rev);
assert.equal(doc.number, 2);
assert.equal(doc.hello, undefined);
```

## collection.update

`async collection.update(documentHandle, newValue, [opts]): Object`

Updates (merges) the content of the document with the given _documentHandle_
with the given _newValue_ and returns an object containing the document's
metadata.

**Arguments**

- **documentHandle**: `string`

  Handle of the document to update. This can be either the `_id` or the `_key`
  of a document in the collection, or a document (i.e. an object with an `_id`
  or `_key` property).

- **newValue**: `Object`

  The new data of the document.

- **opts**: `Object` (optional)

  If _opts_ is set, it must be an object with any of the following properties:

  - **waitForSync**: `boolean` (Default: `false`)

    Wait until document has been synced to disk.

  - **keepNull**: `boolean` (Default: `true`)

    If set to `false`, properties with a value of `null` indicate that a
    property should be deleted.

  - **mergeObjects**: `boolean` (Default: `true`)

    If set to `false`, object properties that already exist in the old document
    will be overwritten rather than merged. This does not affect arrays.

  - **returnOld**: `boolean` (Default: `false`)

    If set to `true`, return additionally the complete previous revision of the
    changed documents under the attribute `old` in the result.

  - **returnNew**: `boolean` (Default: `false`)

    If set to `true`, return additionally the complete new documents under the
    attribute `new` in the result.

  - **ignoreRevs**: `boolean` (Default: `true`)

    By default, or if this is set to true, the `_rev` attributes in the given
    documents are ignored. If this is set to false, then any `_rev` attribute
    given in a body document is taken as a precondition. The document is only
    updated if the current revision is the one specified.

  - **rev**: `string` (optional)

    Only update the document if it matches this revision.

  - **policy**: `string` (optional)

    {% hint 'warning' %}
    This option has no effect in ArangoDB 3.0 and later.
    {% endhint %}

    Determines the behavior when the revision is not matched:

    - if _policy_ is set to `"last"`, the document will be replaced regardless
      of the revision.
    - if _policy_ is set to `"error"` or not set, the replacement will fail with
      an error.

If a string is passed instead of an options object, it will be interpreted as
the _rev_ option.

For more information on the _opts_ object, see the
[HTTP API documentation for working with documents](../../../..//HTTP/Document/WorkingWithDocuments.html).

**Examples**

```js
const db = new Database();
const collection = db.collection("some-collection");
const doc = { number: 1, hello: "world" };
const doc1 = await collection.save(doc);
const doc2 = await collection.update(doc1, { number: 2 });
assert.equal(doc2._id, doc1._id);
assert.notEqual(doc2._rev, doc1._rev);
const doc3 = await collection.document(doc2);
assert.equal(doc3._id, doc2._id);
assert.equal(doc3._rev, doc2._rev);
assert.equal(doc3.number, 2);
assert.equal(doc3.hello, doc.hello);
```

## collection.bulkUpdate

`async collection.bulkUpdate(documents, [opts]): Object`

Updates (merges) the content of the documents with the given _documents_ and
returns an array containing the documents' metadata.

{% hint 'info' %}
This method is only available when targeting ArangoDB 3.0 or later,
see [Compatibility](../../GettingStarted/README.md#compatibility).
{% endhint %}

**Arguments**

- **documents**: `Array<Object>`

  Documents to update. Each object must have either the `_id` or the `_key`
  property.

- **opts**: `Object` (optional)

  If _opts_ is set, it must be an object with any of the following properties:

  - **waitForSync**: `boolean` (Default: `false`)

    Wait until document has been synced to disk.

  - **keepNull**: `boolean` (Default: `true`)

    If set to `false`, properties with a value of `null` indicate that a
    property should be deleted.

  - **mergeObjects**: `boolean` (Default: `true`)

    If set to `false`, object properties that already exist in the old document
    will be overwritten rather than merged. This does not affect arrays.

  - **returnOld**: `boolean` (Default: `false`)

    If set to `true`, return additionally the complete previous revision of the
    changed documents under the attribute `old` in the result.

  - **returnNew**: `boolean` (Default: `false`)

    If set to `true`, return additionally the complete new documents under the
    attribute `new` in the result.

  - **ignoreRevs**: `boolean` (Default: `true`)

    By default, or if this is set to true, the `_rev` attributes in the given
    documents are ignored. If this is set to false, then any `_rev` attribute
    given in a body document is taken as a precondition. The document is only
    updated if the current revision is the one specified.

For more information on the _opts_ object, see the
[HTTP API documentation for working with documents](../../../..//HTTP/Document/WorkingWithDocuments.html).

**Examples**

```js
const db = new Database();
const collection = db.collection("some-collection");
const doc1 = { number: 1, hello: "world1" };
const info1 = await collection.save(doc1);
const doc2 = { number: 2, hello: "world2" };
const info2 = await collection.save(doc2);
const result = await collection.bulkUpdate(
  [{ _key: info1._key, number: 3 }, { _key: info2._key, number: 4 }],
  { returnNew: true }
);
```

## collection.remove

`async collection.remove(documentHandle, [opts]): Object`

Deletes the document with the given _documentHandle_ from the collection.

**Arguments**

- **documentHandle**: `string`

  The handle of the document to delete. This can be either the `_id` or the
  `_key` of a document in the collection, or a document (i.e. an object with an
  `_id` or `_key` property).

- **opts**: `Object` (optional)

  If _opts_ is set, it must be an object with any of the following properties:

  - **waitForSync**: `boolean` (Default: `false`)

    Wait until document has been synced to disk.

  - **rev**: `string` (optional)

    Only update the document if it matches this revision.

  - **policy**: `string` (optional)

    {% hint 'warning' %}
    This option has no effect in ArangoDB 3.0 and later.
    {% endhint %}

    Determines the behavior when the revision is not matched:

    - if _policy_ is set to `"last"`, the document will be replaced regardless
      of the revision.
    - if _policy_ is set to `"error"` or not set, the replacement will fail with
      an error.

If a string is passed instead of an options object, it will be interpreted as
the _rev_ option.

For more information on the _opts_ object, see the
[HTTP API documentation for working with documents](../../../..//HTTP/Document/WorkingWithDocuments.html).

**Examples**

```js
const db = new Database();
const collection = db.collection("some-collection");

await collection.remove("some-doc");
// document 'some-collection/some-doc' no longer exists

// -- or --

await collection.remove("some-collection/some-doc");
// document 'some-collection/some-doc' no longer exists
```

## collection.list

`async collection.list([type]): Array<string>`

Retrieves a list of references for all documents in the collection.

**Arguments**

- **type**: `string` (Default: `"id"`)

  The format of the document references:

  - if _type_ is set to `"id"`, each reference will be the `_id` of the
    document.
  - if _type_ is set to `"key"`, each reference will be the `_key` of the
    document.
  - if _type_ is set to `"path"`, each reference will be the URI path of the
    document.
