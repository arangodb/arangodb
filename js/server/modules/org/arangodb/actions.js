////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript actions modules
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
var console = require("console");

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

var RoutingCache = {};

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
/// @brief looks up a callback for a string
////////////////////////////////////////////////////////////////////////////////

function LookupCallbackString (callback) {
  var components;
  var module;
  var fn;

  if (callback == "") {
    return undefined;
  }

  components = callback.split("/");
  fn = components[components.length - 1];

  components.pop();
  module = components.join("/");

  try {
    module = require(module);
  }
  catch (err) {
    console.error("cannot load callback '%s' from module '%s': %s",
                  fn, module, "" + err);
    return undefined;
  }

  if (fn in module) {
    return module[fn];
  }

  console.error("callback '%s' is not defined in module '%s'", fn, module);
  return undefined;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a callback for a function
////////////////////////////////////////////////////////////////////////////////

function LookupCallbackFunction (callback) {
  var module;
  var fn;

  try {
    module = require(callback.module);
  }
  catch (err) {
    console.error("cannot load callback '%s' from module '%s': %s",
                  fn, callback.module, "" + err);
    return undefined;
  }

  fn = callback.function;

  if (fn in module) {
    return module[fn];
  }

  console.error("callback '%s' is not defined in module '%s'", fn, module);
  return undefined;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a callback for static data
////////////////////////////////////////////////////////////////////////////////

function LookupCallbackStatic (callback) {
  var type;
  var body;

  type = callback.contentType || "text/plain";
  body = callback.body || "";

  return function (req, res) {
    res.responseCode = exports.HTTP_OK;
    res.contentType = type;
    res.body = body;
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a callback for a redirect
////////////////////////////////////////////////////////////////////////////////

function LookupCallbackRedirect (callback) {
  var redirect;

  redirect = callback.redirect;

  if (redirect == "") {
    console.error("empty redirect not allowed");
    return undefined;
  }

  return function (req, res, next, options) {
    var perm = true;

    if ('permanent' in options) {
      perm = options.perm;
    }

    if (perm) {
      actions.resultPermanentRedirect(req, res, redirect);
    }
    else {
      actions.resultTemporaryRedirect(req, res, redirect);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a callback
////////////////////////////////////////////////////////////////////////////////

function LookupCallback (callback) {
  var type;

  if (typeof callback === 'object') {
    if ('body' in callback) {
      return LookupCallbackStatic(callback);
    }
    else if ('for' in callback && 'do' in callback) {
      return LookupCallbackFunction(callback);
    }
    else if ('redirect' in callback) {
      return LookupCallbackRedirect(callback);
    }
    else {
      console.error("unknown callback type '%s'", JSON.stringify(callback));
      return undefined;
    }
  }
  else if (typeof callback === "string") {
    return LookupCallbackString(callback);
  }

  return undefined;
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

function DefineHttp (options) {
  var url = options.url;
  var contexts = options.context;
  var callback = options.callback;
  var parameters = options.parameters;
  var prefix = true;
  var userContext = false;

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
    for (var i = 0;  i < contexts.length && ! userContext;  ++i) {
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

function GetErrorMessage (code) {
  for (var key in internal.errors) {
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

function GetJsonBody (req, res, code) {
  var body;

  try {
    body = JSON.parse(req.requestBody || "{}") || {};
  }
  catch (err) {
    ResultBad(req, res, exports.ERROR_HTTP_CORRUPTED_JSON, err);
    return undefined;
  }

  if (! body || ! (body instanceof Object)) {
    if (code === undefined) {
      code = exports.ERROR_HTTP_CORRUPTED_JSON;
    }

    ResultBad(req, res, code, err);
    return undefined;
  }

  return body;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates an error
///
/// @FUN{actions.resultError(@FA{req}, @FA{res}, @FA{code}, @FA{errorNum}, @FA{errorMessage}, @FA{headers}, @FA{keyvals})}
///
/// The functions generates an error response. The response body is an array
/// with an attribute @LIT{errorMessage} containing the error message
/// @FA{errorMessage}, @LIT{error} containing @LIT{true}, @LIT{code} containing
/// @FA{code}, @LIT{errorNum} containing @FA{errorNum}, and @LIT{errorMessage}
/// containing the error message @FA{errorMessage}. @FA{keyvals} are mixed
/// into the result.
////////////////////////////////////////////////////////////////////////////////

function ResultError (req, res, httpReturnCode, errorNum, errorMessage, headers, keyvals) {  
  res.responseCode = httpReturnCode;
  res.contentType = "application/json";

  if (typeof errorNum === "string") {
    keyvals = headers;
    headers = errorMessage;
    errorMessage = errorNum;
    errorNum = httpReturnCode;
  }
  
  var result = {};

  if (keyvals !== undefined) {
    for (var i in keyvals) {
      if (keyvals.hasOwnProperty(i)) {
        result[i] = keyvals[i];
      }
    }
  }

  result["error"]        = true;
  result["code"]         = httpReturnCode;
  result["errorNum"]     = errorNum;
  result["errorMessage"] = errorMessage;
  
  res.body = JSON.stringify(result);

  if (headers !== undefined) {
    res.headers = headers;    
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flushes cache and reload routing information
////////////////////////////////////////////////////////////////////////////////

function ReloadRouting () {
  var all = [ exports.DELETE, exports.GET, exports.HEAD, exports.POST, exports.PUT, exports.PATCH ];
  var callbacks;
  var i;
  var method;
  var routes;
  var routing = internal.db._collection("_routing");
  var routings;

  RoutingCache = {};

  for (i = 0;  i < all.length;  ++i) {
    method = all[i];
    RoutingCache[method] = {};
  }

  if (routing === null) {
    return;
  }

  routes = routing.all();

  while (routes.hasNext()) {
    var route = routes.next();

    var callback;
    var methods = [ exports.GET, exports.HEAD ];
    var topdown = false;
    var path;
    var prefix = false;
    var prio = 1.0;
    var tmp;

    // extract path
    if (! ('path' in route)) {
      console.error("route %s is missing the 'path' attribute", route._id);
      continue;
    }

    path = route.path;

    // extract callback
    if (! ('callback' in route)) {
      console.error("route %s is missing the 'callback' attribute", route._id);
      continue;
    }

    callback = route.callback;

    if ( !(callback instanceof Array)) {
      callback = [ callback ];
    }

    tmp = [];

    for (i = 0;  i < callback.length;  ++i) {
      var cb = LookupCallback(callback[i]);

      if (cb === undefined) {
        console.error("route '%s' contains invalid callback '%s'", 
                      route._id, JSON.stringify(callback[i]));
      }
      else if (typeof cb !== "function") {
        console.error("route '%s' contains non-function callback '%s'",
                      route._id, JSON.stringify(callback[i]));
      }
      else {
        var result = { 
          func: cb,
          options: callback[i].options || {}
        };

        tmp.push(result);
      }
    }

    callback = tmp;

    // extract method
    if ('method' in route) {
      methods = route.method;

      if (typeof methods === 'string') {
        if (methods === "all" || methods === "*") {
          methods = all;
        }
        else {
          methods = [ methods ];
        }
      }
    }

    // extract priority
    if ('priority' in route) {
      prio = route.priority;
    }

    // extract topdown flag
    if ('topdown' in route) {
      topdown = route.topdown;
    }

    // extract prefix flag
    if ('prefix' in route) {
      prefix = route.prefix;
    }

    for (i = 0;  i < methods.length;  ++i) {
      method = methods[i].toUpperCase();
      routings = RoutingCache[method];

      if (routings === undefined) {
        console.error("unknown method '%s' in route %s", method, route._id);
        continue;
      }

      if (path in routings) {
        callbacks = routings[path];
      }
      else {
        callbacks = routings[path] = []
      }

      callbacks.push({
        route : route._id,
        path : path,
        topdown : topdown,
        priority : prio,
        prefix : prefix,
        callback : callback
      });
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extratcs the routing for a path and a a method
////////////////////////////////////////////////////////////////////////////////

function Routing (method, path) {
  var bottomup;
  var components;
  var current;
  var i;
  var j;
  var l;
  var result;
  var routings;
  var topdown;

  routings = RoutingCache[method];

  if (routings === undefined) {
    return [];
  }

  topdown = [];
  bottomup = [];

  current = "/";
  components = [""].concat(path);

  l = components.length;

  for (i = 0;  i < l;  ++i) {
    var full = (i + 1) == l;

    if (1 < i) {
      current += "/";
    }

    current += components[i];

    if (current in routings) {
      var callbacks = routings[current];

      for (j = 0;  j < callbacks.length;  ++j) {
        var callback = callbacks[j];

        if (callback.prefix || full) {
          if (callback.topdown) {
            topdown.push(callback);
          }
          else {
            bottomup.push(callback);
          }
        }
      }
    }
  }

  var sortTD = function (a, b) {
    var la = a.path.length;
    var lb = b.path.length;

    if (la == lb) {
      return a.priority - b.priority;
    }

    return la - lb;
  }

  var sortBU = function (a, b) {
    var la = a.path.length;
    var lb = b.path.length;

    if (la == lb) {
      return b.priority - a.priority;
    }

    return lb - la;
  }

  topdown.sort(sortTD);
  bottomup.sort(sortBU);

  result = [];

  for (i = 0;  i < topdown.length;  ++i) {
    var td = topdown[i];
    var callback = td.callback;

    for (j = 0;  j < callback.length;  ++j) {
      result.push({ 
        func: callback[j].func,
        options: callback[j].options,
        path: td.path
      });
    }
  }

  for (i = 0;  i < bottomup.length;  ++i) {
    var bu = bottomup[i];
    var callback = bu.callback;

    for (j = 0;  j < callback.length;  ++j) {
      result.push({
        func: callback[j].func,
        options: callback[j].options,
        path: bu.path
      });
    }
  }

  return result;
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
/// @brief returns a result
///
/// @FUN{actions.resultOk(@FA{req}, @FA{res}, @FA{code}, @FA{result}, @FA{headers}})}
///
/// The functions defines a response. @FA{code} is the status code to
/// return. @FA{result} is the result object, which will be returned as JSON
/// object in the body. @LIT{headers} is an array of headers to returned.
/// The function adds the attribute @LIT{error} with value @LIT{false}
/// and @LIT{code} with value @FA{code} to the @FA{result}.
////////////////////////////////////////////////////////////////////////////////

function ResultOk (req, res, httpReturnCode, result, headers) {  
  res.responseCode = httpReturnCode;
  res.contentType = "application/json";
  
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
/// The functions generates an error response.
////////////////////////////////////////////////////////////////////////////////

function ResultBad (req, res, code, msg, headers) {
  if (msg === undefined || msg === null) {
    msg = GetErrorMessage(code);
  }
  else {
    msg = "" + msg;
  }

  ResultError(req, res, exports.HTTP_BAD, code, msg, headers);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates an error for not found 
///
/// @FUN{actions.resultNotFound(@FA{req}, @FA{res}, @FA{msg}, @FA{headers})}
///
/// The functions generates an error response.
////////////////////////////////////////////////////////////////////////////////

function ResultNotFound (req, res, msg, headers) {
  ResultError(req, res, exports.HTTP_NOT_FOUND, exports.ERROR_HTTP_NOT_FOUND, "" + msg, headers);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates an error for not implemented
///
/// @FUN{actions.resultNotImplemented(@FA{req}, @FA{res}, @FA{msg}, @FA{headers})}
///
/// The functions generates an error response.
////////////////////////////////////////////////////////////////////////////////

function ResultNotImplemented (req, res, msg, headers) {
  ResultError(req, res, exports.HTTP_NOT_IMPLEMENTED, exports.ERROR_NOT_IMPLEMENTED, "" + msg, headers);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates an error for unsupported methods
///
/// @FUN{actions.resultUnsupported(@FA{req}, @FA{res}, @FA{headers})}
///
/// The functions generates an error response.
////////////////////////////////////////////////////////////////////////////////

function ResultUnsupported (req, res, headers) {
  ResultError(req, res,
              exports.HTTP_METHOD_NOT_ALLOWED, exports.ERROR_HTTP_METHOD_NOT_ALLOWED,
              "Unsupported method",
              headers);  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a permanently redirect
///
/// @FUN{actions.resultPermanentRedirect(@FA{req}, @FA{res}, @FA{destination}, @FA{headers})}
///
/// The functions generates an error response.
////////////////////////////////////////////////////////////////////////////////

function ResultPermanentRedirect (req, res, destination, headers) {
  res.responseCode = actions.HTTP_MOVED_PERMANENTLY;
  res.contentType = "text/html";

  res.body = "<html><head><title>Moved</title></head><body><h1>Moved</h1><p>This page has moved to <a href=\""
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

  res.headers.location = dest;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a temporary redirect
///
/// @FUN{actions.resultTemporaryRedirect(@FA{req}, @FA{res}, @FA{destination}, @FA{headers})}
///
/// The functions generates an error response.
////////////////////////////////////////////////////////////////////////////////

function ResultTemporaryRedirect (req, res, destination, headers) {
  res.responseCode = actions.HTTP_TEMPORARY_REDIRECT;
  res.contentType = "text/html";

  res.body = "<html><head><title>Moved</title></head><body><h1>Moved</h1><p>This page has moved to <a href=\""
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

  res.headers.location = dest;
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

function ResultCursor (req, res, cursor, code, options) {
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
    result["id"] = cursorId;
  }
    
  if (hasCount) {
    result["count"] = count;
  }

  if (code === undefined) {
    code = exports.HTTP_CREATED;
  }

  ResultOk(req, res, code, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates an error for unknown collection
///
/// @FUN{actions.collectionNotFound(@FA{req}, @FA{res}, @FA{collection}, @FA{headers})}
///
/// The functions generates an error response.
////////////////////////////////////////////////////////////////////////////////

function CollectionNotFound (req, res, collection, headers) {
  if (collection === undefined) {
    ResultError(req, res,
                exports.HTTP_BAD, exports.ERROR_HTTP_BAD_PARAMETER,
                "expecting a collection name or identifier",
                headers);
  }
  else {
    ResultError(req, res,
                exports.HTTP_NOT_FOUND, exports.ERROR_ARANGO_COLLECTION_NOT_FOUND,
                "unknown collection '" + collection + "'", headers);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates an error for unknown index
///
/// @FUN{actions.collectionNotFound(@FA{req}, @FA{res}, @FA{collection}, @FA{index}, @FA{headers})}
///
/// The functions generates an error response.
////////////////////////////////////////////////////////////////////////////////

function IndexNotFound (req, res, collection, index, headers) {
  if (collection === undefined) {
    ResultError(req, res,
                exports.HTTP_BAD, exports.ERROR_HTTP_BAD_PARAMETER,
                "expecting a collection name or identifier",
                headers);
  }
  else if (index === undefined) {
    ResultError(req, res,
                exports.HTTP_BAD, exports.ERROR_HTTP_BAD_PARAMETER,
                "expecting an index identifier",
                headers);
  }
  else {
    ResultError(req, res,
                exports.HTTP_NOT_FOUND, exports.ERROR_ARANGO_INDEX_NOT_FOUND,
                "unknown index '" + index + "'", headers);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates an error for an exception
///
/// @FUN{actions.resultException(@FA{req}, @FA{res}, @FA{err}, @FA{headers})}
///
/// The functions generates an error response.
////////////////////////////////////////////////////////////////////////////////

function ResultException (req, res, err, headers) {
  if (err instanceof ArangoError) {
    var num = err.errorNum;
    var msg = err.errorMessage;
    var code = exports.HTTP_BAD;

    switch (num) {
      case exports.ERROR_INTERNAL: code = exports.HTTP_SERVER_ERROR; break;
    }

    ResultError(req, res, code, num, msg, headers);
  }
  else {
    ResultError(req, res,
                exports.HTTP_SERVER_ERROR, exports.ERROR_HTTP_SERVER_ERROR,
                "" + err,
                headers);
  }
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
/// @brief echos a request
////////////////////////////////////////////////////////////////////////////////

function EchoRequest (req, res, next, options) {
  var result;

  result = { request : req, options : options };

  res.responseCode = exports.HTTP_OK;
  res.contentType = "application/json";
  res.body = JSON.stringify(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a request
////////////////////////////////////////////////////////////////////////////////

function LogRequest (req, res, next, options) {
  var log;
  var level;
  var token;

  if ('level' in options) {
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
    else if (level === "warn") {
      log = console.warn;
    }
    else {
      log = console.log;
    }
  }
  else {
    log = console.log;
  }

  if ('token' in options) {
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
exports.defineHttp              = DefineHttp;
exports.getErrorMessage         = GetErrorMessage;
exports.getJsonBody             = GetJsonBody;
exports.reloadRouting           = ReloadRouting;
exports.routing                 = Routing;

// standard HTTP responses
exports.resultBad               = ResultBad;
exports.resultError             = ResultError;
exports.resultNotFound          = ResultNotFound;
exports.resultNotImplemented    = ResultNotImplemented;
exports.resultOk                = ResultOk;
exports.resultPermanentRedirect = ResultPermanentRedirect;
exports.resultTemporaryRedirect = ResultTemporaryRedirect;
exports.resultUnsupported       = ResultUnsupported;

// ArangoDB specific responses
exports.resultCursor            = ResultCursor;
exports.collectionNotFound      = CollectionNotFound;
exports.indexNotFound           = IndexNotFound;
exports.resultException         = ResultException;

// standard actions
exports.echoRequest             = EchoRequest;
exports.logRequest              = LogRequest;

// some useful constants
exports.COLLECTION              = "collection";
exports.COLLECTION_IDENTIFIER   = "collection-identifier";
exports.COLLECTION_NAME         = "collection-name";
exports.NUMBER                  = "number";

exports.DELETE                  = "DELETE";
exports.GET                     = "GET";
exports.HEAD                    = "HEAD";
exports.POST                    = "POST";
exports.PUT                     = "PUT";
exports.PATCH                   = "PATCH";

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

// copy error codes
for (var name in internal.errors) {
  if (internal.errors.hasOwnProperty(name)) {
    exports[name] = internal.errors[name].code;
  }
}

// load routing
ReloadRouting();

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
