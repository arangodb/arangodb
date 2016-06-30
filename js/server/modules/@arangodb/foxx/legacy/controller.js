'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2013-2014 triAGENS GmbH, Cologne, Germany
// / Copyright 2015-2016 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Lucas Dohmen
// / @author Alan Plum
// //////////////////////////////////////////////////////////////////////////////

const RequestContext = require('@arangodb/foxx/legacy/request_context');
const BaseMiddleware = require('@arangodb/foxx/legacy/base_middleware').BaseMiddleware;
const is = require('@arangodb/is');
const internal = require('@arangodb/foxx/legacy/internals');
const swagger = require('@arangodb/foxx/legacy/swagger');

var authControllerProps = {
  // //////////////////////////////////////////////////////////////////////////////
  // / JSF_foxx_controller_getUsers
  // / @brief Get the users of this controller
  // //////////////////////////////////////////////////////////////////////////////
  getUsers() {
    const foxxAuthentication = require('@arangodb/foxx/legacy/authentication');
    return new foxxAuthentication.Users(this.applicationContext);
  },

  // //////////////////////////////////////////////////////////////////////////////
  // / JSF_foxx_controller_getAuth
  // / @brief Get the auth object of this controller
  // //////////////////////////////////////////////////////////////////////////////
  getAuth() {
    return this.auth;
  },

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_controller_login
  // //////////////////////////////////////////////////////////////////////////////
  login(route, opts) {
    var authentication = require('@arangodb/foxx/legacy/authentication');
    return this.post(route, authentication.createStandardLoginHandler(this.getAuth(), this.getUsers(), opts));
  },

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_controller_logout
  // //////////////////////////////////////////////////////////////////////////////
  logout(route, opts) {
    var authentication = require('@arangodb/foxx/legacy/authentication');
    return this.post(route, authentication.createStandardLogoutHandler(this.getAuth(), opts));
  },

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_controller_register
  // //////////////////////////////////////////////////////////////////////////////
  register(route, opts) {
    var authentication = require('@arangodb/foxx/legacy/authentication');
    return this.post(
      route,
      authentication.createStandardRegistrationHandler(this.getAuth(), this.getUsers(), opts)
    ).errorResponse(authentication.UserAlreadyExistsError, 400, 'User already exists');
  },

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_controller_changePassword
  // //////////////////////////////////////////////////////////////////////////////
  changePassword(route, opts) {
    var authentication = require('@arangodb/foxx/legacy/authentication');
    return this.post(route, authentication.createStandardChangePasswordHandler(this.getUsers(), opts));
  }
};

var sessionControllerProps = {
  // //////////////////////////////////////////////////////////////////////////////
  // / JSF_foxx_controller_getSessions
  // / @brief Get the sessions object of this controller
  // //////////////////////////////////////////////////////////////////////////////
  getSessions() {
    return this.sessions;
  },

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief defines a route to logout/destroy the session
  // //////////////////////////////////////////////////////////////////////////////

  destroySession(route, opts) {
    var method = opts.method;
    if (typeof method === 'string') {
      method = method.toLowerCase();
    }
    if (!method || typeof this[method] !== 'function') {
      method = 'post';
    }
    var sessions = require('@arangodb/foxx/legacy/sessions');
    return this[method](route, sessions.createDestroySessionHandler(this.getSessions(), opts));
  }
};

class Controller {

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_controller_initializer
  // //////////////////////////////////////////////////////////////////////////////

  constructor (context, options) {
    this.currentPriority = 0;

    var urlPrefix, baseMiddleware;

    if (is.notExisty(context)) {
      throw new Error('parameter <context> is missing');
    }

    this.routingInfo = {
      routes: []
    };

    // Models for the Documentation
    this.models = {};

    options = options || {};
    urlPrefix = options.urlPrefix || '';

    if (urlPrefix === '') {
      urlPrefix = context.prefix;
    } else if (context.prefix !== '') {
      urlPrefix = context.prefix + '/' + urlPrefix;
    }

    this.injected = Object.create(null);
    this.injectors = Object.create(null);
    this.routingInfo.urlPrefix = urlPrefix;
    this.collectionPrefix = context.collectionPrefix;

    this.extensions = {};

    baseMiddleware = new BaseMiddleware();

    this.routingInfo.middleware = [
      {
        url: { match: '/*' },
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

    this.allRoutes = new RequestContext.Buffer();

    context.foxxes.push(this);

    if (is.existy(context.manifest.rootElement)) {
      this.rootElement = context.manifest.rootElement;
    } else {
      this.rootElement = false;
    }

    this.applicationContext = context;
  }

  addInjector (name, factory) {
    if (factory === undefined) {
      Object.assign(this.injectors, name);
    } else {
      this.injectors[name] = factory;
    }
  }

  // //////////////////////////////////////////////////////////////////////////////
  // /
  // / The *handleRequest* method is the raw way to create a new route. You
  // / probably wont call it directly, but it is used in the other request methods:
  // /
  // / When defining a route you can also define a so called 'parameterized' route
  // / like */goose/:barn*. In this case you can later get the value the user
  // / provided for *barn* via the *params* function (see the Request object).
  // //////////////////////////////////////////////////////////////////////////////

  handleRequest (method, route, callback) {
    var constraints = {queryParams: {}, urlParams: {}};
    var newRoute = internal.constructRoute(method, route, callback, this, constraints);
    var requestContext = new RequestContext(
      this.allRoutes, this.models, newRoute, route, this.rootElement, constraints, this.extensions
    );

    this.routingInfo.routes.push(newRoute);

    if (method === 'post' || method === 'put' || method === 'patch') {
      const Model = require('@arangodb/foxx/legacy').Model;
      let UndocumentedBody = Model.extend({});
      requestContext.bodyParam('undocumented body', {
        description: 'Undocumented body param',
        type: UndocumentedBody,
        allowInvalid: true
      });
    }

    return requestContext;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_controller_head
  // //////////////////////////////////////////////////////////////////////////////

  head (route, callback) {
    return this.handleRequest('head', route, callback);
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_controller_get
  // //////////////////////////////////////////////////////////////////////////////

  get (route, callback) {
    return this.handleRequest('get', route, callback);
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_controller_post
  // //////////////////////////////////////////////////////////////////////////////

  post (route, callback) {
    return this.handleRequest('post', route, callback);
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_controller_put
  // //////////////////////////////////////////////////////////////////////////////

  put (route, callback) {
    return this.handleRequest('put', route, callback);
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_controller_patch
  // //////////////////////////////////////////////////////////////////////////////

  patch (route, callback) {
    return this.handleRequest('patch', route, callback);
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_controller_delete
  // //////////////////////////////////////////////////////////////////////////////

  delete (route, callback) {
    return this.handleRequest('delete', route, callback);
  }

  del (route, callback) {
    return this.delete(route, callback);
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_controller_before
  // //////////////////////////////////////////////////////////////////////////////

  before (path, func) {
    if (is.notExisty(func)) {
      func = path;
      path = '/*';
    }

    this.routingInfo.middleware.push({
      priority: this.currentPriority = this.currentPriority + 1,
      url: {match: path},
      action: {
        callback(req, res, opts, next) {
          var result = func(req, res, opts);
          if (result !== false) {
            next();
          }
        }
      }
    });
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_controller_after
  // //////////////////////////////////////////////////////////////////////////////

  after (path, func) {
    if (is.notExisty(func)) {
      func = path;
      path = '/*';
    }

    this.routingInfo.middleware.push({
      priority: this.currentPriority = this.currentPriority + 1,
      url: {match: path},
      action: {
        callback(req, res, opts, next) { next(); func(req, res, opts); }
      }
    });
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_controller_around
  // //////////////////////////////////////////////////////////////////////////////

  around (path, func) {
    if (is.notExisty(func)) {
      func = path;
      path = '/*';
    }

    this.routingInfo.middleware.push({
      priority: this.currentPriority = this.currentPriority + 1,
      url: {match: path},
      action: {
        callback(req, res, opts, next) {
          func(req, res, opts, next);
        }
      }
    });
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_controller_activateAuthentication
  // //////////////////////////////////////////////////////////////////////////////
  activateAuthentication (opts) {
    var authentication = require('@arangodb/foxx/legacy/authentication');
    Object.assign(this, authControllerProps);

    this.auth = authentication.createAuthObject(this.applicationContext, opts);
    this.before('/*', authentication.createAuthenticationMiddleware(this.auth, this.applicationContext));
    this.after('/*', authentication.createSessionUpdateMiddleware());
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief activate sessions with the giveon options for this controller
  // //////////////////////////////////////////////////////////////////////////////

  activateSessions (opts) {
    var sessions = require('@arangodb/foxx/legacy/sessions');
    Object.assign(this, sessionControllerProps);

    this.sessions = new sessions.Sessions(opts);
    sessions.decorateController(this.sessions, this);
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_controller_apiDocumentation
  // //////////////////////////////////////////////////////////////////////////////
  apiDocumentation (route, opts) {
    if (route.charAt(route.length - 1) !== '/') {
      route += '/';
    }
    var mountPath = this.applicationContext.mount;
    return this.get(route + '*', swagger.createSwaggerRouteHandler(mountPath, opts));
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_controller_extend
  // //////////////////////////////////////////////////////////////////////////////

  extend (extensions) {
    var attr;
    var extensionWrapper = function (scope, functionName) {
      return function () {
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

}

exports.Controller = Controller;
