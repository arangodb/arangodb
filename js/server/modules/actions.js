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
// --SECTION--                                                  public constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoActions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief error codes 
////////////////////////////////////////////////////////////////////////////////

exports.queryNotFound = 10404;
exports.queryNotModified = 10304;

exports.collectionNotFound = 20404;
exports.documentNotFound = 30404;
exports.documentNotModified = 30304;

exports.cursorNotFound = 40404;
exports.cursorNotModified = 40304;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoActions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a result of a query as documents
///
/// @FUN{defineHttp(@FA{options})}
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
/// @FA{options.context}
///
/// The context to which this actions belongs. Possible values are "admin",
/// "monitoring", "api", and "user". All contexts apart from "user" are reserved
/// for system actions and are database independent. All actions except "user"
/// and "api" are executed in a different worker queue than the normal queue for
/// clients. The "api" actions are used by the client api to communicate with
/// the AvocadoDB server.  Both the "api" and "user" actions are using the same
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
/// You can use the functions @FN{actionResult} and @FN{actionError} to
/// easily generate a response.
///
/// @FA{options.parameters}
///
/// Normally the paramaters are passed to the callback as strings. You can
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
  var parameter = options.parameter;
  var userContext = false;

  if (! contexts) {
    contexts = "user";
  }

  if (typeof contexts == "string") {
    if (contexts == "user") {
      userContext = true;
    }

    contexts = [ contexts ];
  }
  else {
    for (var i = 0;  i < contexts.length && ! userContext;  ++i) {
      if (context == "user") {
        userContext = true;
      }
    }
  }

  if (userContext) {
    url = "_action/" + url;
  }

  if (typeof callback !== "function") {
    console.error("callback for '%s' must be a function, got '%s'", url + (typeof callback));
    return;
  }

  // console.debug("callback: %s", callback);

  for (var i = 0;  i < contexts.length;  ++i) {
    var context = contexts[i];
    var use = false;

    if (context == "admin") {
      if (SYS_ACTION_QUEUE == "SYSTEM") {
        use = true;
      }
    }
    else if (context == "api") {
      if (SYS_ACTION_QUEUE == "SYSTEM" || SYS_ACTION_QUEUE == "CLIENT") {
        use = true;
      }
    }
    else if (context == "user") {
      if (SYS_ACTION_QUEUE == "SYSTEM" || SYS_ACTION_QUEUE == "CLIENT") {
        use = true;
      }
    }
    else if (context == "monitoring") {
      if (SYS_ACTION_QUEUE == "MONITORING") {
        use = true;
      }
    }

    if (use) {
      try {
        internal.defineAction(url, SYS_ACTION_QUEUE, callback, parameter);
        console.debug("defining action '%s' in context '%s' using queue '%s'", url, context, SYS_ACTION_QUEUE);
      }
      catch (err) {
        console.error("action '%s' encountered error: %s", url, err);
      }
    }
    else {
      console.debug("ignoring '%s' for context '%s' in queue '%s'", url, context, SYS_ACTION_QUEUE);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a response
///
/// @FUN{actionResult(@FA{req}, @FA{res}, @FA{code}, @FA{result}, @FA{headers})}
///
/// The functions defines a response. @FA{code} is the status code to
/// return. @FA{result} is the result object, which will be returned as JSON
/// object in the body. @LIT{headers} is an array of headers to returned.
////////////////////////////////////////////////////////////////////////////////

function actionResult (req, res, code, result, headers) {
  res.responseCode = code;

  if (result) {
    res.contentType = "application/json";
    res.body = JSON.stringify(result);
  }

  if (headers != undefined) {
    res.headers = headers;    
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates an error response
///
/// @FUN{actionError(@FA{req}, @FA{res}, @FA{errorMessage})}
///
/// The functions generates an error response. The status code is 500 and the
/// returned object is an array with an attribute @LIT{error} containing
/// the error message @FA{errorMessage}.
////////////////////////////////////////////////////////////////////////////////

function actionError (req, res, err) {
  res.responseCode = 500;
  res.contentType = "application/json";
  res.body = JSON.stringify({ 'error' : "" + err });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a result
///
/// @FUN{actionResultOK(@FA{req}, @FA{res}, @FA{code}, @FA{result}, @FA{headers}})}
///
/// Works like @FN{actionResult} but adds the attribute @LIT{error} with
/// value @LIT{false} and @LIT{code} with value @FA{code} to the @FA{result}.
////////////////////////////////////////////////////////////////////////////////

function actionResultOK (req, res, httpReturnCode, result, headers) {  
  res.responseCode = httpReturnCode;
  res.contentType = "application/json";
  
  // add some default attributes to result
  if (result == undefined) {
    result = {};
  }

  result.error = false;  
  result.code = httpReturnCode;
  
  res.body = JSON.stringify(result);
  
  if (headers != undefined) {
    res.headers = headers;    
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns an error
///
/// @FUN{actionResultError(@FA{req}, @FA{res}, @FA{code}, @FA{errorNum}, @FA{errorMessage}, @FA{headers})}
///
/// The functions generates an error response. The response body is an array
/// with an attribute @LIT{errorMessage} containing the error message
/// @FA{errorMessage}, @LIT{error} containing @LIT{true}, @LIT{code}
/// containing @FA{code}, @LIT{errorNum} containing @FA{errorNum}, and
/// $LIT{errorMessage} containing the error message @FA{errorMessage}.
////////////////////////////////////////////////////////////////////////////////

function actionResultError (req, res, httpReturnCode, errorNum, errorMessage, headers) {  
  res.responseCode = httpReturnCode;
  res.contentType = "application/json";
  
  var result = {
    "error"        : true,
    "code"         : httpReturnCode,
    "errorNum"     : errorNum,
    "errorMessage" : errorMessage
  }
  
  res.body = JSON.stringify(result);

  if (headers != undefined) {
    res.headers = headers;    
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns an error for unsupported methods
///
/// @FUN{actionResultUnsupported(@FA{req}, @FA{res}, @FA{headers})}
///
/// The functions generates an error response.
////////////////////////////////////////////////////////////////////////////////

function actionResultUnsupported (req, res, headers) {
  actionResultError(req, res, 405, 405, "Unsupported method", headers);  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns an error for unknown collection
///
/// @FUN{actionCollectionUnknown(@FA{req}, @FA{res}, @FA{collection}, @FA{headers})}
///
/// The functions generates an error response.
////////////////////////////////////////////////////////////////////////////////

function resultCollectionUnknown (req, res, collection, headers) {
  if (collection == null) {
    actionResultError(req, res, 400, 400, "expecting a collection name or identifier", headers);
  }
  else {
    actionResultError(req, res, 405, 405, "unknown collection '" + collection + "'", headers);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoActions
/// @{
////////////////////////////////////////////////////////////////////////////////

exports.defineHttp = defineHttp;
exports.actionResult = actionResult;
exports.actionResultOK = actionResultOK;
exports.actionResultError = actionResultError;
exports.actionResultUnsupported = actionResultUnsupported;
exports.actionError = actionError;

exports.result = actionResult;
exports.resultOK = actionResultOK;
exports.resultError = actionResultError;
exports.resultUnsupported = actionResultUnsupported;

exports.resultCollectionUnknown = resultCollectionUnknown;

exports.COLLECTION            = "collection";
exports.COLLECTION_IDENTIFIER = "collection-identifier";
exports.COLLECTION_NAME       = "collection-name";
exports.NUMBER                = "number";

exports.DELETE = "DELETE";
exports.GET    = "GET";
exports.HEAD   = "HEAD";
exports.POST   = "POST";
exports.PUT    = "PUT";

exports.HTTP_OK                 = 200;

exports.HTTP_NOT_FOUND          = 404;
exports.HTTP_METHOD_NOT_ALLOWED = 405;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
