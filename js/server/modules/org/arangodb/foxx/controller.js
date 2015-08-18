'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx Controller
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2013 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Lucas Dohmen
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var RequestContext = require("org/arangodb/foxx/request_context").RequestContext,
  RequestContextBuffer = require("org/arangodb/foxx/request_context").RequestContextBuffer,
  BaseMiddleware = require("org/arangodb/foxx/base_middleware").BaseMiddleware,
  _ = require("underscore"),
  extend = _.extend,
  is = require("org/arangodb/is"),
  internal = require("org/arangodb/foxx/internals"),
  swagger = require("org/arangodb/foxx/swagger");

// -----------------------------------------------------------------------------
// --SECTION--                                                       Controller
// -----------------------------------------------------------------------------

var authControllerProps = {
////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_controller_getUsers
/// @brief Get the users of this controller
////////////////////////////////////////////////////////////////////////////////
  getUsers: function () {
    var foxxAuthentication = require("org/arangodb/foxx/authentication"),
      users = new foxxAuthentication.Users(this.applicationContext);

    return users;
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_controller_getAuth
/// @brief Get the auth object of this controller
////////////////////////////////////////////////////////////////////////////////
  getAuth: function () {
    return this.auth;
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_controller_login
///
/// `FoxxController#login(path, opts)`
///
/// Add a route for the login. You can provide further customizations via the
/// the options:
///
/// * *usernameField* and *passwordField* can be used to adjust the expected attributes
///   in the *post* request. They default to *username* and *password*.
/// * *onSuccess* is a function that you can define to do something if the login was
///   successful. This includes sending a response to the user. This defaults to a
///   function that returns a JSON with *user* set to the identifier of the user and
/// * *key* set to the session key.
/// * *onError* is a function that you can define to do something if the login did not
///   work. This includes sending a response to the user. This defaults to a function
///   that sets the response to 401 and returns a JSON with *error* set to
///   "Username or Password was wrong".
///
/// Both *onSuccess* and *onError* should take request and result as arguments.
///
/// @EXAMPLES
///
/// ```js
/// app.login('/login', {
///   onSuccess: function (req, res) {
///     res.json({"success": true});
///   }
/// });
/// ```
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
  login: function (route, opts) {
    var authentication = require("org/arangodb/foxx/authentication");
    return this.post(route, authentication.createStandardLoginHandler(this.getAuth(), this.getUsers(), opts));
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_controller_logout
///
/// `FoxxController#logout(path, opts)`
///
/// This works pretty similar to the logout function and adds a path to your
/// app for the logout functionality. You can customize it with a custom *onSuccess*
/// and *onError* function:
///
/// * *onSuccess* is a function that you can define to do something if the logout was
///   successful. This includes sending a response to the user. This defaults to a
///   function that returns a JSON with *message* set to "logged out".
/// * *onError* is a function that you can define to do something if the logout did not
///   work. This includes sending a response to the user. This defaults to a function
///   that sets the response to 401 and returns a JSON with *error* set to
///   "No session was found".
///
/// Both *onSuccess* and *onError* should take request and result as arguments.
///
///
/// @EXAMPLES
///
/// ```js
/// app.logout('/logout', {
///   onSuccess: function (req, res) {
///     res.json({"message": "Bye, Bye"});
///   }
/// });
/// ```
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
  logout: function (route, opts) {
    var authentication = require("org/arangodb/foxx/authentication");
    return this.post(route, authentication.createStandardLogoutHandler(this.getAuth(), opts));
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_controller_register
///
/// `FoxxController#register(path, opts)`
///
/// This works pretty similar to the logout function and adds a path to your
/// app for the register functionality. You can customize it with a custom *onSuccess*
/// and *onError* function:
///
/// * *onSuccess* is a function that you can define to do something if the registration was
///   successful. This includes sending a response to the user. This defaults to a
///   function that returns a JSON with *user* set to the created user document.
/// * *onError* is a function that you can define to do something if the registration did not
///   work. This includes sending a response to the user. This defaults to a function
///   that sets the response to 401 and returns a JSON with *error* set to
///   "Registration failed".
///
/// Both *onSuccess* and *onError* should take request and result as arguments.
///
/// You can also set the fields containing the username and password via *usernameField*
/// (defaults to *username*) and *passwordField* (defaults to *password*).
/// If you want to accept additional attributes for the user document, use the option
/// *acceptedAttributes* and set it to an array containing strings with the names of
/// the additional attributes you want to accept. All other attributes in the request
/// will be ignored.
///
/// If you want default attributes for the accepted attributes or set additional fields
/// (for example *admin*) use the option *defaultAttributes* which should be a hash
/// mapping attribute names to default values.
///
/// @EXAMPLES
///
/// ```js
/// app.register('/logout', {
///   acceptedAttributes: ['name'],
///   defaultAttributes: {
///     admin: false
///   }
/// });
/// ```
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
  register: function (route, opts) {
    var authentication = require("org/arangodb/foxx/authentication");
    return this.post(
      route,
      authentication.createStandardRegistrationHandler(this.getAuth(), this.getUsers(), opts)
    ).errorResponse(authentication.UserAlreadyExistsError, 400, "User already exists");
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_controller_changePassword
///
/// FoxxController#changePassword(route, opts)`
///
/// Add a route for the logged in user to change the password.
/// You can provide further customizations via the
/// the options:
///
/// * *passwordField* can be used to adjust the expected attribute
///   in the *post* request. It defaults to *password*.
/// * *onSuccess* is a function that you can define to do something if the change was
///   successful. This includes sending a response to the user. This defaults to a
///   function that returns a JSON with *notice* set to "Changed password!".
/// * *onError* is a function that you can define to do something if the login did not
///   work. This includes sending a response to the user. This defaults to a function
///   that sets the response to 401 and returns a JSON with *error* set to
///   "No session was found".
///
/// Both *onSuccess* and *onError* should take request and result as arguments.
///
/// @EXAMPLES
///
/// ```js
/// app.changePassword('/changePassword', {
///   onSuccess: function (req, res) {
///     res.json({"success": true});
///   }
/// });
/// ```
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
  changePassword: function (route, opts) {
    var authentication = require("org/arangodb/foxx/authentication");
    return this.post(route, authentication.createStandardChangePasswordHandler(this.getUsers(), opts));
  }
};

var sessionControllerProps = {
////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_controller_getSessions
/// @brief Get the sessions object of this controller
////////////////////////////////////////////////////////////////////////////////
  getSessions: function () {
    return this.sessions;
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_controller_destroySession
///
/// `FoxxController#destroySession(path, opts)`
///
/// This adds a path to your app for the destroySession functionality.
/// You can customize it with custom *before* and *after* function:
/// *before* is a function that you can define to do something before
/// the session is destroyed.
/// *after* is a function that you can define to do something after the
/// session is destroyed. This defaults to a function that returns a
/// JSON object with *message* set to "logged out".
/// Both *before* and *after* should take request and result as arguments.
/// If you only want to provide an *after* function, you can pass the
/// function directly instead of an object.
///
/// @EXAMPLES
///
/// ```js
/// app.destroySession('/logout', function (req, res) {
///   res.json({"message": "Bye, Bye"});
/// });
/// ```
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
  destroySession: function (route, opts) {
    var method = opts.method;
    if (typeof method === 'string') {
      method = method.toLowerCase();
    }
    if (!method || typeof this[method] !== 'function') {
      method = 'post';
    }
    var sessions = require("org/arangodb/foxx/sessions");
    return this[method](route, sessions.createDestroySessionHandler(this.getSessions(), opts));
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_controller_initializer
///
/// `new Controller(applicationContext, options)`
///
/// This creates a new Controller. The first argument is the controller
/// context available in the variable *applicationContext*. The second one is an
/// options array with the following attributes:
///
/// * *urlPrefix*: All routes you define within will be prefixed with it.
///
/// @EXAMPLES
///
/// ```js
/// app = new Controller(applicationContext, {
///   urlPrefix: "/meadow"
/// });
/// ```
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function Controller(context, options) {
  var urlPrefix, baseMiddleware;
  context.clearComments();

  if (is.notExisty(context)) {
    throw new Error("parameter <context> is missing");
  }
  context.clearComments();

  this.routingInfo = {
    routes: []
  };

  // Models for the Documentation
  this.models = {};

  options = options || {};
  urlPrefix = options.urlPrefix || "";

  if (urlPrefix === "") {
    urlPrefix = context.prefix;
  } else if (context.prefix !== "") {
    urlPrefix = context.prefix + "/" + urlPrefix;
  }

  this.injected = Object.create(null);
  this.injectors = Object.create(null);
  this.routingInfo.urlPrefix = urlPrefix;
  this.collectionPrefix = context.collectionPrefix;

  this.extensions = {};

  baseMiddleware = new BaseMiddleware();

  this.routingInfo.middleware = [
    {
      url: { match: "/*" },
      action: {
        callback: baseMiddleware.functionRepresentation,
        options: {
          name: context.name,
          version: context.version,
          mount: context.mount,
          isDevelopment: context.isDevelopment,
          isProduction: context.isProduction,
          prefix: context.prefix,
          options: context.options
        }
      }
    }
  ];

  this.allRoutes = new RequestContextBuffer();

  context.foxxes.push(this);

  if (is.existy(context.manifest.rootElement)) {
    this.rootElement = context.manifest.rootElement;
  } else {
    this.rootElement = false;
  }

  this.applicationContext = context;
}

extend(Controller.prototype, {
  currentPriority: 0,

  addInjector: function (name, factory) {
    if (factory === undefined) {
      _.extend(this.injectors, name);
    } else {
      this.injectors[name] = factory;
    }
  },

////////////////////////////////////////////////////////////////////////////////
///
/// The *handleRequest* method is the raw way to create a new route. You
/// probably wont call it directly, but it is used in the other request methods:
///
/// When defining a route you can also define a so called 'parameterized' route
/// like */goose/:barn*. In this case you can later get the value the user
/// provided for *barn* via the *params* function (see the Request object).
////////////////////////////////////////////////////////////////////////////////

  handleRequest: function (method, route, callback) {
    var constraints = {queryParams: {}, urlParams: {}};
    var newRoute = internal.constructRoute(method, route, callback, this, constraints);
    var requestContext = new RequestContext(
      this.allRoutes, this.models, newRoute, route, this.rootElement, constraints, this.extensions
    );
    var summary;
    var undocumentedBody;

    this.routingInfo.routes.push(newRoute);

    if (this.applicationContext.comments.length > 0) {
      do {
        summary = this.applicationContext.comments.shift();
      } while (summary === "");
      requestContext.summary(summary || "");
      requestContext.notes(this.applicationContext.comments.join("\n"));
    }

    this.applicationContext.clearComments();

    if (method === 'post' || method === 'put' || method === 'patch') {
      undocumentedBody = require('org/arangodb/foxx').Model.extend();
      requestContext.bodyParam("undocumented body", {
        description: "Undocumented body param",
        type: undocumentedBody,
        allowInvalid: true
      });
    }

    return requestContext;
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_controller_head
///
/// `Controller.head(path, callback)`
///
/// Defines a new route on `path` that handles requests from the HTTP verb `head`.
/// This route can also be 'parameterized' like `/goose/:barn`.
/// In this case you can later get the value the user provided for `barn`
/// via the `params` function in the `request`.
/// The function defined in `callback` will be invoked whenever this type of
/// request is recieved.
/// `callback` get's two arguments `request` and `response`, see below for further
/// information about these objects.
///
/// @EXAMPLES
///
/// ```js
/// app.head('/goose/barn', function (req, res) {
///   // Take this request and deal with it!
/// });
/// ```
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

  head: function (route, callback) {
    return this.handleRequest("head", route, callback);
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_controller_get
///
/// `Controller.get(path, callback)`
///
/// Defines a new route on `path` that handles requests from the HTTP verb `get`.
/// This route can also be 'parameterized' like `/goose/:barn`.
/// In this case you can later get the value the user provided for `barn`
/// via the `params` function in the `request`.
/// The function defined in `callback` will be invoked whenever this type of
/// request is recieved.
/// `callback` get's two arguments `request` and `response`, see below for further
/// information about these objects.
///
/// @EXAMPLES
///
/// ```js
/// app.get('/goose/barn', function (req, res) {
///   // Take this request and deal with it!
/// });
/// ```
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

  get: function (route, callback) {
    return this.handleRequest("get", route, callback);
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_controller_post
///
/// `Controller.post(path, callback)`
///
/// Defines a new route on `path` that handles requests from the HTTP verb `post`.
/// This route can also be 'parameterized' like `/goose/:barn`.
/// In this case you can later get the value the user provided for `barn`
/// via the `params` function in the `request`.
/// The function defined in `callback` will be invoked whenever this type of
/// request is recieved.
/// `callback` get's two arguments `request` and `response`, see below for further
/// information about these objects.
///
/// @EXAMPLES
///
/// ```js
/// app.post('/goose/barn', function (req, res) {
///   // Take this request and deal with it!
/// });
/// ```
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

  post: function (route, callback) {
    return this.handleRequest("post", route, callback);
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_controller_put
///
/// `Controller.put(path, callback)`
///
/// Defines a new route on `path` that handles requests from the HTTP verb `put`.
/// This route can also be 'parameterized' like `/goose/:barn`.
/// In this case you can later get the value the user provided for `barn`
/// via the `params` function in the `request`.
/// The function defined in `callback` will be invoked whenever this type of
/// request is recieved.
/// `callback` get's two arguments `request` and `response`, see below for further
/// information about these objects.
///
/// @EXAMPLES
///
/// ```js
/// app.put('/goose/barn', function (req, res) {
///   // Take this request and deal with it!
/// });
/// ```
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

  put: function (route, callback) {
    return this.handleRequest("put", route, callback);
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_controller_patch
///
/// `Controller.patch(path, callback)`
///
/// Defines a new route on `path` that handles requests from the HTTP verb `patch`.
/// This route can also be 'parameterized' like `/goose/:barn`.
/// In this case you can later get the value the user provided for `barn`
/// via the `params` function in the `request`.
/// The function defined in `callback` will be invoked whenever this type of
/// request is recieved.
/// `callback` get's two arguments `request` and `response`, see below for further
/// information about these objects.
///
/// @EXAMPLES
///
/// ```js
/// app.patch('/goose/barn', function (req, res) {
///   // Take this request and deal with it!
/// });
/// ```
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

  patch: function (route, callback) {
    return this.handleRequest("patch", route, callback);
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_controller_delete
///
/// `Controller.delete(path, callback)`
///
/// Defines a new route on `path` that handles requests from the HTTP verb `delete`.
/// This route can also be 'parameterized' like `/goose/:barn`.
/// In this case you can later get the value the user provided for `barn`
/// via the `params` function in the `request`.
/// The function defined in `callback` will be invoked whenever this type of
/// request is recieved.
/// `callback` get's two arguments `request` and `response`, see below for further
/// information about these objects.
///
/// **Warning**: Do not forget that *delete* is a reserved word in JavaScript and
/// therefore needs to be called as app['delete']. There is also an alias *del*
/// for this very reason.
///
/// @EXAMPLES
///
/// ```js
/// app['delete']('/goose/barn', function (req, res) {
///   // Take this request and deal with it!
/// });
///
/// app.del('/goose/barn', function (req, res) {
///   // Take this request and deal with it!
/// });
/// ```
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

  delete: function (route, callback) {
    return this.handleRequest("delete", route, callback);
  },

  del: function (route, callback) {
    return this.delete(route, callback);
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_controller_before
///
/// `Controller.before(path, callback)`
///
/// Defines an additional function on the route `path` which will be executed
/// before the callback defined for a specific HTTP verb is executed.
/// The `callback` function has the same signature as the `callback` in the
/// specific route.
/// You can also omit the `path`, in this case `callback` will be executed
/// before handleing any request in this Controller.
///
/// If `callback` returns the Boolean value `false`, the route handling
/// will not proceed. You can use this to intercept invalid or unauthorized
/// requests and prevent them from being passed to the matching routes.
///
/// @EXAMPLES
///
/// ```js
/// app.before('/high/way', function(req, res) {
///   //Do some crazy request logging
/// });
/// ```
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

  before: function (path, func) {
    if (is.notExisty(func)) {
      func = path;
      path = "/*";
    }

    this.routingInfo.middleware.push({
      priority: this.currentPriority = this.currentPriority + 1,
      url: {match: path},
      action: {
        callback: function (req, res, opts, next) {
          var result = func(req, res, opts);
          if (result !== false) {
            next();
          }
        }
      }
    });
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_controller_after
///
/// `Controller.after(path, callback)`
///
/// Similar to `Controller.before(path, callback)` but `callback` will be invoked
/// after the request is handled in the specific route.
///
/// @EXAMPLES
///
/// ```js
/// app.after('/high/way', function(req, res) {
///   //Do some crazy response logging
/// });
/// ```
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

  after: function (path, func) {
    if (is.notExisty(func)) {
      func = path;
      path = "/*";
    }

    this.routingInfo.middleware.push({
      priority: this.currentPriority = this.currentPriority + 1,
      url: {match: path},
      action: {
        callback: function (req, res, opts, next) { next(); func(req, res, opts); }
      }
    });
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_controller_around
///
/// `Controller.around(path, callback)`
///
/// Similar to `Controller.before(path, callback)` `callback` will be invoked
/// instead of the specific handler.
/// `callback` takes two additional paramaters `opts` and `next` where
/// `opts` contains options assigned to the route and `next` is a function.
/// Whenever you call `next` in `callback` the specific handler is invoked,
/// if you do not call `next` the specific handler will not be invoked at all.
/// So using around you can execute code before and after a specific handler
/// and even call the handler only under certain circumstances.
/// If you omit `path` `callback` will be called on every request.
///
/// @EXAMPLES
///
/// ```js
/// app.around('/high/way', function(req, res, opts, next) {
///   //Do some crazy request logging
///   next();
///   //Do some more crazy request logging
/// });
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

  around: function (path, func) {

    if (is.notExisty(func)) {
      func = path;
      path = "/*";
    }

    this.routingInfo.middleware.push({
      priority: this.currentPriority = this.currentPriority + 1,
      url: {match: path},
      action: {
        callback: function (req, res, opts, next) {
          func(req, res, opts, next);
        }
      }
    });
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_controller_activateAuthentication
///
/// `Controller.activateAuthentication(opts)`
///
/// To activate authentication for this controller, call this function before defining any routes.
/// In the `opts` object you can set the following keys:
///
/// * *type*: Currently we only support *cookie*, but this will change in the future
/// * *cookieLifetime*: An integer. Lifetime of cookies in seconds
/// * *cookieName*: A string used as the name of the cookie
/// * *sessionLifetime*: An integer. Lifetime of sessions in seconds
///
/// @EXAMPLES
///
/// ```js
/// app.activateAuthentication({
///   type: "cookie",
///   cookieLifetime: 360000,
///   cookieName: "my_cookie",
///   sessionLifetime: 400,
/// });
/// ```
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
  activateAuthentication: function (opts) {
    var authentication = require("org/arangodb/foxx/authentication");
    _.extend(this, authControllerProps);

    this.auth = authentication.createAuthObject(this.applicationContext, opts);
    this.before("/*", authentication.createAuthenticationMiddleware(this.auth, this.applicationContext));
    this.after("/*", authentication.createSessionUpdateMiddleware());
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_controller_activateSessions
///
/// `Controller.activateSessions(opts)`
///
/// To activate sessions for this controller, call this function before defining any routes.
/// In the `opts` object you can set the following keys:
///
/// * *type*: Currently we only support *cookie*, but this will change in the future. Defaults to *"cookie"*.
/// * *cookieName*: A string used as the name of the cookie. Defaults to *"sid"*.
/// * *cookieSecret*: A secret string used to sign the cookie (as "*cookieName*_sig"). Optional.
/// * *autoCreateSession*: Whether to always create a session if none exists. Defaults to *true*.
/// * *sessionStorage*: Mount path of the app to use for sessions. Defaults to */_system/sessions*
///
///
/// @EXAMPLES
///
/// ```js
/// app.activateSessions({
///   type: "cookie",
///   cookieName: "my_cookie",
///   autoCreateSession: true,
///   sessionStorage: "/my-sessions"
/// });
/// ```
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
  activateSessions: function (opts) {
    var sessions = require("org/arangodb/foxx/sessions");
    _.extend(this, sessionControllerProps);

    this.sessions = new sessions.Sessions(opts);
    sessions.decorateController(this.sessions, this);
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_controller_apiDocumentation
///
/// `Controller.apiDocumentation(path, [opts])`
///
/// Mounts the API documentation (Swagger) at the given `path`.
///
/// Note that the `path` can use URL parameters as usual but must not use any
/// wildcard (`*`) or optional (`:name?`) parameters.
///
/// The optional **opts** can be an object with any of the following properties:
///
/// * **before**: a function that will be executed before a request to
///   this endpoint is processed further.
/// * **appPath**: the mount point of the app for which documentation will be
///   shown. Default: the mount point of the active app.
/// * **indexFile**: file path or file name of the Swagger HTML file.
///   Default: `"index.html"`.
/// * **swaggerJson**: file path or file name of the Swagger API description JSON
///   file or a function `swaggerJson(req, res, opts)` that sends a Swagger API
///   description in JSON. Default: the built-in Swagger description generator.
/// * **swaggerRoot**: absolute path that will be used as the path path for any
///   relative paths of the documentation assets, **swaggerJson** file and
///   the **indexFile**. Default: the built-in Swagger distribution.
///
/// If **opts** is a function, it will be used as the value of **opts.before**.
///
/// If **opts.before** returns `false`, the request will not be processed
/// further.
///
/// If **opts.before** returns an object, any properties will override the
/// equivalent properties of **opts** for the current request.
///
/// Of course all **before**, **after** or **around** functions defined on the
/// controller will also be executed as usual.
///
/// **Examples**
///
/// ```js
/// controller.apiDocumentation('/my/dox');
///
/// ```
///
/// A request to `/my/dox` will be redirect to `/my/dox/index.html`,
/// which will show the API documentation of the active app.
///
/// ```js
/// controller.apiDocumentation('/my/dox', function (req, res) {
///   if (!req.session.get('uid')) {
///     res.status(403);
///     res.json({error: 'only logged in users may see the API'});
///     return false;
///   }
///   return {appPath: req.parameters.mount};
/// });
/// ```
///
/// A request to `/my/dox/index.html?mount=/_admin/aardvark` will show the
/// API documentation of the admin frontend (mounted at `/_admin/aardvark`).
/// If the user is not logged in, the error message will be shown instead.
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
  apiDocumentation: function (route, opts) {
    if (route.charAt(route.length - 1) !== '/') {
      route += '/';
    }
    var mountPath = this.applicationContext.mount;
    return this.get(route + '*', swagger.createSwaggerRouteHandler(mountPath, opts));
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_controller_extend
///
/// `Controller.extend(extensions)`
///
/// Extends all functions to define routes in this controller.
/// This allows to combine several route extensions with the invocation
/// of a single function.
/// This is especially useful if you use the same input parameter in several routes of
/// your controller and want to apply the same validation, documentation and error handling
/// for it.
///
/// The `extensions` parameter is a JSON object with arbitrary keys.
/// Each key is used as the name of the function you want to define (you cannot overwrite
/// internal functions like `pathParam`) and the value is a function that will be invoked.
/// This function can get arbitrary many arguments and the `this` of the function is bound
/// to the route definition object (e.g. you can use `this.pathParam()`).
/// Your newly defined function is chainable similar to the internal functions.
///
/// **Examples**
///
/// Define a validator for a queryParameter, including documentation and errorResponses
/// in a single command:
///
/// ```js
/// controller.extend({
///   myParam: function (maxValue) {
///     this.queryParam("value", {type: joi.number().required()});
///     this.onlyIf(function(req) {
///       var v = req.param("value");
///       if (v > maxValue) {
///         throw new NumberTooLargeError();
///       }
///     });
///     this.errorResponse(NumberTooLargeError, 400, "The given value is too large");
///   }
/// });
///
/// controller.get("/goose/barn", function(req, res) {
///   // Will only be invoked if the request has parameter value and it is less or equal 5.
/// }).myParam(5);
/// ```
/// 
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
  extend: function(extensions) {
    var attr;
    var extensionWrapper = function(scope, functionName) {
      return function() {
        this.applyChain.push({
          functionName: functionName,
          argumentList: arguments
        });
        return this;
      }.bind(scope);
    };
    for (attr in extensions) {
      if (extensions.hasOwnProperty(attr)) {
        this.extensions[attr] = extensions[attr];
        this.allRoutes[attr] = extensionWrapper(this.allRoutes, attr);
      }
    }
  }

});

exports.Controller = Controller;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
