/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, CURSOR, DELETE_CURSOR */

////////////////////////////////////////////////////////////////////////////////
/// @brief query results cursor actions
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
/// @author Achim Brandt
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var arangodb = require("org/arangodb");
var actions = require("org/arangodb/actions");
var internal = require("internal");

var ArangoError = arangodb.ArangoError; 
var QUERY = internal.AQL_QUERY;

// -----------------------------------------------------------------------------
// --SECTION--                                                  global variables
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create a cursor and return the first results
///
/// @RESTHEADER{POST /_api/cursor,creates a cursor}
///
/// @RESTBODYPARAM{query,json,required}
/// The following attributes can be used inside the JSON object:
/// - `query`: contains the query string to be executed (mandatory)
/// - `count`: boolean flag that indicates whether the number of documents
///   found should be returned as "count" attribute in the result set (optional).
///   Calculating the "count" attribute might have a performance penalty for
///   some queries so this option is turned off by default.
/// - `batchSize`: maximum number of result documents to be transferred from
///   the server to the client in one roundtrip (optional). If this attribute is
///   not set, a server-controlled default value will be used.
/// - `bindVars`: key/value list of bind parameters (optional). 
///
/// @RESTDESCRIPTION
/// The query details include the query string plus optional query options and
/// bind parameters. These values need to be passed in a JSON representation in
/// the body of the POST request.
///
/// The following attributes can be used inside the JSON object:
///
/// - `query`: contains the query string to be executed (mandatory)
///
/// - `count`: boolean flag that indicates whether the number of documents
///   found should be returned as "count" attribute in the result set (optional).
///   Calculating the "count" attribute might have a performance penalty for
///   some queries so this option is turned off by default.
///
/// - `batchSize`: maximum number of result documents to be transferred from
///   the server to the client in one roundtrip (optional). If this attribute is
///   not set, a server-controlled default value will be used.
///
/// - `bindVars`: key/value list of bind parameters (optional). 
///
/// If the result set can be created by the server, the server will respond with
/// `HTTP 201`. The body of the response will contain a JSON object with the
/// result set.
///
/// The JSON object has the following properties:
///
/// - `error`: boolean flag to indicate that an error occurred (`false`
///   in this case)
///
/// - `code`: the HTTP status code
///
/// - `result`: an array of result documents (might be empty if query has no results)
///
/// - `hasMore`: a boolean indicator whether there are more results
///   available on the server
///
/// - `count`: the total number of result documents available (only
///   available if the query was executed with the `count` attribute set.
///
/// - `id`: id of temporary cursor created on the server (optional, see above)
///
/// If the JSON representation is malformed or the query specification is
/// missing from the request, the server will respond with `HTTP 400`.
///
/// The body of the response will contain a JSON object with additional error
/// details. The object has the following attributes:
///
/// - `error`: boolean flag to indicate that an error occurred (`true` in this case)
///
/// - `code`: the HTTP status code
///
/// - `errorNum`: the server error number
///
/// - `errorMessage`: a descriptive error message
///
/// If the query specification is complete, the server will process the query. If an
/// error occurs during query processing, the server will respond with `HTTP 400`.
/// Again, the body of the response will contain details about the error.
///
/// A list of query errors can be found @ref ArangoErrors here.
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{201}
/// is returned if the result set can be created by the server.
///
/// @RESTRETURNCODE{400}
/// is returned if the JSON representation is malformed or the query specification is
/// missing from the request.
///
/// @RESTRETURNCODE{404}
/// The server will respond with `HTTP 404` in case a non-existing collection is
/// accessed in the query.
///
/// @EXAMPLES
///
/// Executes a query and extract the result in a single go:
///
/// @EXAMPLE_ARANGOSH_RUN{RestCursorCreateCursorForLimitReturnSingle}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
/// 
///     db.products.save({"hello1":"world1"});
///     db.products.save({"hello2":"world1"});
///
///     var url = "/_api/cursor";
///     var body = {
///       query: "FOR p IN products LIMIT 2 RETURN p",
///       count: true, 
///       batchSize: 2
///     };
/// 
///     var response = logCurlRequest('POST', url, JSON.stringify(body));
/// 
///     assert(response.code === 201);
/// 
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Executes a query and extract part of the result:
///
/// @EXAMPLE_ARANGOSH_RUN{RestCursorCreateCursorForLimitReturn}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
/// 
///     db.products.save({"hello1":"world1"});
///     db.products.save({"hello2":"world1"});
///     db.products.save({"hello3":"world1"});
///     db.products.save({"hello4":"world1"});
///     db.products.save({"hello5":"world1"});
///
///     var url = "/_api/cursor";
///     var body = { 
///       query: "FOR p IN products LIMIT 5 RETURN p",
///       count: true,
///       batchSize: 2
///     };
/// 
///     var response = logCurlRequest('POST', url, JSON.stringify(body));
/// 
///     assert(response.code === 201);
/// 
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Bad queries:
///
/// Missing body:
///
/// @EXAMPLE_ARANGOSH_RUN{RestCursorCreateCursorMissingBody}
///     var url = "/_api/cursor";
/// 
///     var response = logCurlRequest('POST', url, '');
/// 
///     assert(response.code === 400);
/// 
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Unknown collection:
///
/// @EXAMPLE_ARANGOSH_RUN{RestCursorCreateCursorUnknownCollection}
///     var url = "/_api/cursor";
///     var body = { 
///       query: "FOR u IN unknowncoll LIMIT 2 RETURN u", 
///       count: true, 
///       batchSize: 2 
///     };
/// 
///     var response = logCurlRequest('POST', url, JSON.stringify(body));
/// 
///     assert(response.code === 404);
/// 
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

function post_api_cursor(req, res) {
  if (req.suffix.length !== 0) {
    actions.resultNotFound(req, res, arangodb.ERROR_CURSOR_NOT_FOUND);
    return;
  }

  var json = actions.getJsonBody(req, res);

  if (json === undefined) {
    actions.resultBad(req, res, arangodb.ERROR_QUERY_EMPTY);
    return;
  }

  var cursor;

  if (json.query !== undefined) {
    cursor = QUERY(json.query, 
                   json.bindVars, 
                   (json.count !== undefined ? json.count : false),
                   json.batchSize, 
                   (json.batchSize === undefined));  
  }
  else {
    actions.resultBad(req, res, arangodb.ERROR_QUERY_EMPTY);
    return;
  }
   
  // error occurred
  if (cursor instanceof Error) {
    actions.resultException(req, res, cursor, undefined, false);
    return;
  }

  // this might dispose or persist the cursor
  actions.resultCursor(req, 
                       res, 
                       cursor, 
                       actions.HTTP_CREATED, 
                       { countRequested: json.count ? true : false });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the next results from an existing cursor
///
/// @RESTHEADER{PUT /_api/cursor/`cursor-identifier`,reads next batch from a cursor}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{cursor-identifer,string,required}
/// The name of the cursor
///
/// @RESTDESCRIPTION
///
/// If the cursor is still alive, returns an object with the following
/// attributes.
///
/// - `id`: the `cursor-identifier`
/// - `result`: a list of documents for the current batch
/// - `hasMore`: `false` if this was the last batch
/// - `count`: if present the total number of elements
///
/// Note that even if `hasMore` returns `true`, the next call might
/// still return no documents. If, however, `hasMore` is `false`, then
/// the cursor is exhausted.  Once the `hasMore` attribute has a value of
/// `false`, the client can stop.
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// The server will respond with `HTTP 200` in case of success. 
///
/// @RESTRETURNCODE{404}
/// If the cursor identifier is ommitted or somehow invalid, the server will respond
/// with `HTTP 404`.
///
/// @EXAMPLES
///
/// Valid request for next batch:
///
/// @EXAMPLE_ARANGOSH_RUN{RestCursorForLimitReturnCont}
///     var url = "/_api/cursor";
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
/// 
///     db.products.save({"hello1":"world1"});
///     db.products.save({"hello2":"world1"});
///     db.products.save({"hello3":"world1"});
///     db.products.save({"hello4":"world1"});
///     db.products.save({"hello5":"world1"});
///
///     var url = "/_api/cursor";
///     var body = {
///       query: "FOR p IN products LIMIT 5 RETURN p", 
///       count: true, 
///       batchSize: 2 
///     };
///     var response = logCurlRequest('POST', url, JSON.stringify(body));
///
///     var body = response.body.replace(/\\/g, '');
///     var _id = JSON.parse(body).id;
///     response = logCurlRequest('PUT', url + '/' + _id, '');
///     assert(response.code === 200);
/// 
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Missing identifier
///
/// @EXAMPLE_ARANGOSH_RUN{RestCursorMissingCursorIdentifier}
///     var url = "/_api/cursor";
/// 
///     var response = logCurlRequest('PUT', url, '');
/// 
///     assert(response.code === 400);
/// 
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Unknown identifier
///
/// @EXAMPLE_ARANGOSH_RUN{RestCursorInvalidCursorIdentifier}
///     var url = "/_api/cursor/123123";
/// 
///     var response = logCurlRequest('PUT', url, '');
/// 
///     assert(response.code === 400);
/// 
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

function put_api_cursor (req, res) {
  if (req.suffix.length !== 1) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  var cursorId = decodeURIComponent(req.suffix[0]); 
  var cursor = CURSOR(cursorId);

  if (! (cursor instanceof arangodb.ArangoCursor)) {
    actions.resultBad(req, res, arangodb.ERROR_CURSOR_NOT_FOUND);
    return;
  }
    
  try { 
    // note: this might dispose or persist the cursor
    actions.resultCursor(req, res, cursor, actions.HTTP_OK);
  }
  catch (e) {
  }

  // unuse the cursor and garbage-collect it
  cursor.unuse();
  cursor = null;
  internal.wait(0.0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dispose an existing cursor
///
/// @RESTHEADER{DELETE /_api/cursor/`cursor-identifier`,deletes a cursor}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{cursor-identifer,string,required}
/// The name of the cursor
///
/// @RESTDESCRIPTION
/// Deletes the cursor and frees the resources associated with it. 
///
/// The cursor will automatically be destroyed on the server when the client has
/// retrieved all documents from it. The client can also explicitly destroy the
/// cursor at any earlier time using an HTTP DELETE request. The cursor id must
/// be included as part of the URL.
/// 
/// Note: the server will also destroy abandoned cursors automatically after a 
/// certain server-controlled timeout to avoid resource leakage.
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{202}
/// is returned if the server is aware of the cursor.
///
/// @RESTRETURNCODE{404}
/// is returned if the server is not aware of the cursor. It is also
/// returned if a cursor is used after it has been destroyed.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestCursorDelete}
///     var url = "/_api/cursor";
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
/// 
///     db.products.save({"hello1":"world1"});
///     db.products.save({"hello2":"world1"});
///     db.products.save({"hello3":"world1"});
///     db.products.save({"hello4":"world1"});
///     db.products.save({"hello5":"world1"});
///
///     var url = "/_api/cursor";
///     var body = {
///       query: "FOR p IN products LIMIT 5 RETURN p", 
///       count: true,
///       batchSize: 2
///     };
///     var response = logCurlRequest('POST', url, JSON.stringify(body));
///     logJsonResponse(response);
///     var body = response.body.replace(/\\/g, '');
///     var _id = JSON.parse(body).id;
///     response = logCurlRequest('DELETE', url + '/' + _id);
/// 
///     assert(response.code === 202);
/// 
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

function delete_api_cursor(req, res) {
  if (req.suffix.length !== 1) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  var cursorId = decodeURIComponent(req.suffix[0]);
  if (! DELETE_CURSOR(cursorId)) {
    actions.resultNotFound(req, res, arangodb.ERROR_CURSOR_NOT_FOUND);
    return;
  }

  actions.resultOk(req, res, actions.HTTP_ACCEPTED, { id : cursorId });

  // we want the garbage collection to clean unused cursors immediately
  internal.wait(0.0);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       initialiser
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief cursor actions gateway 
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_api/cursor",
  context : "api",

  callback : function (req, res) {
    try {
      switch (req.requestType) {
        case actions.POST: 
          post_api_cursor(req, res); 
          break;

        case actions.PUT:  
          put_api_cursor(req, res); 
          break;

        case actions.DELETE:  
          delete_api_cursor(req, res); 
          break;

        default:
          actions.resultUnsupported(req, res);
      }
    }
    catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
