/*jshint strict: false, unused: false */
/*global require, exports, module */

////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript actions module
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014-2015 ArangoDB GmbH, Cologne, Germany
/// Copyright 2012-2014 triagens GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

module.isSystem = true;

var internal = require("internal");

var fs = require("fs");
var console = require("console");
var _ = require("underscore");

var arangodb = require("org/arangodb");
var foxxManager = require("org/arangodb/foxx/manager");

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief current routing tree
///
/// The route tree contains the URL hierarchy. In order to find a handler for a
/// request, you can traverse this tree. You need to backtrack if you handler
/// issues a `next()`.
///
/// In order to speed up URL matching and next-finding, the tree is converted in
/// to a flat list, such that the least-specific middleware method comes
/// first. Then all other middleware methods with increasing specificity
/// followed by the most-specific method. Then all other methods with decreasing
/// specificity.
///
/// Note that the object first on an entry for each database.
///
///     {
///       _system: {
///         middleware: [ ... ],
///         routes: [ ... ],
///         ...
///       }
///     }
////////////////////////////////////////////////////////////////////////////////

var RoutingTree = {};

////////////////////////////////////////////////////////////////////////////////
/// @brief current routing list
///
/// The flattened routing tree. The methods `flattenRoutingTree` takes a routing
/// tree and returns a routing list.
///
/// Note that the object first contains an entry for each database, then an
/// entry for each method (like `GET`).
///
///     {
///       _system: {
///         GET: [ ... ],
///         POST: [ ... ],
///         ...
///       }
///     }
////////////////////////////////////////////////////////////////////////////////

var RoutingList = {};

////////////////////////////////////////////////////////////////////////////////
/// @brief all methods
////////////////////////////////////////////////////////////////////////////////

var ALL_METHODS = [ "DELETE", "GET", "HEAD", "OPTIONS", "POST", "PUT", "PATCH" ];

// -----------------------------------------------------------------------------
// --SECTION--                                   private functions for callbacks
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief function that's returned for non-implemented actions
////////////////////////////////////////////////////////////////////////////////

function notImplementedFunction (route, message) {
  'use strict';

  message += "\nThis error was triggered by the following route " + JSON.stringify(route);

  console.errorLines("%s", message);

  return function (req, res, options, next) {
    res.responseCode = exports.HTTP_NOT_IMPLEMENTED;
    res.contentType = "text/plain";
    res.body = message;
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a callback for a callback action given as string
////////////////////////////////////////////////////////////////////////////////

function createCallbackActionCallbackString (callback, foxxModule, route) {
  'use strict';

  var sandbox = {};
  var key;

  sandbox.module = module.root;

  sandbox.require = function (path) {
    return foxxModule.require(path);
  };

  var content = "(function(__myenv__) {";

  for (key in sandbox) {
    if (sandbox.hasOwnProperty(key)) {
      content += "var " + key + " = __myenv__['" + key + "'];";
    }
  }

  content += "delete __myenv__;"
    + "return (" + callback + ")"
    + "\n});";

  var func = internal.executeScript(content, undefined, route);

  if (func !== undefined) {
    func = func(sandbox);
  }

  if (func === undefined) {
    func = notImplementedFunction(
      route,
      "could not generate callback for '" + callback + "'");
  }

  return func;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a callback for a callback action given as string
////////////////////////////////////////////////////////////////////////////////

function lookupCallbackActionCallbackString (route, action, foxxModule) {
  'use strict';

  var func;

  try {
    func = createCallbackActionCallbackString(action.callback, foxxModule, route);
  }
  catch (err) {
    func = errorFunction(
      route,
      "an error occurred construction callback for '"
        + action.callback + "': " + String(err.stack || err));
  }

  return {
    controller: func,
    options: action.options || {},
    methods: action.methods || ALL_METHODS
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a callback for a callback action
////////////////////////////////////////////////////////////////////////////////

function lookupCallbackActionCallback (route, action, foxxModule) {
  'use strict';

  if (typeof action.callback === 'string') {
    return lookupCallbackActionCallbackString(route, action, foxxModule);
  }

  if (typeof action.callback === 'function') {
    return {
      controller: action.callback,
      options: action.options || {},
      methods: action.methods || ALL_METHODS
    };
  }

  return {
    controller: errorFunction(
      route,
      "an error occurred while generating callback '"
        + action.callback + "': unknown type '" + String(typeof action.callback) + "'"),
    options: action.options || {},
    methods: action.methods || ALL_METHODS
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns module or error function
////////////////////////////////////////////////////////////////////////////////

function requireModule (name, route) {
  'use strict';

  try {
    return { module: require(name) };
  }
  catch (err) {
    if (err.hasOwnProperty("moduleNotFound") && err.moduleNotFound) {
      return notImplementedFunction(
        route,
        "an error occurred while loading the action module '" + name
          + "': " + String(err.stack || err));
    }
    else {
      return errorFunction(
        route,
        "an error occurred while loading action module '" + name
          + "': " + String(err.stack || err));
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns function in module or error function
////////////////////////////////////////////////////////////////////////////////

function moduleFunction (actionModule, name, route) {
  'use strict';

  var func = actionModule[name];
  var type = typeof func;

  if (type !== 'function') {
    func = errorFunction(
      route,
      "invalid definition ('" + type + "') for '" + name 
        + "' action in action module '" + actionModule.id + "'");
  }

  return func;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a callback for a module action
////////////////////////////////////////////////////////////////////////////////

function lookupCallbackActionDo (route, action) {
  'use strict';

  var path = action['do'].split("/");
  var funcName = path.pop();
  var moduleName = path.join("/");

  var actionModule = requireModule(moduleName, route);

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
  }
  else {
    func = notImplementedFunction(
      route,
      "could not find action named '" + funcName + "' in module '" + moduleName + "'");
  }

  return {
    controller: func,
    options: action.options || {},
    methods: action.methods || ALL_METHODS
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates the callback for a controller action
////////////////////////////////////////////////////////////////////////////////

function createCallbackActionController (actionModule, route) {
  'use strict';

  return function (req, res, options, next) {
    var m = req.requestType.toLowerCase();

    if (! actionModule.hasOwnProperty(m)) {
      m = 'do';
    }

    if (actionModule.hasOwnProperty(m)) {
      return moduleFunction(actionModule, m, route)(req, res, options, next);
    }

    next();
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a callback for a controller action
////////////////////////////////////////////////////////////////////////////////

function lookupCallbackActionController (route, action) {
  'use strict';

  var actionModule = requireModule(action.controller, route);

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

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a callback for a prefix controller action
////////////////////////////////////////////////////////////////////////////////

function lookupCallbackActionPrefixController (route, action) {
  'use strict';

  var prefix = action.prefixController;

  return {
    controller: function (req, res, options, next) {
      var func;

      // determine path
      var path;

      if (req.hasOwnProperty('suffix')) {
        path = prefix + "/" + req.suffix.join("/");
      }
      else {
        path = prefix;
      }

      // load module
      var actionModule = requireModule(path, route);

      if (typeof actionModule === 'function') {
        actionModule(req, res, options, next);
      }
      else {
        createCallbackActionController(actionModule.module, route)(req, res, options, next);
      }
    },
    options: action.options || {},
    methods: action.methods || ALL_METHODS
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a callback for an action
////////////////////////////////////////////////////////////////////////////////

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
    return lookupCallbackActionCallback(route, action, context.appModule);
  }

  // .............................................................................
  // function from module
  // .............................................................................

  if (action.hasOwnProperty('do')) {
    return lookupCallbackActionDo(route, action);
  }

  // .............................................................................
  // controller module
  // .............................................................................

  if (action.hasOwnProperty('controller')) {
    return lookupCallbackActionController(route, action);
  }

  // .............................................................................
  // prefix controller
  // .............................................................................

  if (action.hasOwnProperty('prefixController')) {
    return lookupCallbackActionPrefixController(route, action);
  }

  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a callback for static data
////////////////////////////////////////////////////////////////////////////////

function createCallbackStatic (code, type, body) {
  return function (req, res, options, next) {
    res.responseCode = code;
    res.contentType = type;
    res.body = body;
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a callback for static data
////////////////////////////////////////////////////////////////////////////////

function lookupCallbackStatic (content) {
  'use strict';

  var type;
  var body;
  var methods;
  var options;

  if (typeof content === 'string') {
    type = "text/plain";
    body = content;
    methods = [ exports.GET, exports.HEAD ];
    options = {};
  }
  else {
    type = content.contentType || "text/plain";
    body = content.body || "";
    methods = content.methods || [ exports.GET, exports.HEAD ];
    options = content.options || {};
  }

  return {
    controller: createCallbackStatic(exports.HTTP_OK, type, body),
    options: options,
    methods: methods
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a callback
///
/// A callback is a structure with `controller`, `options`, and `methods`.
///
///     {
///       controller: function (req, res, options, next) ...,
///       options: { ... },
///       methods: [ ... ]
///     }
///
/// The controller contains the function to call. `req` is the request, `res`
/// holds the response, `options` is the options array above and `methods` is a
/// list of methods to which the controller applies.
////////////////////////////////////////////////////////////////////////////////

function lookupCallback (route, context) {
  'use strict';

  if (route.hasOwnProperty('content')) {
    return lookupCallbackStatic(route.content);
  }
  else if (route.hasOwnProperty('action')) {
    return lookupCallbackAction(route, route.action, context);
  }

  return null;
}

// -----------------------------------------------------------------------------
// --SECTION--                                     private functions for routing
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief splits a URL into parts
////////////////////////////////////////////////////////////////////////////////

function splitUrl (url) {
  'use strict';

  var cleaned;
  var i;
  var parts;
  var re1;
  var re2;
  var cut;
  var ors;

  re1 = /^(:[a-zA-Z]+)(\|:[a-zA-Z]+)*$/;
  re2 = /^(:[a-zA-Z]+)\?$/;

  parts = url.split("/");
  cleaned = [];

  cut = function (x) {
    return x.substr(1);
  };

  for (i = 0;  i < parts.length;  ++i) {
    var part = parts[i];

    if (part !== "" && part !== ".") {
      if (part === "..") {
        cleaned.pop();
      }
      else if (part === "*") {
        cleaned.push({ prefix: true });
      }
      else if (re1.test(part)) {
        ors = part.split("|").map(cut);
        cleaned.push({ parameters: ors });
      }
      else if (re2.test(part)) {
        ors = [ part.substr(1, part.length - 2) ];
        cleaned.push({ parameters: ors, optional: true });
      }
      else {
        cleaned.push(part);
      }
    }
  }

  return cleaned;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an URL
///
/// Splits an URL into
///
///     {
///       urlParts: [ ... ],
///       methods: [ ... ],
///       constraints: {}
///     }
////////////////////////////////////////////////////////////////////////////////

function lookupUrl (prefix, url) {
  'use strict';

  if (url === undefined || url === '') {
    return null;
  }

  if (typeof url === 'string') {
    return {
      urlParts: splitUrl(prefix + "/" + url),
      methods: [ exports.GET, exports.HEAD ],
      constraint: {}
    };
  }

  if (url.hasOwnProperty('match')) {
    return {
      urlParts: splitUrl(prefix + "/" + url.match),
      methods: url.methods || ALL_METHODS,
      constraint: url.constraint || {}
    };
  }

  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief defines a new route part
////////////////////////////////////////////////////////////////////////////////

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

  if (typeof part === "string") {
    if (! storage.hasOwnProperty('exact')) {
      storage.exact = {};
    }

    if (! storage.exact.hasOwnProperty(part)) {
      storage.exact[part] = {};
    }

    var subpart = storage.exact[part];

    if (pos + 1 < parts.length) {
      defineRoutePart(subpart, route, parts, pos + 1, constraint, callback);
    }
    else {
      if (! subpart.hasOwnProperty('routes')) {
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
    if (! storage.hasOwnProperty('parameters')) {
      storage.parameters = [];
    }

    var ok = true;

    if (part.optional) {
      if (pos + 1 < parts.length) {
        console.error("cannot define optional parameter within url, ignoring route '%s'", route.name);
        ok = false;
      }
      else if (part.parameters.length !== 1) {
        console.error("cannot define multiple optional parameters, ignoring route '%s'", route.name);
        ok = false;
      }
    }

    if (ok) {
      for (i = 0;  i < part.parameters.length;  ++i) {
        var p = part.parameters[i];
        var subsub = { parameter: p, match: {} };
        storage.parameters.push(subsub);

        if (constraint.hasOwnProperty(p)) {
          subsub.constraint = constraint[p];
        }

        subsub.optional = part.optional || false;

        if (pos + 1 < parts.length) {
          defineRoutePart(subsub.match, route, parts, pos + 1, constraint, callback);
        }
        else {
          var match = subsub.match;

          if (! match.hasOwnProperty('routes')) {
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
    if (! storage.hasOwnProperty('prefix')) {
      storage.prefix = {};
    }

    if (pos + 1 < parts.length) {
      console.error("cannot define prefix match within url, ignoring route");
    }
    else {
      var subprefix = storage.prefix;

      if (! subprefix.hasOwnProperty('routes')) {
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

////////////////////////////////////////////////////////////////////////////////
/// @brief intersect methods
////////////////////////////////////////////////////////////////////////////////

function intersectMethods (a, b) {
  'use strict';

  var d = {};
  var i;
  var j;
  var results = [];

  a = a || ALL_METHODS;
  b = b || ALL_METHODS;

  for (i = 0; i < b.length;  i++) {
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

////////////////////////////////////////////////////////////////////////////////
/// @brief defines a new route
////////////////////////////////////////////////////////////////////////////////

function defineRoute (storage, route, url, callback) {
  'use strict';

  var j;

  var methods = intersectMethods(url.methods, callback.methods);

  for (j = 0;  j < methods.length;  ++j) {
    var method = methods[j];

    defineRoutePart(storage[method], route, url.urlParts, 0, url.constraint, callback);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief installs a new route in a tree
////////////////////////////////////////////////////////////////////////////////

function installRoute (storage, route, urlPrefix, context) {
  'use strict';

  var url = lookupUrl(urlPrefix, route.url);

  if (url === null) {
    console.error("route '%s' has an unkown url, ignoring", JSON.stringify(route));
    return;
  }

  var callback = lookupCallback(route, context);

  if (callback === null) {
    console.error("route '%s' has an unknown callback, ignoring", JSON.stringify(route));
    return;
  }

  defineRoute(storage, route, url, callback);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates additional routing information
////////////////////////////////////////////////////////////////////////////////

function analyseRoute (storage, routes) {
  'use strict';

  var j;
  var i;

  var urlPrefix = routes.urlPrefix || "";

  // create the application context
  var appModule = module.root;

  var appContext = {
    collectionPrefix: ""
  };

  if (routes.hasOwnProperty('appContext')) {
    appContext = routes.appContext;

    var appId = appContext.appId;
    var app = module.createApp(appId);

    if (app === null) {
      throw new Error("unknown application '" + appId + "'");
    }

    appModule = app.createAppModule(appContext);
  }

  // install the routes
  var keys = [ 'routes', 'middleware' ];

  for (j = 0;  j < keys.length;  ++j) {
    var key = keys[j];

    if (routes.hasOwnProperty(key)) {
      var r = routes[key];

      for (i = 0;  i < r.length;  ++i) {
        var context = { appModule: appModule };

        installRoute(storage[key],
                     r[i],
                     urlPrefix,
                     context);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief builds a routing tree
////////////////////////////////////////////////////////////////////////////////

function routingTree (routes) {
  'use strict';

  var j;

  var storage = { routes: {}, middleware: {} };

  for (j = 0;  j < ALL_METHODS.length;  ++j) {
    var method = ALL_METHODS[j];

    storage.routes[method] = {};
    storage.middleware[method] = {};
  }

  for (j = 0;  j < routes.length;  ++j) {
    var route = routes[j];

    try {
      if (route.hasOwnProperty('routes') || route.hasOwnProperty('middleware')) {
        analyseRoute(storage, route);
      }
      else {
        installRoute(storage.routes, route, "", {});
       }
    }
    catch (err) {
      console.errorLines("cannot install route '%s': %s",
                         route.name,
                         String(err.stack || err));
    }
  }

  return storage;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flattens the routing tree for either middleware or normal methods
////////////////////////////////////////////////////////////////////////////////

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
          path + "/" + k,
          rexpr + "/" + k.replace(/([\.\+\*\?\^\$\(\)\[\]])/g, "\\$1"),
          _.clone(urlParameters),
          depth + 1,
          false));
      }
    }
  }

  // .............................................................................
  // descend down the parameter matches
  // .............................................................................

  if (routes.hasOwnProperty('parameters')) {
    for (i = 0;  i < routes.parameters.length;  ++i) {
      parameter = routes.parameters[i];

      if (parameter.hasOwnProperty('constraint')) {
        var constraint = parameter.constraint;
        var pattern = /\/.*\//;

        if (pattern.test(constraint)) {
          match = "/" + constraint.substr(1, constraint.length - 2);
        }
        else {
          match = "/" + constraint;
        }
      }
      else {
        match = "/[^/]+";
      }

      if (parameter.optional) {
        cur = rexpr + "(" + match + ")?";
      }
      else {
        cur = rexpr + match;
      }

      newUrlParameters = _.clone(urlParameters);
      newUrlParameters[parameter.parameter] = depth;

      result = result.concat(flattenRouting(
        parameter.match,
        path + "/<parameters>",
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
    var sorted = _.clone(routes.routes.sort(function(a,b) {
      return b.priority - a.priority;
    }));

    for (i = 0;  i < sorted.length;  ++i) {
      sorted[i] = {
        path: path,
        regexp: new RegExp("^" + rexpr + "$"),
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
      path + "/...",
      rexpr + "(/[^/]+)*",
      urlParameters._shallowCopy,
      depth + 1,
      true));
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flattens the routing tree complete
////////////////////////////////////////////////////////////////////////////////

function flattenRoutingTree (tree) {
  'use strict';

  var i;

  var flat = {};

  for (i = 0;  i < ALL_METHODS.length;  ++i) {
    var method = ALL_METHODS[i];

    var a = flattenRouting(tree.routes[method], "", "", {}, 0, false);
    var b = flattenRouting(tree.middleware[method], "", "", {}, 0, false);

    flat[method] = b.reverse().concat(a);
  }

  return flat;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flushes cache and reload routing information
////////////////////////////////////////////////////////////////////////////////

function buildRouting (dbname) {
  'use strict';

  // compute all routes
  var routes = [];
  var routing = arangodb.db._collection("_routing");

  var i = routing.all();

  while (i.hasNext()) {
    var n = i.next();
    var c = n._shallowCopy;
    
    c.name = "_routing.document(" + n._key + ")";

    routes.push(c);
  }

  // allow the collection to unload
  routing = null;

  // install the foxx routes
  // TODO routes = routes.concat(foxxManager.appRoutes());

  // build the routing tree
  RoutingTree[dbname] = routingTree(routes);

  // compute the flat routes
  RoutingList[dbname] = flattenRoutingTree(RoutingTree[dbname]);
}

// -----------------------------------------------------------------------------
// --SECTION--                                      public functions for routing
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief finds the next routing
////////////////////////////////////////////////////////////////////////////////

function nextRouting (state) {
  'use strict';

  var i;
  var k;

  for (i = state.position + 1;  i < state.routing.length;  ++i) {
    var route = state.routing[i];

    if (route.regexp.test(state.url)) {
      state.position = i;

      state.route = route;

      if (route.prefix) {
        state.prefix = "/" + state.parts.slice(0, route.depth - 1).join("/");
        state.suffix = state.parts.slice(route.depth - 1, state.parts.length);
      }
      else {
        delete state.prefix;
        delete state.suffix;
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
  state.urlParameters = {};

  return state;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds the first routing
////////////////////////////////////////////////////////////////////////////////

function firstRouting (type, parts) {
  'use strict';

  var dbname = arangodb.db._name();
  var url = parts;

  if (undefined === RoutingList[dbname]) {
    buildRouting(dbname);
  }

  var routes = RoutingList[dbname];

  if (typeof url === 'string') {
    parts = url.split("/");

    if (parts[0] === '') {
      parts.shift();
    }
  }
  else {
    url = "/" + parts.join("/");
  }

  if (! routes || ! routes.hasOwnProperty(type)) {
    return {
      parts: parts,
      position: -1,
      url: url,
      urlParameters: {}
    };
  }

  return nextRouting({
    routing: routes[type],
    parts: parts,
    position: -1,
    url: url,
    urlParameters: {}
  });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief routing function
////////////////////////////////////////////////////////////////////////////////

function routeRequest (req, res) {
  var path = req.suffix.join("/");
  var action = firstRouting(req.requestType, req.suffix);

  function execute () {
    if (action.route === undefined) {
      resultNotFound(req, res, arangodb.ERROR_HTTP_NOT_FOUND,
        "unknown path '" + path + "'");
      return;
    }

    if (action.route.path !== undefined) {
      req.path = action.route.path;
    }
    else {
      delete req.path;
    }

    if (action.prefix !== undefined) {
      req.prefix = action.prefix;
    }
    else {
      delete req.prefix;
    }

    if (action.suffix !== undefined) {
      req.suffix = action.suffix;
    }
    else {
      delete req.suffix;
    }

    if (action.urlParameters !== undefined) {
      req.urlParameters = action.urlParameters;
    }
    else {
      req.urlParameters = {};
    }

    var func = action.route.callback.controller;

    if (func === null || typeof func !== 'function') {
      func = errorFunction(action.route,
                           'Invalid callback definition found for route '
                           + JSON.stringify(action.route));
    }

    try {
      func(req, res, action.route.callback.options, next);
    }
    catch (err) {
      if (! err.hasOwnProperty("route")) {
        err.route = action.route;
      }

      throw err;
    }
  }

  function next (restart) {
    if (restart) {
      routeRequest(req, res);
    }
    else {
      action = nextRouting(action);
      execute();
    }
  }

  execute();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flushes the routing cache
////////////////////////////////////////////////////////////////////////////////

function reloadRouting () {
  'use strict';

  RoutingTree = {};
  RoutingList = {};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads all actions
////////////////////////////////////////////////////////////////////////////////

function startup () {
  if (internal.defineAction === undefined) {
    return;
  }

  var actionPath = fs.join(internal.startupPath, "actions");
  var actions = fs.list(actionPath);
  var i;

  for (i = 0;  i < actions.length;  ++i) {
    var file = actions[i];

    if (file.match(/api-.*\.js$/)) {
      var full = fs.join(actionPath, file);
      var content = fs.read(full);

      content = "(function () {\n" + content + "\n}());";

      internal.executeScript(content, undefined, full);
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock actionsDefineHttp
///
/// `actions.defineHttp(options)`
///
/// Defines a new action. The *options* are as follows:
///
/// `options.url`
///
/// The URL, which can be used to access the action. This path might contain
/// slashes. Note that this action will also be called, if a url is given such that
/// *options.url* is a prefix of the given url and no longer definition
/// matches.
///
/// `options.prefix`
///
/// If *false*, then only use the action for exact matches. The default is
/// *true*.
///
/// `options.callback(request, response)`
///
/// The request argument contains a description of the request. A request
/// parameter *foo* is accessible as *request.parametrs.foo*. A request
/// header *bar* is accessible as *request.headers.bar*. Assume that
/// the action is defined for the url */foo/bar* and the request url is
/// */foo/bar/hugo/egon*. Then the suffix parts *[ "hugo", "egon" ]*
/// are availible in *request.suffix*.
///
/// The callback must define fill the *response*.
///
/// * *response.responseCode*: the response code
/// * *response.contentType*: the content type of the response
/// * *response.body*: the body of the response
///
/// You can use the functions *ResultOk* and *ResultError* to easily
/// generate a response.
/// @endDocuBLock
////////////////////////////////////////////////////////////////////////////////

function defineHttp (options) {
  'use strict';

  var url = options.url;
  var callback = options.callback;

  if (typeof callback !== "function") {
    console.error("callback for '%s' must be a function, got '%s'", url, (typeof callback));
    return;
  }

  var parameter = {
    prefix: true,
    allowUseDatabase: false
  };

  if (options.hasOwnProperty("prefix")) {
    parameter.prefix = options.prefix;
  }

  if (options.hasOwnProperty("allowUseDatabase")) {
    parameter.allowUseDatabase = options.allowUseDatabase;
  }

  try {
    internal.defineAction(url, callback, parameter);
  }
  catch (err) {
    console.error("action '%s' encountered error: %s", url, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a cookie
////////////////////////////////////////////////////////////////////////////////

function addCookie (res, name, value, lifeTime, path, domain, secure, httpOnly) {
  'use strict';

  if (name === undefined) {
    return;
  }
  if (value === undefined) {
    return;
  }

  var cookie = {
    'name' : name,
    'value' : value
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

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock actionsGetErrorMessage
///
/// `actions.getErrorMessage(code)`
///
/// Returns the error message for an error code.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

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

  return "";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the body as json
////////////////////////////////////////////////////////////////////////////////

function getJsonBody (req, res, code) {
  'use strict';

  var body;
  var err;

  try {
    body = JSON.parse(req.requestBody || "{}") || {};
  }
  catch (err1) {
    resultBad(req, res, arangodb.ERROR_HTTP_CORRUPTED_JSON, err1);
    return undefined;
  }

  if (! body || ! (body instanceof Object)) {
    if (code === undefined) {
      code = arangodb.ERROR_HTTP_CORRUPTED_JSON;
    }

    err = new internal.ArangoError();
    err.errorNum = code;
    err.errorMessage = "expecting a valid JSON object as body";

    resultBad(req, res, code, err);
    return undefined;
  }

  return body;
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock actionsResultError
///
/// *actions.resultError(*req*, *res*, *code*, *errorNum*,
///                          *errorMessage*, *headers*, *keyvals)*
///
/// The function generates an error response. The response body is an array
/// with an attribute *errorMessage* containing the error message
/// *errorMessage*, *error* containing *true*, *code* containing
/// *code*, *errorNum* containing *errorNum*, and *errorMessage*
/// containing the error message *errorMessage*. *keyvals* are mixed
/// into the result.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function resultError (req, res, httpReturnCode, errorNum, errorMessage, headers, keyvals) {
  'use strict';

  var i;
  var msg;

  res.responseCode = httpReturnCode;
  res.contentType = "application/json; charset=utf-8";

  if (typeof errorNum === "string") {
    keyvals = headers;
    headers = errorMessage;
    errorMessage = errorNum;
    errorNum = httpReturnCode;
  }

  if (errorMessage === undefined || errorMessage === null) {
    msg = getErrorMessage(errorNum);
  }
  else {
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

  result.error        = true;
  result.code         = httpReturnCode;
  result.errorNum     = errorNum;
  result.errorMessage = msg;

  res.body = JSON.stringify(result);

  if (headers !== undefined && headers !== null) {
    res.headers = headers;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function that's returned for actions that produce an error
////////////////////////////////////////////////////////////////////////////////

function errorFunction (route, message) {
  'use strict';

  message += "\nThis error was triggered by the following route " + JSON.stringify(route);

  console.error("%s", message);

  return function (req, res, options, next) {
    res.responseCode = exports.HTTP_SERVER_ERROR;
    res.contentType = "text/plain";
    res.body = message;
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convenience function for a bad parameter
////////////////////////////////////////////////////////////////////////////////

function badParameter (req, res, name) {
  'use strict';

  resultError(req, res, exports.HTTP_BAD, exports.HTTP_BAD,
              "invalid value for parameter '" + name + "'");
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock actionsResultOk
///
/// `actions.resultOk(req, res, code, result, headers)`
///
/// The function defines a response. *code* is the status code to
/// return. *result* is the result object, which will be returned as JSON
/// object in the body. *headers* is an array of headers to returned.
/// The function adds the attribute *error* with value *false*
/// and *code* with value *code* to the *result*.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function resultOk (req, res, httpReturnCode, result, headers) {
  'use strict';

  res.responseCode = httpReturnCode;
  res.contentType = "application/json; charset=utf-8";

  // add some default attributes to result
  if (result === undefined) {
    result = {};
  }

  // check the type of the result. 
  if (typeof result !== "string" && 
      typeof result !== "number" &&
      typeof result !== "boolean") {
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

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock actionsResultBad
///
/// `actions.resultBad(req, res, error-code, msg, headers)`
///
/// The function generates an error response.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function resultBad (req, res, code, msg, headers) {
  'use strict';

  resultError(req, res, exports.HTTP_BAD, code, msg, headers);
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock actionsResultNotFound
///
/// `actions.resultNotFound(req, res, code, msg, headers)`
///
/// The function generates an error response.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function resultNotFound (req, res, code, msg, headers) {
  'use strict';

  resultError(req, res, exports.HTTP_NOT_FOUND, code, msg, headers);
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock actionsResultNotImplemented
///
/// `actions.resultNotImplemented(req, res, msg, headers)`
///
/// The function generates an error response.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function resultNotImplemented (req, res, msg, headers) {
  'use strict';

  resultError(req,
              res,
              exports.HTTP_NOT_IMPLEMENTED,
              arangodb.ERROR_NOT_IMPLEMENTED,
              msg,
              headers);
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock actionsResultUnsupported
///
/// `actions.resultUnsupported(req, res, headers)`
///
/// The function generates an error response.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function resultUnsupported (req, res, headers) {
  'use strict';

  resultError(req, res,
              exports.HTTP_METHOD_NOT_ALLOWED,
              arangodb.ERROR_HTTP_METHOD_NOT_ALLOWED,
              "Unsupported method",
              headers);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief internal function for handling redirects
////////////////////////////////////////////////////////////////////////////////

function handleRedirect (req, res, options, headers) {
  var destination;
  var url;

  destination = options.destination;

  if (destination.substr(0,5) !== "http:" && destination.substr(0,6) !== "https:") {
    url = req.protocol + "://";

    if (req.headers.hasOwnProperty('host')) {
      url += req.headers.host;
      if (req.headers.host.indexOf(':') === -1) {
        // append port number if not present in "host" header
        url += ":" + req.server.port;
      }
    }
    else {
      url += req.server.address + ":" + req.server.port;
    }

    if (options.relative) {
      var u = req.url;

      if (0 < u.length && u[u.length - 1] === '/') {
        url += "/_db/" + encodeURIComponent(req.database) + u + destination;
      }
      else {
        url += "/_db/" + encodeURIComponent(req.database) + u + "/" + destination;
      }
    }
    else {
      url += destination;
    }
  }
  else {
    url = destination;
  }

  res.contentType = "text/html";
  res.body = "<html><head><title>Moved</title>"
    + "</head><body><h1>Moved</h1><p>This page has moved to <a href=\""
    + url
    + "\">"
    + url
    + "</a>.</p></body></html>";

  if (headers !== undefined) {
    res.headers = headers._shallowCopy;
  }
  else {
    res.headers = {};
  }

  res.headers.location = url;
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock actionsResultPermanentRedirect
///
/// `actions.resultPermanentRedirect(req, res, options, headers)`
///
/// The function generates a redirect response.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function resultPermanentRedirect (req, res, options, headers) {
  'use strict';

  res.responseCode = exports.HTTP_MOVED_PERMANENTLY;

  handleRedirect(req, res, options, headers);
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock actionsResultTemporaryRedirect
///
/// `actions.resultTemporaryRedirect(req, res, options, headers)`
///
/// The function generates a redirect response.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function resultTemporaryRedirect (req, res, options, headers) {
  'use strict';

  res.responseCode = exports.HTTP_TEMPORARY_REDIRECT;

  handleRedirect(req, res, options, headers);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function describing a cursor
////////////////////////////////////////////////////////////////////////////////

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
  }
  else if (typeof cursor === 'object') {
    if (cursor.getExtra !== undefined) {
      // cursor is assumed to be an ArangoCursor
      hasCount = cursor.hasCount();
      count = cursor.count();
      rows = cursor.toArray();

      // must come after toArray()
      hasNext = cursor.hasNext();
      extra = cursor.getExtra();

      if (hasNext) {
        cursor.persist();
        cursorId = cursor.id();
      }
      else {
        cursorId = null;
        cursor.dispose();
      }
    }
    else if (cursor.hasOwnProperty('json')) {
      // cursor is a regular JS object (performance optimisation)
      hasCount = ((options && options.countRequested) ? true : false);
      count = cursor.json.length;
      rows = cursor.json;
      extra = { };
      [ "stats", "warnings", "profile" ].forEach(function(d) {
        if (cursor.hasOwnProperty(d)) {
          extra[d] = cursor[d];
        }
      });
      hasNext = false;
      cursorId = null;
    }
  }

  // do not use cursor after this

  var result = {
    result : rows,
    hasMore : hasNext
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

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock actionsCollectionNotFound
///
/// `actions.collectionNotFound(req, res, collection, headers)`
///
/// The function generates an error response.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function collectionNotFound (req, res, collection, headers) {
  'use strict';

  if (collection === undefined) {
    resultError(req, res,
                exports.HTTP_BAD, arangodb.ERROR_HTTP_BAD_PARAMETER,
                "expecting a collection name or identifier",
                headers);
  }
  else {
    resultError(req, res,
                exports.HTTP_NOT_FOUND, arangodb.ERROR_ARANGO_COLLECTION_NOT_FOUND,
                "unknown collection '" + collection + "'", headers);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock actionsIndexNotFound
///
/// `actions.indexNotFound(req, res, collection, index, headers)`
///
/// The function generates an error response.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function indexNotFound (req, res, collection, index, headers) {
  'use strict';

  if (collection === undefined) {
    resultError(req, res,
                exports.HTTP_BAD, arangodb.ERROR_HTTP_BAD_PARAMETER,
                "expecting a collection name or identifier",
                headers);
  }
  else if (index === undefined) {
    resultError(req, res,
                exports.HTTP_BAD, arangodb.ERROR_HTTP_BAD_PARAMETER,
                "expecting an index identifier",
                headers);
  }
  else {
    resultError(req, res,
                exports.HTTP_NOT_FOUND, arangodb.ERROR_ARANGO_INDEX_NOT_FOUND,
                "unknown index '" + index + "'", headers);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock actionsResultException
///
/// `actions.resultException(req, res, err, headers, verbose)`
///
/// The function generates an error response. If @FA{verbose} is set to
/// *true* or not specified (the default), then the error stack trace will
/// be included in the error message if available.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function resultException (req, res, err, headers, verbose) {
  'use strict';

  var code;
  var msg;
  var num;

  if (verbose || verbose === undefined) {
    msg = String(err.stack || err);
    verbose = true;
  }
  else {
    msg = String(err);
  }

  if (err instanceof internal.ArangoError) {
    num = err.errorNum;
    code = exports.HTTP_BAD;

    if (num === 0) {
      num = arangodb.ERROR_INTERNAL;
    }

    if (err.errorMessage !== "" && ! verbose) {
      msg = err.errorMessage;
    }

    switch (num) {
      case arangodb.ERROR_INTERNAL:
      case arangodb.ERROR_OUT_OF_MEMORY:
      case arangodb.ERROR_GRAPH_TOO_MANY_ITERATIONS:
        code = exports.HTTP_SERVER_ERROR;
        break;

      case arangodb.ERROR_FORBIDDEN:
      case arangodb.ERROR_ARANGO_USE_SYSTEM_DATABASE:
        code = exports.HTTP_FORBIDDEN;
        break;

      case arangodb.ERROR_ARANGO_COLLECTION_NOT_FOUND:
      case arangodb.ERROR_ARANGO_DOCUMENT_NOT_FOUND:
      case arangodb.ERROR_ARANGO_DATABASE_NOT_FOUND:
      case arangodb.ERROR_ARANGO_ENDPOINT_NOT_FOUND:
      case arangodb.ERROR_ARANGO_NO_INDEX:
      case arangodb.ERROR_ARANGO_INDEX_NOT_FOUND:
      case arangodb.ERROR_CURSOR_NOT_FOUND:
      case arangodb.ERROR_USER_NOT_FOUND:
      case arangodb.ERROR_TASK_NOT_FOUND:
        code = exports.HTTP_NOT_FOUND;
        break;

      case arangodb.ERROR_REQUEST_CANCELED:
        code = exports.HTTP_REQUEST_TIMEOUT;
        break;

      case arangodb.ERROR_ARANGO_DUPLICATE_NAME:
      case arangodb.ERROR_ARANGO_DUPLICATE_IDENTIFIER:
        code = exports.HTTP_CONFLICT;
        break;

      case arangodb.ERROR_CLUSTER_UNSUPPORTED:
        code = exports.HTTP_NOT_IMPLEMENTED;
        break;
    }
  }
  else if (err instanceof TypeError) {
    num = arangodb.ERROR_TYPE_ERROR;
    code = exports.HTTP_BAD;
  }
  else {
    num = arangodb.ERROR_HTTP_SERVER_ERROR;
    code = exports.HTTP_SERVER_ERROR;
  }

  resultError(req, res, code, num, msg, headers);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a simple post callback
////////////////////////////////////////////////////////////////////////////////

function easyPostCallback (opts) {
  'use strict';

  return function (req, res) {
    var body = getJsonBody(req, res);

    if (opts.body === true && body === undefined) {
      return;
    }

    try {
      var result = opts.callback(body);
      resultOk(req, res, exports.HTTP_OK, result);
    }
    catch (err) {
      resultException(req, res, err, undefined, false);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief echos a request
////////////////////////////////////////////////////////////////////////////////

function echoRequest (req, res, options, next) {
  'use strict';

  var result = { request : req, options : options };

  res.responseCode = exports.HTTP_OK;
  res.contentType = "application/json; charset=utf-8";
  res.body = JSON.stringify(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a request
////////////////////////////////////////////////////////////////////////////////

function logRequest (req, res, options, next) {
  'use strict';

  var log;
  var level;
  var token;

  if (options.hasOwnProperty('level')) {
    level = options.level;

    if (level === "debug") {
      log = console.debug;
    }
    else if (level === "error") {
      log = console.error;
    }
    else if (level === "info") {
      log = console.info;
    }
    else if (level === "log") {
      log = console.log;
    }
    else if (level === "warn" || level === "warning") {
      log = console.warn;
    }
    else {
      log = console.log;
    }
  }
  else {
    log = console.log;
  }

  if (options.hasOwnProperty('token')) {
    token = options.token;
  }
  else {
    token = "@(#" + internal.time() + ") ";
  }

  log("received request %s: %s", token, JSON.stringify(req));
  next();
  log("produced response %s: %s", token, JSON.stringify(res));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief redirects a request
////////////////////////////////////////////////////////////////////////////////

function redirectRequest (req, res, options, next) {
  'use strict';

  if (options.permanently) {
    resultPermanentRedirect(req, res, options);
  }
  else {
    resultTemporaryRedirect(req, res, options);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief rewrites a request
////////////////////////////////////////////////////////////////////////////////

function rewriteRequest (req, res, options, next) {
  'use strict';

  var i = 0;
  var suffix = options.destination.split("/");

  for (i = 0;  i < suffix.length;  ++i) {
    if (suffix[i] !== "") {
      break;
    }
  }

  if (0 < i) {
    suffix = suffix.splice(i);
  }

  req.suffix = suffix;

  next(true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief redirects a request
////////////////////////////////////////////////////////////////////////////////

function pathHandler (req, res, options, next) {
  'use strict';

  var filename;

  filename = fs.join(options.path, fs.join.apply(fs.join, req.suffix));

  if (options.hasOwnProperty('root')) {
    var root = options.root;

    filename = fs.join(root, filename);
  }

  if (fs.exists(filename)) {
    res.responseCode = exports.HTTP_OK;
    res.contentType = arangodb.guessContentType(filename);
    res.bodyFromFile = filename;
  }
  else {
    res.responseCode = exports.HTTP_NOT_FOUND;
    res.contentType = "text/plain";
    res.body = "cannot find file '" + filename + "'";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function to stringify a request
////////////////////////////////////////////////////////////////////////////////

function stringifyRequestAddress (req) {
  var out = req.requestType + " " +
            req.protocol + "://" +
            req.server.address + ":" +
            req.server.port +
            req.url;

  return out;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

// load all actions from the actions directory
exports.startup                  = startup;

// only for debugging
exports.buildRouting             = buildRouting;
exports.routingTree              = function() { return RoutingTree; };
exports.routingList              = function() { return RoutingList; };

// public functions
exports.routeRequest             = routeRequest;
exports.defineHttp               = defineHttp;
exports.getErrorMessage          = getErrorMessage;
exports.getJsonBody              = getJsonBody;
exports.errorFunction            = errorFunction;
exports.reloadRouting            = reloadRouting;
exports.firstRouting             = firstRouting;
exports.nextRouting              = nextRouting;
exports.addCookie                = addCookie;
exports.stringifyRequestAddress  = stringifyRequestAddress;

// standard HTTP responses
exports.badParameter             = badParameter;
exports.resultBad                = resultBad;
exports.resultError              = resultError;
exports.resultNotFound           = resultNotFound;
exports.resultNotImplemented     = resultNotImplemented;
exports.resultOk                 = resultOk;
exports.resultPermanentRedirect  = resultPermanentRedirect;
exports.resultTemporaryRedirect  = resultTemporaryRedirect;
exports.resultUnsupported        = resultUnsupported;

// ArangoDB specific responses
exports.resultCursor             = resultCursor;
exports.collectionNotFound       = collectionNotFound;
exports.indexNotFound            = indexNotFound;
exports.resultException          = resultException;

// standard actions
exports.easyPostCallback         = easyPostCallback;
exports.echoRequest              = echoRequest;
exports.logRequest               = logRequest;
exports.redirectRequest          = redirectRequest;
exports.rewriteRequest           = rewriteRequest;
exports.pathHandler              = pathHandler;

// some useful constants
exports.DELETE                   = "DELETE";
exports.GET                      = "GET";
exports.HEAD                     = "HEAD";
exports.OPTIONS                  = "OPTIONS";
exports.POST                     = "POST";
exports.PUT                      = "PUT";
exports.PATCH                    = "PATCH";

exports.ALL_METHODS              = ALL_METHODS;

// HTTP 2xx
exports.HTTP_OK                  = 200;
exports.HTTP_CREATED             = 201;
exports.HTTP_ACCEPTED            = 202;
exports.HTTP_PARTIAL             = 203;
exports.HTTP_NO_CONTENT          = 204;

// HTTP 3xx
exports.HTTP_MOVED_PERMANENTLY   = 301;
exports.HTTP_FOUND               = 302;
exports.HTTP_SEE_OTHER           = 303;
exports.HTTP_NOT_MODIFIED        = 304;
exports.HTTP_TEMPORARY_REDIRECT  = 307;

// HTTP 4xx
exports.HTTP_BAD                 = 400;
exports.HTTP_UNAUTHORIZED        = 401;
exports.HTTP_PAYMENT             = 402;
exports.HTTP_FORBIDDEN           = 403;
exports.HTTP_NOT_FOUND           = 404;
exports.HTTP_METHOD_NOT_ALLOWED  = 405;
exports.HTTP_REQUEST_TIMEOUT     = 408;
exports.HTTP_CONFLICT            = 409;
exports.HTTP_PRECONDITION_FAILED = 412;
exports.HTTP_ENTITY_TOO_LARGE    = 413;
exports.HTTP_I_AM_A_TEAPOT       = 418;
exports.HTTP_UNPROCESSABLE_ENTIT = 422;
exports.HTTP_HEADER_TOO_LARGE    = 431;

// HTTP 5xx
exports.HTTP_SERVER_ERROR        = 500;
exports.HTTP_NOT_IMPLEMENTED     = 501;
exports.HTTP_BAD_GATEWAY         = 502;
exports.HTTP_SERVICE_UNAVAILABLE = 503;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @startDocuBlock\\|/// @\\}"
// End:
