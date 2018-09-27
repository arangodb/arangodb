<!-- don't edit here, its from https://@github.com/arangodb/arangodbjs.git / docs/Drivers/ -->
# Bulk importing documents

This function implements the
[HTTP API for bulk imports](../../../..//HTTP/BulkImports/index.html).

## collection.import

`async collection.import(data, [opts]): Object`

Bulk imports the given _data_ into the collection.

**Arguments**

* **data**: `Array<Array<any>> | Array<Object>`

  The data to import. This can be an array of documents:

  ```js
  [
    {key1: value1, key2: value2}, // document 1
    {key1: value1, key2: value2}, // document 2
    ...
  ]
  ```

  Or it can be an array of value arrays following an array of keys.

  ```js
  [
    ['key1', 'key2'], // key names
    [value1, value2], // document 1
    [value1, value2], // document 2
    ...
  ]
  ```

* **opts**: `Object` (optional) If _opts_ is set, it must be an object with any
  of the following properties:

  * **waitForSync**: `boolean` (Default: `false`)

    Wait until the documents have been synced to disk.

  * **details**: `boolean` (Default: `false`)

    Whether the response should contain additional details about documents that
    could not be imported.false\*.

  * **type**: `string` (Default: `"auto"`)

    Indicates which format the data uses. Can be `"documents"`, `"array"` or
    `"auto"`.

If _data_ is a JavaScript array, it will be transmitted as a line-delimited JSON
stream. If _opts.type_ is set to `"array"`, it will be transmitted as regular
JSON instead. If _data_ is a string, it will be transmitted as it is without any
processing.

For more information on the _opts_ object, see
[the HTTP API documentation for bulk imports](../../../..//HTTP/BulkImports/ImportingSelfContained.html).

**Examples**

```js
const db = new Database();
const collection = db.collection('users');

// document stream
const result = await collection.import([
  {username: 'admin', password: 'hunter2'},
  {username: 'jcd', password: 'bionicman'},
  {username: 'jreyes', password: 'amigo'},
  {username: 'ghermann', password: 'zeitgeist'}
]);
assert.equal(result.created, 4);

// -- or --

// array stream with header
const result = await collection.import([
  ['username', 'password'], // keys
  ['admin', 'hunter2'], // row 1
  ['jcd', 'bionicman'], // row 2
  ['jreyes', 'amigo'],
  ['ghermann', 'zeitgeist']
]);
assert.equal(result.created, 4);

// -- or --

// raw line-delimited JSON array stream with header
const result = await collection.import([
  '["username", "password"]',
  '["admin", "hunter2"]',
  '["jcd", "bionicman"]',
  '["jreyes", "amigo"]',
  '["ghermann", "zeitgeist"]'
].join('\r\n') + '\r\n');
assert.equal(result.created, 4);
```
