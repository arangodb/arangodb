Patterns
========

# Collections

In Foxx collection names are automatically qualified with a prefix based on the mount path to avoid conflicts between services using identical otherwise collection names (or multiple copies of the same service mounted at different mount paths).

Just use `module.context.collection` bla bla.

## Low-level collection access

When managing collections in your [lifecycle scripts]() bla bla `db._collection`/`db._create`/`db._drop` bla bla `module.context.collectionName` to get the prefixed collection name.

## Sharing collections

While `db._collection` can also be used to share collections between services, bla bla discouraged.

Instead, decide which service should own and manage the collections & expose using `module.exports` and Foxx dependencies to import into other services.

# Lifecycle scripts and migrations

Bla bla `setup` is used automatically on install/upgrade/replace unless you disable via option. Use `teardown` to clean up when service is uninstalled.

Since the `setup` script can be run multiple times on an already installed service it's a good idea to make it reentrant (i.e. not result in errors if parts of it already have been run before).

The `teardown` script should remove all traces of the service for a clean uninstall. Keep in mind this results in catastrophic data loss. May want to rename to something else and invoke manually if that's too risky.

## Migrations

Running large `setup` on every upgrade can be very expensive during development. Alternative solution is to only handle basic collection creation and indexes and have one-off migration scripts and invoke them manually. Bla bla.

# Nested routers

Don't have to define service all in one file. Don't have to mount routers where you define them.

Have a folder structure like e.g.
```
api/
  index.js
  auth.js
  tickets.js
  admin/
    index.js
    users.js
```

Can define router per file and export/import, mount with or without prefix in parent router.

## Guarded child routers

Use middleware per router. Example: guard admin router (and child routers) with auth middleware like
```js
router.use((req, res, next) => {
  try {
    req.user = users.document(req.session.uid);
  } catch (e) {
    res.throw('forbidden');
  }
});
```
guards entire subtree but leaves parent router alone.

# Queries

Use `query` helper to execute queries and automatically escape bindvars.

Collection objects can be passed directly so you don't have to worry about collection names.

Dynamic snippets can be escaped with `aql.literal` but create injection risk.

## Don't repeat yourself

Stick `query` calls in helper functions, e.g.:
```js
'use strict';
const users = module.context.collection('users');
exports.getUserEmails = (activeOnly = true) => query`
  FOR u IN ${users}
  ${aql.literal(activeOnly ? 'FILTER u.active' : '')}
  RETURN u.email
`.toArray();
```

Easier to create tests this way. Can also create folder for them:
```
api/
  index.js
  ...
queries/
  getUserEmails.js
```

## Low-level query access

Use `aql` template string to create query objects, use `db._query` to execute.
Almost never necessary. Avoid if possible.

# TypeScript

Have all code in subfolder
```
  manifest.json
  src/
    api/
      index.js
      ...
    scripts/
      setup.js
```
Use `tsc` with `tsconfig.json` like
```json
{
  "compilerOptions": {
    // ...
    "outDir": "./build/",
    "rootDir": "./src/"
  }
}
```
Add build dir to `.gitignore`, add src dir to `.foxxignore`, use `foxx-cli` to install.

# Testing

Extract common code into modules and write unit tests:
```
api/
  index.js
  ...
queries/
  getUserEmails.js
  ...
util/
  sluggify.js
  formatDate.js
  auth.js
test/
  queries/
    getUserEmails.js
    ...
  util/
    sluggify.js
    ...
```

## Integration tests

Use `request` module to test JSON API with `module.context.baseUrl`.

Make sure to disable development mode to avoid instability. Also make sure you didn't accidentally lower the amount of V8 contexts so Foxx won't lock up trying to talk to itself.

# Error handling

Foxx will automatically generate machine-readable error responses when an error isn't caught but this is likely not what you want to expose to API users. Instead:

Have application specific error objects with numeric codes to help API users, e.g.
```js
class MyFoxxError extends Error {
  constructor (code, status = 500) {
    super(`My Foxx Error #${code}`);
    this.code = code;
    this.status = status;
  }
  toJSON () {
    return {error: true, code: this.code, status: this.status};
  }
}
```

Look into `require('@arangodb').errors` when checking ArangoError codes to determine what went wrong and throw appropriate custom error.

Try to handle expected errors in each route and rethrow an application-specific error.

In root index.js have something like
```js
const UNEXPECTED_ERROR = 1234;
module.context.use((req, res, next) => {
  try {
    next();
  } catch (e) {
    if (e instanceof MyFoxxError) {
      res.status(e.status);
      res.json(e);
    } else {
      res.status(500);
      res.json(new MyFoxxError(UNEXPECTED_ERROR));
    }
  }
})
```

# Sessions

Built-in solution: [session middleware]()

When using collection storage: clean up expired sessions via script:

* extract collection storage into file
* import file where you use storage & in cleanup script
* call script via cron or manually

Cleanup script:
```js
'use strict';
const storage = require('../util/sessionStorage');
module.exports = storage.prune();
```

## Roll your own

Sessions can be implemented easily yourself if you need more control:
```js
const sessions = module.context.collection('sessions');
const secret = module.context.configuration.cookieSecret;
module.context.use((req, res, next) => {
  let sid = req.cookie('sid', {secret});
  if (sid) {
    try {
      req.session = sessions.document(sid);
    } catch (e) {
      sid = null;
      res.cookie('sid', '', {ttl: -1, secret});
    }
  }
  try {
    next();
  } finally {
    if (req.session) {
      if (sid) {
        sessions.update(sid, req.session);
      } else {
        sid = sessions.save(req.session)._key;
      }
      res.cookie('sid', sid, {ttl: 24 * 60 * 60, secret});
    }
  }
})
```

# Auth

Naive solution: use `@arangodb/foxx/auth` for simple hashing.

Alternative: [OAuth]() or [OAuth 2]() for e.g. GitHub/Facebook.

Use [sessions]() to track active user
```js
router.post('/login', (req, res) => {
  const user = verifyCredentials(req.body.username, req.body.password)
  if (user) {
    req.session.uid = user._key;
    req.json({success: true});
  } else {
    res.throw('unauthorized');
  }
})
.body(joi.object().keys({
  username: joi.string(),
  password: joi.string()
}));
```

Stronger crypto: PBKDF2

Or use `request` to delegate

## Use ArangoDB auth

Probably a bad idea unless already giving users direct low-level access via ArangoDB permissions.

Use `req.arangoUser` to see if logged in and who.

# External requests

Avoid if possible because slow.

Can be used to integrate 3rd party services.

Use `request` module.

Can be used for [auth](./auth) or [integration tests](./testing).

# Troubleshooting

Check arangod.log for errors

Make sure to check surrounding stacktraces because they may be related

When catching ArangoError exceptions use `console.errorStack(err)` to log as internal errors may have been rethrown and that function preserves the full stack trace.

<!-- State of debugging in Foxx is bad and we should feel bad -->

Common errors/problems observed by community and how to solve?

# Cluster

Avoid writing to filesystem and you'll be fine.

# Development mode

Enable if you want service to reload from disk every time you access it. Consider using foxx-cli instead. Keep in mind it's a bad idea to use the service folder as your workspace as you should consider it extremely volatile (cluster will hurt you).

## In a cluster

Service will propagate from coordinator devmode was disabled on. Beware.