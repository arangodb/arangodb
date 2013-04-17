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
/// @REST{POST /_api/cursor}
///
/// The query details include the query string plus optional query options and
/// bind parameters. These values need to be passed in a JSON representation in
/// the body of the POST request.
///
/// The following attributes can be used inside the JSON object:
///
/// - @LIT{query}: contains the query string to be executed (mandatory)
///
/// - @LIT{count}: boolean flag that indicates whether the number of documents
///   found should be returned as "count" attribute in the result set (optional).
///   Calculating the "count" attribute might have a performance penalty for
///   some queries so this option is turned off by default.
///
/// - @LIT{batchSize}: maximum number of result documents to be transferred from
///   the server to the client in one roundtrip (optional). If this attribute is
///   not set, a server-controlled default value will be used.
///
/// - @LIT{bindVars}: key/value list of bind parameters (optional). 
///
/// If the result set can be created by the server, the server will respond with
/// @LIT{HTTP 201}. The body of the response will contain a JSON object with the
/// result set.
///
/// The JSON object has the following properties:
///
/// - @LIT{error}: boolean flag to indicate that an error occurred (@LIT{false}
///   in this case)
///
/// - @LIT{code}: the HTTP status code
///
/// - @LIT{result}: an array of result documents (might be empty if query has no results)
///
/// - @LIT{hasMore}: a boolean indicator whether there are more results
///   available on the server
///
/// - @LIT{count}: the total number of result documents available (only
///   available if the query was executed with the @LIT{count} attribute set.
///
/// - @LIT{id}: id of temporary cursor created on the server (optional, see above)
///
/// If the JSON representation is malformed or the query specification is
/// missing from the request, the server will respond with @LIT{HTTP 400}.
///
/// The body of the response will contain a JSON object with additional error
/// details. The object has the following attributes:
///
/// - @LIT{error}: boolean flag to indicate that an error occurred (@LIT{true} in this case)
///
/// - @LIT{code}: the HTTP status code
///
/// - @LIT{errorNum}: the server error number
///
/// - @LIT{errorMessage}: a descriptive error message
///
/// If the query specification is complete, the server will process the query. If an
/// error occurs during query processing, the server will respond with @LIT{HTTP 400}.
/// Again, the body of the response will contain details about the error.
///
/// A list of query errors can be found @ref ArangoErrors here.
///
/// @EXAMPLES
///
/// Executes a query and extract the result in a single go:
///
/// @verbinclude api-cursor-create-for-limit-return-single
///
/// Bad queries:
///
/// @verbinclude api-cursor-missing-body
///
/// @verbinclude api-cursor-unknown-collection
////////////////////////////////////////////////////////////////////////////////

function POST_api_cursor(req, res) {
  if (req.suffix.length != 0) {
    actions.resultNotFound(req, res, arangodb.ERROR_CURSOR_NOT_FOUND);
    return;
  }

  var json = actions.getJsonBody(req, res);

  if (json === undefined) {
    actions.resultBad(req, res, arangodb.ERROR_QUERY_EMPTY);
    return;
  }

  var cursor;

  if (json.query != undefined) {
    cursor = QUERY(json.query, 
                   json.bindVars, 
                   (json.count != undefined ? json.count : false),
                   json.batchSize, 
                   (json.batchSize == undefined));  
  }
  else {
    actions.resultBad(req, res, arangodb.ERROR_QUERY_EMPTY);
    return;
  }
   
  // error occurred
  if (cursor instanceof Error) {
    actions.resultException(req, res, cursor);
    return;
  }

  // this might dispose or persist the cursor
  actions.resultCursor(req, res, cursor, actions.HTTP_CREATED, { countRequested: json.count ? true : false });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the next results from an existing cursor
///
/// @RESTHEADER{PUT /_api/cursor,reads next batch from a cursor}
///
/// @REST{PUT /_api/cursor/@FA{cursor-identifier}}
///
/// If the cursor is still alive, returns an object with the following
/// attributes.
///
/// - @LIT{id}: the @FA{cursor-identifier}
/// - @LIT{result}: a list of documents for the current batch
/// - @LIT{hasMore}: @LIT{false} if this was the last batch
/// - @LIT{count}: if present the total number of elements
///
/// Note that even if @LIT{hasMore} returns @LIT{true}, the next call might
/// still return no documents. If, however, @LIT{hasMore} is @LIT{false}, then
/// the cursor is exhausted.  Once the @LIT{hasMore} attribute has a value of
/// @LIT{false}, the client can stop.
///
/// The server will respond with @LIT{HTTP 200} in case of success. If the
/// cursor identifier is ommitted or somehow invalid, the server will respond
/// with @LIT{HTTP 404}.
///
/// @EXAMPLES
///
/// Valid request for next batch:
///
/// @verbinclude api-cursor-create-for-limit-return-cont
///
/// Missing identifier
///
/// @verbinclude api-cursor-missing-cursor-identifier
///
/// Unknown identifier
///
/// @verbinclude api-cursor-invalid-cursor-identifier
////////////////////////////////////////////////////////////////////////////////

function PUT_api_cursor (req, res) {
  if (req.suffix.length != 1) {
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
/// @RESTHEADER{DELETE /_api/cursor,deletes a cursor}
///
/// @REST{DELETE /_api/cursor/@FA{cursor-identifier}}
///
/// Deletes the cursor and frees the resources associated with it. 
///
/// The cursor will automatically be destroyed on the server when the client has
/// retrieved all documents from it. The client can also explicitly destroy the
/// cursor at any earlier time using an HTTP DELETE request. The cursor id must
/// be included as part of the URL.
/// 
/// In case the server is aware of the cursor, it will respond with @LIT{HTTP
/// 202}. Otherwise, it will respond with @LIT{404}.
/// 
/// Cursors that have been explicitly destroyed must not be used afterwards. If
/// a cursor is used after it has been destroyed, the server will respond with
/// @LIT{HTTP 404} as well.
///
/// Note: the server will also destroy abandoned cursors automatically after a 
/// certain server-controlled timeout to avoid resource leakage.
///
/// @EXAMPLES
///
/// @verbinclude api-cursor-delete
////////////////////////////////////////////////////////////////////////////////

function DELETE_api_cursor(req, res) {
  if (req.suffix.length != 1) {
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
        case (actions.POST) : 
          POST_api_cursor(req, res); 
          break;

        case (actions.PUT) :  
          PUT_api_cursor(req, res); 
          break;

        case (actions.DELETE) :  
          DELETE_api_cursor(req, res); 
          break;

        default:
          actions.resultUnsupported(req, res);
      }
    }
    catch (err) {
      actions.resultException(req, res, err);
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
