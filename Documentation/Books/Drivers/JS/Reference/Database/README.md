<!-- don't edit here, it's from https://@github.com/arangodb/arangojs.git / docs/Drivers/ -->
# Database API

## new Database

`new Database([config]): Database`

Creates a new _Database_ instance.

If _config_ is a string, it will be interpreted as _config.url_.

**Arguments**

- **config**: `Object` (optional)

  An object with the following properties:

  - **url**: `string | Array<string>` (Default: `http://localhost:8529`)

    Base URL of the ArangoDB server or list of server URLs.

    When working with a cluster or a single server with leader/follower failover,
    [the method `db.acquireHostList`](#databaseacquirehostlist)
    can be used to automatically pick up additional coordinators/followers at
    any point.

    When running ArangoDB on a unix socket, e.g. `/tmp/arangodb.sock`, the
    following URL formats are supported for unix sockets:

    - `unix:///tmp/arangodb.sock` (no SSL)
    - `http+unix:///tmp/arangodb.sock` (or `https+unix://` for SSL)
    - `http://unix:/tmp/arangodb.sock` (or `https://unix:` for SSL)

    Additionally `ssl` and `tls` are treated as synonymous with `https` and
    `tcp` is treated as synonymous with `http`, so the following URLs are
    considered identical:

    - `tcp://localhost:8529` and `http://localhost:8529`
    - `ssl://localhost:8529` and `https://localhost:8529`
    - `tcp+unix:///tmp/arangodb.sock` and `http+unix:///tmp/arangodb.sock`
    - `ssl+unix:///tmp/arangodb.sock` and `https+unix:///tmp/arangodb.sock`
    - `tcp://unix:/tmp/arangodb.sock` and `http://unix:/tmp/arangodb.sock`
    - `ssl://unix:/tmp/arangodb.sock` and `https://unix:/tmp/arangodb.sock`

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

  - **isAbsolute**: `boolean` (Default: `false`)

    If this option is explicitly set to `true`, the _url_ will be treated as the
    absolute database path and arangojs will not append the database path to it.

    **Note:** This makes it impossible to switch databases with _useDatabase_
    or using _acquireHostList_. This is only intended to be used as an escape
    hatch when working with standalone servers exposing a single database API
    from behind a reverse proxy, which is not a recommended setup.

  - **arangoVersion**: `number` (Default: `30000`)

    Numeric representation of the ArangoDB version the driver should expect.
    The format is defined as `XYYZZ` where `X` is the major version, `Y` is
    the zero-filled two-digit minor version and `Z` is the zero-filled two-digit
    bugfix version, e.g. `30102` for 3.1.2, `20811` for 2.8.11.

    Depending on this value certain methods may become unavailable or change
    their behavior to remain compatible with different versions of ArangoDB.

  - **headers**: `Object` (optional)

    An object with additional headers to send with every request.

    Header names should always be lowercase. If an `"authorization"` header is
    provided, it will be overridden when using _useBasicAuth_ or _useBearerAuth_.

  - **agent**: `Agent` (optional)

    An http Agent instance to use for connections.

    By default a new
    [`http.Agent`](https://nodejs.org/api/http.html#http_new_agent_options) (or
    https.Agent) instance will be created using the _agentOptions_.

    This option has no effect when using the browser version of arangojs.

  - **agentOptions**: `Object` (Default: see below)

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

  - **loadBalancingStrategy**: `string` (Default: `"NONE"`)

    Determines the behavior when multiple URLs are provided:

    - `NONE`: No load balancing. All requests will be handled by the first
      URL in the list until a network error is encountered. On network error,
      arangojs will advance to using the next URL in the list.

    - `ONE_RANDOM`: Randomly picks one URL from the list initially, then
      behaves like `NONE`.

    - `ROUND_ROBIN`: Every sequential request uses the next URL in the list.

  - **maxRetries**: `number` or `false` (Default: `0`)

    Determines the behavior when a request fails because the underlying
    connection to the server could not be opened
    (i.e. [`ECONNREFUSED` in Node.js](https://nodejs.org/api/errors.html#errors_common_system_errors)):

    - `false`: the request fails immediately.

    - `0`: the request is retried until a server can be reached but only a
      total number of times matching the number of known servers (including
      the initial failed request).

    - any other number: the request is retried until a server can be reached
      the request has been retried a total of `maxRetries` number of times
      (not including the initial failed request).

    When working with a single server without leader/follower failover, the
    retries (if any) will be made to the same server.

    This setting currently has no effect when using arangojs in a browser.

    **Note**: Requests bound to a specific server (e.g. fetching query results)
    will never be retried automatically and ignore this setting.

## database.acquireHostList

`async database.acquireHostList(): this`

Updates the URL list by requesting a list of all coordinators in the cluster
and adding any endpoints not initially specified in the _url_ configuration.

For long-running processes communicating with an ArangoDB cluster it is
recommended to run this method repeatedly (e.g. once per hour) to make sure
new coordinators are picked up correctly and can be used for fail-over or
load balancing.

**Note**: This method can not be used when the arangojs instance was created
with `isAbsolute: true`.

## database.useDatabase

`database.useDatabase(databaseName): this`

Updates the _Database_ instance and its connection string to use the given
_databaseName_, then returns itself.

**Note**: This method can not be used when the arangojs instance was created
with `isAbsolute: true`.

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

## database.close

`database.close(): void`

Closes all active connections of the database instance.
Can be used to clean up idling connections during longer periods of inactivity.

**Note**: This method currently has no effect in the browser version of arangojs.

**Examples**

```js
const db = new Database();
const sessions = db.collection("sessions");
// Clean up expired sessions once per hour
setInterval(async () => {
  await db.query(aql`
    FOR session IN ${sessions}
    FILTER session.expires < DATE_NOW()
    REMOVE session IN ${sessions}
  `);
  // Make sure to close the connections because they're no longer used
  db.close();
}, 1000 * 60 * 60);
```
