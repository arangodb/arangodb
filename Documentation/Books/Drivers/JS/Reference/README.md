<!-- don't edit here, its from https://@github.com/arangodb/arangodbjs.git / docs/Drivers/ -->
# ArangoDB JavaScript Driver - Reference

## Database API

### new Database

`new Database([config]): Database`

Creates a new _Database_ instance.

If _config_ is a string, it will be interpreted as _config.url_.

**Arguments**

* **config**: `Object` (optional)

  An object with the following properties:

  * **url**: `string | Array<string>` (Default: `http://localhost:8529`)

    Base URL of the ArangoDB server or list of server URLs.

    **Note**: As of arangojs 6.0.0 it is no longer possible to pass
    the username or password from the URL.

    If you want to use ArangoDB with authentication, see
    _useBasicAuth_ or _useBearerAuth_ methods.

    If you need to support self-signed HTTPS certificates, you may have to add
    your certificates to the _agentOptions_, e.g.:

    ```js
    agentOptions: {
      ca: [
        fs.readFileSync(".ssl/sub.class1.server.ca.pem"),
        fs.readFileSync(".ssl/ca.pem")
      ];
    }
    ```

  * **isAbsolute**: `boolean` (Default: `false`)

    If this option is explicitly set to `true`, the _url_ will be treated as the
    absolute database path. This is an escape hatch to allow using arangojs with
    database APIs exposed with a reverse proxy and makes it impossible to switch
    databases with _useDatabase_ or using _acquireHostList_.

  * **arangoVersion**: `number` (Default: `30000`)

    Value of the `x-arango-version` header. This should match the lowest
    version of ArangoDB you expect to be using. The format is defined as
    `XYYZZ` where `X` is the major version, `Y` is the two-digit minor version
    and `Z` is the two-digit bugfix version.

    **Example**: `30102` corresponds to version 3.1.2 of ArangoDB.

    **Note**: The driver will behave differently when using different major
    versions of ArangoDB to compensate for API changes. Some functions are
    not available on every major version of ArangoDB as indicated in their
    descriptions below (e.g. _collection.first_, _collection.bulkUpdate_).

  * **headers**: `Object` (optional)

    An object with additional headers to send with every request.

    Header names should always be lowercase. If an `"authorization"` header is
    provided, it will be overridden when using _useBasicAuth_ or _useBearerAuth_.

  * **agent**: `Agent` (optional)

    An http Agent instance to use for connections.

    By default a new
    [`http.Agent`](https://nodejs.org/api/http.html#http_new_agent_options) (or
    https.Agent) instance will be created using the _agentOptions_.

    This option has no effect when using the browser version of arangojs.

  * **agentOptions**: `Object` (Default: see below)

    An object with options for the agent. This will be ignored if _agent_ is
    also provided.

    Default: `{maxSockets: 3, keepAlive: true, keepAliveMsecs: 1000}`.
    Browser default: `{maxSockets: 3, keepAlive: false}`;

    The option `maxSockets` can also be used to limit how many requests
    arangojs will perform concurrently. The maximum number of requests is
    equal to `maxSockets * 2` with `keepAlive: true` or
    equal to `maxSockets` with `keepAlive: false`.

    In the browser version of arangojs this option can be used to pass
    additional options to the underlying calls of the
    [`xhr`](https://www.npmjs.com/package/xhr) module.

  * **loadBalancingStrategy**: `string` (Default: `"NONE"`)

    Determines the behaviour when multiple URLs are provided:

    * `NONE`: No load balancing. All requests will be handled by the first
      URL in the list until a network error is encountered. On network error,
      arangojs will advance to using the next URL in the list.

    * `ONE_RANDOM`: Randomly picks one URL from the list initially, then
      behaves like `NONE`.

    * `ROUND_ROBIN`: Every sequential request uses the next URL in the list.

### Manipulating databases

These functions implement the
[HTTP API for manipulating databases](https://docs.arangodb.com/latest/HTTP/Database/index.html).

#### database.acquireHostList

`async database.acquireHostList(): this`

Updates the URL list by requesting a list of all coordinators in the cluster and adding any endpoints not initially specified in the _url_ configuration.

For long-running processes communicating with an ArangoDB cluster it is recommended to run this method repeatedly (e.g. once per hour) to make sure new coordinators are picked up correctly and can be used for fail-over or load balancing.

**Note**: This method can not be used when the arangojs instance was created with `isAbsolute: true`.

#### database.useDatabase

`database.useDatabase(databaseName): this`

Updates the _Database_ instance and its connection string to use the given
_databaseName_, then returns itself.

**Note**: This method can not be used when the arangojs instance was created with `isAbsolute: true`.

**Arguments**

* **databaseName**: `string`

  The name of the database to use.

**Examples**

```js
const db = new Database();
db.useDatabase("test");
// The database instance now uses the database "test".
```

#### database.useBasicAuth

`database.useBasicAuth(username, password): this`

Updates the _Database_ instance's `authorization` header to use Basic
authentication with the given _username_ and _password_, then returns itself.

**Arguments**

* **username**: `string` (Default: `"root"`)

  The username to authenticate with.

* **password**: `string` (Default: `""`)

  The password to authenticate with.

**Examples**

```js
const db = new Database();
db.useDatabase("test");
db.useBasicAuth("admin", "hunter2");
// The database instance now uses the database "test"
// with the username "admin" and password "hunter2".
```

#### database.useBearerAuth

`database.useBearerAuth(token): this`

Updates the _Database_ instance's `authorization` header to use Bearer
authentication with the given authentication token, then returns itself.

**Arguments**

* **token**: `string`

  The token to authenticate with.

**Examples**

```js
const db = new Database();
db.useBearerAuth("keyboardcat");
// The database instance now uses Bearer authentication.
```

#### database.createDatabase

`async database.createDatabase(databaseName, [users]): Object`

Creates a new database with the given _databaseName_.

**Arguments**

* **databaseName**: `string`

  Name of the database to create.

* **users**: `Array<Object>` (optional)

  If specified, the array must contain objects with the following properties:

  * **username**: `string`

    The username of the user to create for the database.

  * **passwd**: `string` (Default: empty)

    The password of the user.

  * **active**: `boolean` (Default: `true`)

    Whether the user is active.

  * **extra**: `Object` (optional)

    An object containing additional user data.

**Examples**

```js
const db = new Database();
const info = await db.createDatabase('mydb', [{username: 'root'}]);
// the database has been created
```

#### database.get

`async database.get(): Object`

Fetches the database description for the active database from the server.

**Examples**

```js
const db = new Database();
const info = await db.get();
// the database exists
```

#### database.listDatabases

`async database.listDatabases(): Array<string>`

Fetches all databases from the server and returns an array of their names.

**Examples**

```js
const db = new Database();
const names = await db.listDatabases();
// databases is an array of database names
```

#### database.listUserDatabases

`async database.listUserDatabases(): Array<string>`

Fetches all databases accessible to the active user from the server and returns
an array of their names.

**Examples**

```js
const db = new Database();
const names = await db.listUserDatabases();
// databases is an array of database names
```

#### database.dropDatabase

`async database.dropDatabase(databaseName): Object`

Deletes the database with the given _databaseName_ from the server.

```js
const db = new Database();
await db.dropDatabase('mydb');
// database "mydb" no longer exists
```

#### database.truncate

`async database.truncate([excludeSystem]): Object`

Deletes **all documents in all collections** in the active database.

**Arguments**

* **excludeSystem**: `boolean` (Default: `true`)

  Whether system collections should be excluded. Note that this option will be
  ignored because truncating system collections is not supported anymore for
  some system collections.

**Examples**

```js
const db = new Database();

await db.truncate();
// all non-system collections in this database are now empty
```

### Accessing collections

These functions implement the
[HTTP API for accessing collections](https://docs.arangodb.com/latest/HTTP/Collection/Getting.html).

#### database.collection

`database.collection(collectionName): DocumentCollection`

Returns a _DocumentCollection_ instance for the given collection name.

**Arguments**

* **collectionName**: `string`

  Name of the edge collection.

**Examples**

```js
const db = new Database();
const collection = db.collection("potatos");
```

#### database.edgeCollection

`database.edgeCollection(collectionName): EdgeCollection`

Returns an _EdgeCollection_ instance for the given collection name.

**Arguments**

* **collectionName**: `string`

  Name of the edge collection.

**Examples**

```js
const db = new Database();
const collection = db.edgeCollection("potatos");
```

#### database.listCollections

`async database.listCollections([excludeSystem]): Array<Object>`

Fetches all collections from the database and returns an array of collection
descriptions.

**Arguments**

* **excludeSystem**: `boolean` (Default: `true`)

  Whether system collections should be excluded.

**Examples**

```js
const db = new Database();

const collections = await db.listCollections();
// collections is an array of collection descriptions
// not including system collections

// -- or --

const collections = await db.listCollections(false);
// collections is an array of collection descriptions
// including system collections
```

#### database.collections

`async database.collections([excludeSystem]): Array<Collection>`

Fetches all collections from the database and returns an array of
_DocumentCollection_ and _EdgeCollection_ instances for the collections.

**Arguments**

* **excludeSystem**: `boolean` (Default: `true`)

  Whether system collections should be excluded.

**Examples**

```js
const db = new Database();

const collections = await db.collections()
// collections is an array of DocumentCollection
// and EdgeCollection instances
// not including system collections

// -- or --

const collections = await db.collections(false)
// collections is an array of DocumentCollection
// and EdgeCollection instances
// including system collections
```

### Accessing graphs

These functions implement the
[HTTP API for accessing general graphs](https://docs.arangodb.com/latest/HTTP/Gharial/index.html).

#### database.graph

`database.graph(graphName): Graph`

Returns a _Graph_ instance representing the graph with the given graph name.

#### database.listGraphs

`async database.listGraphs(): Array<Object>`

Fetches all graphs from the database and returns an array of graph descriptions.

**Examples**

```js
const db = new Database();
const graphs = await db.listGraphs();
// graphs is an array of graph descriptions
```

#### database.graphs

`async database.graphs(): Array<Graph>`

Fetches all graphs from the database and returns an array of _Graph_ instances
for the graphs.

**Examples**

```js
const db = new Database();
const graphs = await db.graphs();
// graphs is an array of Graph instances
```

### Transactions

This function implements the
[HTTP API for transactions](https://docs.arangodb.com/latest/HTTP/Transaction/index.html).

#### database.transaction

`async database.transaction(collections, action, [params, [options]]):
Object`

Performs a server-side transaction and returns its return value.

**Arguments**

* **collections**: `Object`

  An object with the following properties:

  * **read**: `Array<string>` (optional)

    An array of names (or a single name) of collections that will be read from
    during the transaction.

  * **write**: `Array<string>` (optional)

    An array of names (or a single name) of collections that will be written to
    or read from during the transaction.

* **action**: `string`

  A string evaluating to a JavaScript function to be executed on the server.

  **Note**: For accessing the database from within ArangoDB, see
  [the documentation for the `@arangodb` module in ArangoDB](https://docs.arangodb.com/3.1/Manual/Appendix/JavaScriptModules/ArangoDB.html).

* **params**: `Object` (optional)

  Available as variable `params` when the _action_ function is being executed on
  server. Check the example below.

* **options**: `Object` (optional)

  An object with any of the following properties:

  * **lockTimeout**: `number` (optional)

    Determines how long the database will wait while attemping to gain locks on
    collections used by the transaction before timing out.

  * **waitForSync**: `boolean` (optional)

    Determines whether to force the transaction to write all data to disk before returning.

  * **maxTransactionSize**: `number` (optional)

    Determines the transaction size limit in bytes. Honored by the RocksDB storage engine only.

  * **intermediateCommitCount**: `number` (optional)

    Determines the maximum number of operations after which an intermediate commit is
    performed automatically. Honored by the RocksDB storage engine only.

  * **intermediateCommitSize**: `number` (optional)

    Determine the maximum total size of operations after which an intermediate commit is
    performed automatically. Honored by the RocksDB storage engine only.

If _collections_ is an array or string, it will be treated as
_collections.write_.

Please note that while _action_ should be a string evaluating to a well-formed
JavaScript function, it's not possible to pass in a JavaScript function directly
because the function needs to be evaluated on the server and will be transmitted
in plain text.

For more information on transactions, see
[the HTTP API documentation for transactions](https://docs.arangodb.com/latest/HTTP/Transaction/index.html).

**Examples**

```js
const db = new Database();
const action = String(function (params) {
  // This code will be executed inside ArangoDB!
  const db = require('@arangodb').db;
  return db._query(aql`
    FOR user IN _users
    FILTER user.age > ${params.age}
    RETURN u.user
  `).toArray();
});

const result = await db.transaction(
  {read: '_users'},
  action,
  {age: 12}
);
// result contains the return value of the action
```

### Queries

This function implements the
[HTTP API for single roundtrip AQL queries](https://docs.arangodb.com/latest/HTTP/AqlQueryCursor/QueryResults.html).

For collection-specific queries see [simple queries](#simple-queries).

#### database.query

`async database.query(query, [bindVars,] [opts]): Cursor`

Performs a database query using the given _query_ and _bindVars_, then returns a
[new _Cursor_ instance](#cursor-api) for the result list.

**Arguments**

* **query**: `string`

  An AQL query string or a [query builder](https://npmjs.org/package/aqb)
  instance.

* **bindVars**: `Object` (optional)

  An object defining the variables to bind the query to.

* **opts**: `Object` (optional)

  Additional parameter object that will be passed to the query API.
  Possible keys are _count_ and _options_ (explained below)

If _opts.count_ is set to `true`, the cursor will have a _count_ property set to
the query result count.
Possible key options in _opts.options_ include: _failOnWarning_, _cache_, profile or _skipInaccessibleCollections_.
For a complete list of query settings please reference the [arangodb.com documentation](https://docs.arangodb.com/3.3/AQL/Invocation/WithArangosh.html#setting-options).

If _query_ is an object with _query_ and _bindVars_ properties, those will be
used as the values of the respective arguments instead.

**Examples**

```js
const db = new Database();
const active = true;

// Using the aql template tag
const cursor = await db.query(aql`
  FOR u IN _users
  FILTER u.authData.active == ${active}
  RETURN u.user
`);
// cursor is a cursor for the query result

// -- or --

// Old-school JS with explicit bindVars:
db.query(
  'FOR u IN _users ' +
  'FILTER u.authData.active == @active ' +
  'RETURN u.user',
  {active: true}
).then(function (cursor) {
  // cursor is a cursor for the query result
});
```

#### aql

`aql(strings, ...args): Object`

Template string handler (aka template tag) for AQL queries. Converts a template
string to an object that can be passed to `database.query` by converting
arguments to bind variables.

**Note**: If you want to pass a collection name as a bind variable, you need to
pass a _Collection_ instance (e.g. what you get by passing the collection name
to `db.collection`) instead. If you see the error `"array expected as operand to
FOR loop"`, you're likely passing a collection name instead of a collection
instance.

**Examples**

```js
const userCollection = db.collection("_users");
const role = "admin";

const query = aql`
  FOR user IN ${userCollection}
  FILTER user.role == ${role}
  RETURN user
`;

// -- is equivalent to --
const query = {
  query: "FOR user IN @@value0 FILTER user.role == @value1 RETURN user",
  bindVars: { "@value0": userCollection.name, value1: role }
};
```

Note how the aql template tag automatically handles collection references
(`@@value0` instead of `@value0`) for us so you don't have to worry about
counting at-symbols.

Because the aql template tag creates actual bindVars instead of inlining values
directly, it also avoids injection attacks via malicious parameters:

```js
// malicious user input
const email = '" || (FOR x IN secrets REMOVE x IN secrets) || "';

// DON'T do this!
const query = `
  FOR user IN users
  FILTER user.email == "${email}"
  RETURN user
`;
// FILTER user.email == "" || (FOR x IN secrets REMOVE x IN secrets) || ""

// instead do this!
const query = aql`
  FOR user IN users
  FILTER user.email == ${email}
  RETURN user
`;
// FILTER user.email == @value0
```

### Managing AQL user functions

These functions implement the
[HTTP API for managing AQL user functions](https://docs.arangodb.com/latest/HTTP/AqlUserFunctions/index.html).

#### database.listFunctions

`async database.listFunctions(): Array<Object>`

Fetches a list of all AQL user functions registered with the database.

**Examples**

```js
const db = new Database();
const functions = db.listFunctions();
// functions is a list of function descriptions
```

#### database.createFunction

`async database.createFunction(name, code): Object`

Creates an AQL user function with the given _name_ and _code_ if it does not
already exist or replaces it if a function with the same name already existed.

**Arguments**

* **name**: `string`

  A valid AQL function name, e.g.: `"myfuncs::accounting::calculate_vat"`.

* **code**: `string`

  A string evaluating to a JavaScript function (not a JavaScript function
  object).

**Examples**

```js
const db = new Database();
await db.createFunction(
  'ACME::ACCOUNTING::CALCULATE_VAT',
  String(function (price) {
    return price * 0.19;
  })
);
// Use the new function in an AQL query with template handler:
const cursor = await db.query(aql`
  FOR product IN products
  RETURN MERGE(
    {vat: ACME::ACCOUNTING::CALCULATE_VAT(product.price)},
    product
  )
`);
// cursor is a cursor for the query result
```

#### database.dropFunction

`async database.dropFunction(name, [group]): Object`

Deletes the AQL user function with the given name from the database.

**Arguments**

* **name**: `string`

  The name of the user function to drop.

* **group**: `boolean` (Default: `false`)

  If set to `true`, all functions with a name starting with _name_ will be
  deleted; otherwise only the function with the exact name will be deleted.

**Examples**

```js
const db = new Database();
await db.dropFunction('ACME::ACCOUNTING::CALCULATE_VAT');
// the function no longer exists
```

### Managing Foxx services

#### database.listServices

`async database.listServices([excludeSystem]): Array<Object>`

Fetches a list of all installed service.

**Arguments**

* **excludeSystem**: `boolean` (Default: `true`)

  Whether system services should be excluded.

**Examples**

```js
const services = await db.listServices();

// -- or --

const services = await db.listServices(false);
```

#### database.installService

`async database.installService(mount, source, [options]): Object`

Installs a new service.

**Arguments**

* **mount**: `string`

  The service's mount point, relative to the database.

* **source**: `Buffer | Readable | File | string`

  The service bundle to install.

* **options**: `Object` (optional)

  An object with any of the following properties:

  * **configuration**: `Object` (optional)

    An object mapping configuration option names to values.

  * **dependencies**: `Object` (optional)

    An object mapping dependency aliases to mount points.

  * **development**: `boolean` (Default: `false`)

    Whether the service should be installed in development mode.

  * **legacy**: `boolean` (Default: `false`)

    Whether the service should be installed in legacy compatibility mode.

    This overrides the `engines` option in the service manifest (if any).

  * **setup**: `boolean` (Default: `true`)

    Whether the setup script should be executed.

**Examples**

```js
const source = fs.createReadStream('./my-foxx-service.zip');
const info = await db.installService('/hello', source);

// -- or --

const source = fs.readFileSync('./my-foxx-service.zip');
const info = await db.installService('/hello', source);

// -- or --

const element = document.getElementById('my-file-input');
const source = element.files[0];
const info = await db.installService('/hello', source);
```

#### database.replaceService

`async database.replaceService(mount, source, [options]): Object`

Replaces an existing service with a new service by completely removing the old
service and installing a new service at the same mount point.

**Arguments**

* **mount**: `string`

  The service's mount point, relative to the database.

* **source**: `Buffer | Readable | File | string`

  The service bundle to replace the existing service with.

* **options**: `Object` (optional)

  An object with any of the following properties:

  * **configuration**: `Object` (optional)

    An object mapping configuration option names to values.

    This configuration will replace the existing configuration.

  * **dependencies**: `Object` (optional)

    An object mapping dependency aliases to mount points.

    These dependencies will replace the existing dependencies.

  * **development**: `boolean` (Default: `false`)

    Whether the new service should be installed in development mode.

  * **legacy**: `boolean` (Default: `false`)

    Whether the new service should be installed in legacy compatibility mode.

    This overrides the `engines` option in the service manifest (if any).

  * **teardown**: `boolean` (Default: `true`)

    Whether the teardown script of the old service should be executed.

  * **setup**: `boolean` (Default: `true`)

    Whether the setup script of the new service should be executed.

**Examples**

```js
const source = fs.createReadStream('./my-foxx-service.zip');
const info = await db.replaceService('/hello', source);

// -- or --

const source = fs.readFileSync('./my-foxx-service.zip');
const info = await db.replaceService('/hello', source);

// -- or --

const element = document.getElementById('my-file-input');
const source = element.files[0];
const info = await db.replaceService('/hello', source);
```

#### database.upgradeService

`async database.upgradeService(mount, source, [options]): Object`

Replaces an existing service with a new service while retaining the old
service's configuration and dependencies.

**Arguments**

* **mount**: `string`

  The service's mount point, relative to the database.

* **source**: `Buffer | Readable | File | string`

  The service bundle to replace the existing service with.

* **options**: `Object` (optional)

  An object with any of the following properties:

  * **configuration**: `Object` (optional)

    An object mapping configuration option names to values.

    This configuration will be merged into the existing configuration.

  * **dependencies**: `Object` (optional)

    An object mapping dependency aliases to mount points.

    These dependencies will be merged into the existing dependencies.

  * **development**: `boolean` (Default: `false`)

    Whether the new service should be installed in development mode.

  * **legacy**: `boolean` (Default: `false`)

    Whether the new service should be installed in legacy compatibility mode.

    This overrides the `engines` option in the service manifest (if any).

  * **teardown**: `boolean` (Default: `false`)

    Whether the teardown script of the old service should be executed.

  * **setup**: `boolean` (Default: `true`)

    Whether the setup script of the new service should be executed.

**Examples**

```js
const source = fs.createReadStream('./my-foxx-service.zip');
const info = await db.upgradeService('/hello', source);

// -- or --

const source = fs.readFileSync('./my-foxx-service.zip');
const info = await db.upgradeService('/hello', source);

// -- or --

const element = document.getElementById('my-file-input');
const source = element.files[0];
const info = await db.upgradeService('/hello', source);
```

#### database.uninstallService

`async database.uninstallService(mount, [options]): void`

Completely removes a service from the database.

**Arguments**

* **mount**: `string`

  The service's mount point, relative to the database.

* **options**: `Object` (optional)

  An object with any of the following properties:

  * **teardown**: `boolean` (Default: `true`)

    Whether the teardown script should be executed.

**Examples**

```js
await db.uninstallService('/my-service');
// service was uninstalled
```

#### database.getService

`async database.getService(mount): Object`

Retrieves information about a mounted service.

**Arguments**

* **mount**: `string`

  The service's mount point, relative to the database.

**Examples**

```js
const info = await db.getService('/my-service');
// info contains detailed information about the service
```

#### database.getServiceConfiguration

`async database.getServiceConfiguration(mount, [minimal]): Object`

Retrieves an object with information about the service's configuration options
and their current values.

**Arguments**

* **mount**: `string`

  The service's mount point, relative to the database.

* **minimal**: `boolean` (Default: `false`)

  Only return the current values.

**Examples**

```js
const config = await db.getServiceConfiguration('/my-service');
// config contains information about the service's configuration
```

#### database.replaceServiceConfiguration

`async database.replaceServiceConfiguration(mount, configuration, [minimal]):
Object`

Replaces the configuration of the given service.

**Arguments**

* **mount**: `string`

  The service's mount point, relative to the database.

* **configuration**: `Object`

  An object mapping configuration option names to values.

* **minimal**: `boolean` (Default: `false`)

  Only return the current values and warnings (if any).

  **Note:** when using ArangoDB 3.2.8 or older, enabling this option avoids
  triggering a second request to the database.

**Examples**

```js
const config = {currency: 'USD', locale: 'en-US'};
const info = await db.replaceServiceConfiguration('/my-service', config);
// info.values contains information about the service's configuration
// info.warnings contains any validation errors for the configuration
```

#### database.updateServiceConfiguration

`async database.updateServiceConfiguration(mount, configuration, [minimal]):
Object`

Updates the configuration of the given service my merging the new values into
the existing ones.

**Arguments**

* **mount**: `string`

  The service's mount point, relative to the database.

* **configuration**: `Object`

  An object mapping configuration option names to values.

* **minimal**: `boolean` (Default: `false`)

  Only return the current values and warnings (if any).

  **Note:** when using ArangoDB 3.2.8 or older, enabling this option avoids
  triggering a second request to the database.

**Examples**

```js
const config = {locale: 'en-US'};
const info = await db.updateServiceConfiguration('/my-service', config);
// info.values contains information about the service's configuration
// info.warnings contains any validation errors for the configuration
```

#### database.getServiceDependencies

`async database.getServiceDependencies(mount, [minimal]): Object`

Retrieves an object with information about the service's dependencies and their
current mount points.

**Arguments**

* **mount**: `string`

  The service's mount point, relative to the database.

* **minimal**: `boolean` (Default: `false`)

  Only return the current values and warnings (if any).

**Examples**

```js
const deps = await db.getServiceDependencies('/my-service');
// deps contains information about the service's dependencies
```

#### database.replaceServiceDependencies

`async database.replaceServiceDependencies(mount, dependencies, [minimal]):
Object`

Replaces the dependencies for the given service.

**Arguments**

* **mount**: `string`

  The service's mount point, relative to the database.

* **dependencies**: `Object`

  An object mapping dependency aliases to mount points.

* **minimal**: `boolean` (Default: `false`)

  Only return the current values and warnings (if any).

  **Note:** when using ArangoDB 3.2.8 or older, enabling this option avoids
  triggering a second request to the database.

**Examples**

```js
const deps = {mailer: '/mailer-api', auth: '/remote-auth'};
const info = await db.replaceServiceDependencies('/my-service', deps);
// info.values contains information about the service's dependencies
// info.warnings contains any validation errors for the dependencies
```

#### database.updateServiceDependencies

`async database.updateServiceDependencies(mount, dependencies, [minimal]):
Object`

Updates the dependencies for the given service by merging the new values into
the existing ones.

**Arguments**

* **mount**: `string`

  The service's mount point, relative to the database.

* **dependencies**: `Object`

  An object mapping dependency aliases to mount points.

* **minimal**: `boolean` (Default: `false`)

  Only return the current values and warnings (if any).

  **Note:** when using ArangoDB 3.2.8 or older, enabling this option avoids
  triggering a second request to the database.

**Examples**

```js
const deps = {mailer: '/mailer-api'};
const info = await db.updateServiceDependencies('/my-service', deps);
// info.values contains information about the service's dependencies
// info.warnings contains any validation errors for the dependencies
```

#### database.enableServiceDevelopmentMode

`async database.enableServiceDevelopmentMode(mount): Object`

Enables development mode for the given service.

**Arguments**

* **mount**: `string`

  The service's mount point, relative to the database.

**Examples**

```js
const info = await db.enableServiceDevelopmentMode('/my-service');
// the service is now in development mode
// info contains detailed information about the service
```

#### database.disableServiceDevelopmentMode

`async database.disableServiceDevelopmentMode(mount): Object`

Disabled development mode for the given service and commits the service state to
the database.

**Arguments**

* **mount**: `string`

  The service's mount point, relative to the database.

**Examples**

```js
const info = await db.disableServiceDevelopmentMode('/my-service');
// the service is now in production mode
// info contains detailed information about the service
```

#### database.listServiceScripts

`async database.listServiceScripts(mount): Object`

Retrieves a list of the service's scripts.

Returns an object mapping each name to a more readable representation.

**Arguments**

* **mount**: `string`

  The service's mount point, relative to the database.

**Examples**

```js
const scripts = await db.listServiceScripts('/my-service');
// scripts is an object listing the service scripts
```

#### database.runServiceScript

`async database.runServiceScript(mount, name, [scriptArg]): any`

Runs a service script and returns the result.

**Arguments**

* **mount**: `string`

  The service's mount point, relative to the database.

* **name**: `string`

  Name of the script to execute.

* **scriptArg**: `any`

  Value that will be passed as an argument to the script.

**Examples**

```js
const result = await db.runServiceScript('/my-service', 'setup');
// result contains the script's exports (if any)
```

#### database.runServiceTests

`async database.runServiceTests(mount, [reporter]): any`

Runs the tests of a given service and returns a formatted report.

**Arguments**

* **mount**: `string`

  The service's mount point, relative to the database

* **options**: `Object` (optional)

  An object with any of the following properties:

  * **reporter**: `string` (Default: `default`)

    The reporter to use to process the test results.

    As of ArangoDB 3.2 the following reporters are supported:

    * **stream**: an array of event objects
    * **suite**: nested suite objects with test results
    * **xunit**: JSONML representation of an XUnit report
    * **tap**: an array of TAP event strings
    * **default**: an array of test results

  * **idiomatic**: `boolean` (Default: `false`)

    Whether the results should be converted to the apropriate `string`
    representation:

    * **xunit** reports will be formatted as XML documents
    * **tap** reports will be formatted as TAP streams
    * **stream** reports will be formatted as JSON-LD streams

**Examples**

```js
const opts = {reporter: 'xunit', idiomatic: true};
const result = await db.runServiceTests('/my-service', opts);
// result contains the XUnit report as a string
```

#### database.downloadService

`async database.downloadService(mount): Buffer | Blob`

Retrieves a zip bundle containing the service files.

Returns a `Buffer` in Node or `Blob` in the browser version.

**Arguments**

* **mount**: `string`

  The service's mount point, relative to the database.

**Examples**

```js
const bundle = await db.downloadService('/my-service');
// bundle is a Buffer/Blob of the service bundle
```

#### database.getServiceReadme

`async database.getServiceReadme(mount): string?`

Retrieves the text content of the service's `README` or `README.md` file.

Returns `undefined` if no such file could be found.

**Arguments**

* **mount**: `string`

  The service's mount point, relative to the database.

**Examples**

```js
const readme = await db.getServiceReadme('/my-service');
// readme is a string containing the service README's
// text content, or undefined if no README exists
```

#### database.getServiceDocumentation

`async database.getServiceDocumentation(mount): Object`

Retrieves a Swagger API description object for the service installed at the
given mount point.

**Arguments**

* **mount**: `string`

  The service's mount point, relative to the database.

**Examples**

```js
const spec = await db.getServiceDocumentation('/my-service');
// spec is a Swagger API description of the service
```

#### database.commitLocalServiceState

`async database.commitLocalServiceState([replace]): void`

Writes all locally available services to the database and updates any service
bundles missing in the database.

**Arguments**

* **replace**: `boolean` (Default: `false`)

  Also commit outdated services.

  This can be used to solve some consistency problems when service bundles are
  missing in the database or were deleted manually.

**Examples**

```js
await db.commitLocalServiceState();
// all services available on the coordinator have been written to the db

// -- or --

await db.commitLocalServiceState(true);
// all service conflicts have been resolved in favor of this coordinator
```

### Arbitrary HTTP routes

#### database.route

`database.route([path,] [headers]): Route`

Returns a new _Route_ instance for the given path (relative to the database)
that can be used to perform arbitrary HTTP requests.

**Arguments**

* **path**: `string` (optional)

  The database-relative URL of the route.

* **headers**: `Object` (optional)

  Default headers that should be sent with each request to the route.

If _path_ is missing, the route will refer to the base URL of the database.

For more information on _Route_ instances see the
[_Route API_ below](#route-api).

**Examples**

```js
const db = new Database();
const myFoxxService = db.route('my-foxx-service');
const response = await myFoxxService.post('users', {
  username: 'admin',
  password: 'hunter2'
});
// response.body is the result of
// POST /_db/_system/my-foxx-service/users
// with JSON request body '{"username": "admin", "password": "hunter2"}'
```

## Cursor API

_Cursor_ instances provide an abstraction over the HTTP API's limitations.
Unless a method explicitly exhausts the cursor, the driver will only fetch as
many batches from the server as necessary. Like the server-side cursors,
_Cursor_ instances are incrementally depleted as they are read from.

```js
const db = new Database();
const cursor = await db.query('FOR x IN 1..5 RETURN x');
// query result list: [1, 2, 3, 4, 5]
const value = await cursor.next();
assert.equal(value, 1);
// remaining result list: [2, 3, 4, 5]
```

### cursor.count

`cursor.count: number`

The total number of documents in the query result. This is only available if the
`count` option was used.

### cursor.all

`async cursor.all(): Array<Object>`

Exhausts the cursor, then returns an array containing all values in the cursor's
remaining result list.

**Examples**

```js
const cursor = await db._query('FOR x IN 1..5 RETURN x');
const result = await cursor.all()
// result is an array containing the entire query result
assert.deepEqual(result, [1, 2, 3, 4, 5]);
assert.equal(cursor.hasNext(), false);
```

### cursor.next

`async cursor.next(): Object`

Advances the cursor and returns the next value in the cursor's remaining result
list. If the cursor has already been exhausted, returns `undefined` instead.

**Examples**

```js
// query result list: [1, 2, 3, 4, 5]
const val = await cursor.next();
assert.equal(val, 1);
// remaining result list: [2, 3, 4, 5]

const val2 = await cursor.next();
assert.equal(val2, 2);
// remaining result list: [3, 4, 5]
```

### cursor.hasNext

`cursor.hasNext(): boolean`

Returns `true` if the cursor has more values or `false` if the cursor has been
exhausted.

**Examples**

```js
await cursor.all(); // exhausts the cursor
assert.equal(cursor.hasNext(), false);
```

### cursor.each

`async cursor.each(fn): any`

Advances the cursor by applying the function _fn_ to each value in the cursor's
remaining result list until the cursor is exhausted or _fn_ explicitly returns
`false`.

Returns the last return value of _fn_.

Equivalent to _Array.prototype.forEach_ (except async).

**Arguments**

* **fn**: `Function`

  A function that will be invoked for each value in the cursor's remaining
  result list until it explicitly returns `false` or the cursor is exhausted.

  The function receives the following arguments:

  * **value**: `any`

    The value in the cursor's remaining result list.

  * **index**: `number`

    The index of the value in the cursor's remaining result list.

  * **cursor**: `Cursor`

    The cursor itself.

**Examples**

```js
const results = [];
function doStuff(value) {
  const VALUE = value.toUpperCase();
  results.push(VALUE);
  return VALUE;
}

const cursor = await db.query('FOR x IN ["a", "b", "c"] RETURN x')
const last = await cursor.each(doStuff);
assert.deepEqual(results, ['A', 'B', 'C']);
assert.equal(cursor.hasNext(), false);
assert.equal(last, 'C');
```

### cursor.every

`async cursor.every(fn): boolean`

Advances the cursor by applying the function _fn_ to each value in the cursor's
remaining result list until the cursor is exhausted or _fn_ returns a value that
evaluates to `false`.

Returns `false` if _fn_ returned a value that evaluates to `false`, or `true`
otherwise.

Equivalent to _Array.prototype.every_ (except async).

**Arguments**

* **fn**: `Function`

  A function that will be invoked for each value in the cursor's remaining
  result list until it returns a value that evaluates to `false` or the cursor
  is exhausted.

  The function receives the following arguments:

  * **value**: `any`

    The value in the cursor's remaining result list.

  * **index**: `number`

    The index of the value in the cursor's remaining result list.

  * **cursor**: `Cursor`

    The cursor itself.

```js
const even = value => value % 2 === 0;

const cursor = await db.query('FOR x IN 2..5 RETURN x');
const result = await cursor.every(even);
assert.equal(result, false); // 3 is not even
assert.equal(cursor.hasNext(), true);

const value = await cursor.next();
assert.equal(value, 4); // next value after 3
```

### cursor.some

`async cursor.some(fn): boolean`

Advances the cursor by applying the function _fn_ to each value in the cursor's
remaining result list until the cursor is exhausted or _fn_ returns a value that
evaluates to `true`.

Returns `true` if _fn_ returned a value that evalutes to `true`, or `false`
otherwise.

Equivalent to _Array.prototype.some_ (except async).

**Examples**

```js
const even = value => value % 2 === 0;

const cursor = await db.query('FOR x IN 1..5 RETURN x');
const result = await cursor.some(even);
assert.equal(result, true); // 2 is even
assert.equal(cursor.hasNext(), true);

const value = await cursor.next();
assert.equal(value, 3); // next value after 2
```

### cursor.map

`cursor.map(fn): Array<any>`

Advances the cursor by applying the function _fn_ to each value in the cursor's
remaining result list until the cursor is exhausted.

Returns an array of the return values of _fn_.

Equivalent to _Array.prototype.map_ (except async).

**Note**: This creates an array of all return values. It is probably a bad idea
to do this for very large query result sets.

**Arguments**

* **fn**: `Function`

  A function that will be invoked for each value in the cursor's remaining
  result list until the cursor is exhausted.

  The function receives the following arguments:

  * **value**: `any`

    The value in the cursor's remaining result list.

  * **index**: `number`

    The index of the value in the cursor's remaining result list.

  * **cursor**: `Cursor`

    The cursor itself.

**Examples**

```js
const square = value => value * value;
const cursor = await db.query('FOR x IN 1..5 RETURN x');
const result = await cursor.map(square);
assert.equal(result.length, 5);
assert.deepEqual(result, [1, 4, 9, 16, 25]);
assert.equal(cursor.hasNext(), false);
```

### cursor.reduce

`cursor.reduce(fn, [accu]): any`

Exhausts the cursor by reducing the values in the cursor's remaining result list
with the given function _fn_. If _accu_ is not provided, the first value in the
cursor's remaining result list will be used instead (the function will not be
invoked for that value).

Equivalent to _Array.prototype.reduce_ (except async).

**Arguments**

* **fn**: `Function`

  A function that will be invoked for each value in the cursor's remaining
  result list until the cursor is exhausted.

  The function receives the following arguments:

  * **accu**: `any`

    The return value of the previous call to _fn_. If this is the first call,
    _accu_ will be set to the _accu_ value passed to _reduce_ or the first value
    in the cursor's remaining result list.

  * **value**: `any`

    The value in the cursor's remaining result list.

  * **index**: `number`

    The index of the value in the cursor's remaining result list.

  * **cursor**: `Cursor`

    The cursor itself.

**Examples**

```js
const add = (a, b) => a + b;
const baseline = 1000;

const cursor = await db.query('FOR x IN 1..5 RETURN x');
const result = await cursor.reduce(add, baseline)
assert.equal(result, baseline + 1 + 2 + 3 + 4 + 5);
assert.equal(cursor.hasNext(), false);

// -- or --

const result = await cursor.reduce(add);
assert.equal(result, 1 + 2 + 3 + 4 + 5);
assert.equal(cursor.hasNext(), false);
```

## Route API

_Route_ instances provide access for arbitrary HTTP requests. This allows easy
access to Foxx services and other HTTP APIs not covered by the driver itself.

### route.route

`route.route([path], [headers]): Route`

Returns a new _Route_ instance for the given path (relative to the current
route) that can be used to perform arbitrary HTTP requests.

**Arguments**

* **path**: `string` (optional)

  The relative URL of the route.

* **headers**: `Object` (optional)

  Default headers that should be sent with each request to the route.

If _path_ is missing, the route will refer to the base URL of the database.

**Examples**

```js
const db = new Database();
const route = db.route("my-foxx-service");
const users = route.route("users");
// equivalent to db.route('my-foxx-service/users')
```

### route.get

`async route.get([path,] [qs]): Response`

Performs a GET request to the given URL and returns the server response.

**Arguments**

* **path**: `string` (optional)

  The route-relative URL for the request. If omitted, the request will be made
  to the base URL of the route.

* **qs**: `string` (optional)

  The query string for the request. If _qs_ is an object, it will be translated
  to a query string.

**Examples**

```js
const db = new Database();
const route = db.route('my-foxx-service');
const response = await route.get();
// response.body is the response body of calling
// GET _db/_system/my-foxx-service

// -- or --

const response = await route.get('users');
// response.body is the response body of calling
// GET _db/_system/my-foxx-service/users

// -- or --

const response = await route.get('users', {group: 'admin'});
// response.body is the response body of calling
// GET _db/_system/my-foxx-service/users?group=admin
```

### route.post

`async route.post([path,] [body, [qs]]): Response`

Performs a POST request to the given URL and returns the server response.

**Arguments**

* **path**: `string` (optional)

  The route-relative URL for the request. If omitted, the request will be made
  to the base URL of the route.

* **body**: `string` (optional)

  The response body. If _body_ is an object, it will be encoded as JSON.

* **qs**: `string` (optional)

  The query string for the request. If _qs_ is an object, it will be translated
  to a query string.

**Examples**

```js
const db = new Database();
const route = db.route('my-foxx-service');
const response = await route.post()
// response.body is the response body of calling
// POST _db/_system/my-foxx-service

// -- or --

const response = await route.post('users')
// response.body is the response body of calling
// POST _db/_system/my-foxx-service/users

// -- or --

const response = await route.post('users', {
  username: 'admin',
  password: 'hunter2'
});
// response.body is the response body of calling
// POST _db/_system/my-foxx-service/users
// with JSON request body {"username": "admin", "password": "hunter2"}

// -- or --

const response = await route.post('users', {
  username: 'admin',
  password: 'hunter2'
}, {admin: true});
// response.body is the response body of calling
// POST _db/_system/my-foxx-service/users?admin=true
// with JSON request body {"username": "admin", "password": "hunter2"}
```

### route.put

`async route.put([path,] [body, [qs]]): Response`

Performs a PUT request to the given URL and returns the server response.

**Arguments**

* **path**: `string` (optional)

  The route-relative URL for the request. If omitted, the request will be made
  to the base URL of the route.

* **body**: `string` (optional)

  The response body. If _body_ is an object, it will be encoded as JSON.

* **qs**: `string` (optional)

  The query string for the request. If _qs_ is an object, it will be translated
  to a query string.

**Examples**

```js
const db = new Database();
const route = db.route('my-foxx-service');
const response = await route.put();
// response.body is the response body of calling
// PUT _db/_system/my-foxx-service

// -- or --

const response = await route.put('users/admin');
// response.body is the response body of calling
// PUT _db/_system/my-foxx-service/users

// -- or --

const response = await route.put('users/admin', {
  username: 'admin',
  password: 'hunter2'
});
// response.body is the response body of calling
// PUT _db/_system/my-foxx-service/users/admin
// with JSON request body {"username": "admin", "password": "hunter2"}

// -- or --

const response = await route.put('users/admin', {
  username: 'admin',
  password: 'hunter2'
}, {admin: true});
// response.body is the response body of calling
// PUT _db/_system/my-foxx-service/users/admin?admin=true
// with JSON request body {"username": "admin", "password": "hunter2"}
```

### route.patch

`async route.patch([path,] [body, [qs]]): Response`

Performs a PATCH request to the given URL and returns the server response.

**Arguments**

* **path**: `string` (optional)

  The route-relative URL for the request. If omitted, the request will be made
  to the base URL of the route.

* **body**: `string` (optional)

  The response body. If _body_ is an object, it will be encoded as JSON.

* **qs**: `string` (optional)

  The query string for the request. If _qs_ is an object, it will be translated
  to a query string.

**Examples**

```js
const db = new Database();
const route = db.route('my-foxx-service');
const response = await route.patch();
// response.body is the response body of calling
// PATCH _db/_system/my-foxx-service

// -- or --

const response = await route.patch('users/admin');
// response.body is the response body of calling
// PATCH _db/_system/my-foxx-service/users

// -- or --

const response = await route.patch('users/admin', {
  password: 'hunter2'
});
// response.body is the response body of calling
// PATCH _db/_system/my-foxx-service/users/admin
// with JSON request body {"password": "hunter2"}

// -- or --

const response = await route.patch('users/admin', {
  password: 'hunter2'
}, {admin: true});
// response.body is the response body of calling
// PATCH _db/_system/my-foxx-service/users/admin?admin=true
// with JSON request body {"password": "hunter2"}
```

### route.delete

`async route.delete([path,] [qs]): Response`

Performs a DELETE request to the given URL and returns the server response.

**Arguments**

* **path**: `string` (optional)

  The route-relative URL for the request. If omitted, the request will be made
  to the base URL of the route.

* **qs**: `string` (optional)

  The query string for the request. If _qs_ is an object, it will be translated
  to a query string.

**Examples**

```js
const db = new Database();
const route = db.route('my-foxx-service');
const response = await route.delete()
// response.body is the response body of calling
// DELETE _db/_system/my-foxx-service

// -- or --

const response = await route.delete('users/admin')
// response.body is the response body of calling
// DELETE _db/_system/my-foxx-service/users/admin

// -- or --

const response = await route.delete('users/admin', {permanent: true})
// response.body is the response body of calling
// DELETE _db/_system/my-foxx-service/users/admin?permanent=true
```

### route.head

`async route.head([path,] [qs]): Response`

Performs a HEAD request to the given URL and returns the server response.

**Arguments**

* **path**: `string` (optional)

  The route-relative URL for the request. If omitted, the request will be made
  to the base URL of the route.

* **qs**: `string` (optional)

  The query string for the request. If _qs_ is an object, it will be translated
  to a query string.

**Examples**

```js
const db = new Database();
const route = db.route('my-foxx-service');
const response = await route.head();
// response is the response object for
// HEAD _db/_system/my-foxx-service
```

### route.request

`async route.request([opts]): Response`

Performs an arbitrary request to the given URL and returns the server response.

**Arguments**

* **opts**: `Object` (optional)

  An object with any of the following properties:

  * **path**: `string` (optional)

    The route-relative URL for the request. If omitted, the request will be made
    to the base URL of the route.

  * **absolutePath**: `boolean` (Default: `false`)

    Whether the _path_ is relative to the connection's base URL instead of the
    route.

  * **body**: `string` (optional)

    The response body. If _body_ is an object, it will be encoded as JSON.

  * **qs**: `string` (optional)

    The query string for the request. If _qs_ is an object, it will be
    translated to a query string.

  * **headers**: `Object` (optional)

    An object containing additional HTTP headers to be sent with the request.

  * **method**: `string` (Default: `"GET"`)

    HTTP method of this request.

**Examples**

```js
const db = new Database();
const route = db.route('my-foxx-service');
const response = await route.request({
  path: 'hello-world',
  method: 'POST',
  body: {hello: 'world'},
  qs: {admin: true}
});
// response.body is the response body of calling
// POST _db/_system/my-foxx-service/hello-world?admin=true
// with JSON request body '{"hello": "world"}'
```

## Collection API

These functions implement the
[HTTP API for manipulating collections](https://docs.arangodb.com/latest/HTTP/Collection/index.html).

The _Collection API_ is implemented by all _Collection_ instances, regardless of
their specific type. I.e. it represents a shared subset between instances of
[_DocumentCollection_](#documentcollection-api),
[_EdgeCollection_](#edgecollection-api),
[_GraphVertexCollection_](#graphvertexcollection-api) and
[_GraphEdgeCollection_](#graphedgecollection-api).

### Getting information about the collection

See
[the HTTP API documentation](https://docs.arangodb.com/latest/HTTP/Collection/Getting.html)
for details.

#### collection.get

`async collection.get(): Object`

Retrieves general information about the collection.

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');
const data = await collection.get();
// data contains general information about the collection
```

#### collection.properties

`async collection.properties(): Object`

Retrieves the collection's properties.

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');
const data = await collection.properties();
// data contains the collection's properties
```

#### collection.count

`async collection.count(): Object`

Retrieves information about the number of documents in a collection.

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');
const data = await collection.count();
// data contains the collection's count
```

#### collection.figures

`async collection.figures(): Object`

Retrieves statistics for a collection.

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');
const data = await collection.figures();
// data contains the collection's figures
```

#### collection.revision

`async collection.revision(): Object`

Retrieves the collection revision ID.

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');
const data = await collection.revision();
// data contains the collection's revision
```

#### collection.checksum

`async collection.checksum([opts]): Object`

Retrieves the collection checksum.

**Arguments**

* **opts**: `Object` (optional)

  For information on the possible options see
  [the HTTP API for getting collection information](https://docs.arangodb.com/latest/HTTP/Collection/Getting.html).

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');
const data = await collection.checksum();
// data contains the collection's checksum
```

### Manipulating the collection

These functions implement
[the HTTP API for modifying collections](https://docs.arangodb.com/latest/HTTP/Collection/Modifying.html).

#### collection.create

`async collection.create([properties]): Object`

Creates a collection with the given _properties_ for this collection's name,
then returns the server response.

**Arguments**

* **properties**: `Object` (optional)

  For more information on the _properties_ object, see
  [the HTTP API documentation for creating collections](https://docs.arangodb.com/latest/HTTP/Collection/Creating.html).

**Examples**

```js
const db = new Database();
const collection = db.collection('potatos');
await collection.create()
// the document collection "potatos" now exists

// -- or --

const collection = db.edgeCollection('friends');
await collection.create({
  waitForSync: true // always sync document changes to disk
});
// the edge collection "friends" now exists
```

#### collection.load

`async collection.load([count]): Object`

Tells the server to load the collection into memory.

**Arguments**

* **count**: `boolean` (Default: `true`)

  If set to `false`, the return value will not include the number of documents
  in the collection (which may speed up the process).

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');
await collection.load(false)
// the collection has now been loaded into memory
```

#### collection.unload

`async collection.unload(): Object`

Tells the server to remove the collection from memory.

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');
await collection.unload()
// the collection has now been unloaded from memory
```

#### collection.setProperties

`async collection.setProperties(properties): Object`

Replaces the properties of the collection.

**Arguments**

* **properties**: `Object`

  For information on the _properties_ argument see
  [the HTTP API for modifying collections](https://docs.arangodb.com/latest/HTTP/Collection/Modifying.html).

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');
const result = await collection.setProperties({waitForSync: true})
assert.equal(result.waitForSync, true);
// the collection will now wait for data being written to disk
// whenever a document is changed
```

#### collection.rename

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

#### collection.rotate

`async collection.rotate(): Object`

Rotates the journal of the collection.

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');
const data = await collection.rotate();
// data.result will be true if rotation succeeded
```

#### collection.truncate

`async collection.truncate(): Object`

Deletes **all documents** in the collection in the database.

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');
await collection.truncate();
// the collection "some-collection" is now empty
```

#### collection.drop

`async collection.drop([properties]): Object`

Deletes the collection from the database.

**Arguments**

* **properties**: `Object` (optional)

  An object with the following properties:

  * **isSystem**: `Boolean` (Default: `false`)

    Whether the collection should be dropped even if it is a system collection.

    This parameter must be set to `true` when dropping a system collection.

  For more information on the _properties_ object, see
  [the HTTP API documentation for dropping collections](https://docs.arangodb.com/3/HTTP/Collection/Creating.html#drops-a-collection).
  **Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');
await collection.drop();
// the collection "some-collection" no longer exists
```

### Manipulating indexes

These functions implement the
[HTTP API for manipulating indexes](https://docs.arangodb.com/latest/HTTP/Indexes/index.html).

#### collection.createIndex

`async collection.createIndex(details): Object`

Creates an arbitrary index on the collection.

**Arguments**

* **details**: `Object`

  For information on the possible properties of the _details_ object, see
  [the HTTP API for manipulating indexes](https://docs.arangodb.com/latest/HTTP/Indexes/WorkingWith.html).

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');
const index = await collection.createIndex({type: 'cap', size: 20});
// the index has been created with the handle `index.id`
```

#### collection.createCapConstraint

`async collection.createCapConstraint(size): Object`

Creates a cap constraint index on the collection.

**Note**: This method is not available when using the driver with ArangoDB 3.0
and higher as cap constraints are no longer supported.

**Arguments**

* **size**: `Object`

  An object with any of the following properties:

  * **size**: `number` (optional)

    The maximum number of documents in the collection.

  * **byteSize**: `number` (optional)

    The maximum size of active document data in the collection (in bytes).

If _size_ is a number, it will be interpreted as _size.size_.

For more information on the properties of the _size_ object see
[the HTTP API for creating cap constraints](https://docs.arangodb.com/latest/HTTP/Indexes/Cap.html).

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');

const index = await collection.createCapConstraint(20)
// the index has been created with the handle `index.id`
assert.equal(index.size, 20);

// -- or --

const index = await collection.createCapConstraint({size: 20})
// the index has been created with the handle `index.id`
assert.equal(index.size, 20);
```

#### collection.createHashIndex

`async collection.createHashIndex(fields, [opts]): Object`

Creates a hash index on the collection.

**Arguments**

* **fields**: `Array<string>`

  An array of names of document fields on which to create the index. If the
  value is a string, it will be wrapped in an array automatically.

* **opts**: `Object` (optional)

  Additional options for this index. If the value is a boolean, it will be
  interpreted as _opts.unique_.

For more information on hash indexes, see
[the HTTP API for hash indexes](https://docs.arangodb.com/latest/HTTP/Indexes/Hash.html).

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');

const index = await collection.createHashIndex('favorite-color');
// the index has been created with the handle `index.id`
assert.deepEqual(index.fields, ['favorite-color']);

// -- or --

const index = await collection.createHashIndex(['favorite-color']);
// the index has been created with the handle `index.id`
assert.deepEqual(index.fields, ['favorite-color']);
```

#### collection.createSkipList

`async collection.createSkipList(fields, [opts]): Object`

Creates a skiplist index on the collection.

**Arguments**

* **fields**: `Array<string>`

  An array of names of document fields on which to create the index. If the
  value is a string, it will be wrapped in an array automatically.

* **opts**: `Object` (optional)

  Additional options for this index. If the value is a boolean, it will be
  interpreted as _opts.unique_.

For more information on skiplist indexes, see
[the HTTP API for skiplist indexes](https://docs.arangodb.com/latest/HTTP/Indexes/Skiplist.html).

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');

const index = await collection.createSkipList('favorite-color')
// the index has been created with the handle `index.id`
assert.deepEqual(index.fields, ['favorite-color']);

// -- or --

const index = await collection.createSkipList(['favorite-color'])
// the index has been created with the handle `index.id`
assert.deepEqual(index.fields, ['favorite-color']);
```

#### collection.createGeoIndex

`async collection.createGeoIndex(fields, [opts]): Object`

Creates a geo-spatial index on the collection.

**Arguments**

* **fields**: `Array<string>`

  An array of names of document fields on which to create the index. Currently,
  geo indexes must cover exactly one field. If the value is a string, it will be
  wrapped in an array automatically.

* **opts**: `Object` (optional)

  An object containing additional properties of the index.

For more information on the properties of the _opts_ object see
[the HTTP API for manipulating geo indexes](https://docs.arangodb.com/latest/HTTP/Indexes/Geo.html).

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');

const index = await collection.createGeoIndex(['latitude', 'longitude']);
// the index has been created with the handle `index.id`
assert.deepEqual(index.fields, ['longitude', 'latitude']);

// -- or --

const index = await collection.createGeoIndex('location', {geoJson: true});
// the index has been created with the handle `index.id`
assert.deepEqual(index.fields, ['location']);
```

#### collection.createFulltextIndex

`async collection.createFulltextIndex(fields, [minLength]): Object`

Creates a fulltext index on the collection.

**Arguments**

* **fields**: `Array<string>`

  An array of names of document fields on which to create the index. Currently,
  fulltext indexes must cover exactly one field. If the value is a string, it
  will be wrapped in an array automatically.

* **minLength** (optional):

  Minimum character length of words to index. Uses a server-specific default
  value if not specified.

For more information on fulltext indexes, see
[the HTTP API for fulltext indexes](https://docs.arangodb.com/latest/HTTP/Indexes/Fulltext.html).

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');

const index = await collection.createFulltextIndex('description');
// the index has been created with the handle `index.id`
assert.deepEqual(index.fields, ['description']);

// -- or --

const index = await collection.createFulltextIndex(['description']);
// the index has been created with the handle `index.id`
assert.deepEqual(index.fields, ['description']);
```

#### collection.createPersistentIndex

`async collection.createPersistentIndex(fields, [opts]): Object`

Creates a Persistent index on the collection. Persistent indexes are similarly
in operation to skiplist indexes, only that these indexes are in disk as opposed
to in memory. This reduces memory usage and DB startup time, with the trade-off
being that it will always be orders of magnitude slower than in-memory indexes.

**Arguments**

* **fields**: `Array<string>`

  An array of names of document fields on which to create the index.

* **opts**: `Object` (optional)

  An object containing additional properties of the index.

For more information on the properties of the _opts_ object see
[the HTTP API for manipulating Persistent indexes](https://docs.arangodb.com/latest/HTTP/Indexes/Persistent.html).

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');

const index = await collection.createPersistentIndex(['name', 'email']);
// the index has been created with the handle `index.id`
assert.deepEqual(index.fields, ['name', 'email']);
```

#### collection.index

`async collection.index(indexHandle): Object`

Fetches information about the index with the given _indexHandle_ and returns it.

**Arguments**

* **indexHandle**: `string`

  The handle of the index to look up. This can either be a fully-qualified
  identifier or the collection-specific key of the index. If the value is an
  object, its _id_ property will be used instead.

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');
const index = await collection.createFulltextIndex('description');
const result = await collection.index(index.id);
assert.equal(result.id, index.id);
// result contains the properties of the index

// -- or --

const result = await collection.index(index.id.split('/')[1]);
assert.equal(result.id, index.id);
// result contains the properties of the index
```

#### collection.indexes

`async collection.indexes(): Array<Object>`

Fetches a list of all indexes on this collection.

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');
await collection.createFulltextIndex('description')
const indexes = await collection.indexes();
assert.equal(indexes.length, 1);
// indexes contains information about the index
```

#### collection.dropIndex

`async collection.dropIndex(indexHandle): Object`

Deletes the index with the given _indexHandle_ from the collection.

**Arguments**

* **indexHandle**: `string`

  The handle of the index to delete. This can either be a fully-qualified
  identifier or the collection-specific key of the index. If the value is an
  object, its _id_ property will be used instead.

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');
const index = await collection.createFulltextIndex('description');
await collection.dropIndex(index.id);
// the index has been removed from the collection

// -- or --

await collection.dropIndex(index.id.split('/')[1]);
// the index has been removed from the collection
```

### Simple queries

These functions implement the
[HTTP API for simple queries](https://docs.arangodb.com/latest/HTTP/SimpleQuery/index.html).

#### collection.all

`async collection.all([opts]): Cursor`

Performs a query to fetch all documents in the collection. Returns a
[new _Cursor_ instance](#cursor-api) for the query results.

**Arguments**

* **opts**: `Object` (optional)

  For information on the possible options see
  [the HTTP API for returning all documents](https://docs.arangodb.com/latest/HTTP/SimpleQuery/index.html#return-all-documents).

#### collection.any

`async collection.any(): Object`

Fetches a document from the collection at random.

#### collection.first

`async collection.first([opts]): Array<Object>`

Performs a query to fetch the first documents in the collection. Returns an
array of the matching documents.

**Note**: This method is not available when using the driver with ArangoDB 3.0
and higher as the corresponding API method has been removed.

**Arguments**

* **opts**: `Object` (optional)

  For information on the possible options see
  [the HTTP API for returning the first documents of a collection](https://docs.arangodb.com/latest/HTTP/SimpleQuery/index.html#find-documents-matching-an-example).

  If _opts_ is a number it is treated as _opts.count_.

#### collection.last

`async collection.last([opts]): Array<Object>`

Performs a query to fetch the last documents in the collection. Returns an array
of the matching documents.

**Note**: This method is not available when using the driver with ArangoDB 3.0
and higher as the corresponding API method has been removed.

**Arguments**

* **opts**: `Object` (optional)

  For information on the possible options see
  [the HTTP API for returning the last documents of a collection](https://docs.arangodb.com/latest/HTTP/SimpleQuery/index.html#last-document-of-a-collection).

  If _opts_ is a number it is treated as _opts.count_.

#### collection.byExample

`async collection.byExample(example, [opts]): Cursor`

Performs a query to fetch all documents in the collection matching the given
_example_. Returns a [new _Cursor_ instance](#cursor-api) for the query results.

**Arguments**

* **example**: _Object_

  An object representing an example for documents to be matched against.

* **opts**: _Object_ (optional)

  For information on the possible options see
  [the HTTP API for fetching documents by example](https://docs.arangodb.com/latest/HTTP/SimpleQuery/index.html#find-documents-matching-an-example).

#### collection.firstExample

`async collection.firstExample(example): Object`

Fetches the first document in the collection matching the given _example_.

**Arguments**

* **example**: _Object_

  An object representing an example for documents to be matched against.

#### collection.removeByExample

`async collection.removeByExample(example, [opts]): Object`

Removes all documents in the collection matching the given _example_.

**Arguments**

* **example**: _Object_

  An object representing an example for documents to be matched against.

* **opts**: _Object_ (optional)

  For information on the possible options see
  [the HTTP API for removing documents by example](https://docs.arangodb.com/latest/HTTP/SimpleQuery/index.html#remove-documents-by-example).

#### collection.replaceByExample

`async collection.replaceByExample(example, newValue, [opts]): Object`

Replaces all documents in the collection matching the given _example_ with the
given _newValue_.

**Arguments**

* **example**: _Object_

  An object representing an example for documents to be matched against.

* **newValue**: _Object_

  The new value to replace matching documents with.

* **opts**: _Object_ (optional)

  For information on the possible options see
  [the HTTP API for replacing documents by example](https://docs.arangodb.com/latest/HTTP/SimpleQuery/index.html#replace-documents-by-example).

#### collection.updateByExample

`async collection.updateByExample(example, newValue, [opts]): Object`

Updates (patches) all documents in the collection matching the given _example_
with the given _newValue_.

**Arguments**

* **example**: _Object_

  An object representing an example for documents to be matched against.

* **newValue**: _Object_

  The new value to update matching documents with.

* **opts**: _Object_ (optional)

  For information on the possible options see
  [the HTTP API for updating documents by example](https://docs.arangodb.com/latest/HTTP/SimpleQuery/index.html#update-documents-by-example).

#### collection.lookupByKeys

`async collection.lookupByKeys(keys): Array<Object>`

Fetches the documents with the given _keys_ from the collection. Returns an
array of the matching documents.

**Arguments**

* **keys**: _Array_

  An array of document keys to look up.

#### collection.removeByKeys

`async collection.removeByKeys(keys, [opts]): Object`

Deletes the documents with the given _keys_ from the collection.

**Arguments**

* **keys**: _Array_

  An array of document keys to delete.

* **opts**: _Object_ (optional)

  For information on the possible options see
  [the HTTP API for removing documents by keys](https://docs.arangodb.com/latest/HTTP/SimpleQuery/index.html#remove-documents-by-their-keys).

#### collection.fulltext

`async collection.fulltext(fieldName, query, [opts]): Cursor`

Performs a fulltext query in the given _fieldName_ on the collection.

**Arguments**

* **fieldName**: _String_

  Name of the field to search on documents in the collection.

* **query**: _String_

  Fulltext query string to search for.

* **opts**: _Object_ (optional)

  For information on the possible options see
  [the HTTP API for fulltext queries](https://docs.arangodb.com/latest/HTTP/Indexes/Fulltext.html).

### Bulk importing documents

This function implements the
[HTTP API for bulk imports](https://docs.arangodb.com/latest/HTTP/BulkImports/index.html).

#### collection.import

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
[the HTTP API documentation for bulk imports](https://docs.arangodb.com/latest/HTTP/BulkImports/ImportingSelfContained.html).

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

### Manipulating documents

These functions implement the
[HTTP API for manipulating documents](https://docs.arangodb.com/latest/HTTP/Document/index.html).

#### collection.replace

`async collection.replace(documentHandle, newValue, [opts]): Object`

Replaces the content of the document with the given _documentHandle_ with the
given _newValue_ and returns an object containing the document's metadata.

**Note**: The _policy_ option is not available when using the driver with
ArangoDB 3.0 as it is redundant when specifying the _rev_ option.

**Arguments**

* **documentHandle**: `string`

  The handle of the document to replace. This can either be the `_id` or the
  `_key` of a document in the collection, or a document (i.e. an object with an
  `_id` or `_key` property).

* **newValue**: `Object`

  The new data of the document.

* **opts**: `Object` (optional)

  If _opts_ is set, it must be an object with any of the following properties:

  * **waitForSync**: `boolean` (Default: `false`)

    Wait until the document has been synced to disk. Default: `false`.

  * **rev**: `string` (optional)

    Only replace the document if it matches this revision.

  * **policy**: `string` (optional)

    Determines the behaviour when the revision is not matched:

    * if _policy_ is set to `"last"`, the document will be replaced regardless
      of the revision.
    * if _policy_ is set to `"error"` or not set, the replacement will fail with
      an error.

If a string is passed instead of an options object, it will be interpreted as
the _rev_ option.

For more information on the _opts_ object, see
[the HTTP API documentation for working with documents](https://docs.arangodb.com/latest/HTTP/Document/WorkingWithDocuments.html).

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');
const data = {number: 1, hello: 'world'};
const info1 = await collection.save(data);
const info2 = await collection.replace(info1, {number: 2});
assert.equal(info2._id, info1._id);
assert.notEqual(info2._rev, info1._rev);
const doc = await collection.document(info1);
assert.equal(doc._id, info1._id);
assert.equal(doc._rev, info2._rev);
assert.equal(doc.number, 2);
assert.equal(doc.hello, undefined);
```

#### collection.update

`async collection.update(documentHandle, newValue, [opts]): Object`

Updates (merges) the content of the document with the given _documentHandle_
with the given _newValue_ and returns an object containing the document's
metadata.

**Note**: The _policy_ option is not available when using the driver with
ArangoDB 3.0 as it is redundant when specifying the _rev_ option.

**Arguments**

* **documentHandle**: `string`

  Handle of the document to update. This can be either the `_id` or the `_key`
  of a document in the collection, or a document (i.e. an object with an `_id`
  or `_key` property).

* **newValue**: `Object`

  The new data of the document.

* **opts**: `Object` (optional)

  If _opts_ is set, it must be an object with any of the following properties:

  * **waitForSync**: `boolean` (Default: `false`)

    Wait until document has been synced to disk.

  * **keepNull**: `boolean` (Default: `true`)

    If set to `false`, properties with a value of `null` indicate that a
    property should be deleted.

  * **mergeObjects**: `boolean` (Default: `true`)

    If set to `false`, object properties that already exist in the old document
    will be overwritten rather than merged. This does not affect arrays.

  * **returnOld**: `boolean` (Default: `false`)

    If set to `true`, return additionally the complete previous revision of the
    changed documents under the attribute `old` in the result.

  * **returnNew**: `boolean` (Default: `false`)

    If set to `true`, return additionally the complete new documents under the
    attribute `new` in the result.

  * **ignoreRevs**: `boolean` (Default: `true`)

    By default, or if this is set to true, the _rev attributes in the given
    documents are ignored. If this is set to false, then any _rev attribute
    given in a body document is taken as a precondition. The document is only
    updated if the current revision is the one specified.

  * **rev**: `string` (optional)

    Only update the document if it matches this revision.

  * **policy**: `string` (optional)

    Determines the behaviour when the revision is not matched:

    * if _policy_ is set to `"last"`, the document will be replaced regardless
      of the revision.
    * if _policy_ is set to `"error"` or not set, the replacement will fail with
      an error.

If a string is passed instead of an options object, it will be interpreted as
the _rev_ option.

For more information on the _opts_ object, see
[the HTTP API documentation for working with documents](https://docs.arangodb.com/latest/HTTP/Document/WorkingWithDocuments.html).

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');
const doc = {number: 1, hello: 'world'};
const doc1 = await collection.save(doc);
const doc2 = await collection.update(doc1, {number: 2});
assert.equal(doc2._id, doc1._id);
assert.notEqual(doc2._rev, doc1._rev);
const doc3 = await collection.document(doc2);
assert.equal(doc3._id, doc2._id);
assert.equal(doc3._rev, doc2._rev);
assert.equal(doc3.number, 2);
assert.equal(doc3.hello, doc.hello);
```

#### collection.bulkUpdate

`async collection.bulkUpdate(documents, [opts]): Object`

Updates (merges) the content of the documents with the given _documents_ and
returns an array containing the documents' metadata.

**Note**: This method is new in 3.0 and is available when using the driver with
ArangoDB 3.0 and higher.

**Arguments**

* **documents**: `Array<Object>`

  Documents to update. Each object must have either the `_id` or the `_key`
  property.

* **opts**: `Object` (optional)

  If _opts_ is set, it must be an object with any of the following properties:

  * **waitForSync**: `boolean` (Default: `false`)

    Wait until document has been synced to disk.

  * **keepNull**: `boolean` (Default: `true`)

    If set to `false`, properties with a value of `null` indicate that a
    property should be deleted.

  * **mergeObjects**: `boolean` (Default: `true`)

    If set to `false`, object properties that already exist in the old document
    will be overwritten rather than merged. This does not affect arrays.

  * **returnOld**: `boolean` (Default: `false`)

    If set to `true`, return additionally the complete previous revision of the
    changed documents under the attribute `old` in the result.

  * **returnNew**: `boolean` (Default: `false`)

    If set to `true`, return additionally the complete new documents under the
    attribute `new` in the result.

  * **ignoreRevs**: `boolean` (Default: `true`)

    By default, or if this is set to true, the _rev attributes in the given
    documents are ignored. If this is set to false, then any _rev attribute
    given in a body document is taken as a precondition. The document is only
    updated if the current revision is the one specified.

For more information on the _opts_ object, see
[the HTTP API documentation for working with documents](https://docs.arangodb.com/latest/HTTP/Document/WorkingWithDocuments.html).

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');
const doc1 = {number: 1, hello: 'world1'};
const info1 = await collection.save(doc1);
const doc2 = {number: 2, hello: 'world2'};
const info2 = await collection.save(doc2);
const result = await collection.bulkUpdate([
  {_key: info1._key, number: 3},
  {_key: info2._key, number: 4}
], {returnNew: true})
```

#### collection.remove

`async collection.remove(documentHandle, [opts]): Object`

Deletes the document with the given _documentHandle_ from the collection.

**Note**: The _policy_ option is not available when using the driver with
ArangoDB 3.0 as it is redundant when specifying the _rev_ option.

**Arguments**

* **documentHandle**: `string`

  The handle of the document to delete. This can be either the `_id` or the
  `_key` of a document in the collection, or a document (i.e. an object with an
  `_id` or `_key` property).

* **opts**: `Object` (optional)

  If _opts_ is set, it must be an object with any of the following properties:

  * **waitForSync**: `boolean` (Default: `false`)

    Wait until document has been synced to disk.

  * **rev**: `string` (optional)

    Only update the document if it matches this revision.

  * **policy**: `string` (optional)

    Determines the behaviour when the revision is not matched:

    * if _policy_ is set to `"last"`, the document will be replaced regardless
      of the revision.
    * if _policy_ is set to `"error"` or not set, the replacement will fail with
      an error.

If a string is passed instead of an options object, it will be interpreted as
the _rev_ option.

For more information on the _opts_ object, see
[the HTTP API documentation for working with documents](https://docs.arangodb.com/latest/HTTP/Document/WorkingWithDocuments.html).

**Examples**

```js
const db = new Database();
const collection = db.collection('some-collection');

await collection.remove('some-doc');
// document 'some-collection/some-doc' no longer exists

// -- or --

await collection.remove('some-collection/some-doc');
// document 'some-collection/some-doc' no longer exists
```

#### collection.list

`async collection.list([type]): Array<string>`

Retrieves a list of references for all documents in the collection.

**Arguments**

* **type**: `string` (Default: `"id"`)

  The format of the document references:

  * if _type_ is set to `"id"`, each reference will be the `_id` of the
    document.
  * if _type_ is set to `"key"`, each reference will be the `_key` of the
    document.
  * if _type_ is set to `"path"`, each reference will be the URI path of the
    document.

## DocumentCollection API

The _DocumentCollection API_ extends the
[_Collection API_ (see above)](#collection-api) with the following methods.

### documentCollection.document

`async documentCollection.document(documentHandle): Object`

Retrieves the document with the given _documentHandle_ from the collection.

**Arguments**

* **documentHandle**: `string`

  The handle of the document to retrieve. This can be either the `_id` or the
  `_key` of a document in the collection, or a document (i.e. an object with an
  `_id` or `_key` property).

**Examples**

```js
const db = new Database();
const collection = db.collection('my-docs');

try {
  const doc = await collection.document('some-key');
  // the document exists
  assert.equal(doc._key, 'some-key');
  assert.equal(doc._id, 'my-docs/some-key');
} catch (err) {
  // something went wrong or
  // the document does not exist
}

// -- or --

try {
  const doc = await collection.document('my-docs/some-key');
  // the document exists
  assert.equal(doc._key, 'some-key');
  assert.equal(doc._id, 'my-docs/some-key');
} catch (err) {
  // something went wrong or
  // the document does not exist
}
```

### documentCollection.save

`async documentCollection.save(data, [opts]): Object`

Creates a new document with the given _data_ and returns an object containing
the document's metadata.

**Arguments**

* **data**: `Object`

  The data of the new document, may include a `_key`.

* **opts**: `Object` (optional)

  If _opts_ is set, it must be an object with any of the following properties:

  * **waitForSync**: `boolean` (Default: `false`)

    Wait until document has been synced to disk.

  * **returnNew**: `boolean` (Default: `false`)

    If set to `true`, return additionally the complete new documents under the
    attribute `new` in the result.

  * **silent**: `boolean` (Default: `false`)

    If set to true, an empty object will be returned as response. No meta-data
    will be returned for the created document. This option can be used to save
    some network traffic.

If a boolean is passed instead of an options object, it will be interpreted as
the _returnNew_ option.

For more information on the _opts_ object, see
[the HTTP API documentation for working with documents](https://docs.arangodb.com/latest/HTTP/Document/WorkingWithDocuments.html).

**Examples**

```js
const db = new Database();
const collection = db.collection('my-docs');
const data = {some: 'data'};
const info = await collection.save(data);
assert.equal(info._id, 'my-docs/' + info._key);
const doc2 = await collection.document(info)
assert.equal(doc2._id, info._id);
assert.equal(doc2._rev, info._rev);
assert.equal(doc2.some, data.some);

// -- or --

const db = new Database();
const collection = db.collection('my-docs');
const data = {some: 'data'};
const opts = {returnNew: true};
const doc = await collection.save(data, opts)
assert.equal(doc1._id, 'my-docs/' + doc1._key);
assert.equal(doc1.new.some, data.some);
```

## EdgeCollection API

The _EdgeCollection API_ extends the
[_Collection API_ (see above)](#collection-api) with the following methods.

### edgeCollection.edge

`async edgeCollection.edge(documentHandle): Object`

Retrieves the edge with the given _documentHandle_ from the collection.

**Arguments**

* **documentHandle**: `string`

  The handle of the edge to retrieve. This can be either the `_id` or the `_key`
  of an edge in the collection, or an edge (i.e. an object with an `_id` or
  `_key` property).

**Examples**

```js
const db = new Database();
const collection = db.edgeCollection('edges');

const edge = await collection.edge('some-key');
// the edge exists
assert.equal(edge._key, 'some-key');
assert.equal(edge._id, 'edges/some-key');

// -- or --

const edge = await collection.edge('edges/some-key');
// the edge exists
assert.equal(edge._key, 'some-key');
assert.equal(edge._id, 'edges/some-key');
```

### edgeCollection.save

`async edgeCollection.save(data, [fromId, toId]): Object`

Creates a new edge between the documents _fromId_ and _toId_ with the given
_data_ and returns an object containing the edge's metadata.

**Arguments**

* **data**: `Object`

  The data of the new edge. If _fromId_ and _toId_ are not specified, the _data_
  needs to contain the properties __from_ and __to_.

* **fromId**: `string` (optional)

  The handle of the start vertex of this edge. This can be either the `_id` of a
  document in the database, the `_key` of an edge in the collection, or a
  document (i.e. an object with an `_id` or `_key` property).

* **toId**: `string` (optional)

  The handle of the end vertex of this edge. This can be either the `_id` of a
  document in the database, the `_key` of an edge in the collection, or a
  document (i.e. an object with an `_id` or `_key` property).

**Examples**

```js
const db = new Database();
const collection = db.edgeCollection('edges');
const data = {some: 'data'};

const info = await collection.save(
  data,
  'vertices/start-vertex',
  'vertices/end-vertex'
);
assert.equal(info._id, 'edges/' + info._key);
const edge = await collection.edge(edge)
assert.equal(edge._key, info._key);
assert.equal(edge._rev, info._rev);
assert.equal(edge.some, data.some);
assert.equal(edge._from, 'vertices/start-vertex');
assert.equal(edge._to, 'vertices/end-vertex');

// -- or --

const info = await collection.save({
  some: 'data',
  _from: 'verticies/start-vertex',
  _to: 'vertices/end-vertex'
});
// ...
```

### edgeCollection.edges

`async edgeCollection.edges(documentHandle): Array<Object>`

Retrieves a list of all edges of the document with the given _documentHandle_.

**Arguments**

* **documentHandle**: `string`

  The handle of the document to retrieve the edges of. This can be either the
  `_id` of a document in the database, the `_key` of an edge in the collection,
  or a document (i.e. an object with an `_id` or `_key` property).

**Examples**

```js
const db = new Database();
const collection = db.edgeCollection('edges');
await collection.import([
  ['_key', '_from', '_to'],
  ['x', 'vertices/a', 'vertices/b'],
  ['y', 'vertices/a', 'vertices/c'],
  ['z', 'vertices/d', 'vertices/a']
])
const edges = await collection.edges('vertices/a');
assert.equal(edges.length, 3);
assert.deepEqual(edges.map(edge => edge._key), ['x', 'y', 'z']);
```

### edgeCollection.inEdges

`async edgeCollection.inEdges(documentHandle): Array<Object>`

Retrieves a list of all incoming edges of the document with the given
_documentHandle_.

**Arguments**

* **documentHandle**: `string`

  The handle of the document to retrieve the edges of. This can be either the
  `_id` of a document in the database, the `_key` of an edge in the collection,
  or a document (i.e. an object with an `_id` or `_key` property).

**Examples**

```js
const db = new Database();
const collection = db.edgeCollection('edges');
await collection.import([
  ['_key', '_from', '_to'],
  ['x', 'vertices/a', 'vertices/b'],
  ['y', 'vertices/a', 'vertices/c'],
  ['z', 'vertices/d', 'vertices/a']
]);
const edges = await collection.inEdges('vertices/a');
assert.equal(edges.length, 1);
assert.equal(edges[0]._key, 'z');
```

### edgeCollection.outEdges

`async edgeCollection.outEdges(documentHandle): Array<Object>`

Retrieves a list of all outgoing edges of the document with the given
_documentHandle_.

**Arguments**

* **documentHandle**: `string`

  The handle of the document to retrieve the edges of. This can be either the
  `_id` of a document in the database, the `_key` of an edge in the collection,
  or a document (i.e. an object with an `_id` or `_key` property).

**Examples**

```js
const db = new Database();
const collection = db.edgeCollection('edges');
await collection.import([
  ['_key', '_from', '_to'],
  ['x', 'vertices/a', 'vertices/b'],
  ['y', 'vertices/a', 'vertices/c'],
  ['z', 'vertices/d', 'vertices/a']
]);
const edges = await collection.outEdges('vertices/a');
assert.equal(edges.length, 2);
assert.deepEqual(edges.map(edge => edge._key), ['x', 'y']);
```

### edgeCollection.traversal

`async edgeCollection.traversal(startVertex, opts): Object`

Performs a traversal starting from the given _startVertex_ and following edges
contained in this edge collection.

**Arguments**

* **startVertex**: `string`

  The handle of the start vertex. This can be either the `_id` of a document in
  the database, the `_key` of an edge in the collection, or a document (i.e. an
  object with an `_id` or `_key` property).

* **opts**: `Object`

  See
  [the HTTP API documentation](https://docs.arangodb.com/latest/HTTP/Traversal/index.html)
  for details on the additional arguments.

  Please note that while _opts.filter_, _opts.visitor_, _opts.init_,
  _opts.expander_ and _opts.sort_ should be strings evaluating to well-formed
  JavaScript code, it's not possible to pass in JavaScript functions directly
  because the code needs to be evaluated on the server and will be transmitted
  in plain text.

**Examples**

```js
const db = new Database();
const collection = db.edgeCollection('edges');
await collection.import([
  ['_key', '_from', '_to'],
  ['x', 'vertices/a', 'vertices/b'],
  ['y', 'vertices/b', 'vertices/c'],
  ['z', 'vertices/c', 'vertices/d']
]);
const result = await collection.traversal('vertices/a', {
  direction: 'outbound',
  visitor: 'result.vertices.push(vertex._key);',
  init: 'result.vertices = [];'
});
assert.deepEqual(result.vertices, ['a', 'b', 'c', 'd']);
```

## Graph API

These functions implement the
[HTTP API for manipulating graphs](https://docs.arangodb.com/latest/HTTP/Gharial/index.html).

### graph.get

`async graph.get(): Object`

Retrieves general information about the graph.

**Examples**

```js
const db = new Database();
const graph = db.graph('some-graph');
const data = await graph.get();
// data contains general information about the graph
```

### graph.create

`async graph.create(properties): Object`

Creates a graph with the given _properties_ for this graph's name, then returns
the server response.

**Arguments**

* **properties**: `Object`

  For more information on the _properties_ object, see
  [the HTTP API documentation for creating graphs](https://docs.arangodb.com/latest/HTTP/Gharial/Management.html).

**Examples**

```js
const db = new Database();
const graph = db.graph('some-graph');
const info = await graph.create({
  edgeDefinitions: [{
    collection: 'edges',
    from: ['start-vertices'],
    to: ['end-vertices']
  }]
});
// graph now exists
```

### graph.drop

`async graph.drop([dropCollections]): Object`

Deletes the graph from the database.

**Arguments**

* **dropCollections**: `boolean` (optional)

  If set to `true`, the collections associated with the graph will also be
  deleted.

**Examples**

```js
const db = new Database();
const graph = db.graph('some-graph');
await graph.drop();
// the graph "some-graph" no longer exists
```

### Manipulating vertices

#### graph.vertexCollection

`graph.vertexCollection(collectionName): GraphVertexCollection`

Returns a new [_GraphVertexCollection_ instance](#graphvertexcollection-api)
with the given name for this graph.

**Arguments**

* **collectionName**: `string`

  Name of the vertex collection.

**Examples**

```js
const db = new Database();
const graph = db.graph("some-graph");
const collection = graph.vertexCollection("vertices");
assert.equal(collection.name, "vertices");
// collection is a GraphVertexCollection
```

#### graph.listVertexCollections

`async graph.listVertexCollections([excludeOrphans]): Array<Object>`

Fetches all vertex collections from the graph and returns an array of collection descriptions.

**Arguments**

* **excludeOrphans**: `boolean` (Default: `false`)

  Whether orphan collections should be excluded.

**Examples**

```js
const graph = db.graph('some-graph');

const collections = await graph.listVertexCollections();
// collections is an array of collection descriptions
// including orphan collections

// -- or --

const collections = await graph.listVertexCollections(true);
// collections is an array of collection descriptions
// not including orphan collections
```

#### graph.vertexCollections

`async graph.vertexCollections([excludeOrphans]): Array<Collection>`

Fetches all vertex collections from the database and returns an array of _GraphVertexCollection_ instances for the collections.

**Arguments**

* **excludeOrphans**: `boolean` (Default: `false`)

  Whether orphan collections should be excluded.

**Examples**

```js
const graph = db.graph('some-graph');

const collections = await graph.vertexCollections()
// collections is an array of GraphVertexCollection
// instances including orphan collections

// -- or --

const collections = await graph.vertexCollections(true)
// collections is an array of GraphVertexCollection
// instances not including orphan collections
```

#### graph.addVertexCollection

`async graph.addVertexCollection(collectionName): Object`

Adds the collection with the given _collectionName_ to the graph's vertex
collections.

**Arguments**

* **collectionName**: `string`

  Name of the vertex collection to add to the graph.

**Examples**

```js
const db = new Database();
const graph = db.graph('some-graph');
await graph.addVertexCollection('vertices');
// the collection "vertices" has been added to the graph
```

#### graph.removeVertexCollection

`async graph.removeVertexCollection(collectionName, [dropCollection]): Object`

Removes the vertex collection with the given _collectionName_ from the graph.

**Arguments**

* **collectionName**: `string`

  Name of the vertex collection to remove from the graph.

* **dropCollection**: `boolean` (optional)

  If set to `true`, the collection will also be deleted from the database.

**Examples**

```js
const db = new Database();
const graph = db.graph('some-graph');
await graph.removeVertexCollection('vertices')
// collection "vertices" has been removed from the graph

// -- or --

await graph.removeVertexCollection('vertices', true)
// collection "vertices" has been removed from the graph
// the collection has also been dropped from the database
// this may have been a bad idea
```

### Manipulating edges

#### graph.edgeCollection

`graph.edgeCollection(collectionName): GraphEdgeCollection`

Returns a new [_GraphEdgeCollection_ instance](#graphedgecollection-api) with
the given name bound to this graph.

**Arguments**

* **collectionName**: `string`

  Name of the edge collection.

**Examples**

```js
const db = new Database();
// assuming the collections "edges" and "vertices" exist
const graph = db.graph("some-graph");
const collection = graph.edgeCollection("edges");
assert.equal(collection.name, "edges");
// collection is a GraphEdgeCollection
```

#### graph.addEdgeDefinition

`async graph.addEdgeDefinition(definition): Object`

Adds the given edge definition _definition_ to the graph.

**Arguments**

* **definition**: `Object`

  For more information on edge definitions see
  [the HTTP API for managing graphs](https://docs.arangodb.com/latest/HTTP/Gharial/Management.html).

**Examples**

```js
const db = new Database();
// assuming the collections "edges" and "vertices" exist
const graph = db.graph('some-graph');
await graph.addEdgeDefinition({
  collection: 'edges',
  from: ['vertices'],
  to: ['vertices']
});
// the edge definition has been added to the graph
```

#### graph.replaceEdgeDefinition

`async graph.replaceEdgeDefinition(collectionName, definition): Object`

Replaces the edge definition for the edge collection named _collectionName_ with
the given _definition_.

**Arguments**

* **collectionName**: `string`

  Name of the edge collection to replace the definition of.

* **definition**: `Object`

  For more information on edge definitions see
  [the HTTP API for managing graphs](https://docs.arangodb.com/latest/HTTP/Gharial/Management.html).

**Examples**

```js
const db = new Database();
// assuming the collections "edges", "vertices" and "more-vertices" exist
const graph = db.graph('some-graph');
await graph.replaceEdgeDefinition('edges', {
  collection: 'edges',
  from: ['vertices'],
  to: ['more-vertices']
});
// the edge definition has been modified
```

#### graph.removeEdgeDefinition

`async graph.removeEdgeDefinition(definitionName, [dropCollection]): Object`

Removes the edge definition with the given _definitionName_ form the graph.

**Arguments**

* **definitionName**: `string`

  Name of the edge definition to remove from the graph.

* **dropCollection**: `boolean` (optional)

  If set to `true`, the edge collection associated with the definition will also
  be deleted from the database.

**Examples**

```js
const db = new Database();
const graph = db.graph('some-graph');

await graph.removeEdgeDefinition('edges')
// the edge definition has been removed

// -- or --

await graph.removeEdgeDefinition('edges', true)
// the edge definition has been removed
// and the edge collection "edges" has been dropped
// this may have been a bad idea
```

#### graph.traversal

`async graph.traversal(startVertex, opts): Object`

Performs a traversal starting from the given _startVertex_ and following edges
contained in any of the edge collections of this graph.

**Arguments**

* **startVertex**: `string`

  The handle of the start vertex. This can be either the `_id` of a document in
  the graph or a document (i.e. an object with an `_id` property).

* **opts**: `Object`

  See
  [the HTTP API documentation](https://docs.arangodb.com/latest/HTTP/Traversal/index.html)
  for details on the additional arguments.

  Please note that while _opts.filter_, _opts.visitor_, _opts.init_,
  _opts.expander_ and _opts.sort_ should be strings evaluating to well-formed
  JavaScript functions, it's not possible to pass in JavaScript functions
  directly because the functions need to be evaluated on the server and will be
  transmitted in plain text.

**Examples**

```js
const db = new Database();
const graph = db.graph('some-graph');
const collection = graph.edgeCollection('edges');
await collection.import([
  ['_key', '_from', '_to'],
  ['x', 'vertices/a', 'vertices/b'],
  ['y', 'vertices/b', 'vertices/c'],
  ['z', 'vertices/c', 'vertices/d']
])
const result = await graph.traversal('vertices/a', {
  direction: 'outbound',
  visitor: 'result.vertices.push(vertex._key);',
  init: 'result.vertices = [];'
});
assert.deepEqual(result.vertices, ['a', 'b', 'c', 'd']);
```

## GraphVertexCollection API

The _GraphVertexCollection API_ extends the
[_Collection API_ (see above)](#collection-api) with the following methods.

#### graphVertexCollection.remove

`async graphVertexCollection.remove(documentHandle): Object`

Deletes the vertex with the given _documentHandle_ from the collection.

**Arguments**

* **documentHandle**: `string`

  The handle of the vertex to retrieve. This can be either the `_id` or the
  `_key` of a vertex in the collection, or a vertex (i.e. an object with an
  `_id` or `_key` property).

**Examples**

```js
const graph = db.graph('some-graph');
const collection = graph.vertexCollection('vertices');

await collection.remove('some-key')
// document 'vertices/some-key' no longer exists

// -- or --

await collection.remove('vertices/some-key')
// document 'vertices/some-key' no longer exists
```

### graphVertexCollection.vertex

`async graphVertexCollection.vertex(documentHandle): Object`

Retrieves the vertex with the given _documentHandle_ from the collection.

**Arguments**

* **documentHandle**: `string`

  The handle of the vertex to retrieve. This can be either the `_id` or the
  `_key` of a vertex in the collection, or a vertex (i.e. an object with an
  `_id` or `_key` property).

**Examples**

```js
const graph = db.graph('some-graph');
const collection = graph.vertexCollection('vertices');

const doc = await collection.vertex('some-key');
// the vertex exists
assert.equal(doc._key, 'some-key');
assert.equal(doc._id, 'vertices/some-key');

// -- or --

const doc = await collection.vertex('vertices/some-key');
// the vertex exists
assert.equal(doc._key, 'some-key');
assert.equal(doc._id, 'vertices/some-key');
```

### graphVertexCollection.save

`async graphVertexCollection.save(data): Object`

Creates a new vertex with the given _data_.

**Arguments**

* **data**: `Object`

  The data of the vertex.

**Examples**

```js
const db = new Database();
const graph = db.graph('some-graph');
const collection = graph.vertexCollection('vertices');
const doc = await collection.save({some: 'data'});
assert.equal(doc._id, 'vertices/' + doc._key);
assert.equal(doc.some, 'data');
```

## GraphEdgeCollection API

The _GraphEdgeCollection API_ extends the _Collection API_ (see above) with the
following methods.

#### graphEdgeCollection.remove

`async graphEdgeCollection.remove(documentHandle): Object`

Deletes the edge with the given _documentHandle_ from the collection.

**Arguments**

* **documentHandle**: `string`

  The handle of the edge to retrieve. This can be either the `_id` or the `_key`
  of an edge in the collection, or an edge (i.e. an object with an `_id` or
  `_key` property).

**Examples**

```js
const graph = db.graph('some-graph');
const collection = graph.edgeCollection('edges');

await collection.remove('some-key')
// document 'edges/some-key' no longer exists

// -- or --

await collection.remove('edges/some-key')
// document 'edges/some-key' no longer exists
```

### graphEdgeCollection.edge

`async graphEdgeCollection.edge(documentHandle): Object`

Retrieves the edge with the given _documentHandle_ from the collection.

**Arguments**

* **documentHandle**: `string`

  The handle of the edge to retrieve. This can be either the `_id` or the `_key`
  of an edge in the collection, or an edge (i.e. an object with an `_id` or
  `_key` property).

**Examples**

```js
const graph = db.graph('some-graph');
const collection = graph.edgeCollection('edges');

const edge = await collection.edge('some-key');
// the edge exists
assert.equal(edge._key, 'some-key');
assert.equal(edge._id, 'edges/some-key');

// -- or --

const edge = await collection.edge('edges/some-key');
// the edge exists
assert.equal(edge._key, 'some-key');
assert.equal(edge._id, 'edges/some-key');
```

### graphEdgeCollection.save

`async graphEdgeCollection.save(data, [fromId, toId]): Object`

Creates a new edge between the vertices _fromId_ and _toId_ with the given
_data_.

**Arguments**

* **data**: `Object`

  The data of the new edge. If _fromId_ and _toId_ are not specified, the _data_
  needs to contain the properties __from_ and __to_.

* **fromId**: `string` (optional)

  The handle of the start vertex of this edge. This can be either the `_id` of a
  document in the database, the `_key` of an edge in the collection, or a
  document (i.e. an object with an `_id` or `_key` property).

* **toId**: `string` (optional)

  The handle of the end vertex of this edge. This can be either the `_id` of a
  document in the database, the `_key` of an edge in the collection, or a
  document (i.e. an object with an `_id` or `_key` property).

**Examples**

```js
const db = new Database();
const graph = db.graph('some-graph');
const collection = graph.edgeCollection('edges');
const edge = await collection.save(
  {some: 'data'},
  'vertices/start-vertex',
  'vertices/end-vertex'
);
assert.equal(edge._id, 'edges/' + edge._key);
assert.equal(edge.some, 'data');
assert.equal(edge._from, 'vertices/start-vertex');
assert.equal(edge._to, 'vertices/end-vertex');
```

### graphEdgeCollection.edges

`async graphEdgeCollection.edges(documentHandle): Array<Object>`

Retrieves a list of all edges of the document with the given _documentHandle_.

**Arguments**

* **documentHandle**: `string`

  The handle of the document to retrieve the edges of. This can be either the
  `_id` of a document in the database, the `_key` of an edge in the collection,
  or a document (i.e. an object with an `_id` or `_key` property).

**Examples**

```js
const db = new Database();
const graph = db.graph('some-graph');
const collection = graph.edgeCollection('edges');
await collection.import([
  ['_key', '_from', '_to'],
  ['x', 'vertices/a', 'vertices/b'],
  ['y', 'vertices/a', 'vertices/c'],
  ['z', 'vertices/d', 'vertices/a']
]);
const edges = await collection.edges('vertices/a');
assert.equal(edges.length, 3);
assert.deepEqual(edges.map(edge => edge._key), ['x', 'y', 'z']);
```

### graphEdgeCollection.inEdges

`async graphEdgeCollection.inEdges(documentHandle): Array<Object>`

Retrieves a list of all incoming edges of the document with the given
_documentHandle_.

**Arguments**

* **documentHandle**: `string`

  The handle of the document to retrieve the edges of. This can be either the
  `_id` of a document in the database, the `_key` of an edge in the collection,
  or a document (i.e. an object with an `_id` or `_key` property).

**Examples**

```js
const db = new Database();
const graph = db.graph('some-graph');
const collection = graph.edgeCollection('edges');
await collection.import([
  ['_key', '_from', '_to'],
  ['x', 'vertices/a', 'vertices/b'],
  ['y', 'vertices/a', 'vertices/c'],
  ['z', 'vertices/d', 'vertices/a']
]);
const edges = await collection.inEdges('vertices/a');
assert.equal(edges.length, 1);
assert.equal(edges[0]._key, 'z');
```

### graphEdgeCollection.outEdges

`async graphEdgeCollection.outEdges(documentHandle): Array<Object>`

Retrieves a list of all outgoing edges of the document with the given
_documentHandle_.

**Arguments**

* **documentHandle**: `string`

  The handle of the document to retrieve the edges of. This can be either the
  `_id` of a document in the database, the `_key` of an edge in the collection,
  or a document (i.e. an object with an `_id` or `_key` property).

**Examples**

```js
const db = new Database();
const graph = db.graph('some-graph');
const collection = graph.edgeCollection('edges');
await collection.import([
  ['_key', '_from', '_to'],
  ['x', 'vertices/a', 'vertices/b'],
  ['y', 'vertices/a', 'vertices/c'],
  ['z', 'vertices/d', 'vertices/a']
]);
const edges = await collection.outEdges('vertices/a');
assert.equal(edges.length, 2);
assert.deepEqual(edges.map(edge => edge._key), ['x', 'y']);
```

### graphEdgeCollection.traversal

`async graphEdgeCollection.traversal(startVertex, opts): Object`

Performs a traversal starting from the given _startVertex_ and following edges
contained in this edge collection.

**Arguments**

* **startVertex**: `string`

  The handle of the start vertex. This can be either the `_id` of a document in
  the database, the `_key` of an edge in the collection, or a document (i.e. an
  object with an `_id` or `_key` property).

* **opts**: `Object`

  See
  [the HTTP API documentation](https://docs.arangodb.com/latest/HTTP/Traversal/index.html)
  for details on the additional arguments.

  Please note that while _opts.filter_, _opts.visitor_, _opts.init_,
  _opts.expander_ and _opts.sort_ should be strings evaluating to well-formed
  JavaScript code, it's not possible to pass in JavaScript functions directly
  because the code needs to be evaluated on the server and will be transmitted
  in plain text.

**Examples**

```js
const db = new Database();
const graph = db.graph('some-graph');
const collection = graph.edgeCollection('edges');
await collection.import([
  ['_key', '_from', '_to'],
  ['x', 'vertices/a', 'vertices/b'],
  ['y', 'vertices/b', 'vertices/c'],
  ['z', 'vertices/c', 'vertices/d']
]);
const result = await collection.traversal('vertices/a', {
  direction: 'outbound',
  visitor: 'result.vertices.push(vertex._key);',
  init: 'result.vertices = [];'
});
assert.deepEqual(result.vertices, ['a', 'b', 'c', 'd']);
```
