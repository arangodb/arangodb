<!-- don't edit here, its from https://@github.com/arangodb/arangodbjs.git / docs/Drivers/ -->
# Database API

## new Database

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
