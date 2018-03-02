Foxx Sessions
=============

Foxx provides some convenience methods to make working with sessions easier.

### Activate sessions

Enables session features for the controller.

`controller.activateSessions(options)`

Once sessions have been activated, a *session* property will be added to the *request* object passed to route handlers defined on the controller, which will be a saved instance of the session model provided by the session storage.

If the option *autoCreateSession* has not explicitly been set to *false*, a new session will be created for users that do not yet have an active session.

If *cookie* sessions are used, the session cookie will be updated after every route.

*Parameter*

* *options* (optional): an object with any of the following properties:
  * *sessionStorage* (optional): mount point of the session storage app to use. Alternatively can be passed a session storage implementation directly. Default: `"/_system/sessions"` (the built-in session storage used by system apps).
  * *cookie* (optional): an object with the following properties:
    * *name*: name of the session cookie if using cookie sessions. Default: `"sid"`.
    * *secret* (optional): secret string to sign session cookies with if using cookie sessions.
    * *algorithm* (optional): algorithm to sign session cookies with if using cookie sessions. Default: `"sha256"`.
  * *header* (optional): name of the session header if using header sessions. Default: `"X-Session-Id"`.
  * *autoCreateSession* (optional): whether a session should always be created if none exists. Default: `true`.

If *cookie* is set to a string, its value will be used as the *cookie.name* instead.

If *cookie* is set to `true`, unsigned cookies will be used with the default algorithm and cookie name.

If *header* is set to `true`, it will be used with the default header name.

*Examples*

Example configuration for using signed cookies:

```js
var controller = new FoxxController(applicationContext);
controller.activateSessions({
  sessionStorage: '/_system/sessions',
  cookie: {secret: 'keep-this-string-secret'}
});
```

Example configuration for using a header:

```js
var controller = new FoxxController(applicationContext);
controller.activateSessions({
  sessionStorage: '/_system/sessions',
  header: true
});
```

Example configuration for using a header with a custom name:

```js
var controller = new FoxxController(applicationContext);
controller.activateSessions({
  sessionStorage: '/_system/sessions',
  header: 'X-My-Session-Token'
});
```

Example configuration for using unsigned cookies:

```js
var controller = new FoxxController(applicationContext);
controller.activateSessions({
  sessionStorage: '/_system/sessions',
  cookie: true
});
```

Example configuration for using unsigned cookies and a header:

```js
var controller = new FoxxController(applicationContext);
controller.activateSessions({
  sessionStorage: '/_system/sessions',
  cookie: true,
  header: true
});
```

### Define a session destruction route

Defines a route that will destroy the session.

`controller.destroySession(path, options)`

Defines a route handler on the controller that destroys the session.

When using cookie sessions, this function will clear the session cookie (if *autoCreateSession* was disabled) or create a new session cookie, before calling the *after* function.

*Parameter*

* *path*: route path as passed to *controller.get*, *controller.post*, etc.
* *options* (optional): an object with any of the following properties:
  * *method* (optional): HTTP method to handle. Default: `"post"`.
  * *before* (optional): function to execute before the session is destroyed. Receives the same arguments as a regular route handler.
  * *after* (optional): function to execute after the session is destroyed. Receives the same arguments as a regular route handler. Default: a function that sends a *{"message": "logged out"}* JSON response.
