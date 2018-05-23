# Foxx Authentication

## Problem

I want to add authentication to my existing Foxx app: Users need to be able to register, log in and log out. I also want to be able to restrict access to certain routes in my app.

**Note:** For this recipe you need at least ArangoDB 2.5. For ArangoDB 2.4 look at the [old Foxx Authentication](FoxxAuthLegacy.md).

## Solution

In order to introduce authentication to the todo app we [built](FoxxFirstSteps.md), we first need to create a route to register users. In order to do that, we first introduce a new Foxx.Model *Credentials* with `username` and `password`:

```js
var Foxx = require('org/arangodb/foxx'),
  Joi = require('joi'),
  Credentials;

Credentials = Foxx.Model.extend({
  schema: {
    username: Joi.string().required(),
    password: Joi.string().required()
  }
});

exports.Credentials = Credentials;
```

In our controller we first require it via `Credentials = require('models/credentials').Credentials`, then we create a new route to register people. Let's first see that we get all information we require by giving back the username and password. We also set the status code to be a 201 as we will create a new resource.

```js
/** Register a new user
 *
 * Add a new user to the system with a given username and password
 */
controller.post('/register', function(req, res) {
  var credentials = req.params('credentials'),
    username = credentials.get('username'),
    password = credentials.get('password');

  res.status(201);
  res.json({
    username: username,
    password: password
  });
}).bodyParam('credentials', {description: 'Username and Password', type: Credentials});
```

Now we need to have a place to store users. In order to do that, we will use the functionality of two other foxx applications by requiring them. We're loading the exported APIs of the built-in user and auth apps (we will see what that means in a minute). Like the built-in sessions app, a copy of the user and auth apps are automatically mounted at these mount points by ArangoDB. So all we do in our controller is the following:

```js
var auth = Foxx.requireApp('/_system/simple-auth').auth,
  users = Foxx.requireApp('/_system/users').userStorage;
```

We can now use the exports of the two other Foxx apps as `auth` and `users`. Let's use the authentication app to hash and salt our password:

```js
controller.post('/register', function(req, res) {
  var credentials = req.params('credentials'),
    username = credentials.get('username'),
    password = auth.hashPassword(credentials.get('password'));

  res.status(201);
  res.json({
    username: username,
    password: password
  });
}).bodyParam('credentials', {description: 'Username and Password', type: Credentials});
```

Great! Now let's use our users app to store a new user. The `users` object has a create method which takes three arguments: The username, a user profile (which we will leave empty for now) and a password object. Let's create a user:

```js
controller.post('/register', function(req, res) {
  var credentials = req.params('credentials'),
    username = credentials.get('username'),
    password = auth.hashPassword(credentials.get('password')),
    user;

  user = users.create(username, {}, { simple: password });

  res.status(201);
  res.json({
    username: username
  });
}).bodyParam('credentials', {description: 'Username and Password', type: Credentials});
```

Now create a user with username and password via the route. To see the created user, we will look into the user collection of ArangoDB. First, go to the collections tab. It is a system collection, so we first need to show system collections by clicking on the cogwheel and activating system collections. Then click on the collection users and you will see your new users. Great!

Now let's clean this up a little. We want to have a helper function that will create users for us and do hashing etc. Let's create a `util` folder and create an `auth.js` file there:

```js
var _ = require('underscore'),
  Foxx = require('org/arangodb/foxx'),
  auth = Foxx.requireApp('/_system/simple-auth').auth,
  users = Foxx.requireApp('/_system/users').userStorage;

function createUser(credentials) {
  var password = auth.hashPassword(credentials.get('password'));
  return users.create(credentials.get('username'), {}, { simple: password });
}

exports.createUser = createUser;
```

This will export a function, that we can now use instead of the other two imports:

```js
var createUser = require('./util/auth').createUser;
```

And now we can simplify our controller:

```js
controller.post('/register', function(req, res) {
  var credentials = req.params('credentials'),
    user = createUser(credentials);

  res.status(201);
  res.json({
    username: user.get('user')
  });
}).bodyParam('credentials', {description: 'Username and Password', type: Credentials});
```

We now have a dedicated place to handle creating and searching for users. It is also a good place for raising exceptions if something goes wrong -- for example if the user is not found. In that case we would catch the exception from the module and translate it into our own, application specific error that we can then add as an errorReponse to the routes in our FoxxController. It is also a good place to put our code to find a user by his or her credentials. But first we need to activate sessions for this controller (in your app, the cookie secret should be something app-specific, **really secret**):

```js
controller.activateSessions({
  type: 'cookie',
  cookie: {
    secret: 'keep this secret'
  }
});
```

Now we can add another utility function to log in a user. It will take the provided credentials and the session. It will throw an error when the credentials are not valid (you can create a file `not-logged-in.js` in an `error` folder and require it from both here and the controller) -- otherwise it will set the session data and return the user.

```js
function login(credentials, session) {
  var user = users.resolve(credentials.get('username'));
  var valid = auth.verifyPassword(
    user ? user.get('authData').simple : {},
    credentials.get('password')
  );

  if (!valid) {
    throw new NotLoggedIn();
  }

  session.get('sessionData').username = user.get('user');
  session.setUser(user);
  session.save();
  return user;
}

exports.login = login;
```

Let's use this in a new login route:

```js
var login = require('./util/auth').login;

controller.post('/login', function (req, res) {
  var credentials = req.params('credentials'),
    user = login(credentials, req.session);

  res.status(201);
  res.json({
    msg: 'Welcome ' + user.get('user')
  });
}).bodyParam('credentials', {description: 'Username and Password', type: Credentials})
  .errorResponse(NotLoggedIn, 401, 'Login failed');
```

And a logout route like that:

```js
controller.destroySession('/logout', function(req, res) {
  res.json({success: true});
});
```

And now we have authentication and registration set up. We can now restrict access to logged in users only in any of our routes with `.onlyIfAuthenticated();` and do more complex things by accessing the session in an `onlyIf` call. Let's restrict the access to the list of todos to logged in users:

```js
controller.get('/', function (req, res) {
  res.json(_.map(todos.all(), function (todo) {
    return todo.forClient();
  }));
}).onlyIfAuthenticated();
```

**Author**: [Lucas Dohmen](https://github.com/moonglum)

**Tags**: #foxx
