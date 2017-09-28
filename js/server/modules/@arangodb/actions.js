/* jshint strict: false, unused: false, esnext: true */
/* global JSON_CURSOR */

//
// @brief JavaScript actions module
//
// @file
//
// DISCLAIMER
//
// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
// Copyright 2012-2014 triagens GmbH, Cologne, Germany
//
// Licensed under the Apache License, Version 2.0 (the "License")
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Copyright holder is ArangoDB GmbH, Cologne, Germany
//
// @author Dr. Frank Celler
// @author Alan Plum
// @author Copyright 2014-2016, ArangoDB GmbH, Cologne, Germany
// @author Copyright 2012-2014, triAGENS GmbH, Cologne, Germany
//

module.isSystem = true;

var internal = require('internal');
var Module = require('module');

var fs = require('fs');
var util = require('util');
var path = require('path');
var mimeTypes = require('mime-types');
var console = require('console');

var arangodb = require('@arangodb');
var FoxxManager = require('@arangodb/foxx/manager');
var shallowCopy = require('@arangodb/util').shallowCopy;

const MIME_DEFAULT = 'text/plain; charset=utf-8';

//
// @brief current routing tree
//
// The route tree contains the URL hierarchy. In order to find a handler for a
// request, you can traverse this tree. You need to backtrack if you handler
// issues a `next()`.
//
// In order to speed up URL matching and next-finding, the tree is converted in
// to a flat list, such that the least-specific middleware method comes
// first. Then all other middleware methods with increasing specificity
// followed by the most-specific method. Then all other methods with decreasing
// specificity.
//
// Note that the object first on an entry for each database.
//
//     {
//       _system: {
//         middleware: [ ... ],
//         routes: [ ... ],
//         ...
//       }
//     }
//

var RoutingTree = {};

//
// @brief current routing list
//
// The flattened routing tree. The methods `flattenRoutingTree` takes a routing
// tree and returns a routing list.
//
// Note that the object first contains an entry for each database, then an
// entry for each method (like `GET`).
//
//     {
//       _system: {
//         GET: [ ... ],
//         POST: [ ... ],
//         ...
//       }
//     }
//

var RoutingList = {};

//
// @brief all methods
//

var ALL_METHODS = [ 'DELETE', 'GET', 'HEAD', 'OPTIONS', 'POST', 'PUT', 'PATCH' ];
var BODYFREE_METHODS = [ 'CONNECT', 'DELETE', 'GET', 'HEAD', 'TRACE' ];

//
// @brief function that's returned for non-implemented actions
//

function notImplementedFunction (route, message) {
  'use strict';

  message += '\nThis error was triggered by the following route: ' + route.name;

  console.errorLines(message);

  return function (req, res, options, next) {
    res.responseCode = exports.HTTP_NOT_IMPLEMENTED;
    res.contentType = 'text/plain';
    res.body = message;
  };
}

//
// @brief creates a callback for a callback action given as string
//

function createCallbackFromActionCallbackString (callback, parentModule, route) {
  'use strict';

  var actionModule = new Module(route.name, parentModule);

  try {
    actionModule._compile(`module.exports = ${callback}`, route.name);
  } catch (e) {
    console.errorStack(e);
    return notImplementedFunction(route, util.format(
      "could not generate callback for '%s'",
      callback
    ));
  }

  return actionModule.exports;
}

//
// @brief looks up a callback for a callback action given as string
//

function lookupCallbackActionCallbackString (route, action, actionModule) {
  'use strict';

  var func;

  try {
    func = createCallbackFromActionCallbackString(action.callback, actionModule, route);
  } catch (err) {
    func = errorFunction(route, util.format(
      "an error occurred constructing callback for '%s': %s",
      action.callback, String(err.stack || err)
    ));
  }

  return {
    controller: func,
    options: action.options || {},
    methods: action.methods || ALL_METHODS
  };
}

//
// @brief looks up a callback for a callback action
//

function lookupCallbackActionCallback (route, action, actionModule) {
  'use strict';

  if (typeof action.callback === 'string') {
    return lookupCallbackActionCallbackString(route, action, actionModule);
  }

  if (typeof action.callback === 'function') {
    return {
      controller: action.callback,
      options: action.options || {},
      methods: action.methods || ALL_METHODS
    };
  }

  return {
    controller: errorFunction(route, util.format(
      "an error occurred while generating callback '%s': unknown type '%s'",
      action.callback, typeof action.callback
    )),
    options: action.options || {},
    methods: action.methods || ALL_METHODS
  };
}

//
// @brief returns module or error function
//

function requireModule (name, route, parentModule) {
  'use strict';

  try {
    return { module: parentModule.require(name) };
  } catch (err) {
    if (err instanceof internal.ArangoError && err.errorNum === internal.errors.ERROR_MODULE_NOT_FOUND.code) {
      return notImplementedFunction(route, util.format(
        "an error occurred while loading the action module '%s': %s",
        name, String(err.stack || err)
      ));
    } else {
      return errorFunction(route, util.format(
        "an error occurred while loading action module '%s': %s",
        name, String(err.stack || err)
      ));
    }
  }
}

//
// @brief returns function in module or error function
//

function moduleFunction (actionModule, name, route) {
  'use strict';

  var func = actionModule[name];
  var type = typeof func;

  if (type !== 'function') {
    func = errorFunction(route, util.format(
      "invalid definition ('%s') for '%s' action in action module '%s'",
      type, name, actionModule.id
    ));
  }

  return func;
}

//
// @brief looks up a callback for a module action
//

function lookupCallbackActionDo (route, action, parentModule) {
  'use strict';

  var path = action['do'].split('/');
  var funcName = path.pop();
  var moduleName = path.join('/');

  var actionModule = requireModule(moduleName, route, parentModule);

  // module not found or load error
  if (typeof actionModule === 'function') {
    return {
      controller: func,
      options: action.options || {},
      methods: action.methods || ALL_METHODS
    };
  }

  // module found, extract function
  actionModule = actionModule.module;

  var func;

  if (actionModule.hasOwnProperty(funcName)) {
    func = moduleFunction(actionModule, funcName, route);
  } else {
    func = notImplementedFunction(route, util.format(
      "could not find action named '%s' in module '%s'",
      funcName, moduleName
    ));
  }

  return {
    controller: func,
    options: action.options || {},
    methods: action.methods || ALL_METHODS
  };
}

//
// @brief creates the callback for a controller action
//

function createCallbackActionController (actionModule, route) {
  'use strict';

  return function (req, res, options, next) {
    var m = req.requestType.toLowerCase();

    if (!actionModule.hasOwnProperty(m)) {
      m = 'do';
    }

    if (actionModule.hasOwnProperty(m)) {
      return moduleFunction(actionModule, m, route)(req, res, options, next);
    }

    next();
  };
}

//
// @brief looks up a callback for a controller action
//

function lookupCallbackActionController (route, action, parentModule) {
  'use strict';

  var actionModule = requireModule(action.controller, route, parentModule);

  if (typeof actionModule === 'function') {
    return {
      controller: actionModule,
      options: action.options || {},
      methods: action.methods || ALL_METHODS
    };
  }

  return {
    controller: createCallbackActionController(actionModule.module, route),
    options: action.options || {},
    methods: action.methods || ALL_METHODS
  };
}

//
// @brief looks up a callback for a prefix controller action
//

function lookupCallbackActionPrefixController (route, action, parentModule) {
  'use strict';

  var prefix = action.prefixController;

  return {
    controller: function (req, res, options, next) {
      // determine path
      var path;

      if (req.hasOwnProperty('suffix')) {
        path = prefix + '/' + req.suffix.join('/');
      } else {
        path = prefix;
      }

      // load module
      var actionModule = requireModule(path, route, parentModule);

      if (typeof actionModule === 'function') {
        actionModule(req, res, options, next);
      } else {
        createCallbackActionController(actionModule.module, route)(req, res, options, next);
      }
    },
    options: action.options || {},
    methods: action.methods || ALL_METHODS
  };
}

//
// @brief looks up a callback for an action
//

function lookupCallbackAction (route, action, context) {
  'use strict';

  // .............................................................................
  // short-cut for prefix controller
  // .............................................................................

  if (typeof action === 'string') {
    return lookupCallbackAction(route, { prefixController: action });
  }

  // .............................................................................
  // user-defined function
  // .............................................................................

  if (action.hasOwnProperty('callback')) {
    return lookupCallbackActionCallback(route, action, context.module);
  }

  // .............................................................................
  // function from module
  // .............................................................................

  if (action.hasOwnProperty('do')) {
    return lookupCallbackActionDo(route, action, context.module);
  }

  // .............................................................................
  // controller module
  // .............................................................................

  if (action.hasOwnProperty('controller')) {
    return lookupCallbackActionController(route, action, context.module);
  }

  // .............................................................................
  // prefix controller
  // .............................................................................

  if (action.hasOwnProperty('prefixController')) {
    return lookupCallbackActionPrefixController(route, action, context.module);
  }

  return null;
}

//
// @brief creates a callback for static data
//

function createCallbackStatic (code, type, body) {
  return function (req, res, options, next) {
    res.responseCode = code;
    res.contentType = type;
    res.body = body;
  };
}

//
// @brief looks up a callback for static data
//

function lookupCallbackStatic (content) {
  'use strict';

  var type;
  var body;
  var methods;
  var options;

  if (typeof content === 'string') {
    type = 'text/plain';
    body = content;
    methods = [ exports.GET, exports.HEAD ];
    options = {};
  } else {
    type = content.contentType || 'text/plain';
    body = content.body || '';
    methods = content.methods || [ exports.GET, exports.HEAD ];
    options = content.options || {};
  }

  return {
    controller: createCallbackStatic(exports.HTTP_OK, type, body),
    options: options,
    methods: methods
  };
}

//
// @brief looks up a callback
//
// A callback is a structure with `controller`, `options`, and `methods`.
//
//     {
//       controller: function (req, res, options, next) ...,
//       options: { ... },
//       methods: [ ... ]
//     }
//
// The controller contains the function to call. `req` is the request, `res`
// holds the response, `options` is the options array above and `methods` is a
// list of methods to which the controller applies.
//

function lookupCallback (route, context) {
  'use strict';

  if (route.hasOwnProperty('content')) {
    return lookupCallbackStatic(route.content);
  } else if (route.hasOwnProperty('action')) {
    return lookupCallbackAction(route, route.action, context);
  }

  return null;
}

//
// @brief splits a URL into parts
//

function splitUrl (url) {
  'use strict';

  var cleaned;
  var i;
  var parts;
  var re1;
  var re2;
  var ors;

  re1 = /^(:[a-zA-Z]+)*$/;
  re2 = /^(:[a-zA-Z]+)\?$/;

  parts = url.split('/');
  cleaned = [];

  for (i = 0; i < parts.length; i++) {
    var part = parts[i];

    if (part !== '' && part !== '.') {
      if (part === '..') {
        cleaned.pop();
      } else if (part === '*') {
        cleaned.push({ prefix: true });
      } else if (re1.test(part)) {
        ors = [ part.substr(1) ];
        cleaned.push({ parameters: ors });
      } else if (re2.test(part)) {
        ors = [ part.substr(1, part.length - 2) ];
        cleaned.push({ parameters: ors, optional: true });
      } else {
        cleaned.push(part);
      }
    }
  }

  return cleaned;
}

//
// @brief looks up an URL
//
// Splits an URL into
//
//     {
//       urlParts: [ ... ],
//       methods: [ ... ],
//       constraints: {}
//     }
//

function lookupUrl (prefix, url) {
  'use strict';

  if (url === undefined || url === '') {
    return null;
  }

  if (typeof url === 'string') {
    return {
      urlParts: splitUrl(prefix + '/' + url),
      methods: [ exports.GET, exports.HEAD ],
      constraint: {}
    };
  }

  if (url.hasOwnProperty('match')) {
    return {
      urlParts: splitUrl(prefix + '/' + url.match),
      methods: url.methods || ALL_METHODS,
      constraint: url.constraint || {}
    };
  }

  return null;
}

//
// @brief defines a new route part
//

function defineRoutePart (storage, route, parts, pos, constraint, callback) {
  'use strict';

  var i;

  var part = parts[pos];

  // otherwise we'll get an exception below
  if (part === undefined) {
    part = '';
  }

  // .............................................................................
  // exact match
  // .............................................................................

  if (typeof part === 'string') {
    if (!storage.hasOwnProperty('exact')) {
      storage.exact = {};
    }

    if (!storage.exact.hasOwnProperty(part)) {
      storage.exact[part] = {};
    }

    var subpart = storage.exact[part];

    if (pos + 1 < parts.length) {
      defineRoutePart(subpart, route, parts, pos + 1, constraint, callback);
    } else {
      if (!subpart.hasOwnProperty('routes')) {
        subpart.routes = [];
      }

      subpart.routes.push({
        route: route,
        priority: route.priority || 0,
        callback: callback
      });
    }
  }

  // .............................................................................
  // parameter match
  // .............................................................................
  else if (part.hasOwnProperty('parameters')) {
    if (!storage.hasOwnProperty('parameters')) {
      storage.parameters = [];
    }

    var ok = true;

    if (part.optional) {
      if (pos + 1 < parts.length) {
        console.error("cannot define optional parameter within url, ignoring route '%s'", route.name);
        ok = false;
      } else if (part.parameters.length !== 1) {
        console.error("cannot define multiple optional parameters, ignoring route '%s'", route.name);
        ok = false;
      }
    }

    if (ok) {
      for (i = 0; i < part.parameters.length; i++) {
        var p = part.parameters[i];
        var subsub = { parameter: p, match: {} };
        storage.parameters.push(subsub);

        if (constraint.hasOwnProperty(p)) {
          subsub.constraint = constraint[p];
        }

        subsub.optional = part.optional || false;

        if (pos + 1 < parts.length) {
          defineRoutePart(subsub.match, route, parts, pos + 1, constraint, callback);
        } else {
          var match = subsub.match;

          if (!match.hasOwnProperty('routes')) {
            match.routes = [];
          }

          match.routes.push({
            route: route,
            priority: route.priority || 0,
            callback: callback
          });
        }
      }
    }
  }

  // .............................................................................
  // prefix match
  // .............................................................................
  else if (part.hasOwnProperty('prefix')) {
    if (!storage.hasOwnProperty('prefix')) {
      storage.prefix = {};
    }

    if (pos + 1 < parts.length) {
      console.error("cannot define prefix match within url, ignoring route '%s'", route.name);
    } else {
      var subprefix = storage.prefix;

      if (!subprefix.hasOwnProperty('routes')) {
        subprefix.routes = [];
      }

      subprefix.routes.push({
        route: route,
        priority: route.priority || 0,
        callback: callback
      });
    }
  }
}

//
// @brief intersect methods
//

function intersectMethods (a, b) {
  'use strict';

  var d = {};
  var i;
  var j;
  var results = [];

  a = a || ALL_METHODS;
  b = b || ALL_METHODS;

  for (i = 0; i < b.length; i++) {
    d[b[i].toUpperCase()] = true;
  }

  for (j = 0; j < a.length; j++) {
    var name = a[j].toUpperCase();

    if (d[name]) {
      results.push(name);
    }
  }

  return results;
}

//
// @brief defines a new route
//

function defineRoute (storage, route, url, callback) {
  'use strict';

  var j;

  var methods = intersectMethods(url.methods, callback.methods);

  for (j = 0; j < methods.length; j++) {
    var method = methods[j];

    defineRoutePart(storage[method], route, url.urlParts, 0, url.constraint, callback);
  }
}

//
// @brief installs a new route in a tree
//

function installRoute (storage, route, urlPrefix, context) {
  'use strict';

  var url = lookupUrl(urlPrefix, route.url);

  if (url === null) {
    console.error("route has an unknown url, ignoring '%s'", route.name);
    return;
  }

  var callback = lookupCallback(route, context);

  if (callback === null) {
    console.error("route has an unknown callback, ignoring '%s'", route.name);
    return;
  }

  defineRoute(storage, route, url, callback);
}

//
// @brief creates additional routing information
//

function analyseRoutes (storage, routes) {
  'use strict';

  var j;
  var i;

  var urlPrefix = routes.urlPrefix || '';

  // use normal root module or app context
  var foxxContext = routes.foxxContext || {
    module: new Module(routes.name || 'anonymous')
  };

  // install the routes
  var keys = [ 'routes', 'middleware' ];

  for (j = 0; j < keys.length; j++) {
    var key = keys[j];

    if (routes.hasOwnProperty(key)) {
      var r = routes[key];

      for (i = 0; i < r.length; i++) {
        installRoute(storage[key],
          r[i],
          urlPrefix,
          foxxContext);
      }
    }
  }
}

//
// @brief builds a routing tree
//

function buildRoutingTree (routes) {
  'use strict';

  var j;

  var storage = { routes: {}, middleware: {} };

  for (j = 0; j < ALL_METHODS.length; j++) {
    var method = ALL_METHODS[j];

    storage.routes[method] = {};
    storage.middleware[method] = {};
  }

  for (j = 0; j < routes.length; j++) {
    var route = routes[j];

    try {
      if (route.hasOwnProperty('routes') || route.hasOwnProperty('middleware')) {
        analyseRoutes(storage, route);
      } else {

        // use normal root module or app context
        var foxxContext = routes.foxxContext || {
          module: new Module(route.name || 'anonymous')
        };

        installRoute(storage.routes, route, '', foxxContext);
      }
    } catch (err) {
      console.errorLines("cannot install route '%s': %s", route.name, String(err.stack || err));
    }
  }

  return storage;
}

//
// @brief flattens the routing tree for either middleware or normal methods
//

function flattenRouting (routes, path, rexpr, urlParameters, depth, prefix) {
  'use strict';

  var cur;
  var i;
  var k;
  var newUrlParameters;
  var parameter;
  var match;

  var result = [];

  // .............................................................................
  // descend down to the next exact matches
  // .............................................................................

  if (routes.hasOwnProperty('exact')) {
    for (k in routes.exact) {
      if (routes.exact.hasOwnProperty(k)) {
        result = result.concat(flattenRouting(
          routes.exact[k],
          path + '/' + k,
          rexpr + '/' + k.replace(/([\.\+\*\?\^\$\(\)\[\]])/g, '\\$1'),
          Object.assign({}, urlParameters),
          depth + 1,
          false));
      }
    }
  }

  // .............................................................................
  // descend down the parameter matches
  // .............................................................................

  if (routes.hasOwnProperty('parameters')) {
    for (i = 0; i < routes.parameters.length; i++) {
      parameter = routes.parameters[i];

      if (parameter.hasOwnProperty('constraint')) {
        var constraint = parameter.constraint;
        var pattern = /\/.*\//;

        if (pattern.test(constraint)) {
          match = '/' + constraint.substr(1, constraint.length - 2);
        } else {
          match = '/' + constraint;
        }
      } else {
        match = '/[^/]+';
      }

      if (parameter.optional) {
        cur = rexpr + '(' + match + ')?';
      } else {
        cur = rexpr + match;
      }

      newUrlParameters = Object.assign({}, urlParameters);
      newUrlParameters[parameter.parameter] = depth;

      result = result.concat(flattenRouting(
        parameter.match,
        path + '/:' + parameter.parameter + (parameter.optional ? '?' : ''),
        cur,
        newUrlParameters,
        depth + 1,
        false));
    }
  }

  // .............................................................................
  // we have done the depth first descend, now add the current callback(s)
  // .............................................................................

  if (routes.hasOwnProperty('routes')) {
    var sorted = [...routes.routes.sort(function (a, b) {
      return b.priority - a.priority;
    })];

    for (i = 0; i < sorted.length; i++) {
      sorted[i] = {
        path: path,
        regexp: new RegExp('^' + rexpr + '$'),
        prefix: prefix,
        depth: depth,
        urlParameters: urlParameters,
        callback: sorted[i].callback,
        route: sorted[i].route
      };
    }

    result = result.concat(sorted);
  }

  // .............................................................................
  // finally append the prefix matches
  // .............................................................................

  if (routes.hasOwnProperty('prefix')) {
    result = result.concat(flattenRouting(
      routes.prefix,
      path + '/*',
      rexpr + '(/[^/]+)*/?',
      shallowCopy(urlParameters),
      depth + 1,
      true));
  }

  return result;
}

//
// @brief flattens the routing tree complete
//

function flattenRoutingTree (tree) {
  'use strict';

  var i;

  var flat = {};

  for (i = 0; i < ALL_METHODS.length; i++) {
    var method = ALL_METHODS[i];

    var a = flattenRouting(tree.routes[method], '', '', {}, 0, false);
    var b = flattenRouting(tree.middleware[method], '', '', {}, 0, false);

    flat[method] = b.reverse().concat(a);
  }

  return flat;
}

//
// @brief creates the foxx routing actions
//

function foxxRouting (req, res, options, next) {
  var mount = options.mount;

  try {
    const service = FoxxManager.lookupService(mount);
    if (service.isDevelopment || !options.routing) {
      delete options.error;

      if (service.isDevelopment) {
        FoxxManager.reloadInstalledService(mount, true);
      }

      options.routing = flattenRoutingTree(buildRoutingTree([
        FoxxManager.ensureRouted(mount).routes
      ]));
    }
  } catch (e) {
    console.errorStack(e, `Failed to load Foxx service mounted at "${mount}"`);
    options.error = {
      code: exports.HTTP_SERVICE_UNAVAILABLE,
      num: e.errorNum || arangodb.ERROR_HTTP_SERVICE_UNAVAILABLE,
      msg: `Failed to load Foxx service mounted at "${mount}"`
    };
  }

  if (options.error) {
    exports.resultError(req,
      res,
      options.error.code,
      options.error.num,
      options.error.msg,
      {},
      options.error.info);
  } else {
    try {
      routeRequest(req, res, options.routing);
    } catch (err2) {
      resultException(
        req,
        res,
        err2,
        {},
        "failed to execute Foxx service mounted at '" + mount + "'");
    }
  }
}

//
// @brief flushes cache and reload routing information
//

function buildRouting (dbname) {
  'use strict';

  // compute all routes
  var routes = [];
  var routing = arangodb.db._collection('_routing');
  if (routing !== null) {
    let i = routing.all();

    while (i.hasNext()) {
      var n = i.next();
      var c = Object.assign({}, n);

      c.name = '_routing.document("' + c._key + '")';

      routes.push(c);
    }
  }

  // allow the collection to unload
  routing = null;

  // install the foxx routes
  var mountPoints = FoxxManager._mountPoints();

  for (let i = 0; i < mountPoints.length; i++) {
    var mountPoint = mountPoints[i];

    routes.push({
      name: "Foxx service mounted at '" + mountPoint + "'",
      url: { match: mountPoint + '/*' },
      action: { callback: foxxRouting, options: { mount: mountPoint } }
    });
  }

  // build the routing tree
  RoutingTree[dbname] = buildRoutingTree(routes);

  // compute the flat routes
  RoutingList[dbname] = flattenRoutingTree(RoutingTree[dbname]);
}

//
// @brief finds the next routing
//

function nextRouting (state) {
  'use strict';

  var i;
  var k;

  for (i = state.position + 1; i < state.routing.length; i++) {
    var route = state.routing[i];

    if (route.regexp.test(state.url)) {
      state.position = i;
      state.route = route;

      if (route.prefix) {
        state.prefix = '/' + state.parts.slice(0, route.depth - 1).join('/');
        state.suffix = state.parts.slice(route.depth - 1, state.parts.length);
        state.rawSuffix = state.rawParts.slice(route.depth - 1, state.rawParts.length);
      } else {
        delete state.prefix;
        delete state.suffix;
        delete state.rawSuffix;
      }

      state.urlParameters = {};

      if (route.urlParameters) {
        for (k in route.urlParameters) {
          if (route.urlParameters.hasOwnProperty(k)) {
            state.urlParameters[k] = state.parts[route.urlParameters[k]];
          }
        }
      }

      return state;
    }
  }

  delete state.route;
  delete state.prefix;
  delete state.suffix;
  delete state.rawSuffix;
  state.urlParameters = {};

  return state;
}

//
// @brief finds the first routing
//

function firstRouting (type, parts, routes, rawParts) {
  'use strict';

  var url = '/' + parts.join('/');

  if (!routes || !routes.hasOwnProperty(type)) {
    return {
      position: -1,
      urlParameters: {},
      parts,
      rawParts,
      url
    };
  }

  return nextRouting({
    routing: routes[type],
    position: -1,
    urlParameters: {},
    parts,
    rawParts,
    url
  });
}

//
// @brief routing function
//

function routeRequest (req, res, routes) {
  if (routes === undefined) {
    var dbname = arangodb.db._name();

    if (!RoutingList[dbname]) {
      buildRouting(dbname);
    }

    routes = RoutingList[dbname];
  }

  var action = firstRouting(req.requestType, req.suffix, routes, req.rawSuffix);

  function execute () {
    if (action.route === undefined) {
      resultNotFound(req, res, arangodb.ERROR_HTTP_NOT_FOUND,
        "unknown path '" + req.url + "'");
      return;
    }

    if (action.route.path !== undefined) {
      req.path = action.route.path;
    } else {
      delete req.path;
    }

    if (action.prefix !== undefined) {
      req.prefix = action.prefix;
    } else {
      delete req.prefix;
    }

    if (action.suffix !== undefined) {
      req.suffix = action.suffix;
    } else {
      delete req.suffix;
    }

    if (action.rawSuffix !== undefined) {
      req.rawSuffix = action.rawSuffix;
    } else {
      delete req.rawSuffix;
    }

    if (action.urlParameters !== undefined) {
      req.urlParameters = action.urlParameters;
    } else {
      req.urlParameters = {};
    }

    if (req.path) {
      var path = req.path;
      req.path = function (params, suffix) {
        var p = path;
        params = params || req.urlParameters;
        Object.keys(params).forEach(function (key) {
          p = p.replace(new RegExp(':' + key + '\\??', 'g'), params[key]);
        });
        p = p.replace(/:[a-zA-Z]+\?$/, '');
        suffix = suffix || req.suffix;
        if (Array.isArray(suffix)) {
          suffix = suffix.join('/');
        }
        return p.replace(/\*$/, suffix || '');
      };
    }

    var func = action.route.callback.controller;

    if (func === null || typeof func !== 'function') {
      func = errorFunction(action.route,
        'Invalid callback definition found for route ' +
        JSON.stringify(action.route) +
        ' while searching for callback.controller');
    }

    try {
      func(req, res, action.route.callback.options, next);
    } catch (err) {
      if (!err.hasOwnProperty('route')) {
        err.route = action.route;
      }

      throw err;
    }
  }

  function next (restart) {
    if (restart) {
      routeRequest(req, res);
    } else {
      action = nextRouting(action);
      execute();
    }
  }

  execute();
}

//
// @brief flushes the routing cache
//

function reloadRouting () {
  'use strict';

  RoutingTree = {};
  RoutingList = {};
  FoxxManager._resetCache();
}

//
// @brief loads all actions
//

function startup () {
  if (internal.defineAction === undefined) {
    return;
  }

  var actionPath = fs.join(internal.startupPath, 'actions');
  var actions = fs.listTree(actionPath);
  var i;

  for (i = 0; i < actions.length; i++) {
    var file = actions[i];
    var full = fs.join(actionPath, file);

    if (full !== '' && fs.isFile(full)) {
      if (file.match(/.*\.js$/)) {
        var content = fs.read(full);

        content = '(function () {\n' + content + '\n}());';

        internal.executeScript(content, undefined, full);
      }
    }
  }
}

//
// @brief was docuBlock actionsDefineHttp
//

function defineHttp (options) {
  'use strict';

  var url = options.url;
  var callback = options.callback;

  if (typeof callback !== 'function') {
    console.error("callback for '%s' must be a function, got '%s'", url, typeof callback);
    return;
  }

  var parameter = {
    prefix: true,
    allowUseDatabase: false
  };

  if (options.hasOwnProperty('prefix')) {
    parameter.prefix = options.prefix;
  }

  if (options.hasOwnProperty('allowUseDatabase')) {
    parameter.allowUseDatabase = options.allowUseDatabase;
  }

  try {
    internal.defineAction(url, callback, parameter);
  } catch (e) {
    console.errorLines("action '%s' encountered error:\n%s", url, e.stack || String(e));
  }
}

//
// @brief add a cookie
//

function addCookie (res, name, value, lifeTime, path, domain, secure, httpOnly) {
  'use strict';

  if (name === undefined) {
    return;
  }
  if (value === undefined) {
    return;
  }

  var cookie = {
    'name': name,
    'value': value
  };

  if (lifeTime !== undefined && lifeTime !== null) {
    cookie.lifeTime = parseInt(lifeTime, 10);
  }
  if (path !== undefined && path !== null) {
    cookie.path = path;
  }
  if (domain !== undefined && domain !== null) {
    cookie.domain = domain;
  }
  if (secure !== undefined && secure !== null) {
    cookie.secure = (secure) ? true : false;
  }
  if (httpOnly !== undefined && httpOnly !== null) {
    cookie.httpOnly = (httpOnly) ? true : false;
  }

  if (res.cookies === undefined || res.cookies === null) {
    res.cookies = [];
  }

  res.cookies.push(cookie);
}

//
// @brief was docuBlock actionsGetErrorMessage
//

function getErrorMessage (code) {
  'use strict';

  var key;

  for (key in internal.errors) {
    if (internal.errors.hasOwnProperty(key)) {
      if (internal.errors[key].code === code) {
        return internal.errors[key].message;
      }
    }
  }

  return '';
}

//
// @brief extracts the body as json
//

function getJsonBody (req, res, code) {
  'use strict';

  var body;
  var err;

  try {
    body = JSON.parse(req.requestBody || '{}') || {};
  } catch (err1) {
    resultBad(req, res, arangodb.ERROR_HTTP_CORRUPTED_JSON, err1);
    return undefined;
  }

  if (!body || !(body instanceof Object)) {
    if (code === undefined) {
      code = arangodb.ERROR_HTTP_CORRUPTED_JSON;
    }

    err = new internal.ArangoError();
    err.errorNum = code;
    err.errorMessage = 'expecting a valid JSON object as body';

    resultBad(req, res, code, err);
    return undefined;
  }

  return body;
}

//
// @brief was docuBlock actionsResultError
//

function resultError (req, res, httpReturnCode, errorNum, errorMessage, headers, keyvals) {
  'use strict';

  var i;
  var msg;

  res.responseCode = httpReturnCode;
  res.contentType = 'application/json; charset=utf-8';

  if (typeof errorNum === 'string') {
    keyvals = headers;
    headers = errorMessage;
    errorMessage = errorNum;
    errorNum = httpReturnCode;
  }

  if (errorMessage === undefined || errorMessage === null) {
    msg = getErrorMessage(errorNum);
  } else {
    msg = String(errorMessage);
  }

  var result = {};

  if (keyvals !== undefined) {
    for (i in keyvals) {
      if (keyvals.hasOwnProperty(i)) {
        result[i] = keyvals[i];
      }
    }
  }

  result.error = true;
  result.code = httpReturnCode;
  result.errorNum = errorNum;
  result.errorMessage = msg;

  res.body = JSON.stringify(result);

  if (headers !== undefined && headers !== null) {
    res.headers = headers;
  }
}

//
// @brief function that's returned for actions that produce an error
//

function errorFunction (route, message) {
  'use strict';

  message += '\nThis error was triggered by the following route ' + JSON.stringify(route);

  console.error('%s', message);

  return function (req, res, options, next) {
    res.responseCode = exports.HTTP_SERVER_ERROR;
    res.contentType = 'text/plain';
    res.body = message;
  };
}

//
// @brief convenience function for a bad parameter
//

function badParameter (req, res, name) {
  'use strict';

  resultError(req, res, exports.HTTP_BAD, exports.HTTP_BAD, util.format(
    "invalid value for parameter '%s'", name
  ));
}

//
// @brief was docuBlock actionsResultOk
//

function resultOk (req, res, httpReturnCode, result, headers) {
  'use strict';

  res.responseCode = httpReturnCode;
  res.contentType = 'application/json; charset=utf-8';

  // add some default attributes to result
  if (result === undefined) {
    result = {};
  }

  // check the type of the result.
  if (typeof result !== 'string' &&
    typeof result !== 'number' &&
    typeof result !== 'boolean') {
    // only modify result properties if none of the above types
    // otherwise the strict mode will throw
    result.error = false;
    result.code = httpReturnCode;
  }

  res.body = JSON.stringify(result);

  if (headers !== undefined) {
    res.headers = headers;
  }
}

//
// @brief was docuBlock actionsResultBad
//

function resultBad (req, res, code, msg, headers) {
  'use strict';

  resultError(req, res, exports.HTTP_BAD, code, msg, headers);
}

//
// @brief was docuBlock actionsResultNotFound
//

function resultNotFound (req, res, code, msg, headers) {
  'use strict';

  resultError(req, res, exports.HTTP_NOT_FOUND, code, msg, headers);
}

//
// @brief was docuBlock actionsResultNotImplemented
//

function resultNotImplemented (req, res, msg, headers) {
  'use strict';

  resultError(req,
    res,
    exports.HTTP_NOT_IMPLEMENTED,
    arangodb.ERROR_NOT_IMPLEMENTED,
    msg,
    headers);
}

//
// @brief was docuBlock actionsResultUnsupported
//

function resultUnsupported (req, res, headers) {
  'use strict';

  resultError(req, res,
    exports.HTTP_METHOD_NOT_ALLOWED,
    arangodb.ERROR_HTTP_METHOD_NOT_ALLOWED,
    'Unsupported method',
    headers);
}

//
// @brief internal function for handling redirects
//

function handleRedirect (req, res, options, headers) {
  var destination;
  var url = '';

  destination = options.destination;

  if (destination.substr(0, 5) !== 'http:' && destination.substr(0, 6) !== 'https:') {
    if (options.relative) {
      var u = req.url;

      if (0 < u.length && u[u.length - 1] === '/') {
        url += '/_db/' + encodeURIComponent(req.database) + u + destination;
      } else {
        url += '/_db/' + encodeURIComponent(req.database) + u + '/' + destination;
      }
    } else {
      url += destination;
    }
  } else {
    url = destination;
  }

  res.contentType = 'text/html';
  res.body = '<html><head><title>Moved</title>'
    + '</head><body><h1>Moved</h1><p>This page has moved to <a href="'
    + url
    + '">'
    + url
    + '</a>.</p></body></html>';

  if (headers !== undefined) {
    res.headers = shallowCopy(headers);
  } else {
    res.headers = {};
  }

  res.headers.location = url;
}

//
// @brief was docuBlock actionsResultPermanentRedirect
//

function resultPermanentRedirect (req, res, options, headers) {
  'use strict';

  res.responseCode = exports.HTTP_MOVED_PERMANENTLY;

  handleRedirect(req, res, options, headers);
}

//
// @brief was docuBlock actionsResultTemporaryRedirect
//

function resultTemporaryRedirect (req, res, options, headers) {
  'use strict';

  res.responseCode = exports.HTTP_TEMPORARY_REDIRECT;

  handleRedirect(req, res, options, headers);
}

//
// @brief function describing a cursor
//

function resultCursor (req, res, cursor, code, options) {
  'use strict';

  var rows;
  var count;
  var hasCount;
  var hasNext;
  var cursorId;
  var extra;

  if (Array.isArray(cursor)) {
    // performance optimisation: if the value passed in is an array, we can
    // use it as it is
    hasCount = ((options && options.countRequested) ? true : false);
    count = cursor.length;
    rows = cursor;
    hasNext = false;
    cursorId = null;
  } else if (typeof cursor === 'object' && cursor.hasOwnProperty('json')) {
    // cursor is a regular JS object (performance optimisation)
    hasCount = Boolean(options && options.countRequested);
    count = cursor.json.length;
    rows = cursor.json;
    extra = {};
    [ 'stats', 'warnings', 'profile' ].forEach(function (d) {
      if (cursor.hasOwnProperty(d)) {
        extra[d] = cursor[d];
      }
    });
    hasNext = false;
    cursorId = null;
  } else if (typeof cursor === 'string' && cursor.match(/^\d+$/)) {
    // cursor is assumed to be a cursor id
    var tmp = JSON_CURSOR(cursor);

    hasCount = true;
    count = tmp.count;
    rows = tmp.result;
    hasNext = tmp.hasOwnProperty('id');
    extra = undefined;

    if (hasNext) {
      cursorId = tmp.id;
    } else {
      cursorId = null;
    }
  }

  var result = {
    result: rows,
    hasMore: hasNext
  };

  if (cursorId) {
    result.id = cursorId;
  }

  if (hasCount) {
    result.count = count;
  }

  if (extra !== undefined && Object.keys(extra).length > 0) {
    result.extra = extra;
  }

  if (code === undefined) {
    code = exports.HTTP_CREATED;
  }

  resultOk(req, res, code, result);
}

//
// @brief was docuBlock actionsCollectionNotFound
//

function collectionNotFound (req, res, collection, headers) {
  'use strict';

  if (collection === undefined) {
    resultError(req, res,
      exports.HTTP_BAD, arangodb.ERROR_HTTP_BAD_PARAMETER,
      'expecting a collection name or identifier',
      headers);
  } else {
    resultError(req, res,
      exports.HTTP_NOT_FOUND, arangodb.ERROR_ARANGO_COLLECTION_NOT_FOUND,
      "unknown collection '" + collection + "'", headers);
  }
}

//
// @brief was docuBlock actionsIndexNotFound
//

function indexNotFound (req, res, collection, index, headers) {
  'use strict';

  if (collection === undefined) {
    resultError(req, res,
      exports.HTTP_BAD, arangodb.ERROR_HTTP_BAD_PARAMETER,
      'expecting a collection name or identifier',
      headers);
  } else if (index === undefined) {
    resultError(req, res,
      exports.HTTP_BAD, arangodb.ERROR_HTTP_BAD_PARAMETER,
      'expecting an index identifier',
      headers);
  } else {
    resultError(req, res,
      exports.HTTP_NOT_FOUND, arangodb.ERROR_ARANGO_INDEX_NOT_FOUND,
      "unknown index '" + index + "'", headers);
  }
}

function arangoErrorToHttpCode (num) {
  if (!num) {
    return exports.HTTP_SERVER_ERROR;
  }

  switch (num) {
    case arangodb.ERROR_INTERNAL:
    case arangodb.ERROR_OUT_OF_MEMORY:
    case arangodb.ERROR_GRAPH_TOO_MANY_ITERATIONS:
    case arangodb.ERROR_ARANGO_DOCUMENT_KEY_BAD:
      return exports.HTTP_SERVER_ERROR;

    case arangodb.ERROR_FORBIDDEN:
    case arangodb.ERROR_ARANGO_USE_SYSTEM_DATABASE:
      return exports.HTTP_FORBIDDEN;

    case arangodb.ERROR_ARANGO_COLLECTION_NOT_FOUND:
    case arangodb.ERROR_ARANGO_DOCUMENT_NOT_FOUND:
    case arangodb.ERROR_ARANGO_DATABASE_NOT_FOUND:
    case arangodb.ERROR_ARANGO_ENDPOINT_NOT_FOUND:
    case arangodb.ERROR_ARANGO_NO_INDEX:
    case arangodb.ERROR_ARANGO_INDEX_NOT_FOUND:
    case arangodb.ERROR_CURSOR_NOT_FOUND:
    case arangodb.ERROR_USER_NOT_FOUND:
    case arangodb.ERROR_TASK_NOT_FOUND:
    case arangodb.ERROR_QUERY_NOT_FOUND:
    case arangodb.ERROR_SERVICE_NOT_FOUND:
      return exports.HTTP_NOT_FOUND;

    case arangodb.ERROR_REQUEST_CANCELED:
      return exports.HTTP_REQUEST_TIMEOUT;

    case arangodb.ERROR_ARANGO_DUPLICATE_NAME:
    case arangodb.ERROR_ARANGO_DUPLICATE_IDENTIFIER:
    case arangodb.ERROR_USER_DUPLICATE:
    case arangodb.ERROR_GRAPH_DUPLICATE:
    case arangodb.ERROR_TASK_DUPLICATE_ID:
    case arangodb.ERROR_SERVICE_MOUNTPOINT_CONFLICT:
      return exports.HTTP_CONFLICT;

    case arangodb.ERROR_CLUSTER_UNSUPPORTED:
      return exports.HTTP_NOT_IMPLEMENTED;

    case arangodb.ERROR_ARANGO_ILLEGAL_NAME:
    case arangodb.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED:
    case arangodb.ERROR_ARANGO_CROSS_COLLECTION_REQUEST:
    case arangodb.ERROR_ARANGO_INDEX_HANDLE_BAD:
    case arangodb.ERROR_ARANGO_DOCUMENT_TOO_LARGE:
    case arangodb.ERROR_ARANGO_COLLECTION_NOT_UNLOADED:
    case arangodb.ERROR_ARANGO_COLLECTION_TYPE_INVALID:
    case arangodb.ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED:
    case arangodb.ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED:
    case arangodb.ERROR_ARANGO_DOCUMENT_KEY_MISSING:
    case arangodb.ERROR_ARANGO_DOCUMENT_TYPE_INVALID:
    case arangodb.ERROR_ARANGO_DATABASE_NAME_INVALID:
    case arangodb.ERROR_ARANGO_INVALID_KEY_GENERATOR:
    case arangodb.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE:
    case arangodb.ERROR_ARANGO_COLLECTION_TYPE_MISMATCH:
    case arangodb.ERROR_ARANGO_COLLECTION_NOT_LOADED:
    case arangodb.ERROR_ARANGO_DOCUMENT_REV_BAD:
      return exports.HTTP_BAD;
    case arangodb.ERROR_CLUSTER_BACKEND_UNAVAILABLE:
    case arangodb.ERROR_CLUSTER_SHARD_LEADER_RESIGNED:
      return exports.HTTP_SERVICE_UNAVAILABLE;
    case arangodb.ERROR_CLUSTER_SHARD_LEADER_REFUSES_REPLICATION:
    case arangodb.ERROR_CLUSTER_SHARD_FOLLOWER_REFUSES_OPERATION:
      return exports.HTTP_NOT_ACCEPTABLE;
  }

  return exports.HTTP_BAD;
}

//
// @brief was docuBlock actionsResultException
//

function resultException (req, res, err, headers, verbose) {
  'use strict';

  var code;
  var msg = err.message;
  var num;

  var info = {};

  if (verbose !== false) {
    console.errorStack(err);
    if (typeof verbose === 'string') {
      msg = verbose;
    }
  }

  if (err instanceof internal.ArangoError) {
    if (err.errorMessage !== '') {
      if (verbose !== false) {
        info.message = err.errorMessage;
      }
      if (typeof verbose !== 'string') {
        msg = err.errorMessage;
      }
    }

    num = err.errorNum || arangodb.ERROR_INTERNAL;
    code = arangoErrorToHttpCode(num);
  } else if (err instanceof TypeError) {
    num = arangodb.ERROR_TYPE_ERROR;
    code = exports.HTTP_BAD;
  } else if (err.statusCode) {
    num = err.statusCode;
    code = err.statusCode;
  } else {
    num = arangodb.ERROR_HTTP_SERVER_ERROR;
    code = exports.HTTP_SERVER_ERROR;
  }

  resultError(req, res, code, num, msg, headers, info);
}

//
// @brief creates a simple post callback
//

function easyPostCallback (opts) {
  'use strict';

  return function (req, res) {
    var body = getJsonBody(req, res);

    if (opts.body === true && body === undefined) {
      return;
    }

    try {
      var result = opts.callback(body, req);
      resultOk(req, res, exports.HTTP_OK, result);
    } catch (err) {
      resultException(req, res, err, undefined, false);
    }
  };
}

//
// @brief echos a request
//

function echoRequest (req, res, options, next) {
  'use strict';

  var result = { request: req, options: options };

  res.responseCode = exports.HTTP_OK;
  res.contentType = 'application/json; charset=utf-8';
  res.body = JSON.stringify(result);
}

//
// @brief logs a request
//

function logRequest (req, res, options, next) {
  'use strict';

  var log;
  var level;
  var token;

  if (options.hasOwnProperty('level')) {
    level = options.level;

    if (level === 'debug') {
      log = console.debug;
    } else if (level === 'error') {
      log = console.error;
    } else if (level === 'info') {
      log = console.info;
    } else if (level === 'log') {
      log = console.log;
    } else if (level === 'warn' || level === 'warning') {
      log = console.warn;
    } else {
      log = console.log;
    }
  } else {
    log = console.log;
  }

  if (options.hasOwnProperty('token')) {
    token = options.token;
  } else {
    token = '@(#' + internal.time() + ') ';
  }

  log('received request %s: %s', token, JSON.stringify(req));
  next();
  log('produced response %s: %s', token, JSON.stringify(res));
}

//
// @brief redirects a request
//

function redirectRequest (req, res, options, next) {
  'use strict';

  if (options.permanently) {
    resultPermanentRedirect(req, res, options);
  } else {
    resultTemporaryRedirect(req, res, options);
  }
}

//
// @brief redirects a request
//

function pathHandler (req, res, options, next) {
  'use strict';

  var filepath, root, filename, encodedFilename;
  filepath = req.suffix.length ? path.normalize(['', ...req.suffix.map((part) => part)].join(path.sep)) : '';
  root = options.path;

  if (options.root) {
    root = fs.join(options.root, root);
  }
  filename = fs.join(root, filepath);
  encodedFilename = filename;
  if (fs.exists(filename)) {
    res.responseCode = exports.HTTP_OK;
    res.contentType = options.type || mimeTypes.lookup(filename) || MIME_DEFAULT;
    if (options.gzip === true) {
      // check if client is capable of gzip encoding
      if (req.headers) {
        var encoding = req.headers['accept-encoding'];
        if (encoding && encoding.indexOf('gzip') >= 0) {
          // check if gzip file is available
          encodedFilename = encodedFilename + '.gz';
          if (fs.exists(encodedFilename)) {
            if (!res.headers) {
              res.headers = {};
            }
            res.headers['Content-Encoding'] = 'gzip';
          }
        }
      }
    }
    res.bodyFromFile = encodedFilename;
  } else {
    console.warn(`File not found "${req.url}": file "${filepath}" does not exist in "${root}".`);
    res.responseCode = exports.HTTP_NOT_FOUND;
    res.contentType = 'text/plain';
    res.body = `cannot find file "${filepath}"`;
  }
}

//
// @brief helper function to stringify a request
//

function stringifyRequest (req) {
  return req.requestType + ' ' + req.absoluteUrl();
}

// load all actions from the actions directory
exports.startup = startup;

// only for debugging
exports.buildRouting = buildRouting;
exports.buildRoutingTree = buildRoutingTree;
exports.flattenRoutingTree = flattenRoutingTree;
exports.nextRouting = nextRouting;
exports.firstRouting = function (method, parts, routes) {
  if (typeof parts === 'string') {
    parts = parts.split('/').filter(s => s !== '');
  }
  return firstRouting(method, parts, routes, parts);
};

// public functions
exports.routingTree = () => RoutingTree;
exports.routingList = () => RoutingList;
exports.routeRequest = routeRequest;
exports.defineHttp = defineHttp;
exports.getErrorMessage = getErrorMessage;
exports.getJsonBody = getJsonBody;
exports.errorFunction = errorFunction;
exports.reloadRouting = reloadRouting;
exports.addCookie = addCookie;
exports.stringifyRequest = stringifyRequest;
exports.arangoErrorToHttpCode = arangoErrorToHttpCode;

// standard HTTP responses
exports.badParameter = badParameter;
exports.resultBad = resultBad;
exports.resultError = resultError;
exports.resultNotFound = resultNotFound;
exports.resultNotImplemented = resultNotImplemented;
exports.resultOk = resultOk;
exports.resultPermanentRedirect = resultPermanentRedirect;
exports.resultTemporaryRedirect = resultTemporaryRedirect;
exports.resultUnsupported = resultUnsupported;

// ArangoDB specific responses
exports.resultCursor = resultCursor;
exports.collectionNotFound = collectionNotFound;
exports.indexNotFound = indexNotFound;
exports.resultException = resultException;

// standard actions
exports.easyPostCallback = easyPostCallback;
exports.echoRequest = echoRequest;
exports.logRequest = logRequest;
exports.redirectRequest = redirectRequest;
exports.pathHandler = pathHandler;

// some useful constants
exports.DELETE = 'DELETE';
exports.GET = 'GET';
exports.HEAD = 'HEAD';
exports.OPTIONS = 'OPTIONS';
exports.POST = 'POST';
exports.PUT = 'PUT';
exports.PATCH = 'PATCH';

exports.ALL_METHODS = ALL_METHODS;
exports.BODYFREE_METHODS = BODYFREE_METHODS;

// HTTP 2xx
exports.HTTP_OK = 200;
exports.HTTP_CREATED = 201;
exports.HTTP_ACCEPTED = 202;
exports.HTTP_PARTIAL = 203;
exports.HTTP_NO_CONTENT = 204;

// HTTP 3xx
exports.HTTP_MOVED_PERMANENTLY = 301;
exports.HTTP_FOUND = 302;
exports.HTTP_SEE_OTHER = 303;
exports.HTTP_NOT_MODIFIED = 304;
exports.HTTP_TEMPORARY_REDIRECT = 307;

// HTTP 4xx
exports.HTTP_BAD = 400;
exports.HTTP_UNAUTHORIZED = 401;
exports.HTTP_PAYMENT = 402;
exports.HTTP_FORBIDDEN = 403;
exports.HTTP_NOT_FOUND = 404;
exports.HTTP_METHOD_NOT_ALLOWED = 405;
exports.HTTP_REQUEST_TIMEOUT = 408;
exports.HTTP_CONFLICT = 409;
exports.HTTP_PRECONDITION_FAILED = 412;
exports.HTTP_ENTITY_TOO_LARGE = 413;
exports.HTTP_I_AM_A_TEAPOT = 418;
exports.HTTP_UNPROCESSABLE_ENTIT = 422;
exports.HTTP_HEADER_TOO_LARGE = 431;

// HTTP 5xx
exports.HTTP_SERVER_ERROR = 500;
exports.HTTP_NOT_IMPLEMENTED = 501;
exports.HTTP_BAD_GATEWAY = 502;
exports.HTTP_SERVICE_UNAVAILABLE = 503;
