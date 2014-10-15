/*global require, exports */

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

var Controller,
  RequestContext = require("org/arangodb/foxx/request_context").RequestContext,
  RequestContextBuffer = require("org/arangodb/foxx/request_context").RequestContextBuffer,
  BaseMiddleware = require("org/arangodb/foxx/base_middleware").BaseMiddleware,
  _ = require("underscore"),
  extend = _.extend,
  is = require("org/arangodb/is"),
  internal = require("org/arangodb/foxx/internals");

// -----------------------------------------------------------------------------
// --SECTION--                                                       Controller
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_controller_initializer
///
/// `new FoxxController(applicationContext, options)`
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
///   app = new Controller(applicationContext, {
///     urlPrefix: "/meadow"
///   });
/// ```
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

Controller = function (context, options) {
  'use strict';
  var urlPrefix, baseMiddleware;

  if (is.notExisty(context)) {
    throw new Error("parameter <context> is missing");
  }

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

  baseMiddleware = new BaseMiddleware();

  this.routingInfo.middleware = [
    {
      url: { match: "/*" },
      action: {
        callback: baseMiddleware.stringRepresentation,
        options: {
          name: context.name,
          version: context.version,
          appId: context.appId,
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
};

extend(Controller.prototype, {
  currentPriority: 0,

  collection: function (name) {
    'use strict';
    require('console').log('Controller#collection is deprecated, use appContext#collection instead');
    return this.applicationContext.collection(name);
  },

  addInjector: function (name, factory) {
    'use strict';
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
    'use strict';
    var constraints = {queryParams: {}, urlParams: {}},
      newRoute = internal.constructRoute(method, route, callback, this, constraints),
      requestContext = new RequestContext(this.allRoutes, this.models, newRoute, this.rootElement, constraints),
      summary,
      undocumentedBody;

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
/// `FoxxController#head(path, callback)`
///
/// This handles requests from the HTTP verb *head*.  You have to give a
/// function as *callback*. It will get a request and response object as its
/// arguments
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

  head: function (route, callback) {
    'use strict';
    return this.handleRequest("head", route, callback);
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_controller_get
///
/// `FoxxController#get(path, callback)`
///
/// This handles requests from the HTTP verb *get*.
///
/// When defining a route you can also define a so called 'parameterized'
/// *path* like */goose/:barn*. In this case you can later get the value
/// the user provided for *barn* via the *params* function (see the Request
/// object).
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
    'use strict';
    return this.handleRequest("get", route, callback);
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_controller_post
///
/// `FoxxController#post(path, callback)`
///
/// This handles requests from the HTTP verb *post*.  See above for the
/// arguments you can give.
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
    'use strict';
    return this.handleRequest("post", route, callback);
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_controller_put
///
/// `FoxxController#put(path, callback)`
///
/// This handles requests from the HTTP verb *put*.  See above for the arguments
/// you can give.
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
    'use strict';
    return this.handleRequest("put", route, callback);
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_controller_patch
///
/// `FoxxController#patch(path, callback)`
///
/// This handles requests from the HTTP verb *patch*.  See above for the
/// arguments you can give.
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
    'use strict';
    return this.handleRequest("patch", route, callback);
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_controller_delete
///
/// `FoxxController#delete(path, callback)`
///
/// This handles requests from the HTTP verb *delete*.  See above for the
/// arguments you can give.
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

  'delete': function (route, callback) {
    'use strict';
    return this.handleRequest("delete", route, callback);
  },

  del: function (route, callback) {
    'use strict';
    return this['delete'](route, callback);
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_controller_before
///
/// `FoxxController#before(path, callback)`
///
/// The before function takes a *path* on which it should watch and a
/// function that it should execute before the routing takes place. If you do
/// omit the path, the function will be executed before each request, no matter
/// the path. Your function gets a Request and a Response object.
///
/// If your callback returns the Boolean value *false*, the route handling
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
    'use strict';
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
/// `FoxxController#after(path, callback)`
///
/// This works pretty similar to the before function.  But it acts after the
/// execution of the handlers (Big surprise, I suppose).
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
    'use strict';
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
/// `FoxxController#around(path, callback)`
///
/// The around function takes a *path* on which it should watch and a function
/// that it should execute around the function which normally handles the
/// route. If you do omit the path, the function will be executed before each
/// request, no matter the path.  Your function gets a Request and a Response
/// object and a next function, which you must call to execute the handler for
/// that route.
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
    'use strict';

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
/// @fn JSF_foxx_controller_getUsers
/// @brief Get the users of this controller
////////////////////////////////////////////////////////////////////////////////
  getUsers: function () {
    'use strict';
    var foxxAuthentication = require("org/arangodb/foxx/authentication"),
      users = new foxxAuthentication.Users(this.applicationContext);

    return users;
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_controller_getAuth
/// @brief Get the auth object of this controller
////////////////////////////////////////////////////////////////////////////////
  getAuth: function () {
    'use strict';
    if (is.notExisty(this.auth)) {
      throw new Error("Setup authentication first");
    }

    return this.auth;
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_controller_activateAuthentication
///
/// `FoxxController#activateAuthentication(opts)`
///
/// To activate authentication for this authentication, first call this function.
/// Provide the following arguments:
///
/// * *type*: Currently we only support *cookie*, but this will change in the future
/// * *cookieLifetime*: An integer. Lifetime of cookies in seconds
/// * *cookieName*: A string used as the name of the cookie
/// * *sessionLifetime*: An integer. Lifetime of sessions in seconds
///
/// @EXAMPLES
///
/// ```js
///     app.activateAuthentication({
///       type: "cookie",
///       cookieLifetime: 360000,
///       cookieName: "my_cookie",
///       sessionLifetime: 400,
///     });
/// ```
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
  activateAuthentication: function (opts) {
    'use strict';
    var authentication = require("org/arangodb/foxx/authentication");

    this.auth = authentication.createAuthObject(this.applicationContext, opts);
    this.before("/*", authentication.createAuthenticationMiddleware(this.auth, this.applicationContext));
    this.after("/*", authentication.createSessionUpdateMiddleware());
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
    'use strict';
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
    'use strict';
    var authentication = require("org/arangodb/foxx/authentication");
    return this.post(route, authentication.createStandardLogoutHandler(this.getAuth(), opts));
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_controller_register
///
/// `FoxxController#register(path, opts)`
///
/// This works pretty similar to the logout function and adds a path to your
/// app for the register functionality. You can customize it with a custom `*nSuccess*
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
    'use strict';
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
    'use strict';
    var authentication = require("org/arangodb/foxx/authentication");
    return this.post(route, authentication.createStandardChangePasswordHandler(this.getUsers(), opts));
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_controller_getSessions
/// @brief Get the sessions object of this controller
////////////////////////////////////////////////////////////////////////////////
  getSessions: function () {
    'use strict';
    return this.sessions;
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_controller_activateSessions
///
/// `FoxxController#activateAuthentication(opts)`
///
/// To activate sessions for this sessions, first call this function.
/// Provide the following arguments:
///
/// * *type*: Currently we only support *cookie*, but this will change in the future. Defaults to *"cookie"*.
/// * *cookieName*: A string used as the name of the cookie. Defaults to *"sid"*.
/// * *cookieSecret*: A secret string used to sign the cookie (as "*cookieName*_sig"). Optional.
/// * *autoCreateSession*: Whether to always create a session if none exists. Defaults to *true*.
/// * *sessionStorageApp*: Mount path of the app to use for sessions. Defaults to */_system/sessions*
///
///
/// @EXAMPLES
///
/// ```js
/// app.activateSessions({
///   type: "cookie",
///   cookieName: "my_cookie",
///   autoCreateSession: true,
///   sessionStorageApp: "/my-sessions"
/// });
/// ```
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
  activateSessions: function (opts) {
    'use strict';
    var sessions = require("org/arangodb/foxx/sessions");

    this.sessions = new sessions.Sessions(opts);
    sessions.decorateController(this.sessions, this);
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
    'use strict';
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
});

exports.Controller = Controller;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
