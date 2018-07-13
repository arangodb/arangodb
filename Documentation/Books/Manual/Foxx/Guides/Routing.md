Working with routers
====================

In Foxx [routers](../Reference/Routers/README.md) are used to define
the URLs of your API. The easiest way to use a router is to mount it
directly in the service using the [context](../Reference/Context.md):

```js
const createRouter = require("@arangodb/foxx/router");
const router = createRouter();

module.context.use(router);
```

Nested routers
--------------

Instead of mounting routers where they are defined, routers can also be
exported from one module and imported in another. This allows you to
structure your routes by splitting them across multiple files:

```js
// in your main file
const usersRouter = require("./api/users");
module.context.use("/users", usersRouter);

// in api/users/index.js
const createRouter = require("@arangodb/foxx/router");
const usersRouter = createRouter();
module.exports = usersRouter;

usersRouter.get("/me", (req, res) => {
  // this will be exposed as /users/me
});
```

You can also mount routers inside of each other:

```js
// in api/users/friends.js
const createRouter = require("@arangodb/foxx/router");
const friendsRouter = createRouter();
module.exports = friendsRouter;

// in api/users/index.js
const friendsRouter = require("./friends");
usersRouter.use("/friends", friendsRouter);
```

Note that you can also mount several routers with the same prefix
or even without a prefix:

```js
adminRouter.use(usersAdminRouter);
adminRouter.use(groupsAdminRouter);
```

### Local middleware

Router-level middleware only applies to the router it is applied to and
is not shared between multiple routers mounted at the same prefix
(or without a prefix).

This can be especially useful when restricting access to
some routes but not others:

```js
const createRouter = require("@arangodb/foxx/router");
const publicRoutes = createRouter();
const authedRoutes = createRouter();

authedRoutes.use((req, res, next) => {
  if (req.session.uid) {
    next();
  } else {
    res.throw("unauthorized");
  }
});

module.context.use(publicRoutes);
module.context.use(authedRoutes);
```

Router factories
----------------

Sometimes you may have routers you want to use in multiple projects or
use at multiple points of your API but with slightly different implementations
or using different collections.

In these cases it can be useful to return the router from a function that
takes these differences as arguments instead of exporting the router directly:

```js
// in your main file
const createUsersRouter = require("../util/users-router");
const usersRouter = createUsersRouter(
  module.context.collection("users"),
  "username"
);
module.context.use(usersRouter);

// in util/users-router.js
const createRouter = require("@arangodb/foxx/router");
const { query } = require("@arangodb");

module.export = (usersCollection, keyField) => {
  const router = createRouter();
  router.use((req, res) => {
    if (!req.session || !req.session.uid) {
      res.throw("unauthorized");
    }
  });
  router.get("/me", (req, res) => {
    const user = query`
      FOR user IN ${usersCollection}
      FILTER user[${keyField}] == ${req.session.uid}
      LIMIT 1
      RETURN user
    `.next();
    res.json(user);
  });
  return router;
};
```
