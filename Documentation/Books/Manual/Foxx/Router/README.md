!CHAPTER Routers

`const createRouter = require('@arangodb/foxx/router');`

Routers let you define routes that extend ArangoDB's HTTP API with custom endpoints.

Routers need to be mounted using the `use` method of a [service context](../Context.md) to expose their HTTP routes at a service's mount path.

You can pass routers between services mounted in the same database [as dependencies](../Dependencies.md). You can even nest routers within each other.

!SECTION Creating a router

`createRouter(): Router`

This returns a new, clean router object that has not yet been mounted in the service and can be exported like any other object.

!SECTION Request handlers

`router.get([path], handler, [name]): Endpoint`

`router.post([path], handler, [name]): Endpoint`

`router.put([path], handler, [name]): Endpoint`

`router.patch([path], handler, [name]): Endpoint`

`router.delete([path], handler, [name]): Endpoint`

`router.all([path], handler, [name]): Endpoint`

These methods let you specify routes on the router. The `all` method defines a route that will match any supported HTTP verb, the other methods define routes that only match the HTTP verb with the same name.

**Arguments**

* **path**: `string` (Default: `"/"`)

  The path of the request handler relative to the base path the Router is mounted at. If omitted, the request handler will handle requests to the base path of the Router. For information on defining dynamic routes see the section on [path parameters in the chapter on router endpoints](Endpoints.md#pathparam).

* **handler**: `Function`

  A function that takes the following arguments:

  * **req**: `Request`

    An incoming server request object.

  * **res**: `Response`

    An outgoing server response.

* **name**: `string` (optional)

  A name that can be used to generate URLs for the endpoint. For more information see the `reverse` method of the [request object](Request.md).

Returns an [Endpoint](Endpoints.md) for the route.

!SECTION Mounting child routers and middleware

`router.use([path], handler, [name]): Endpoint`

The `use` method lets you mount a child router or middleware at a given path.

**Arguments**

* **path**: `string` (optional)

  The path of the request handler relative to the base path the Router is mounted at. If omitted, the request handler will handle requests to the base path of the Router. For information on defining dynamic routes see the section on [path parameters in the chapter on router endpoints](Endpoints.md#pathparam).

* **handler**: `Router | Middleware`

  An unmounted router object or a [middleware](Middleware.md).

* **name**: `string` (optional)

  A name that can be used to generate URLs for endpoints of this router. For more information see the `reverse` method of the [request object](Request.md). Has no effect if *handler* is a Middleware.

Returns an [Endpoint](Endpoints.md) for the middleware or child router.
