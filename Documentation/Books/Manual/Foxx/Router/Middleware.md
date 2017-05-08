Middleware
==========

Middleware in Foxx refers to functions that can be mounted like routes and can manipulate the request and response objects before and after the route itself is invoked. They can also be used to control access or to provide common logic like logging etc. Unlike routes, middleware is mounted with the `use` method like a router.

Instead of a function the `use` method can also accept an object with a `register` function that will be passed the endpoint the middleware will be mounted at and returns the actual middleware function. This allows manipulating the endpoint before creating the middleware (e.g. to document headers, request bodies, path parameters or query parameters).

**Examples**

Restrict access to ArangoDB-authenticated users:

```js
module.context.use(function (req, res, next) {
  if (!req.arangoUser) {
    res.throw(401, 'Not authenticated with ArangoDB');
  }
  next();
});
```

Any truthy argument passed to the `next` function will be thrown as an error:

```js
module.context.use(function (req, res, next) {
  let err = null;
  if (!req.arangoUser) {
    err = new Error('This should never happen');
  }
  next(err); // throws if the error was set
})
```

Trivial logging middleware:

```js
module.context.use(function (req, res, next) {
  const start = Date.now();
  try {
    next();
  } finally {
    console.log(`Handled request in ${Date.now() - start}ms`);
  }
});
```

More complex example for header-based sessions:

```js
const sessions = module.context.collection('sessions');
module.context.use({
  register (endpoint) {
    endpoint.header('x-session-id', joi.string().optional(), 'The session ID.');
    return function (req, res, next) {
      const sid = req.get('x-session-id');
      if (sid) {
        try {
          req.session = sessions.document(sid);
        } catch (e) {
          delete req.headers['x-session-id'];
        }
      }
      next();
      if (req.session) {
        if (req.session._rev) {
          sessions.replace(req.session, req.session);
          res.set('x-session-id', req.session._key);
        } else {
          const meta = sessions.save(req.session);
          res.set('x-session-id', meta._key);
        }
      }
    };
  }
});
```
