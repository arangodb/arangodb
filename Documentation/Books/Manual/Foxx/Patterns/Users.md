User management
===============

Foxx does not provide any user management out of the box but it is very easy to roll your own solution:

The [session middleware](../Reference/Sessions/README.md) provides mechanisms for adding session logic to your service, using e.g. a collection or JSON Web Tokens to store the sessions between requests.

The [auth module](../Reference/Modules/Auth.md) provides utilities for basic password verification and hashing.

The following example service demonstrates how user management can be implemented using these basic building blocks.

Setting up the collections
--------------------------

Let's say we want to store sessions and users in collections. We can use the setup script to make sure these collections are created before the service is mounted.

First add a setup script to your manifest if it isn't already defined:

```json
"scripts": {
  "setup": "scripts/setup.js"
}
```

Then create the setup script with the following content:

```js
'use strict';
const db = require('@arangodb').db;
const sessions = module.context.collectionName('sessions');
const users = module.context.collectionName('users');

if (!db._collection(sessions)) {
  db._createDocumentCollection(sessions);
}

if (!db._collection(users)) {
  db._createDocumentCollection(users);
}

db._collection(users).ensureIndex({
  type: 'hash',
  fields: ['username'],
  unique: true
});
```

Creating the router
-------------------

The following main file demonstrates basic user management:

```js
'use strict';
const joi = require('joi');
const createAuth = require('@arangodb/foxx/auth');
const createRouter = require('@arangodb/foxx/router');
const sessionsMiddleware = require('@arangodb/foxx/sessions');

const auth = createAuth();
const router = createRouter();
const users = module.context.collection('users');
const sessions = sessionsMiddleware({
  storage: module.context.collection('sessions'),
  transport: 'cookie'
});
module.context.use(sessions);
module.context.use(router);

router.get('/whoami', function (req, res) {
  try {
    const user = users.document(req.session.uid);
    res.send({username: user.username});
  } catch (e) {
    res.send({username: null});
  }
})
.description('Returns the currently active username.');

router.post('/login', function (req, res) {
  // This may return a user object or null
  const user = users.firstExample({
    username: req.body.username
  });
  const valid = auth.verify(
    // Pretend to validate even if no user was found
    user ? user.authData : {},
    req.body.password
  );
  if (!valid) res.throw('unauthorized');
  // Log the user in
  req.session.uid = user._key;
  req.sessionStorage.save(req.session);
  res.send({sucess: true});
})
.body(joi.object({
  username: joi.string().required(),
  password: joi.string().required()
}).required(), 'Credentials')
.description('Logs a registered user in.');

router.post('/logout', function (req, res) {
  if (req.session.uid) {
    req.session.uid = null;
    req.sessionStorage.save(req.session);
  }
  res.send({success: true});
})
.description('Logs the current user out.');

router.post('/signup', function (req, res) {
  const user = req.body;
  try {
    // Create an authentication hash
    user.authData = auth.create(user.password);
    delete user.password;
    const meta = users.save(user);
    Object.assign(user, meta);
  } catch (e) {
    // Failed to save the user
    // We'll assume the UniqueConstraint has been violated
    res.throw('bad request', 'Username already taken', e);
  }
  // Log the user in
  req.session.uid = user._key;
  req.sessionStorage.save(req.session);
  res.send({success: true});
})
.body(joi.object({
  username: joi.string().required(),
  password: joi.string().required()
}).required(), 'Credentials')
.description('Creates a new user and logs them in.');
```
