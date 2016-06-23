!CHAPTER Controllers vs routers

Foxx Controllers have been replaced with [routers](../../Router/README.md). This is more than a cosmetic change as there are significant differences in behaviour:

Controllers were automatically mounted when the file defining them was executed. Routers need to be explicitly mounted using the `module.context.use` method. Routers can also be exported, imported and even nested. This makes it easier to split up complex routing trees across multiple files.

Old:

```js
'use strict';
const Foxx = require('org/arangodb/foxx');
const ctrl = new Foxx.Controller(applicationContext);

ctrl.get('/hello', function (req, res) {
  // ...
});
```

New:

```js
'use strict';
const createRouter = require('org/arangodb/foxx/router');
const router = createRouter();
// If you are importing this file from your entry file ("main"):
module.exports = router;
// Otherwise: module.context.use(router);

router.get('/hello', function (req, res) {
  // ...
});
```

Some general changes in behaviour that might trip you up:

* When specifying path parameters with schemas Foxx will now ignore the route if the schema does not match (i.e. `/hello/foxx` will no longer match `/hello/:num` if `num` specifies a schema that doesn't match the value `"foxx"`). With controllers this could previously result in users seeing a 400 (bad request) error when they should instead be served a 404 (not found) response.

* When a request is made with an HTTP verb not supported by an endpoint, Foxx will now respond with a 405 (method not allowed) error with an appropriate `Allowed` header listing the supported HTTP verbs for that endpoint.

* Foxx will no longer parse your JSDoc comments to generate route documentation (use the `summary` and `description` methods of the endpoint instead).

* The `apiDocumentation` method now lives on the service context and behaves slightly differently.

* There is no router equivalent for the `activateAuthentication` and `activateSessions` methods. Instead you should use the session middleware (see the section on sessions below).

* There is no `del` alias for the `delete` method on routers. It has always been safe to use keywords as method names in Foxx, so the use of this alias was already discouraged before.

* The `allRoutes` proxy is no lot available on routers but can easily be replaced with middleware or child routers.
