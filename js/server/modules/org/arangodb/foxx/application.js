/*jslint indent: 2, nomen: true, maxlen: 120 */
/*global module, require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx application
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

var Application,
  RequestContext = require("org/arangodb/foxx/request_context").RequestContext,
  db = require("org/arangodb").db,
  BaseMiddleware = require("org/arangodb/foxx/base_middleware").BaseMiddleware,
  _ = require("underscore"),
  extend = _.extend,
  is = require("org/arangodb/is"),
  internal = require("org/arangodb/foxx/internals"),
  defaultsFor = {};

defaultsFor.login = {
  usernameField: "username",
  passwordField: "password",

  onSuccess: function (req, res) {
    res.json({
      user: req.user.identifier,
      key: req.currentSession._key
    });
  },

  onError: function (req, res) {
    res.status(401);
    res.json({
      error: "Username or Password was wrong"
    });
  }
};

defaultsFor.logout = {
  onSuccess: function (req, res) {
    res.json({
      notice: "Logged out!",
    });
  },

  onError: function (req, res) {
    res.status(401);
    res.json({
      error: "No session was found"
    });
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                       Application
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_application_initializer
/// @brief Create a new Application
///
/// @FUN{new FoxxApplication(@FA{applicationContext}, @FA{options})}
///
/// This creates a new Application. The first argument is the application
/// context available in the variable `applicationContext`. The second one is an
/// options array with the following attributes:
///
/// * `urlPrefix`: All routes you define within will be prefixed with it.
///
/// @EXAMPLES
///
/// @code
///     app = new Application(applicationContext, {
///       urlPrefix: "/meadow"
///     });
/// @endcode
////////////////////////////////////////////////////////////////////////////////

Application = function (context, options) {
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

  context.foxxes.push(this);

  this.applicationContext = context;
};

extend(Application.prototype, {
  currentPriority: 0,

  collection: function (name) {
    'use strict';
    var collection, cname, prefix;
    prefix = this.collectionPrefix;

    if (prefix === "") {
      cname = name;
    } else {
      cname = prefix + "_" + name;
    }

    collection = db._collection(cname);

    if (!collection) {
      throw new Error("collection with name '" + cname + "' does not exist.");
    }
    return collection;
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_application_handleRequest
/// @brief Handle a request
///
/// The `handleRequest` method is the raw way to create a new route. You
/// probably wont call it directly, but it is used in the other request methods:
///
/// When defining a route you can also define a so called 'parameterized' route
/// like `/goose/:barn`. In this case you can later get the value the user
/// provided for `barn` via the `params` function (see the Request object).
////////////////////////////////////////////////////////////////////////////////

  handleRequest: function (method, route, callback) {
    'use strict';
    var newRoute = internal.constructRoute(method, route, callback),
      requestContext = new RequestContext(newRoute),
      summary;

    this.routingInfo.routes.push(newRoute);

    if (this.applicationContext.comments.length > 0) {
      do {
        summary = this.applicationContext.comments.shift();
      } while (summary === "");
      requestContext.summary(summary || "");
      requestContext.notes(this.applicationContext.comments.join("\n"));
    }

    this.applicationContext.clearComments();

    return requestContext;
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_application_head
/// @brief Handle a `head` request
///
/// @FUN{FoxxApplication::head(@FA{path}, @FA{callback})}
///
/// This handles requests from the HTTP verb `head`.  You have to give a
/// function as @FA{callback}. It will get a request and response object as its
/// arguments
////////////////////////////////////////////////////////////////////////////////

  head: function (route, callback) {
    'use strict';
    return this.handleRequest("head", route, callback);
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_application_get
/// @brief Manage a `get` request
///
/// @FUN{FoxxApplication::get(@FA{path}, @FA{callback})}
///
/// This handles requests from the HTTP verb `get`.
///
/// When defining a route you can also define a so called 'parameterized'
/// @FA{path} like `/goose/:barn`. In this case you can later get the value
/// the user provided for `barn` via the `params` function (see the Request
/// object).
///
/// @EXAMPLES
///
/// @code
///     app.get('/goose/barn', function (req, res) {
///       // Take this request and deal with it!
///     });
/// @endcode
////////////////////////////////////////////////////////////////////////////////

  get: function (route, callback) {
    'use strict';
    return this.handleRequest("get", route, callback);
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_application_post
/// @brief Tackle a `post` request
///
/// @FUN{FoxxApplication::post(@FA{path}, @FA{callback})}
///
/// This handles requests from the HTTP verb `post`.  See above for the
/// arguments you can give.
///
/// @EXAMPLES
///
/// @code
///     app.post('/goose/barn', function (req, res) {
///       // Take this request and deal with it!
///     });
/// @endcode
////////////////////////////////////////////////////////////////////////////////

  post: function (route, callback) {
    'use strict';
    return this.handleRequest("post", route, callback);
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_application_put
/// @brief Sort out a `put` request
///
/// @FUN{FoxxApplication::put(@FA{path}, @FA{callback})}
///
/// This handles requests from the HTTP verb `put`.  See above for the arguments
/// you can give.
///
/// @EXAMPLES
///
/// @code
///     app.put('/goose/barn', function (req, res) {
///       // Take this request and deal with it!
///     });
/// @endcode
////////////////////////////////////////////////////////////////////////////////

  put: function (route, callback) {
    'use strict';
    return this.handleRequest("put", route, callback);
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_application_patch
/// @brief Take charge of a `patch` request
///
/// @FUN{FoxxApplication::patch(@FA{path}, @FA{callback})}
///
/// This handles requests from the HTTP verb `patch`.  See above for the
/// arguments you can give.
///
/// @EXAMPLES
///
/// @code
///     app.patch('/goose/barn', function (req, res) {
///       // Take this request and deal with it!
///     });
/// @endcode
////////////////////////////////////////////////////////////////////////////////

  patch: function (route, callback) {
    'use strict';
    return this.handleRequest("patch", route, callback);
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_application_delete
/// @brief Respond to a `delete` request
///
/// @FUN{FoxxApplication::delete(@FA{path}, @FA{callback})}
///
/// This handles requests from the HTTP verb `delete`.  See above for the
/// arguments you can give.
/// 
/// @warning Do not forget that `delete` is a reserved word in JavaScript and
/// therefore needs to be called as app['delete']. There is also an alias `del`
/// for this very reason.
///
/// @EXAMPLES
///
/// @code
///     app['delete']('/goose/barn', function (req, res) {
///       // Take this request and deal with it!
///     });
///
///     app.del('/goose/barn', function (req, res) {
///       // Take this request and deal with it!
///     });
/// @endcode
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
/// @fn JSF_foxx_application_before
/// @brief Before
///
/// @FUN{FoxxApplication::before(@FA{path}, @FA{callback})}
///
/// The before function takes a @FA{path} on which it should watch and a
/// function that it should execute before the routing takes place. If you do
/// omit the path, the function will be executed before each request, no matter
/// the path.  Your function gets a Request and a Response object.
///
/// @EXAMPLES
///
/// @code
///     app.before('/high/way', function(req, res) {
///       //Do some crazy request logging
///     });
/// @endcode
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
          func(req, res, opts);
          next();
        }
      }
    });
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_application_after
/// @brief After
///
/// @FUN{FoxxApplication::after(@FA{path}, @FA{callback})}
///
/// This works pretty similar to the before function.  But it acts after the
/// execution of the handlers (Big surprise, I suppose).
///
/// @EXAMPLES
///
/// @code
///     app.after('/high/way', function(req, res) {
///       //Do some crazy response logging
///     });
/// @endcode
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
/// @fn JSF_foxx_application_activateAuthentication
/// @brief Activate authentication for this app
///
/// @FUN{FoxxApplication::activateAuthentication(@FA{opts})}
///
/// To activate authentication for this authentication, first call this function.
/// Provide the following arguments:
///
/// * `type`: Currently we only support `cookie`, but this will change in the future.
/// * `cookieLifetime`: An integer. Lifetime of cookies in seconds.
/// * `cookieName`: A string used as the name of the cookie.
/// * `sessionLifetime`: An integer. Lifetime of sessions in seconds.
///
///
/// @EXAMPLES
///
/// @code
///     app.activateAuthentication({
///       type: "cookie",
///       cookieLifetime: 360000,
///       cookieName: "my_cookie",
///       sessionLifetime: 400,
///     });
/// @endcode
////////////////////////////////////////////////////////////////////////////////
  activateAuthentication: function (opts) {
    var foxxAuthentication = require("org/arangodb/foxx/authentication"),
      sessions,
      cookieAuth,
      app = this,
      applicationContext = this.applicationContext,
      options = opts || {};

    if (options.type !== "cookie") {
      throw new Error("Currently only the following auth types are supported: cookie");
    }
    if (is.falsy(options.cookieLifetime)) {
      throw new Error("Please provide the cookieLifetime");
    }
    if (is.falsy(options.cookieName)) {
      throw new Error("Please provide the cookieName");
    }
    if (is.falsy(options.sessionLifetime)) {
      throw new Error("Please provide the sessionLifetime");
    }

    sessions = new foxxAuthentication.Sessions(this.applicationContext, {
      lifetime: options.sessionLifetime
    });

    cookieAuth = new foxxAuthentication.CookieAuthentication(this.applicationContext, {
      lifetime: options.cookieLifetime,
      name: options.cookieName
    });

    this.auth = new foxxAuthentication.Authentication(this.applicationContext, sessions, cookieAuth);

    this.before("/*", function (req, res) {
      var users = new foxxAuthentication.Users(applicationContext),
        authResult = app.auth.authenticate(req);

      if (authResult.errorNum === require("internal").errors.ERROR_NO_ERROR) {
        req.currentSession = authResult.session;
        req.user = users.get(authResult.session.identifier);
      } else {
        req.currentSession = null;
        req.user = null;
      }
    });

    this.after("/*", function (req, res) {
      var session = req.currentSession;

      if (is.existy(session)) {
        session.update();
      }
    });
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_application_login
/// @brief Add a login handler
///
/// @FUN{FoxxApplication::login(@FA{path}, @FA{opts})}
///
/// Add a route for the login. You can provide further customizations via the
/// the options:
///
/// `usernameField` and `passwordField` can be used to adjust the expected attributes
/// in the `post` request. They default to `username` and `password`.
/// `onSuccess` is a function that you can define to do something if the login was
/// successful. This includes sending a response to the user. This defaults to a
/// function that returns a JSON with `user` set to the identifier of the user and
/// `key` set to the session key.
/// `onError` is a function that you can define to do something if the login did not
/// work. This includes sending a response to the user. This defaults to a function
/// that sets the response to 401 and returns a JSON with `error` set to
/// "Username or Password was wrong".
/// Both `onSuccess` and `onError` should take request and result as arguments.
///
/// @EXAMPLES
///
/// @code
///     app.login('/login', {
///       onSuccess: function (req, res) {
///         res.json({"success": true});
///       }
///     });
/// @endcode
////////////////////////////////////////////////////////////////////////////////
  login: function (route, opts) {
    var foxxAuthentication = require("org/arangodb/foxx/authentication"),
      auth = this.auth,
      users = new foxxAuthentication.Users(this.applicationContext),
      options = _.defaults(opts || {}, defaultsFor.login);

    this.post(route, function (req, res) {
      var username = req.body()[options.usernameField],
        password = req.body()[options.passwordField];

      if (users.isValid(username, password)) {
        req.currentSession = auth.beginSession(req, res, username, {});
        req.user = users.get(req.currentSession.identifier);
        options.onSuccess(req, res);
      } else {
        options.onError(req, res);
      }
    });
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_application_logout
/// @brief Add a logout handler
///
/// @FUN{FoxxApplication::logout(@FA{path}, @FA{opts})}
///
/// This works pretty similar to the logout function and adds a path to your
/// app for the logout functionality. You can customize it with a custom `onSuccess`
/// and `onError` function:
/// `onSuccess` is a function that you can define to do something if the logout was
/// successful. This includes sending a response to the user. This defaults to a
/// function that returns a JSON with `message` set to "logged out".
/// `onError` is a function that you can define to do something if the logout did not
/// work. This includes sending a response to the user. This defaults to a function
/// that sets the response to 401 and returns a JSON with `error` set to
/// "No session was found".
/// Both `onSuccess` and `onError` should take request and result as arguments.
///
///
/// @EXAMPLES
///
/// @code
///     app.logout('/logout', {
///       onSuccess: function (req, res) {
///         res.json({"message": "Bye, Bye"});
///       }
///     });
/// @endcode
////////////////////////////////////////////////////////////////////////////////
  logout: function (route, opts) {
    var auth = this.auth,
      options = _.defaults(opts || {}, defaultsFor.logout);

    this.post(route, function (req, res) {
      if (is.existy(req.currentSession)) {
        auth.endSession(req, res, req.currentSession._key);
        req.user = null;
        req.currentSession = null;
        options.onSuccess(req, res);
      } else {
        options.onError(req, res);
      }
    });
  }
});

exports.Application = Application;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
