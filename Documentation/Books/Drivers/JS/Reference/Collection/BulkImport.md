<!-- don't edit here, it's from https://@github.com/arangodb/arangojs.git / docs/Drivers/ -->
# Bulk importing documents

This function implements the
[HTTP API for bulk imports](../../../..//HTTP/BulkImports/index.html).

## collection.import

`async collection.import(data, [opts]): Object`

Bulk imports the given _data_ into the collection.

**Arguments**

- **data**: `Array | Buffer | string`

  The data to import. Depending on the _type_ option this can be any of the
  following:

  For type `"documents"` or `"auto"`:

  - an array of documents, e.g.

    ```json
    [
      { "_key": "banana", "color": "yellow" },
      { "_key": "peach", "color": "pink" }
    ]
    ```

  - a string or buffer containing one JSON document per line, e.g.

    ```
    {"_key":"banana","color":"yellow"}
    {"_key":"peach","color":"pink"}
    ```

  For type `"array"` or `"auto"`:

  - a string or buffer containing a JSON array of documents, e.g.

    ```json
    [
      { "_key": "banana", "color": "yellow" },
      { "_key": "peach", "color": "pink" }
    ]
    ```

  For type `null`:

  - an array containing an array of keys followed by arrays of values, e.g.

    ```
    [
      ["_key", "color"],
      ["banana", "yellow"],
      ["peach", "pink"]
    ]
    ```

  - a string or buffer containing a JSON array of keys followed by
    one JSON array of values per line, e.g.

    ```
    ["_key", "color"]
    ["banana", "yellow"]
    ["peach", "pink"]
    ```

- **opts**: `Object` (optional)

  If _opts_ is set, it must be an object with any of the following properties:

  - **type**: `string | null` (Default: `"auto"`)

    Indicates which format the data uses.
    Can be `"documents"`, `"array"` or `"auto"`.
    Use `null` to explicitly set no type.

  - **fromPrefix**: `string` (optional)

    Prefix to prepend to `_from` attributes.

  - **toPrefix**: `string` (optional)

    Prefix to prepend to `_to` attributes.

  - **overwrite**: `boolean` (Default: `false`)

    If set to `true`, the collection is truncated before the data is imported.

  - **waitForSync**: `boolean` (Default: `false`)

    Wait until the documents have been synced to disk.

  - **onDuplicate**: `string` (Default: `"error"`)

    Controls behavior when a unique constraint is violated.
    Can be `"error"`, `"update"`, `"replace"` or `"ignore"`.

  - **complete**: `boolean` (Default: `false`)

    If set to `true`, the import will abort if any error occurs.

  - **details**: `boolean` (Default: `false`)

    Whether the response should contain additional details about documents that
    could not be imported.

For more information on the _opts_ object, see the
[HTTP API documentation for bulk imports](../../../..//HTTP/BulkImports/index.html).

**Examples**

```js
const db = new Database();
const collection = db.collection("users");

const result = await collection.import(
  [
    { username: "jcd", password: "bionicman" },
    { username: "jreyes", password: "amigo" },
    { username: "ghermann", password: "zeitgeist" }
  ],
  { type: "documents" } // optional
);

// -- or --

const buf = fs.readFileSync("dx_users.json");
// [
//   {"username": "jcd", "password": "bionicman"},
//   {"username": "jreyes", "password": "amigo"},
//   {"username": "ghermann", "password": "zeitgeist"}
// ]
const result = await collection.import(
  buf,
  { type: "array" } // optional
);

// -- or --

const result = await collection.import(
  [
    ["username", "password"],
    ["jcd", "bionicman"],
    ["jreyes", "amigo"],
    ["ghermann", "zeitgeist"]
  ],
  { type: null } // required
);
```
