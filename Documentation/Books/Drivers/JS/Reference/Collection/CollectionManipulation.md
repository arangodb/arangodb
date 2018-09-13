<!-- don't edit here, its from https://@github.com/arangodb/arangodbjs.git / docs/Drivers/ -->
# Manipulating the collection

These functions implement the
[HTTP API for modifying collections](../../../..//HTTP/Collection/Modifying.html).

## collection.create

`async collection.create([properties]): Object`

Creates a collection with the given _properties_ for this collection's name,
then returns the server response.

**Arguments**

- **properties**: `Object` (optional)

  For more information on the _properties_ object, see the
  [HTTP API documentation for creating collections](../../../..//HTTP/Collection/Creating.html).

**Examples**

```js
const db = new Database();
const collection = db.collection('potatoes');
await collection.create()
// the document collection "potatoes" now exists

// -- or --

const collection = db.edgeCollection('friends');
await collection.create({
  waitForSync: true // always sync document changes to disk
});
// the edge collection "friends" now exists
```

## collection.load

`async collection.load([count]): Object`

Tells the server to load the collection into memory.

**Arguments**

- **count**: `boolean` (Default: `true`)

  If set to `false`, the return value will not include the number of documents
  in the collection (which may speed up the process).

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');
await collection.load(false)
// the collection has now been loaded into memory
```

## collection.unload

`async collection.unload(): Object`

Tells the server to remove the collection from memory.

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');
await collection.unload()
// the collection has now been unloaded from memory
```

## collection.setProperties

`async collection.setProperties(properties): Object`

Replaces the properties of the collection.

**Arguments**

- **properties**: `Object`

  For information on the _properties_ argument see the
  [HTTP API for modifying collections](../../../..//HTTP/Collection/Modifying.html).

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');
const result = await collection.setProperties({waitForSync: true})
assert.equal(result.waitForSync, true);
// the collection will now wait for data being written to disk
// whenever a document is changed
```

## collection.rename

`async collection.rename(name): Object`

Renames the collection. The _Collection_ instance will automatically update its
name when the rename succeeds.

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');
const result = await collection.rename('new-collection-name')
assert.equal(result.name, 'new-collection-name');
assert.equal(collection.name, result.name);
// result contains additional information about the collection
```

## collection.rotate

`async collection.rotate(): Object`

Rotates the journal of the collection.

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');
const data = await collection.rotate();
// data.result will be true if rotation succeeded
```

## collection.truncate

`async collection.truncate(): Object`

Deletes **all documents** in the collection in the database.

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');
await collection.truncate();
// the collection "some-collection" is now empty
```

## collection.drop

`async collection.drop([properties]): Object`

Deletes the collection from the database.

**Arguments**

- **properties**: `Object` (optional)

  An object with the following properties:

  - **isSystem**: `Boolean` (Default: `false`)

    Whether the collection should be dropped even if it is a system collection.

    This parameter must be set to `true` when dropping a system collection.

  For more information on the _properties_ object, see the
  [HTTP API documentation for dropping collections](../../../..//HTTP/Collection/Creating.html#drops-a-collection).

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');
await collection.drop();
// the collection "some-collection" no longer exists
```
