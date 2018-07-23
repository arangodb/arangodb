Linking services together
=========================

When using multiple services (or multiple copies of the same service) in the
same database, sometimes you may want to share collections or methods between
those services. Typical examples are:

- [collections](Collections.md) or APIs for managing shared data
  (e.g. application users or session data)
- common [middleware](../Reference/Routers/Middleware.md) that requires some
  [configuration](../Reference/Configuration.md) that would be identical
  for multiple services
- [reusable routers](Routing.md) that provide the same API
  for different services

For scenarios like these, Foxx provides a way to link services together and
allow them to export JS APIs other services can use.
In Foxx these JS APIs are called _dependencies_,
the services implementing them are called _providers_,
the services using them are called _consumers_.

{% hint 'info' %}
This chapter is about Foxx dependencies as described above. In JavaScript the
term _dependencies_ can also refer to
[bundled node modules](BundledNodeModules.md), which are an unrelated concept.
{% endhint %}


Declaring dependencies
----------------------

Foxx dependencies can be declared in the
[service manifest](../Reference/Manifest.md)
using the `provides` and `dependencies` fields:

- `provides` lists the dependencies a given service provides,
  i.e. which APIs it claims to be compatible with

- `dependencies` lists the dependencies a given service consumes,
  i.e. which APIs its dependencies need to be compatible with

Explicitly naming your dependencies helps improving tooling support for
managing service dependencies in ArangoDB but is not strictly necessary.
It is possible to omit the `provides` field even if your service provides a
JS API and the `dependencies` field can be used without explicitly specifying
dependency names.

A dependency name should be an alphanumeric identifier, optionally using a
namespace prefix (i.e. `dependency-name` or `@namespace/dependency-name`).
For example, services maintained by the ArangoDB Foxx team typically use
the `@foxx` namespace whereas the `@arangodb` namespace
is reserved for internal use.

There is no official registry for dependency names but we recommend ensuring
the dependency names you use are unambiguous and meaningful
to other developers using your services.

A `provides` definition maps each provided dependency's name
to the provided version:

```json
"provides": {
  "@example/auth": "1.0.0"
}
```

A `dependencies` definition maps the _local alias_ of each consumed dependency
against a short definition that includes the name and version range:

```json
"dependencies": {
  "myAuth": {
    "name": "@example/auth",
    "version": "^1.0.0",
    "description": "This description is entirely optional.",
    "required": false,
    "multiple": false
  }
}
```

The local alias should be a valid JavaScript identifier
(e.g. a valid variable name). When a dependency has been assigned,
its JS API will be exposed in a corresponding property of the
[service context](../Reference/Context.md),
e.g. `module.context.dependencies.myAuth`.

Assigning dependencies
----------------------

Like [configuration](../Reference/Configuration.md),
dependencies can be assigned using
the [web interface](../../Programs/WebInterface/Services.md),
the [Foxx CLI](../../Programs/FoxxCLI/README.md) or
the [Foxx HTTP API](../../../HTTP/Foxx/Configuration.html).

The value for each dependency should be the database-relative mount path of
the service (including the leading slash). Both services need to be mounted in
the same database. The same service can be used to provide a dependency
for multiple services.

Also as with configuration, a service that declares required dependencies which
have not been assigned will not be mounted by Foxx until all required
dependencies have been assigned. Instead any attempt to access the service's
HTTP API will result in an error code.

Exporting a JS API
------------------

In order to provide a JS API other services can consume as a dependency,
the service's _main_ file needs to export something other services can use.
You can do this by assigning a value to the `module.exports` or properties
of the `exports` object as with any other module export:

```js
module.exports = "Hello world";
```

This also includes collections. In the following example, the collection
exported by the provider will use the provider's
[collection prefix](Collections.md) rather than the consumer's,
allowing both services to share the same collection:

```js
module.exports = module.context.collection("shared_documents");
```

Let's imagine we have a service managing our application's users.
Rather than allowing any consuming service to access the collection directly,
we can provide a number of methods to manipulate it:

```js
const auth = require("./util/auth");
const users = module.context.collection("users");

exports.login = (username, password) => {
  const user = users.firstExample({ username });
  if (!user) throw new Error("Wrong username");
  const valid = auth.verify(user.authData, password);
  if (!valid) throw new Error("Wrong password");
  return user;
};
exports.setPassword = (user, password) => {
  const authData = auth.create(password);
  users.update(user, { authData });
  return user;
};
```

Or you could even export a factory function to create an API that uses a
custom error type provided by the consumer rather than the producer:

```js
const auth = require("./util/auth");
const users = module.context.collection("users");

module.exports = (BadCredentialsError = Error) => {
  return {
    login(username, password) {
      const user = users.firstExample({ username });
      if (!user) throw new BadCredentialsError("Wrong username");
      const valid = auth.verify(user.authData, password);
      if (!valid) throw new BadCredentialsError("Wrong password");
      return user;
    },
    setPassword(user, password) {
      const authData = auth.create(password);
      users.update(user, { authData });
      return user;
    }
  };
};
```

Example usage (the consumer uses the local alias `usersApi`):

```js
"use strict";
const createRouter = require("@arangodb/foxx/router");
const joi = require("joi");

// Using the dependency with arguments
const AuthFailureError = require("./errors/auth-failure");
const createUsersApi = module.context.dependencies.usersApi;
const users = createUsersApi(AuthFailureError);

const router = createRouter();
module.context.use(router);

router.use((req, res, next) => {
  try {
    next();
  } catch (e) {
    if (e instanceof AuthFailureError) {
      res.status(401);
      res.json({
        error: true,
        message: e.message
      });
    } else {
      console.error(e.stack);
      res.status(500);
      res.json({
        error: true,
        message: "Something went wrong."
      });
    }
  }
});

router
  .post("/login", (req, res) => {
    const { username, password } = req.body;
    const user = users.login(username, password);
    // handle login success
    res.json({ welcome: username });
  })
  .body(
    joi.object().keys({
      username: joi.string().required(),
      password: joi.string().required()
    })
  );
```
