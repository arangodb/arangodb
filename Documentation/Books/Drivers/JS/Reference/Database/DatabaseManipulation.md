<!-- don't edit here, its from https://@github.com/arangodb/arangodbjs.git / docs/Drivers/ -->
# Manipulating databases

These functions implement the
[HTTP API for manipulating databases](../../../..//HTTP/Database/index.html).

## database.acquireHostList

`async database.acquireHostList(): this`

Updates the URL list by requesting a list of all coordinators in the cluster and adding any endpoints not initially specified in the _url_ configuration.

For long-running processes communicating with an ArangoDB cluster it is recommended to run this method repeatedly (e.g. once per hour) to make sure new coordinators are picked up correctly and can be used for fail-over or load balancing.

**Note**: This method can not be used when the arangojs instance was created with `isAbsolute: true`.

## database.useDatabase

`database.useDatabase(databaseName): this`

Updates the _Database_ instance and its connection string to use the given
_databaseName_, then returns itself.

**Note**: This method can not be used when the arangojs instance was created with `isAbsolute: true`.

**Arguments**

- **databaseName**: `string`

  The name of the database to use.

**Examples**

```js
const db = new Database();
db.useDatabase("test");
// The database instance now uses the database "test".
```

## database.useBasicAuth

`database.useBasicAuth([username, [password]]): this`

Updates the _Database_ instance's `authorization` header to use Basic
authentication with the given _username_ and _password_, then returns itself.

**Arguments**

- **username**: `string` (Default: `"root"`)

  The username to authenticate with.

- **password**: `string` (Default: `""`)

  The password to authenticate with.

**Examples**

```js
const db = new Database();
db.useDatabase("test");
db.useBasicAuth("admin", "hunter2");
// The database instance now uses the database "test"
// with the username "admin" and password "hunter2".
```

## database.useBearerAuth

`database.useBearerAuth(token): this`

Updates the _Database_ instance's `authorization` header to use Bearer
authentication with the given authentication token, then returns itself.

**Arguments**

- **token**: `string`

  The token to authenticate with.

**Examples**

```js
const db = new Database();
db.useBearerAuth("keyboardcat");
// The database instance now uses Bearer authentication.
```

## database.login

`async database.login([username, [password]]): string`

Validates the given database credentials and exchanges them for an
authentication token, then uses the authentication token for future
requests and returns it.

**Arguments**

- **username**: `string` (Default: `"root"`)

  The username to authenticate with.

- **password**: `string` (Default: `""`)

  The password to authenticate with.

**Examples**

```js
const db = new Database();
db.useDatabase("test");
await db.login("admin", "hunter2");
// The database instance now uses the database "test"
// with an authentication token for the "admin" user.
```

## database.version

`async database.version(): Object`

Fetches the ArangoDB version information for the active database from the server.

**Examples**

```js
const db = new Database();
const version = await db.version();
// the version object contains the ArangoDB version information.
```

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
const info = await db.createDatabase('mydb', [{username: 'root'}]);
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
await db.dropDatabase('mydb');
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
