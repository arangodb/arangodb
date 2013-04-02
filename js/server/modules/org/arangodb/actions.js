/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, regexp: true plusplus: true */
/*global require, exports, module */

////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript actions module
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2012 triagens GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");

var fs = require("fs");
var console = require("console");

var arangodb = require("org/arangodb");
var foxxManager = require("org/arangodb/foxx-manager");

var moduleExists = function(name) { return module.exists; };

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoActions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief routing cache
////////////////////////////////////////////////////////////////////////////////

var RoutingCache;

////////////////////////////////////////////////////////////////////////////////
/// @brief all methods
////////////////////////////////////////////////////////////////////////////////

var ALL_METHODS = [ "DELETE", "GET", "HEAD", "OPTIONS", "POST", "PUT", "PATCH" ];

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoActions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief function that's returned for non-implemented actions
////////////////////////////////////////////////////////////////////////////////
        
function notImplementedFunction (route, message) {
  message += "\nThis error was triggered by the following route " + JSON.stringify(route);

  console.error(message);

  return function (req, res, options, next) {
    res.responseCode = exports.HTTP_NOT_IMPLEMENTED;
    res.contentType = "text/plain";
    res.body = message;
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function that's returned for actions that produce an error
////////////////////////////////////////////////////////////////////////////////

function errorFunction (route, message) {
  message += "\nThis error was triggered by the following route " + JSON.stringify(route);

  console.error(message);

  return function (req, res, options, next) {
    res.responseCode = exports.HTTP_SERVER_ERROR;
    res.contentType = "text/plain";
    res.body = message;
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief splits an URL into parts
////////////////////////////////////////////////////////////////////////////////

function splitUrl (url) {
  var cleaned;
  var i;
  var parts;
  var re1;
  var re2;
  var cut;
  var ors;

  re1 = /^(:[a-z]+)(\|:[a-z]+)*$/;
  re2 = /^(:[a-z]+)\?$/;

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
////////////////////////////////////////////////////////////////////////////////

function lookupUrl (prefix, url) {
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
/// @brief looks up a callback for static data
////////////////////////////////////////////////////////////////////////////////

function lookupCallbackStatic (content) {
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
    controller: function (req, res, options, next) {
      res.responseCode = exports.HTTP_OK;
      res.contentType = type;
      res.body = body;
    },

    options: options,
    methods: methods
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a callback for a callback action
////////////////////////////////////////////////////////////////////////////////

function lookupCallbackActionCallback (route, action) {
  var defn;
  var env;
  var func;
  var key;
  var app;
  var appModule;
  var modelModule;

  defn = "func = (function() { var callback = " + action.callback + "; return callback;})();";
  env = {};

  try {
    if (action.hasOwnProperty("context")) {
      app = module.createApp(action.context.appId);

      if (app === null) {
        throw "cannot locate application '" + action.context.name + "'"
          + " in version '" + action.context.version + "'";
      }
      
      appModule = app.createAppModule();

      if (action.hasOwnProperty("requiresModels")) {
        var cp = action.context.collectionPrefix;
        var me;

        modelModule = app.createAppModule('models', appModule._package);
        me = modelModule._package._environment = {};

        if (cp !== "") {
          me.appCollectionName = function (name) {
            return cp + "_" + name;
          };

          me.appCollection = function (name) {
            return arangodb.db._collection(cp + "_" + name);
          };
        }
        else {
          me.appCollectionName = function (name) {
            return name;
          };

          me.appCollection = function (name) {
            return arangodb.db._collection(name);
          };
        }

        me.requireModel = function (path) {
          modelModule.require(path);
        };
      }
      else {
        modelModule = appModule;
      }
    }
    else {
      appModule = module.root;
      modelModule = appModule;
    }

    if (action.hasOwnProperty("requiresLibs")) {
      var requires = action.requiresLibs;

      for (key in requires) {
        if (requires.hasOwnProperty(key)) {
          env[key] = appModule.require(requires[key]);
        }
      }
    }

    if (action.hasOwnProperty("requiresModels")) {
      var models = action.requiresModels;

      for (key in models) {
        if (models.hasOwnProperty(key)) {
          env[key] = modelModule.require(models[key]);
        }
      }
    }

    env.module = module.root;
    env.require = function (path) {
      return appModule.require(path);
    };

    internal.executeScript(defn, env, route);

    if (env.hasOwnProperty("func")) {
      func = env.func;
    }
    else {
      func = notImplementedFunction(
        route,
        "could not define function '" + action.callback);
    }
  }
  catch (err) {
    func = errorFunction(
      route,
      "an error occurred while loading function '" 
        + action.callback + "': " + String(err));
  }

  return {
    controller: func,
    options: action.options || {},
    methods: action.methods || ALL_METHODS
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a callback for a module action
////////////////////////////////////////////////////////////////////////////////

function lookupCallbackActionDo (route, action) {
  var func;
  var joined;
  var name;
  var path;
  var mdl;

  path = action['do'].split("/");
  name = path.pop();
  func = null;
  joined = path.join("/");

  try {
    mdl = require(joined);

    if (mdl.hasOwnProperty(name)) {
      func = mdl[name];
    }
    else {
      func = notImplementedFunction(route, "could not find action named '"
                                    + name + "' in module '" + joined + "'");
    }
  }
  catch (err) {
    if (! moduleExists(joined)) {
      func = notImplementedFunction(
        route, 
        "an error occurred while loading action named '" + name 
          + "' in module '" + joined + "': " + String(err));
    }
    else {
      func = errorFunction(
        route,
        "an error occurred while loading action named '" + name 
          + "' in module '" + joined + "': " + String(err));
    }
  }

  if (func === null || typeof func !== 'function') {
    func = errorFunction(
      route, 
      "invalid definition for the action named '" + name 
        + "' in module '" + joined + "'");
  }

  return {
    controller: func,
    options: action.options || {},
    methods: action.methods || ALL_METHODS
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a callback for a controller action
////////////////////////////////////////////////////////////////////////////////

function lookupCallbackActionController (route, action) {
  var func;
  var mdl;
  var httpMethods = { 
    'get': exports.GET,
    'head': exports.HEAD,
    'put': exports.PUT,
    'post': exports.POST,
    'delete': exports.DELETE,
    'options': exports.OPTIONS,
    'patch': exports.PATCH
  };

  try {
    mdl = require(action.controller);

    return {
      controller: function (req, res, options, next) {
        var m;

        // enum all HTTP methods
        for (m in httpMethods) {
          if (httpMethods.hasOwnProperty(m)) {
            if (req.requestType === httpMethods[m] && mdl.hasOwnProperty(m)) {
              func = mdl[m]
                || errorFunction(
                     route, 
                     "invalid definition for " + m 
                       + " action in action controller module '" 
                       + action.controller + "'");

              return func(req, res, options, next); 
            }
          }
        }

        if (mdl.hasOwnProperty('do')) {
          func = mdl['do']
            || errorFunction(
                 route,
                   "invalid definition for do action in action controller module '"
                   + action.controller + "'");

          return func(req, res, options, next);
        }

        next();
      },
      options: action.options || {},
      methods: action.methods || ALL_METHODS
    };
  }
  catch (err) {
    if (! moduleExists(action.controller)) {
      return notImplementedFunction(
        route, 
        "cannot load/execute action controller module '" 
          + action.controller + ": " + String(err));
    }

    return errorFunction(
      route, 
      "cannot load/execute action controller module '" 
        + action.controller + ": " + String(err));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a callback for a prefix controller action
////////////////////////////////////////////////////////////////////////////////

function lookupCallbackActionPrefixController (route, action) {
  var prefixController = action.prefixController;
  var httpMethods = { 
    'get': exports.GET,
    'head': exports.HEAD,
    'put': exports.PUT,
    'post': exports.POST,
    'delete': exports.DELETE,
    'options': exports.OPTIONS,
    'patch': exports.PATCH
  };

  return {
    controller: function (req, res, options, next) {
      var mdl;
      var path;
      var efunc;
      var func;

      // determine path
      if (req.hasOwnProperty('suffix')) {
        path = prefixController + "/" + req.suffix.join("/");
      }
      else {
        path = prefixController;
      }

      // load module
      try {
        require(path);
      }
      catch (err1) {
        efunc = errorFunction;

        if (! moduleExists(path)) {
          efunc = notImplementedFunction;
        }

        return efunc(route,
                     "cannot load prefix controller: " + String(err1))(
          req, res, options, next);
      }

      try {
        var m;

        // enum all HTTP methods
        for (m in httpMethods) {
          if (httpMethods.hasOwnProperty(m)) {
            if (req.requestType === httpMethods[m] && mdl.hasOwnProperty(m)) {
              func = mdl[m]
                || errorFunction(
                     route,
                     "Invalid definition for " + m + " action in prefix controller '"
                       + action.prefixController + "'");

              return func(req, res, options, next);
            }
          }
        }
  
        if (mdl.hasOwnProperty('do')) {
          func = mdl['do']
            || errorFunction(
                 route, 
                 "Invalid definition for do action in prefix controller '"
                   + action.prefixController + "'");

          return func(req, res, options, next);
        }
      }
      catch (err2) {
        return errorFunction(
          route, 
          "Cannot load/execute prefix controller '"
            + action.prefixController + "': " + String(err2))(
          req, res, options, next);
      }

      next();
    },
    options: action.options || {},
    methods: action.methods || ALL_METHODS
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a callback for an action
////////////////////////////////////////////////////////////////////////////////

function lookupCallbackAction (route, action) {

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
    return lookupCallbackActionCallback(route, action);
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
/// @brief looks up a callback
////////////////////////////////////////////////////////////////////////////////

function lookupCallback (route) {
  var result = null;

  if (route.hasOwnProperty('content')) {
    result = lookupCallbackStatic(route.content);
  }
  else if (route.hasOwnProperty('action')) {
    result = lookupCallbackAction(route, route.action);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief intersect methods 
////////////////////////////////////////////////////////////////////////////////

function intersectMethods (a, b) {
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
/// @brief defines a new route part
////////////////////////////////////////////////////////////////////////////////

function defineRoutePart (route, subwhere, parts, pos, constraint, callback) {
  var i;
  var p;
  var part;
  var subsub;
  var ok;

  part = parts[pos];
  if (part === undefined) {
    // otherwise we'll get an exception below
    part = '';
  }

  if (typeof part === "string") {
    if (! subwhere.hasOwnProperty('exact')) {
      subwhere.exact = {};
    }

    if (! subwhere.exact.hasOwnProperty(part)) {
      subwhere.exact[part] = {};
    }

    if (pos + 1 < parts.length) {
      defineRoutePart(route, subwhere.exact[part], parts, pos + 1, constraint, callback);
    }
    else {
      subwhere.exact[part].route = route;
      subwhere.exact[part].callback = callback;
    }
  }
  else if (part.hasOwnProperty('parameters')) {
    if (! subwhere.hasOwnProperty('parameters')) {
      subwhere.parameters = [];
    }

    ok = true;

    if (part.optional) {
      if (pos + 1 < parts.length) {
        console.error("cannot define optional parameter within url, ignoring route");
        ok = false;
      }
      else if (part.parameters.length !== 1) {
        console.error("cannot define multiple optional parameters, ignoring route");
        ok = false;
      }
    }

    if (ok) {
      for (i = 0;  i < part.parameters.length;  ++i) {
        p = part.parameters[i];
        subsub = { parameter: p, match: {}};
        subwhere.parameters.push(subsub); 

        if (constraint.hasOwnProperty(p)) {
          subsub.constraint = constraint[p];
        }

        subsub.optional = part.optional || false;

        if (pos + 1 < parts.length) {
          defineRoutePart(route, subsub.match, parts, pos + 1, constraint, callback);
        }
        else {
          subsub.match.route = route;
          subsub.match.callback = callback;
        }
      }
    }
  }
  else if (part.hasOwnProperty('prefix')) {
    if (! subwhere.hasOwnProperty('prefix')) {
      subwhere.prefix = {};
    }

    if (pos + 1 < parts.length) {
      console.error("cannot define prefix match within url, ignoring route");
    }
    else {
      subwhere.prefix.route = route;
      subwhere.prefix.callback = callback;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief defines a new route
////////////////////////////////////////////////////////////////////////////////

function defineRoute (route, where, url, callback) {
  var methods;
  var branch;
  var i;
  var j;

  methods = intersectMethods(url.methods, callback.methods);

  for (j = 0;  j < methods.length;  ++j) {
    var method = methods[j];

    defineRoutePart(route, where[method], url.urlParts, 0, url.constraint, callback);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the routing for a path and a a method
////////////////////////////////////////////////////////////////////////////////

function flattenRouting (routes, path, urlParameters, depth, prefix) {
  var cur;
  var i;
  var k;
  var newUrlParameters;
  var parameter;
  var match;
  var result = [];

  if (routes.hasOwnProperty('exact')) {
    for (k in routes.exact) {
      if (routes.exact.hasOwnProperty(k)) {
        newUrlParameters = urlParameters.shallowCopy;

        cur = path + "/" + k.replace(/([\.\+\*\?\^\$\(\)\[\]])/g, "\\$1");
        result = result.concat(flattenRouting(routes.exact[k],
                                              cur,
                                              newUrlParameters,
                                              depth + 1,
                                              false));
      }
    }
  }

  if (routes.hasOwnProperty('parameters')) {
    for (i = 0;  i < routes.parameters.length;  ++i) {
      parameter = routes.parameters[i];
      newUrlParameters = urlParameters.shallowCopy;

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
        cur = path + "(" + match + ")?";
      }
      else {
        cur = path + match;
      }

      newUrlParameters[parameter.parameter] = depth;

      result = result.concat(flattenRouting(parameter.match,
                                            cur,
                                            newUrlParameters,
                                            depth + 1,
                                            false));
    }
  }

  if (routes.hasOwnProperty('callback')) {
    result = result.concat([{
        path: path, 
        regexp: new RegExp("^" + path + "$"),
        prefix: prefix,
        depth: depth,
        urlParameters: urlParameters,
        callback: routes.callback,
        route: routes.route 
    }]);
  }

  if (routes.hasOwnProperty('prefix')) {
    if (! routes.prefix.hasOwnProperty('callback')) {
      console.error("prefix match must end in '/*'");
    }
    else {
      cur = path + "(/[^/]+)*";
      result = result.concat(flattenRouting(routes.prefix,
                             cur,
                             urlParameters.shallowCopy,
                             depth + 1,
                             true));
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoActions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a result of a query as documents
///
/// @FUN{actions.defineHttp(@FA{options})}
///
/// Defines a new action. The @FA{options} are as follows:
///
/// @FA{options.url}
///
/// The URL, which can be used to access the action. This path might contain
/// slashes. Note that this action will also be called, if a url is given such that
/// @FA{options.url} is a prefix of the given url and no longer definition
/// matches.
///
/// @FA{options.prefix}
///
/// If @LIT{false}, then only use the action for exact matches. The default is
/// @LIT{true}.
///
/// @FA{options.context}
///
/// The context to which this actions belongs. Possible values are "admin",
/// "monitoring", "api", and "user". All contexts apart from "user" are reserved
/// for system actions and are database independent. All actions except "user"
/// and "api" are executed in a different worker queue than the normal queue for
/// clients. The "api" actions are used by the client api to communicate with
/// the ArangoDB server.  Both the "api" and "user" actions are using the same
/// worker queue.
///
/// It is possible to specify a list of contexts, in case an actions belongs to
/// more than one context.
///
/// Note that the url for "user" actions is automatically prefixed
/// with @LIT{_action}. This applies to all specified contexts. For example, if
/// the context contains "admin" and "user" and the url is @LIT{hallo}, then the
/// action is accessible under @LIT{/_action/hallo} - even for the admin context.
///
/// @FA{options.callback}(@FA{request}, @FA{response})
///
/// The request argument contains a description of the request. A request
/// parameter @LIT{foo} is accessible as @LIT{request.parametrs.foo}. A request
/// header @LIT{bar} is accessible as @LIT{request.headers.bar}. Assume that
/// the action is defined for the url @LIT{/foo/bar} and the request url is
/// @LIT{/foo/bar/hugo/egon}. Then the suffix parts @LIT{[ "hugon"\, "egon" ]}
/// are availible in @LIT{request.suffix}.
///
/// The callback must define fill the @FA{response}.
///
/// - @LIT{@FA{response}.responseCode}: the response code
/// - @LIT{@FA{response}.contentType}: the content type of the response
/// - @LIT{@FA{response}.body}: the body of the response
///
/// You can use the functions @FN{ResultOk} and @FN{ResultError} to easily
/// generate a response.
///
/// @FA{options.parameters}
///
/// Normally the parameters are passed to the callback as strings. You can
/// use the @FA{options}, to force a converstion of the parameter to
///
/// - @c "collection"
/// - @c "collection-identifier"
/// - @c "collection-name"
/// - @c "number"
/// - @c "string"
////////////////////////////////////////////////////////////////////////////////

function defineHttp (options) {
  var url = options.url;
  var contexts = options.context;
  var callback = options.callback;
  var parameters = options.parameters;
  var prefix = true;
  var userContext = false;
  var i;

  if (! contexts) {
    contexts = "user";
  }

  if (typeof contexts === "string") {
    if (contexts === "user") {
      userContext = true;
    }

    contexts = [ contexts ];
  }
  else {
    for (i = 0;  i < contexts.length && ! userContext;  ++i) {
      var context = contexts[i];

      if (context === "user") {
        userContext = true;
      }
    }
  }

  if (userContext) {
    url = "_action/" + url;
  }

  if (typeof callback !== "function") {
    console.error("callback for '%s' must be a function, got '%s'", url, (typeof callback));
    return;
  }

  if (options.hasOwnProperty("prefix")) {
    prefix = options.prefix;
  }

  var parameter = { parameters : parameters, prefix : prefix };

  if (0 < contexts.length) {
    console.debug("defining action '%s' in contexts '%s'", url, contexts);

    try {
      internal.defineAction(url, callback, parameter, contexts);
    }
    catch (err) {
      console.error("action '%s' encountered error: %s", url, err);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get an error message string for an error code
///
/// @FUN{actions.getErrorMessage(@FA{code})}
///
/// Returns the error message for an error code.
////////////////////////////////////////////////////////////////////////////////

function getErrorMessage (code) {
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
  var body;
  var err;

  try {
    body = JSON.parse(req.requestBody || "{}") || {};
  }
  catch (err1) {
    exports.resultBad(req, res, arangodb.ERROR_HTTP_CORRUPTED_JSON, err1);
    return undefined;
  }

  if (! body || ! (body instanceof Object)) {
    if (code === undefined) {
      code = arangodb.ERROR_HTTP_CORRUPTED_JSON;
    }

    err = new internal.ArangoError();
    err.errorNum = code;
    err.errorMessage = "expecting a valid JSON object as body";

    exports.resultBad(req, res, code, err);
    return undefined;
  }

  return body;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates an error
///
/// @FUN{actions.resultError(@FA{req}, @FA{res}, @FA{code}, @FA{errorNum},
///                          @FA{errorMessage}, @FA{headers}, @FA{keyvals})}
///
/// The function generates an error response. The response body is an array
/// with an attribute @LIT{errorMessage} containing the error message
/// @FA{errorMessage}, @LIT{error} containing @LIT{true}, @LIT{code} containing
/// @FA{code}, @LIT{errorNum} containing @FA{errorNum}, and @LIT{errorMessage}
/// containing the error message @FA{errorMessage}. @FA{keyvals} are mixed
/// into the result.
////////////////////////////////////////////////////////////////////////////////

function resultError (req, res, httpReturnCode, errorNum, errorMessage, headers, keyvals) {  
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

  if (headers !== undefined) {
    res.headers = headers;    
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flushes cache and reload routing information
////////////////////////////////////////////////////////////////////////////////

function reloadRouting () {
  var i;
  var j;
  var routes;
  var routing;
  var handleRoute;
  var handleMiddleware;
  var method;

  // .............................................................................
  // clear the routing cache
  // .............................................................................

  RoutingCache = {};
  RoutingCache.flat = {};
  RoutingCache.routes = {};
  RoutingCache.middleware = {};

  for (i = 0;  i < ALL_METHODS.length;  ++i) {
    method = ALL_METHODS[i];

    RoutingCache.routes[method] = {};
    RoutingCache.middleware[method] = {};
  }

  // .............................................................................
  // lookup all routes
  // .............................................................................

  routes = [];

  routing = arangodb.db._collection("_routing");
  i = routing.all();

  while (i.hasNext()) {
    var n = i.next();

    routes.push(n.shallowCopy);
  }

  // allow the collection to unload
  i  = null;
  routing = null;

  // check development routes
  if (internal.developmentMode) {
    i = foxxManager.developmentRoutes();
    routes = routes.concat(i);
  }

  // .............................................................................
  // defines a new route
  // .............................................................................

  handleRoute = function (storage, urlPrefix, modulePrefix, route) {
    var url;
    var callback;

    url = lookupUrl(urlPrefix, route.url);

    if (url === null) {
      console.error("route '%s' has an unkown url, ignoring", JSON.stringify(route));
      return;
    }

    callback = lookupCallback(route);

    if (callback === null) {
      console.error("route '%s' has an unknown callback, ignoring", JSON.stringify(route));
      return;
    }

    defineRoute(route, storage, url, callback);
  };
  
  // .............................................................................
  // loop over the routes or routes bundle
  // .............................................................................

  for (j = 0;  j < routes.length;  ++j) {

    // clone the route object so the barrier for the collection can be removed soon
    var route = routes[j];
    var r;

    if (route.hasOwnProperty('routes') || route.hasOwnProperty('middleware')) {
      var urlPrefix = route.urlPrefix || "";
      var modulePrefix = route.modulePrefix || "";

      if (route.hasOwnProperty('routes')) {
        r = route.routes;

        for (i = 0;  i < r.length;  ++i) {
          handleRoute(RoutingCache.routes, urlPrefix, modulePrefix, r[i]);
        }
      }

      if (route.hasOwnProperty('middleware')) {
        r = route.middleware;

        for (i = 0;  i < r.length;  ++i) {
          handleRoute(RoutingCache.middleware, urlPrefix, modulePrefix, r[i]);
        }
      }
    }
    else {
      handleRoute(RoutingCache.routes, "", "", route);
    }
  }

  // .............................................................................
  // compute the flat routes
  // .............................................................................

  RoutingCache.flat = {};

  for (i = 0;  i < ALL_METHODS.length;  ++i) {
    var a;
    var b;

    method = ALL_METHODS[i];

    a = flattenRouting(RoutingCache.routes[method], "", {}, 0, false);
    b = flattenRouting(RoutingCache.middleware[method], "", {}, 0, false).reverse();

    RoutingCache.flat[method] = b.concat(a);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds the next routing
////////////////////////////////////////////////////////////////////////////////

function nextRouting (state) {
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
  var url = parts;

  if (typeof url === 'string') {
    parts = url.split("/");

    if (parts[0] === '') {
      parts.shift();
    }
  }
  else {
    url = "/" + parts.join("/");
  }

  if (! RoutingCache.flat || ! RoutingCache.flat.hasOwnProperty(type)) {
    return {
      parts: parts,
      position: -1,
      url: url,
      urlParameters: {}
    };
  }

  return nextRouting({
    routing: RoutingCache.flat[type],
    parts: parts,
    position: -1,
    url: url,
    urlParameters: {}
  });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if development mode is allowed in general
////////////////////////////////////////////////////////////////////////////////

function developmentModeAllowed () {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if development mode is activated for an application
////////////////////////////////////////////////////////////////////////////////

function developmentModeActivated (application) {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                           standard HTTP responses
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoActions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief convenience function to return an HTTP 400 error for a bad parameter
////////////////////////////////////////////////////////////////////////////////

function badParameter (req, res, name) {
  resultError(req, res, exports.HTTP_BAD, exports.HTTP_BAD, 
              "invalid value for parameter '" + name + "'");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a result
///
/// @FUN{actions.resultOk(@FA{req}, @FA{res}, @FA{code}, @FA{result}, @FA{headers}})}
///
/// The function defines a response. @FA{code} is the status code to
/// return. @FA{result} is the result object, which will be returned as JSON
/// object in the body. @LIT{headers} is an array of headers to returned.
/// The function adds the attribute @LIT{error} with value @LIT{false}
/// and @LIT{code} with value @FA{code} to the @FA{result}.
////////////////////////////////////////////////////////////////////////////////

function resultOk (req, res, httpReturnCode, result, headers) {  
  res.responseCode = httpReturnCode;
  res.contentType = "application/json; charset=utf-8";
  
  // add some default attributes to result
  if (result === undefined) {
    result = {};
  }

  result.error = false;  
  result.code = httpReturnCode;
  
  res.body = JSON.stringify(result);
  
  if (headers !== undefined) {
    res.headers = headers;    
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates an error for a bad request
///
/// @FUN{actions.resultBad(@FA{req}, @FA{res}, @FA{error-code}, @FA{msg}, @FA{headers})}
///
/// The function generates an error response.
////////////////////////////////////////////////////////////////////////////////

function resultBad (req, res, code, msg, headers) {
  resultError(req, res, exports.HTTP_BAD, code, msg, headers);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates an error for not found 
///
/// @FUN{actions.resultNotFound(@FA{req}, @FA{res}, @FA{code}, @FA{msg}, @FA{headers})}
///
/// The function generates an error response.
////////////////////////////////////////////////////////////////////////////////

function resultNotFound (req, res, code, msg, headers) {
  resultError(req, res, exports.HTTP_NOT_FOUND, code, msg, headers);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates an error for not implemented
///
/// @FUN{actions.resultNotImplemented(@FA{req}, @FA{res}, @FA{msg}, @FA{headers})}
///
/// The function generates an error response.
////////////////////////////////////////////////////////////////////////////////

function resultNotImplemented (req, res, msg, headers) {
  resultError(req, 
              res,
              exports.HTTP_NOT_IMPLEMENTED,
              arangodb.ERROR_NOT_IMPLEMENTED,
              msg,
              headers);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates an error for unsupported methods
///
/// @FUN{actions.resultUnsupported(@FA{req}, @FA{res}, @FA{headers})}
///
/// The function generates an error response.
////////////////////////////////////////////////////////////////////////////////

function resultUnsupported (req, res, headers) {
  resultError(req, res,
              exports.HTTP_METHOD_NOT_ALLOWED,
              arangodb.ERROR_HTTP_METHOD_NOT_ALLOWED,
              "Unsupported method",
              headers);  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a permanently redirect
///
/// @FUN{actions.resultPermanentRedirect(@FA{req}, @FA{res}, @FA{destination}, @FA{headers})}
///
/// The function generates an error response.
////////////////////////////////////////////////////////////////////////////////

function resultPermanentRedirect (req, res, destination, headers) {
  res.responseCode = exports.HTTP_MOVED_PERMANENTLY;
  res.contentType = "text/html";

  res.body = "<html><head><title>Moved</title>"
    + "</head><body><h1>Moved</h1><p>This page has moved to <a href=\""
    + destination
    + "\">"
    + destination
    + "</a>.</p></body></html>";

  if (headers !== undefined) {
    res.headers = headers.shallowCopy;
  }
  else {
    res.headers = {};
  }

  res.headers.location = destination;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a temporary redirect
///
/// @FUN{actions.resultTemporaryRedirect(@FA{req}, @FA{res}, @FA{destination}, @FA{headers})}
///
/// The function generates an error response.
////////////////////////////////////////////////////////////////////////////////

function resultTemporaryRedirect (req, res, destination, headers) {
  res.responseCode = exports.HTTP_TEMPORARY_REDIRECT;
  res.contentType = "text/html";

  res.body = "<html><head><title>Moved</title>"
    + "</head><body><h1>Moved</h1><p>This page has moved to <a href=\""
    + destination
    + "\">"
    + destination
    + "</a>.</p></body></html>";

  if (headers !== undefined) {
    res.headers = headers.shallowCopy;
  }
  else {
    res.headers = {};
  }

  res.headers.location = destination;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      ArangoDB specific responses
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoActions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a result set from a cursor
////////////////////////////////////////////////////////////////////////////////

function resultCursor (req, res, cursor, code, options) {
  var rows;
  var count;
  var hasCount;
  var hasNext;
  var cursorId;

  if (Array.isArray(cursor)) {
    // performance optimisation: if the value passed in is an array, we can
    // use it as it is
    hasCount = ((options && options.countRequested) ? true : false);
    count = cursor.length;
    rows = cursor;
    hasNext = false;
    cursorId = null;
  }
  else {
    // cursor is assumed to be an ArangoCursor
    hasCount = cursor.hasCount();
    count = cursor.count();
    rows = cursor.getRows();

    // must come after getRows()
    hasNext = cursor.hasNext();
    cursorId = null;
   
    if (hasNext) {
      cursor.persist();
      cursorId = cursor.id(); 
    }
    else {
      cursor.dispose();
    }
  }

  var result = { 
    "result" : rows,
    "hasMore" : hasNext
  };

  if (cursorId) {
    result.id = cursorId;
  }
    
  if (hasCount) {
    result.count = count;
  }

  if (code === undefined) {
    code = exports.HTTP_CREATED;
  }

  resultOk(req, res, code, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates an error for unknown collection
///
/// @FUN{actions.collectionNotFound(@FA{req}, @FA{res}, @FA{collection}, @FA{headers})}
///
/// The function generates an error response.
////////////////////////////////////////////////////////////////////////////////

function collectionNotFound (req, res, collection, headers) {
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
/// @brief generates an error for unknown index
///
/// @FUN{actions.collectionNotFound(@FA{req}, @FA{res}, @FA{collection}, @FA{index}, @FA{headers})}
///
/// The function generates an error response.
////////////////////////////////////////////////////////////////////////////////

function indexNotFound (req, res, collection, index, headers) {
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
/// @brief generates an error for an exception
///
/// @FUN{actions.resultException(@FA{req}, @FA{res}, @FA{err}, @FA{headers})}
///
/// The function generates an error response.
////////////////////////////////////////////////////////////////////////////////

function resultException (req, res, err, headers) {
  var code;
  var msg;
  var num;

  if (err instanceof internal.ArangoError) {
    num = err.errorNum;
    msg = err.errorMessage;
    code = exports.HTTP_BAD;

    if (num === 0) {
      num = arangodb.ERROR_INTERNAL;
    }

    if (msg === "") {
      msg = String(err) + " " + String(err.stack);
    }
    else {
      msg += " " + String(err.stack);
    }
    
    switch (num) {
      case arangodb.ERROR_INTERNAL: 
        code = exports.HTTP_SERVER_ERROR; 
        break;

      case arangodb.ERROR_ARANGO_DUPLICATE_NAME: 
      case arangodb.ERROR_ARANGO_DUPLICATE_IDENTIFIER: 
        code = exports.HTTP_CONFLICT; 
        break;
    }

    resultError(req, res, code, num, msg, headers);
  }
  else if (err instanceof TypeError) {
    num = arangodb.ERROR_HTTP_BAD_PARAMETER;
    code = exports.HTTP_BAD;
    msg = String(err.message) + " " + String(err.stack);

    resultError(req, res, code, num, msg, headers);
  }

  else {
    resultError(req, res,
                exports.HTTP_SERVER_ERROR, arangodb.ERROR_HTTP_SERVER_ERROR,
                String(err) + " " + String(err.stack),
                headers);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  specific actions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoActions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief echos a request
////////////////////////////////////////////////////////////////////////////////

function echoRequest (req, res, options, next) {
  var result;

  result = { request : req, options : options };

  res.responseCode = exports.HTTP_OK;
  res.contentType = "application/json; charset=utf-8";
  res.body = JSON.stringify(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a request
////////////////////////////////////////////////////////////////////////////////

function logRequest (req, res, options, next) {
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
  if (options.permanently) {
    resultPermanentRedirect(req, res, options.destination);
  }
  else {
    resultTemporaryRedirect(req, res, options.destination);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief redirects a request
////////////////////////////////////////////////////////////////////////////////

function pathHandler (req, res, options, next) {
  var filename;
  var result;

  filename = fs.join(options.path, fs.join.apply(fs.join, req.suffix));

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
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoActions
/// @{
////////////////////////////////////////////////////////////////////////////////

// public functions
exports.defineHttp               = defineHttp;
exports.getErrorMessage          = getErrorMessage;
exports.getJsonBody              = getJsonBody;
exports.errorFunction            = errorFunction;
exports.reloadRouting            = reloadRouting;
exports.firstRouting             = firstRouting;
exports.nextRouting              = nextRouting;
exports.routingCache             = function() { return RoutingCache; };
exports.developmentModeAllowed   = developmentModeAllowed;
exports.developmentModeActivated = developmentModeActivated;

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
exports.echoRequest              = echoRequest;
exports.logRequest               = logRequest;
exports.redirectRequest          = redirectRequest;
exports.pathHandler              = pathHandler;

// some useful constants
exports.COLLECTION               = "collection";
exports.COLLECTION_IDENTIFIER    = "collection-identifier";
exports.COLLECTION_NAME          = "collection-name";
exports.NUMBER                   = "number";

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
exports.HTTP_CONFLICT            = 409;
exports.HTTP_PRECONDITION_FAILED = 412;
exports.HTTP_UNPROCESSABLE_ENTIT = 422;

// HTTP 5xx
exports.HTTP_SERVER_ERROR        = 500;
exports.HTTP_NOT_IMPLEMENTED     = 501;
exports.HTTP_BAD_GATEWAY         = 502;
exports.HTTP_SERVICE_UNAVAILABLE = 503;

// load routing
reloadRouting();

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}"
// End:
