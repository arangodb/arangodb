# Foxx Authentication

## Problem

I want to add authentication to my existing Foxx app: Users need to be able to register, log in and log out. I also want to be able to restrict access to certain routes in my app.

**Note:** For this recipe you need ArangoDB 2.4. For ArangoDB since 2.5 look at the [new Foxx Authentcation](FoxxAuth.md).

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
}).bodyParam('credentials', 'Username and Password', Credentials);
```

Now we need to have a place to store users. In order to do that, we will use the functionality of two other foxx applications by first requiring them and then injecting them. We're injecting the exported APIs of the built-in user and auth apps into our controller (we will see what that means in a minute). Like the built-in sessions app, a copy of the user and auth apps are automatically mounted at these mount points by ArangoDB. So all we do in our controller is the following:

```js
controller.addInjector({
  auth: function() { return Foxx.requireApp('/_system/simple-auth').auth; },
  users: function() { return Foxx.requireApp('/_system/users').userStorage; }
});
```

As a result of that our actions now get a third parameter which is an object with the objects we injected -- in our case `auth` and `users`. Let's use the authentication app to hash and salt our password:

```js
controller.post('/register', function(req, res, injected) {
  var credentials = req.params('credentials'),
    username = credentials.get('username'),
    password = injected.auth.hashPassword(credentials.get('password'));

  res.status(201);
  res.json({
    username: username,
    password: password
  });
}).bodyParam('credentials', 'Username and Password', Credentials);
```

Great! Now let's use our users app to store a new user. The injected `users` object has a create method which takes three arguments: The username, a user profile (which we will leave empty for now) and a password object. Let's create a user:

```js
controller.post('/register', function(req, res, injected) {
  var credentials = req.params('credentials'),
    username = credentials.get('username'),
    password = injected.auth.hashPassword(credentials.get('password')),
    user;

  user = injected.users.create(username, {}, { simple: password });

  res.status(201);
  res.json({
    username: username
  });
}).bodyParam('credentials', 'Username and Password', Credentials);
```

Now create a user with username and password via the route. To see the created user, we will look into the user collection of ArangoDB. First, go to the collections tab. It is a system collection, so we first need to show system collections by clicking on the cogwheel and activating system collections. Then click on the collection users and you will see your new users. Great!

Now let's clean this up a little. We want to have a repository-like structure that will create users for us and do hashing etc. As this requires the two other applications, we have to inject it. Let's create an `injectors` folder and create a `users.js` file there:

```js
var _ = require('underscore'),
  Foxx = require('org/arangodb/foxx'),
  UsersRepository,
  injectUsers;

UsersRepository = function () {
  this.auth = Foxx.requireApp('/_system/simple-auth').auth;
  this.users = Foxx.requireApp('/_system/users').userStorage;
};

_.extend(UsersRepository.prototype, {
  create: function (credentials) {
    var password = this.auth.hashPassword(credentials.get('password'));
    return this.users.create(credentials.get('username'), {}, { simple: password });
  }
});

injectUsers = function () {
  var usersRepository = new UsersRepository();
  return usersRepository;
};

exports.injectUsers = injectUsers;
```

This will export an injector function, that we can now use instead of the other two injections:

```js
controller.addInjector({
  users: injectUsers
});
```

And now we can simplify our controller:

```js
controller.post('/register', function(req, res, injected) {
  var credentials = req.params('credentials'),
    user = injected.users.create(credentials);

  res.status(201);
  res.json({
    username: user.get('user')
  });
}).bodyParam('credentials', 'Username and Password', Credentials);
```

We now have a dedicated place to handle creating and searching for users. It is also a good place for raising exceptions if something goes wrong -- for example if the user is not found. In that case we would catch the exception from the module and translate it into our own, application specific error that we can then add as an errorReponse to the routes in our FoxxController. It is also a good place to put our code to find a user by his or her credentials. But first we need to activate sessions for this controller (in your app, the cookie secret should be something app-specific, **really secret**):

```js
controller.activateSessions({
  type: 'cookie',
  cookie: {
    secret: 'secret'
  }
});
```

Now we can add another method to our injector to log in a user. It will take the provided credentials and the session. It will throw an error when the credentials are not valid (you can create a file `not_logged_in.js` in an `error` folder and require it from both here and the controller) -- otherwise it will set the session data and return the user.

```js
login: function (credentials, session) {
  var user = this.users.resolve(credentials.get('username'));
  var valid = this.auth.verifyPassword(
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
```

Let's use this in a new login route:

```js
controller.post('/login', function (req, res, injected) {
  var credentials = req.params('credentials'),
    user = injected.users.login(credentials, req.session);

  res.status(201);
  res.json({
    msg: 'Welcome ' + user.get('user')
  });
}).bodyParam('credentials', 'Username and Password', Credentials)
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
