Authentication
==============

Foxx provides the [auth module](../Reference/Modules/Auth.md) to implement basic password verification and hashing but is not very secure unless using the (very slow) PBKDF2 algorithm. Alternatively you can use the [OAuth 1.0a](../Reference/Modules/OAuth1.md) or [OAuth 2.0](../Reference/Modules/OAuth2.md) modules to offload identity management to a trusted provider (e.g. Facebook, GitHub, Google or Twitter).

The [session middleware](../Reference/Sessions/README.md) provides a mechanism for adding session logic to your service, using e.g. a collection or JSON Web Tokens to store the sessions between requests.

With these building blocks you can implement your own session-based authentication.

Implementing session authentication
-----------------------------------

In this example we'll use two collections: a `users` collection to store the user objects with names and credentials, and a `sessions` collection to store the session data. We'll also make sure usernames are unique by adding a hash index:

```js
"use strict";
const { db } = require("@arangodb");
const users = module.context.collectionName("users");
if (!db._collection(users)) {
  db._createDocumentCollection(users);
}
const sessions = module.context.collectionName("sessions");
if (!db._collection(sessions)) {
  db._createDocumentCollection(sessions);
}
module.context.collection("users").ensureIndex({
  type: "hash",
  unique: true,
  fields: ["username"]
});
```

Next you should create a sessions middleware that uses the `sessions` collection and the "cookie" transport in a separate file, and add it to the service router:

```js
"use strict";
const sessionsMiddleware = require("@arangodb/foxx/sessions");
const sessions = sessionsMiddleware({
  storage: module.context.collection("sessions"),
  transport: "cookie"
});
module.exports = sessions;
```

```js
// ...
const sessions = require("./util/sessions");
module.context.use(sessions);
```

You'll want to be able to use the authenticator throughout multiple parts of your service so it's best to create it in a separate module and export it so we can import it anywhere we need it:

```js
"use strict";
const createAuth = require("@arangodb/foxx/auth");
const auth = createAuth();
module.exports = auth;
```

If you want, you can now use the authenticator to help create an initial user in the setup script. Note we're hardcoding the password here but you could make it configurable via a [service configuration option](../Reference/Configuration.md):

```js
// ...
const auth = require("./util/auth");
const users = module.context.collection("users");
if (!users.firstExample({ username: "admin" })) {
  users.save({
    username: "admin",
    password: auth.create("hunter2")
  });
}
```

We can now put the two together to create a login route:

```js
// ...
const auth = require("./util/auth");
const users = module.context.collection("users");
const joi = require("joi");
const createRouter = require("@arangodb/foxx/router");
const router = createRouter();

router
  .post("/login", function(req, res) {
    const user = users.firstExample({
      username: req.body.username
    });
    const valid = auth.verify(
      // Pretend to validate even if no user was found
      user ? user.authData : {},
      req.body.password
    );
    if (!valid) res.throw("unauthorized");
    // Log the user in using the key
    // because usernames might change
    req.session.uid = user._key;
    req.sessionStorage.save(req.session);
    res.send({ username: user.username });
  })
  .body(
    joi
      .object({
        username: joi.string().required(),
        password: joi.string().required()
      })
      .required()
  );
```

To provide information about the authenticated user we can look up the session user:

```js
router.get("/me", function(req, res) {
  try {
    const user = users.document(req.session.uid);
    res.send({ username: user.username });
  } catch (e) {
    res.throw("not found");
  }
});
```

To log a user out we can remove the user from the session:

```js
router.post("/logout", function(req, res) {
  if (req.session.uid) {
    req.session.uid = null;
    req.sessionStorage.save(req.session);
  }
  res.status("no content");
});
```

Finally when using the collection-based session storage, it's a good idea to clean up expired sessions in a script which we can periodically call via an external tool like `cron` or a [Foxx queue](../Reference/Modules/Queues.md):

```js
"use strict";
const sessions = require("./util/sessions");
module.exports = sessions.storage.prune();
```

Using ArangoDB authentication
-----------------------------

When using HTTP Basic authentication, ArangoDB will set the `arangoUser` attribute of the [request object](../Reference/Routers/Request.md) if the credentials match a valid ArangoDB user for the database.

**Note**: Although the presence and value of this attribute can be used to implement a low-level authentication mechanism this is only useful if your service is only intended to be used by developers who already have access to the HTTP API or the administrative web interface.

Example:

```js
router.get("/me", function(req, res) {
  if (req.arangoUser) {
    res.json({ username: req.arangoUser });
  } else {
    res.throw("not found");
  }
});
```

Alternative sessions implementation
-----------------------------------

If you need more control than the sessions middleware provides, you can also create a basic session system with a few lines of code yourself:

```js
"use strict";
const sessions = module.context.collection("sessions");
// This is the secret string used to sign cookies
// you probably don't want to hardcode this.
const secret = "some secret string";

module.context.use((req, res, next) => {
  // First read the session cookie if present
  let sid = req.cookie("sid", { secret });
  if (sid) {
    try {
      // Try to find a matching session
      req.session = sessions.document(sid);
    } catch (e) {
      // No session found, cookie is invalid
      sid = null;
      // Clear the cookie so it will be discarded
      res.cookie("sid", "", { ttl: -1, secret });
    }
  }
  try {
    // Continue handling the request
    next();
  } finally {
    // Do this even if the request threw
    if (req.session) {
      if (sid) {
        // Sync the session's changes to the db
        sessions.update(sid, req.session);
      } else {
        // Create a new session with a new key
        sid = sessions.save(req.session)._key;
      }
      // Set or update the session cookie
      res.cookie("sid", sid, { ttl: 24 * 60 * 60, secret });
    } else if (sid) {
      // The request handler explicitly cleared
      // the session, so we need to delete it
      sessions.remove(sid);
      // And clear the cookie too
      res.cookie("sid", "", { ttl: -1, secret });
    }
  }
});
```
