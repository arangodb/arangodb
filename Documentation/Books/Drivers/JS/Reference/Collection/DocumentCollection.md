<!-- don't edit here, it's from https://@github.com/arangodb/arangojs.git / docs/Drivers/ -->
# DocumentCollection API

The _DocumentCollection API_ extends the
[_Collection API_](README.md) with the following methods.

## documentCollection.document

`async documentCollection.document(documentHandle, [opts]): Object`

Retrieves the document with the given _documentHandle_ from the collection.

**Arguments**

- **documentHandle**: `string`

  The handle of the document to retrieve. This can be either the `_id` or the
  `_key` of a document in the collection, or a document (i.e. an object with an
  `_id` or `_key` property).

- **opts**: `Object` (optional)

  If _opts_ is set, it must be an object with any of the following properties:

  - **graceful**: `boolean` (Default: `false`)

    If set to `true`, the method will return `null` instead of throwing an
    error if the document does not exist.

  - **allowDirtyRead**: `boolean` (Default: `false`)

    {% hint 'info' %}
    This option is only available when targeting ArangoDB 3.4 or later,
    see [Compatibility](../../GettingStarted/README.md#compatibility).
    {% endhint %}

    If set to `true`, the request will explicitly permit ArangoDB to return a
    potentially dirty or stale result and arangojs will load balance the
    request without distinguishing between leaders and followers.

If a boolean is passed instead of an options object, it will be interpreted as
the _graceful_ option.

**Examples**

```js
const db = new Database();
const collection = db.collection("my-docs");

try {
  const doc = await collection.document("some-key");
  // the document exists
  assert.equal(doc._key, "some-key");
  assert.equal(doc._id, "my-docs/some-key");
} catch (err) {
  // something went wrong or
  // the document does not exist
}

// -- or --

try {
  const doc = await collection.document("my-docs/some-key");
  // the document exists
  assert.equal(doc._key, "some-key");
  assert.equal(doc._id, "my-docs/some-key");
} catch (err) {
  // something went wrong or
  // the document does not exist
}

// -- or --

const doc = await collection.document("some-key", true);
if (doc === null) {
  // the document does not exist
}
```

## documentCollection.documentExists

`async documentCollection.documentExists(documentHandle): boolean`

Checks whether the document with the given _documentHandle_ exists.

**Arguments**

- **documentHandle**: `string`

  The handle of the document to retrieve. This can be either the `_id` or the
  `_key` of a document in the collection, or a document (i.e. an object with an
  `_id` or `_key` property).

**Examples**

```js
const db = new Database();
const collection = db.collection("my-docs");

const exists = await collection.documentExists("some-key");
if (exists === false) {
  // the document does not exist
}
```

## documentCollection.save

`async documentCollection.save(data, [opts]): Object`

Creates a new document with the given _data_ and returns an object containing
the document's metadata.

**Arguments**

- **data**: `Object`

  The data of the new document, may include a `_key`.

- **opts**: `Object` (optional)

  If _opts_ is set, it must be an object with any of the following properties:

  - **waitForSync**: `boolean` (Default: `false`)

    Wait until document has been synced to disk.

  - **returnNew**: `boolean` (Default: `false`)

    If set to `true`, return additionally the complete new documents under the
    attribute `new` in the result.

  - **returnOld**: `boolean` (Default: `false`)

    If set to `true`, return additionally the complete old documents under the
    attribute `old` in the result.

  - **silent**: `boolean` (Default: `false`)

    If set to true, an empty object will be returned as response. No meta-data
    will be returned for the created document. This option can be used to save
    some network traffic.

  - **overwrite**: `boolean` (Default: `false`)

    If set to true, the insert becomes a replace-insert. If a document with the
    same \_key already exists the new document is not rejected with unique
    constraint violated but will replace the old document.

If a boolean is passed instead of an options object, it will be interpreted as
the _returnNew_ option.

For more information on the _opts_ object, see the
[HTTP API documentation for working with documents](../../../..//HTTP/Document/WorkingWithDocuments.html).

**Examples**

```js
const db = new Database();
const collection = db.collection("my-docs");
const data = { some: "data" };
const info = await collection.save(data);
assert.equal(info._id, "my-docs/" + info._key);
const doc2 = await collection.document(info);
assert.equal(doc2._id, info._id);
assert.equal(doc2._rev, info._rev);
assert.equal(doc2.some, data.some);

// -- or --

const db = new Database();
const collection = db.collection("my-docs");
const data = { some: "data" };
const opts = { returnNew: true };
const doc = await collection.save(data, opts);
assert.equal(doc1._id, "my-docs/" + doc1._key);
assert.equal(doc1.new.some, data.some);
```
