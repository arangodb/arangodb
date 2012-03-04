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

////////////////////////////////////////////////////////////////////////////////
/// @page JSModuleActionsTOC
///
/// <ol>
///  <li>@ref JSModuleActionsDefineHttp "defineHttp"</li>
/// </ol>
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @page JSModuleActions Module "actions"
///
/// The action module provides the infrastructure for defining HTTP actions.
///
/// <hr>
/// @copydoc JSModuleActionsTOC
/// <hr>
///
/// @anchor JSModuleActionsDefineHttp
/// @copydetails JSF_defineHttp
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Json V8 JSON
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
/// slashes. Note that this action will also be call, if a url is given such that
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
/// Note that the url for "user" actions is automatically prefixed
/// with @LIT{action}.
///
/// It is possible to specify a list of contexts, in case an actions belongs to
/// more than one context.
///
/// @FA{options.callback}(@FA{request}, @FA{response})
///
/// The request argument contains a description of the request. A request
/// parameter @LIT{foo} is accessible as @LIT{request.parametrs.foo}. A request
/// header @LIT{bar} is accessible as @LIT{request.headers.bar}. Assume that
/// the action is defined for the url "/foo/bar" and the request url is
/// "/foo/bar/hugo/egon". Then the suffix parts @LIT{[ "hugon", "egon" ]}
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
    url = "action/" + url;
  }

  if (typeof callback !== "function") {
    console.error("callback for '" + url + "' must be a function, got '" + (typeof callback) + "'");
    return;
  }

  console.debug("callback ", callback, "\n");

  for (var i = 0;  i < contexts.length;  ++i) {
    var context = contexts[i];

    try {
      internal.defineAction(url, "CLIENT", callback, parameter);
      console.debug("defining action '" + url + "' in context " + context);
    }
    catch (err) {
      console.error("action '" + url + "' encountered error: " + err);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a result
///
/// @FUN{actionResult(@FA{req}, @FA{res}, @FA{code}, @FA{result}, @FA{headers})}
///
/// The functions returns a result object. @FA{code} is the status code
/// to return.
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
/// @brief returns an error
///
/// @FUN{actionError(@FA{req}, @FA{res}, @FA{errorMessage})}
///
/// The functions returns an error message. The status code is 500 and the
/// returned object is an array with an attribute @LIT{error} containing
/// the error message @FA{errorMessage}.
////////////////////////////////////////////////////////////////////////////////

function actionError (req, res, err) {
  res.responseCode = 500;
  res.contentType = "application/json";
  res.body = JSON.stringify({ 'error' : "" + err });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a result of a query as documents
///
/// @FUN{queryResult(@FA{req}, @FA{res}, @FA{query})}
///
/// The functions returns the result of a query using pagination. It assumes
/// that the request has the numerical parameters @LIT{blocksize} and @LIT{page}
/// or @LIT{offset}. @LIT{blocksize} determines the maximal number of result
/// documents to return. You can either specify @LIT{page} or @LIT{offset}.
/// @LIT{page} is the number of pages to skip, i. e. @LIT{page} *
/// @LIT{blocksize} documents. @LIT{offset} is the number of documents to skip.
///
/// If you are using pagination, than the query must be a sorted query.
////////////////////////////////////////////////////////////////////////////////

function queryResult (req, res, query) {
  var result;
  var offset = 0;
  var page = 0;
  var blocksize = 0;
  var t;
  var result;

  t = time();

  if (req.blocksize) {
    blocksize = req.blocksize;

    if (req.page) {
      page = req.page;
      offset = page * blocksize;
      query = query.skip(offset).limit(blocksize);
    }
    else {
      query = query.limit(blocksize);
    }
  }

  result = query.toArray();

  result = {
    total : query.count(),
    count : query.count(true),
    offset : offset,
    blocksize : blocksize,
    page : page,
    runtime : internal.time() - t,
    documents : result
  };

  res.responseCode = 200;
  res.contentType = "application/json";
  res.body = JSON.stringify(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a result of a query as references
///
/// @FUN{queryReferences(@FA{req}, @FA{res}, @FA{query})}
///
/// The methods works like @FN{queryResult} but instead of the documents only
/// document identifiers are returned.
////////////////////////////////////////////////////////////////////////////////

function queryReferences (req, res, query) {
  var result;
  var offset = 0;
  var page = 0;
  var blocksize = 0;
  var t;
  var result;

  t = internal.time();

  if (req.blocksize) {
    blocksize = req.blocksize;

    if (req.page) {
      page = req.page;
      offset = page * blocksize;
      query = query.skip(offset).limit(blocksize);
    }
    else {
      query = query.limit(blocksize);
    }
  }

  result = [];

  while (query.hasNext()) {
    result.push(query.nextRef());
  }

  result = {
    total : query.count(),
    count : query.count(true),
    offset : offset,
    blocksize : blocksize,
    page : page,
    runtime : internal.time() - t,
    references : result
  };

  res.responseCode = 200;
  res.contentType = "application/json";
  res.body = JSON.stringify(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

exports.defineHttp = defineHttp;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
