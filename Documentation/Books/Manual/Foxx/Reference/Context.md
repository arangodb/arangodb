Foxx service context
====================

The service context provides access to methods and attributes that are specific
to a given service. In a Foxx service the context is generally available as the
`module.context` variable. Within a router's request handler the request and
response objects' `context` attribute also provide access to the context of the
service the route was mounted in (which may be different from the one the route
handler was defined in).

**Examples**

```js
// in service /my-foxx-1
const createRouter = require('@arangodb/foxx/router');
const router = createRouter();

// See the chapter on dependencies for more info on
// how exports and dependencies work across services
module.exports = {routes: router};

router.get(function (req, res) {
  module.context.mount === '/my-foxx-1';
  req.context.mount === '/my-foxx-2';
  res.write('Hello from my-foxx-1');
});

// in service /my-foxx-2
const createRouter = require('@arangodb/foxx/router');
const router2 = createRouter();

module.context.use(router2);

router2.post(function (req, res) {
  module.context.mount === '/my-foxx-2';
  req.context.mount === '/my-foxx-2';
  res.write('Hello from my-foxx-2');
});

const router1 = module.context.dependencies.myFoxx1.routes;
module.context.use(router1);
```

The service context specifies the following properties:

* **argv**: `any`

  Any arguments passed in if the current file was executed as a
  [script or queued job](../Guides/Scripts.md).

* **basePath**: `string`

  The file system path of the service, i.e. the folder in which the service
  was installed to by ArangoDB.

* **baseUrl**: `string`

  The base URL of the service, relative to the ArangoDB server,
  e.g. `/_db/_system/my-foxx`.

* **collectionPrefix**: `string`

  The prefix that will be used by *collection* and *collectionName* to derive
  the names of service-specific collections. This is derived from the
  service's mount point, e.g. `/my-foxx` becomes `my_foxx`.

* **configuration**: `Object`

  [Configuration options](Configuration.md) for the service.

* **dependencies**: `Object`

  Configured [dependencies](../Guides/Dependencies.md) for the service.

* **isDevelopment**: `boolean`

  Indicates whether the service is running in [development mode](../README.md).

* **isProduction**: `boolean`

  The inverse of *isDevelopment*.

* **manifest**: `Object`

  The parsed [manifest file](Manifest.md) of the service.

* **mount**: `string`

  The mount point of the service, e.g. `/my-foxx`.

apiDocumentation
----------------

`module.context.apiDocumentation([options]): Function`

**DEPRECATED**

Creates a request handler that serves the API documentation.

**Note**: This method has been deprecated in ArangoDB 3.1 and replaced with
the more straightforward `createDocumentationRouter` method providing the
same functionality.

**Arguments**

See `createDocumentationRouter` below.

**Examples**

```js
// Serve the API docs for the current service
router.get('/docs/*', module.context.apiDocumentation());

// Note that the path must end with a wildcard
// and the route must use HTTP GET.
```

createDocumentationRouter
-------------------------

`module.context.createDocumentationRouter([options]): Router`

Creates a router that serves the API documentation.

**Note**: The router can be mounted like any other child router
(see examples below).

**Arguments**

* **options**: `Object` (optional)

  An object with any of the following properties:

  * **mount**: `string` (Default: `module.context.mount`)

    The mount path of the service to serve the documentation of.

  * **indexFile**: `string` (Default: `"index.html"`)

    File name of the HTML file serving the API documentation.

  * **swaggerRoot**: `string` (optional)

    Full path of the folder containing the Swagger assets and the *indexFile*.
    Defaults to the Swagger assets used by the web interface.

  * **before**: `Function` (optional)

    A function that will be executed before a request is handled.

    If the function returns `false` the request will not be processed any further.

    If the function returns an object, its attributes will be used to override
    the *options* for the current request.

    Any other return value will be ignored.

If *options* is a function it will be used as the *before* option.

If *options* is a string it will be used as the *swaggerRoot* option.

Returns a Foxx router.

**Examples**

```js
// Serve the API docs for the current service
router.use('/docs', module.context.createDocumentationRouter());

// -- or --

// Serve the API docs for the service the router is mounted in
router.use('/docs', module.context.createDocumentationRouter(function (req) {
  return {mount: req.context.mount};
}));

// -- or --

// Serve the API docs only for users authenticated with ArangoDB
router.use('/docs', module.context.createDocumentationRouter(function (req, res) {
  if (req.suffix === 'swagger.json' && !req.arangoUser) {
    res.throw(401, 'Not authenticated');
  }
}));
```

collection
----------

`module.context.collection(name): ArangoCollection | null`

Passes the given name to *collectionName*, then looks up the collection with
the prefixed name.

**Arguments**

* **name**: `string`

  Unprefixed name of the service-specific collection.

Returns a collection or `null` if no collection with the prefixed name exists.

collectionName
--------------

`module.context.collectionName(name): string`

Prefixes the given name with the *collectionPrefix* for this service.

**Arguments**

* **name**: `string`

  Unprefixed name of the service-specific collection.

Returns the prefixed name.

**Examples**

```js
module.context.mount === '/my-foxx'
module.context.collectionName('doodads') === 'my_foxx_doodads'
```

file
----

`module.context.file(name, [encoding]): Buffer | string`

Passes the given name to *fileName*, then loads the file with the resulting name.

**Arguments**

* **name**: `string`

  Name of the file to load, relative to the current service.

* **encoding**: `string` (optional)

  Encoding of the file, e.g. `utf-8`. If omitted the file will be loaded as a
  raw buffer instead of a string.

Returns the file's contents.

fileName
--------

`module.context.fileName(name): string`

Resolves the given file name relative to the current service.

**Arguments**

* **name**: `string`

  Name of the file, relative to the current service.

Returns the absolute file path.

<!-- registerType

`module.context.registerType(type, def): void`

TODO

**Arguments**

* **type**: `string`

  TODO

* **def**: `Object`

  TODO

TODO

-->

use
---

`module.context.use([path], router): Endpoint`

Mounts a given router on the service to expose the router's routes on the
service's mount point.

**Arguments**

* **path**: `string` (Default: `"/"`)

  Path to mount the router at, relative to the service's mount point.

* **router**: `Router | Middleware`

  A router or middleware to mount.

Returns an [Endpoint](Routers/Endpoints.md) for the given router or middleware.

**Note**: Mounting services at run time (e.g. within request handlers or
queued jobs) is not supported.
