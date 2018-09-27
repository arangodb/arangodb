<!-- don't edit here, it's from https://@github.com/arangodb/arangodbjs.git / docs/Drivers/ -->
# Manipulating databases

These functions implement the
[HTTP API for manipulating databases](../../../..//HTTP/Database/index.html).

## database.createDatabase

`async database.createDatabase(databaseName, [users]): Object`

Creates a new database with the given _databaseName_.

**Arguments**

- **databaseName**: `string`

  Name of the database to create.

- **users**: `Array<Object>` (optional)

  If specified, the array must contain objects with the following properties:

  - **username**: `string`

    The username of the user to create for the database.

  - **passwd**: `string` (Default: empty)

    The password of the user.

  - **active**: `boolean` (Default: `true`)

    Whether the user is active.

  - **extra**: `Object` (optional)

    An object containing additional user data.

**Examples**

```js
const db = new Database();
const info = await db.createDatabase("mydb", [{ username: "root" }]);
// the database has been created
```

## database.exists

`async database.exists(): boolean`

Checks whether the database exists.

**Examples**

```js
const db = new Database();
const result = await db.exists();
// result indicates whether the database exists
```

## database.get

`async database.get(): Object`

Fetches the database description for the active database from the server.

**Examples**

```js
const db = new Database();
const info = await db.get();
// the database exists
```

## database.listDatabases

`async database.listDatabases(): Array<string>`

Fetches all databases from the server and returns an array of their names.

**Examples**

```js
const db = new Database();
const names = await db.listDatabases();
// databases is an array of database names
```

## database.listUserDatabases

`async database.listUserDatabases(): Array<string>`

Fetches all databases accessible to the active user from the server and returns
an array of their names.

**Examples**

```js
const db = new Database();
const names = await db.listUserDatabases();
// databases is an array of database names
```

## database.dropDatabase

`async database.dropDatabase(databaseName): Object`

Deletes the database with the given _databaseName_ from the server.

```js
const db = new Database();
await db.dropDatabase("mydb");
// database "mydb" no longer exists
```

## database.truncate

`async database.truncate([excludeSystem]): Object`

Deletes **all documents in all collections** in the active database.

**Arguments**

- **excludeSystem**: `boolean` (Default: `true`)

  Whether system collections should be excluded. Note that this option will be
  ignored because truncating system collections is not supported anymore for
  some system collections.

**Examples**

```js
const db = new Database();

await db.truncate();
// all non-system collections in this database are now empty
```
